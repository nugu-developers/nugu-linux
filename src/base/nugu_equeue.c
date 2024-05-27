/*
 * Copyright (c) 2019 SK Telecom Co., Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <glib.h>

#ifdef HAVE_EVENTFD
#include <sys/eventfd.h>
#elif defined(__MSYS__)
#include "base/nugu_winsock.h"
#else
#include <glib-unix.h>
#endif

#include "base/nugu_log.h"
#include "base/nugu_equeue.h"

struct _equeue_typemap {
	NuguEqueueCallback callback;
	NuguEqueueDestroyCallback destroy_callback;
	void *userdata;
};

struct _equeue {
	/*
	 * - eventfd only uses fds[0].
	 * - pipe is read with fds[0] and writes with fds[1].
	 */
	int fds[2];
	guint source;
	GAsyncQueue *pendings;
	struct _equeue_typemap typemap[NUGU_EQUEUE_TYPE_MAX];
#ifdef __MSYS__
	NuguWinSocket *wsock;
#endif
};

struct _econtainer {
	enum nugu_equeue_type type;
	void *data;
};

static struct _equeue *_equeue;
static pthread_mutex_t _lock;

static void on_item_destroy(gpointer data)
{
	struct _econtainer *item = data;
	struct _equeue_typemap *handler;

	handler = &_equeue->typemap[item->type];

	if (handler->destroy_callback)
		handler->destroy_callback(item->data);

	free(item);
}

static gboolean on_event(GIOChannel *channel, GIOCondition cond,
			 gpointer userdata)
{
	ssize_t nread;

	if (!_equeue) {
		nugu_error("event queue not initialized");
		return FALSE;
	}

#ifdef __MSYS__
	if (nugu_winsock_check_for_data(_equeue->fds[0]) == 0) {
		char ev;

		nread = nugu_winsock_read(_equeue->fds[0], &ev, sizeof(ev));
		if (nread <= 0) {
			nugu_error("read failed");
			return FALSE;
		}
	} else {
		return TRUE;
	}
#else
	if (_equeue->fds[1] == -1) {
		uint64_t ev = 0;

		nread = read(_equeue->fds[0], &ev, sizeof(ev));
		if (nread == -1 || nread != sizeof(ev)) {
			nugu_error("read failed: %d, %d", nread, errno);
			return TRUE;
		}
	} else {
		uint8_t ev = 0;

		nread = read(_equeue->fds[0], &ev, sizeof(ev));
		if (nread == -1 || nread != sizeof(ev)) {
			nugu_error("read failed: %d, %d", nread, errno);
			return TRUE;
		}
	}
#endif
	while (g_async_queue_length(_equeue->pendings) > 0) {
		struct _econtainer *item;
		struct _equeue_typemap *handler;

		item = g_async_queue_try_pop(_equeue->pendings);
		if (!item)
			break;

		handler = &_equeue->typemap[item->type];
		if (handler->callback)
			handler->callback(item->type, item->data,
					  handler->userdata);

		if (handler->destroy_callback)
			handler->destroy_callback(item->data);

		free(item);
	}

	return TRUE;
}

int nugu_equeue_initialize(void)
{
	GIOChannel *channel;
#if !defined(HAVE_EVENTFD) && !defined(__MSYS__)
	GError *error = NULL;
#endif

	pthread_mutex_lock(&_lock);

	if (_equeue != NULL) {
		nugu_dbg("equeue already initialized");
		pthread_mutex_unlock(&_lock);
		return 0;
	}

	_equeue = calloc(1, sizeof(struct _equeue));
	if (!_equeue) {
		nugu_error_nomem();
		pthread_mutex_unlock(&_lock);
		return -1;
	}

	_equeue->fds[0] = -1;
	_equeue->fds[1] = -1;

#ifdef HAVE_EVENTFD
	_equeue->fds[0] = eventfd(0, EFD_CLOEXEC);
	if (_equeue->fds[0] < 0) {
		nugu_error("eventfd() failed");
		free(_equeue);
		_equeue = NULL;
		pthread_mutex_unlock(&_lock);
		return -1;
	}
#elif defined(__MSYS__)
	_equeue->wsock = nugu_winsock_create();
	if (_equeue->wsock == NULL) {
		nugu_error("failed to create window socket");
		free(_equeue);
		_equeue = NULL;
		pthread_mutex_unlock(&_lock);
		return -1;
	}
	_equeue->fds[0] =
		nugu_winsock_get_handle(_equeue->wsock, NUGU_WINSOCKET_CLIENT);
	_equeue->fds[1] =
		nugu_winsock_get_handle(_equeue->wsock, NUGU_WINSOCKET_SERVER);
#else
	if (g_unix_open_pipe(_equeue->fds, FD_CLOEXEC, &error) == FALSE) {
		nugu_error("g_unix_open_pipe() failed: %s", error->message);
		g_error_free(error);
		free(_equeue);
		_equeue = NULL;
		pthread_mutex_unlock(&_lock);
		return -1;
	}
	nugu_dbg("pipe fds[0] = %d", _equeue->fds[0]);
	nugu_dbg("pipe fds[1] = %d", _equeue->fds[1]);
#endif

#ifdef __MSYS__
	channel = g_io_channel_win32_new_socket(_equeue->fds[0]);
#else
	channel = g_io_channel_unix_new(_equeue->fds[0]);
#endif
	_equeue->source = g_io_add_watch(channel, G_IO_IN, on_event, NULL);
	g_io_channel_unref(channel);

	_equeue->pendings = g_async_queue_new_full(on_item_destroy);

	pthread_mutex_unlock(&_lock);

	return 0;
}

void nugu_equeue_deinitialize(void)
{
	pthread_mutex_lock(&_lock);

	if (_equeue == NULL) {
		nugu_error("equeue not initialized");
		pthread_mutex_unlock(&_lock);
		return;
	}

#ifdef __MSYS__
	nugu_winsock_remove(_equeue->wsock);
#else
	if (_equeue->fds[0] != -1)
		close(_equeue->fds[0]);
	if (_equeue->fds[1] != -1)
		close(_equeue->fds[1]);
#endif
	if (_equeue->source > 0)
		g_source_remove(_equeue->source);

	if (_equeue->pendings) {
		/* Remove pendings */
		nugu_info("remove pending equeue items: %d",
			  g_async_queue_length(_equeue->pendings));

		while (g_async_queue_length(_equeue->pendings) > 0) {
			struct _econtainer *item;
			struct _equeue_typemap *handler;

			item = g_async_queue_try_pop(_equeue->pendings);
			if (!item)
				break;

			handler = &_equeue->typemap[item->type];
			if (handler->destroy_callback)
				handler->destroy_callback(item->data);

			free(item);
		}

		g_async_queue_unref(_equeue->pendings);
	}

	free(_equeue);
	_equeue = NULL;

	pthread_mutex_unlock(&_lock);
}

int nugu_equeue_set_handler(enum nugu_equeue_type type,
			    NuguEqueueCallback callback,
			    NuguEqueueDestroyCallback destroy_callback,
			    void *userdata)
{
	g_return_val_if_fail(type < NUGU_EQUEUE_TYPE_MAX, -1);
	g_return_val_if_fail(callback != NULL, -1);

	pthread_mutex_lock(&_lock);

	if (_equeue == NULL) {
		nugu_error("equeue not initialized");
		pthread_mutex_unlock(&_lock);
		return -1;
	}

	_equeue->typemap[type].callback = callback;
	_equeue->typemap[type].destroy_callback = destroy_callback;
	_equeue->typemap[type].userdata = userdata;

	pthread_mutex_unlock(&_lock);

	return 0;
}

int nugu_equeue_unset_handler(enum nugu_equeue_type type)
{
	g_return_val_if_fail(type < NUGU_EQUEUE_TYPE_MAX, -1);

	pthread_mutex_lock(&_lock);

	if (_equeue == NULL) {
		nugu_error("equeue not initialized");
		pthread_mutex_unlock(&_lock);
		return -1;
	}

	_equeue->typemap[type].callback = NULL;
	_equeue->typemap[type].destroy_callback = NULL;
	_equeue->typemap[type].userdata = NULL;

	pthread_mutex_unlock(&_lock);

	return 0;
}

int nugu_equeue_push(enum nugu_equeue_type type, void *data)
{
	struct _econtainer *item;
	ssize_t written;

	g_return_val_if_fail(type < NUGU_EQUEUE_TYPE_MAX, -1);

	pthread_mutex_lock(&_lock);

	if (_equeue == NULL) {
		nugu_error("equeue not initialized");
		pthread_mutex_unlock(&_lock);
		return -1;
	}

	if (!_equeue->typemap[type].callback) {
		nugu_error("Please add handler for %d type first", type);
		pthread_mutex_unlock(&_lock);
		return -1;
	}

	item = malloc(sizeof(struct _econtainer));
	if (!item) {
		nugu_error_nomem();
		pthread_mutex_unlock(&_lock);
		return -1;
	}

	item->data = data;
	item->type = type;

	g_async_queue_push(_equeue->pendings, item);

#ifdef __MSYS__
	if (_equeue->fds[1] != -1) {
		char ev = '1';

		written = nugu_winsock_write(_equeue->fds[1], &ev, sizeof(ev));
		if (written != sizeof(ev)) {
			nugu_error("error write");
			pthread_mutex_unlock(&_lock);
			return -1;
		}
	}
#else
	if (_equeue->fds[1] == -1) {
		uint64_t ev = 1;

		written = write(_equeue->fds[0], &ev, sizeof(ev));
		if (written != sizeof(ev)) {
			nugu_error("write failed: %d, %d", written, errno);
			pthread_mutex_unlock(&_lock);
			return -1;
		}

	} else {
		uint8_t ev = 1;

		written = write(_equeue->fds[1], &ev, sizeof(ev));
		if (written != sizeof(ev)) {
			nugu_error("write failed: %d, %d", written, errno);
			pthread_mutex_unlock(&_lock);
			return -1;
		}
	}
#endif
	pthread_mutex_unlock(&_lock);

	return 0;
}
