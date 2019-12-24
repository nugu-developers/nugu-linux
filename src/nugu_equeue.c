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
#include <sys/eventfd.h>

#include <glib.h>

#include "base/nugu_log.h"
#include "base/nugu_equeue.h"

struct _equeue_typemap {
	NuguEqueueCallback callback;
	NuguEqueueDestroyCallback destroy_callback;
	void *userdata;
};

struct _equeue {
	int efd;
	guint source;
	GAsyncQueue *pendings;
	struct _equeue_typemap typemap[NUGU_EQUEUE_TYPE_MAX];
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
	uint64_t ev = 0;
	ssize_t nread;

	if (!_equeue) {
		nugu_error("event queue not initialized");
		return FALSE;
	}

	nread = read(_equeue->efd, &ev, sizeof(ev));
	if (nread == -1 || nread != sizeof(ev)) {
		nugu_error("read failed");
		return TRUE;
	}

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

	pthread_mutex_lock(&_lock);

	if (_equeue != NULL) {
		nugu_dbg("equeue already initialized");
		pthread_mutex_unlock(&_lock);
		return 0;
	}

	_equeue = calloc(1, sizeof(struct _equeue));
	if (!_equeue) {
		error_nomem();
		pthread_mutex_unlock(&_lock);
		return -1;
	}

	_equeue->efd = eventfd(0, EFD_CLOEXEC);
	if (_equeue->efd < 0) {
		nugu_error("eventfd() failed");
		free(_equeue);
		_equeue = NULL;
		pthread_mutex_unlock(&_lock);
		return -1;
	}

	channel = g_io_channel_unix_new(_equeue->efd);
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

	if (_equeue->efd != -1)
		close(_equeue->efd);

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
	uint64_t ev = 1;
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
		error_nomem();
		pthread_mutex_unlock(&_lock);
		return -1;
	}

	item->data = data;
	item->type = type;

	g_async_queue_push(_equeue->pendings, item);

	written = write(_equeue->efd, &ev, sizeof(uint64_t));
	if (written != sizeof(uint64_t)) {
		nugu_error("write failed");
		pthread_mutex_unlock(&_lock);
		return -1;
	}

	pthread_mutex_unlock(&_lock);

	return 0;
}
