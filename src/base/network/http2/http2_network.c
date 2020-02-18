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

#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <errno.h>

#include <glib.h>

#include "curl/curl.h"

#include "base/nugu_log.h"
#include "base/nugu_network_manager.h"

#include "threadsync.h"
#include "http2_network.h"

enum request_type {
	REQUEST_ADD, /**< http2_network_add_request */
	REQUEST_REMOVE /**< http2_network_remove_request_sync */
};

struct request_item {
	enum request_type type;
	HTTP2Request *req;

	/*
	 * When calling curl_easy_cleanup() through http2_request_free(),
	 * the multi handle is referred to internally, which may cause a
	 * conflict with curl_multi_remove_handle().
	 * Therefore, add a lock to wait for curl_multi_remove_handle().
	 */
	ThreadSync *sync;
};

struct _http2_network {
	int running;
	gboolean log;
	pthread_t thread_id;

	const char *useragent;

	/* Communicate to thread context from gmainloop */
	int wakeup_fd;
	GAsyncQueue *requests;

	/* Handled only within the thread context */
	CURLM *handle;
	GList *hlist;

	/* authorization header */
	gchar *token;

	/* thread creation check */
	ThreadSync *sync_init;
};

static struct request_item *_request_item_new(enum request_type type,
					      HTTP2Request *req)
{
	struct request_item *item;

	item = malloc(sizeof(struct request_item));
	if (!item) {
		nugu_error_nomem();
		return NULL;
	}

	item->type = type;
	item->req = req;
	item->sync = thread_sync_new();

	return item;
}

static void _request_item_free(struct request_item *item)
{
	g_return_if_fail(item != NULL);

	thread_sync_free(item->sync);

	free(item);
}

static int _process_add(HTTP2Network *net, struct request_item *item)
{
	CURLMcode rc;
	CURL *req;

	req = http2_request_get_handle(item->req);
	if (!req)
		return -1;

	rc = curl_multi_add_handle(net->handle, req);
	if (rc != CURLM_OK) {
		nugu_error("curl_multi_add_handle() failed. ret=%d", rc);
		return -1;
	}

	net->hlist = g_list_append(net->hlist, req);

	return 0;
}

static int _process_remove(HTTP2Network *net, struct request_item *item)
{
	CURLMcode rc;
	CURL *req;

	req = http2_request_get_handle(item->req);
	if (!req)
		return -1;

	rc = curl_multi_remove_handle(net->handle, req);
	if (rc != CURLM_OK) {
		nugu_error("curl_multi_remove_handle() failed. ret=%d", rc);
		return -1;
	}

	net->hlist = g_list_remove(net->hlist, req);

	http2_request_unref(item->req);

	return 0;
}

static void *_loop(void *data)
{
	HTTP2Network *net = data;
	CURLMcode mc;
	CURLMsg *curl_message;
	int numfds;
	char *fake_p;
	int still_running = 0;
	int i;
	struct curl_waitfd extra_fds[1];

	nugu_dbg("thread started");

	net->running = 1;
	net->handle = curl_multi_init();
	curl_multi_setopt(net->handle, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);

	thread_sync_signal(net->sync_init);

	while (net->running) {
		mc = curl_multi_perform(net->handle, &still_running);
		if (mc != CURLM_OK) {
			nugu_error("curl_multi failed, code %d.", mc);
			break;
		}

		/*
		 * cleanup the completed requests
		 */
		i = 0;
		while (1) {
			curl_message = curl_multi_info_read(net->handle, &i);
			if (curl_message == NULL)
				break;

			if (curl_message->msg != CURLMSG_DONE)
				continue;

			curl_easy_getinfo(curl_message->easy_handle,
					  CURLINFO_PRIVATE, &fake_p);

			curl_multi_remove_handle(net->handle,
						 curl_message->easy_handle);
			net->hlist = g_list_remove(net->hlist,
						   curl_message->easy_handle);

			nugu_dbg("completed req(%p) (code=%d)", fake_p,
				 http2_request_get_response_code(
					 (HTTP2Request *)fake_p));
			http2_request_emit_completed((HTTP2Request *)fake_p);
			http2_request_unref((HTTP2Request *)fake_p);
		}

		/*
		 * wait for activity, timeout or "nothing"
		 *
		 * Refer: https://curl.haxx.se/libcurl/c/curl_multi_wait.html
		 */
		extra_fds[0].fd = net->wakeup_fd;
		extra_fds[0].events = CURL_WAIT_POLLIN | CURL_WAIT_POLLPRI;
		extra_fds[0].revents = 0;

		mc = curl_multi_wait(net->handle, extra_fds, 1, 1000, &numfds);
		if (mc != CURLM_OK) {
			nugu_error("curl_multi_wait failed, code %d.", mc);
			break;
		}

		/*
		 * wakeup by eventfd (add or remove request)
		 */
		if (extra_fds[0].revents & extra_fds[0].events) {
			uint64_t ev = 0;
			ssize_t nread;

			nread = read(extra_fds[0].fd, &ev, sizeof(ev));
			if (nread == -1 || nread != sizeof(ev)) {
				nugu_error("read failed");
				break;
			}

			while (g_async_queue_length(net->requests) > 0) {
				struct request_item *item;

				item = g_async_queue_try_pop(net->requests);
				if (!item)
					break;

				if (item->type == REQUEST_ADD) {
					_process_add(net, item);
					_request_item_free(item);
				} else if (item->type == REQUEST_REMOVE) {
					_process_remove(net, item);
					thread_sync_signal(item->sync);
				}
			}
		}
	}

	/* remove incomplete requests */
	if (net->hlist) {
		GList *cur;

		cur = net->hlist;
		while (cur) {
			CURL *easy = cur->data;
			char *url = NULL;

			curl_easy_getinfo(easy, CURLINFO_PRIVATE, &fake_p);
			curl_easy_getinfo(easy, CURLINFO_EFFECTIVE_URL, &url);

			nugu_dbg("remove incomplete req=%p (%s)", fake_p, url);
			http2_request_unref((HTTP2Request *)fake_p);
			cur = cur->next;
		}

		g_list_free(net->hlist);
		net->hlist = NULL;
	}

	curl_multi_cleanup(net->handle);
	net->handle = NULL;

	nugu_dbg("thread finished");

	return NULL;
}

HTTP2Network *http2_network_new()
{
	struct _http2_network *net;

	net = calloc(1, sizeof(struct _http2_network));
	if (!net) {
		nugu_error_nomem();
		return NULL;
	}

	net->wakeup_fd = eventfd(0, EFD_CLOEXEC);
	net->requests =
		g_async_queue_new_full((GDestroyNotify)_request_item_free);
	net->useragent = nugu_network_manager_peek_useragent();
	net->sync_init = thread_sync_new();

	return net;
}

void http2_network_free(HTTP2Network *net)
{
	g_return_if_fail(net != NULL);

	if (net->running) {
		uint64_t ev = 1;
		ssize_t written;

		net->running = 0;

		/* wakeup request using eventfd */
		written = write(net->wakeup_fd, &ev, sizeof(uint64_t));
		if (written != sizeof(uint64_t))
			nugu_error("write failed");
	}

	pthread_join(net->thread_id, NULL);

	/* Remove requests that are not yet passed to the thread loop. */
	while (g_async_queue_length(net->requests) > 0) {
		struct request_item *item;

		item = g_async_queue_try_pop(net->requests);
		if (!item)
			break;

		_request_item_free(item);
	}

	if (net->requests)
		g_async_queue_unref(net->requests);

	close(net->wakeup_fd);

	if (net->sync_init)
		thread_sync_free(net->sync_init);

	g_free(net->token);

	memset(net, 0, sizeof(HTTP2Network));
	free(net);
}

int http2_network_set_token(HTTP2Network *net, const char *token)
{
	g_return_val_if_fail(net != NULL, -1);

	g_free(net->token);

	if (token)
		net->token = g_strdup_printf("authorization: Bearer %s", token);
	else
		net->token = NULL;

	return 0;
}

int http2_network_add_request(HTTP2Network *net, HTTP2Request *req)
{
	uint64_t ev = 1;
	ssize_t written;
	struct request_item *item;

	g_return_val_if_fail(net != NULL, -1);
	g_return_val_if_fail(req != NULL, -1);

	if (thread_sync_check(net->sync_init) == 0) {
		nugu_error("network is not started");
		return -1;
	}

	if (net->log)
		http2_request_enable_curl_log(req);

	if (net->token)
		http2_request_add_header(req, net->token);

	http2_request_set_useragent(req, net->useragent);

	item = _request_item_new(REQUEST_ADD, req);
	if (!item)
		return -1;

	http2_request_ref(req);
	g_async_queue_push(net->requests, item);

	/* wakeup request using eventfd */
	written = write(net->wakeup_fd, &ev, sizeof(uint64_t));
	if (written != sizeof(uint64_t))
		nugu_error("write failed");

	return 0;
}

int http2_network_remove_request_sync(HTTP2Network *net, HTTP2Request *req)
{
	uint64_t ev = 1;
	ssize_t written;
	struct request_item *item;

	g_return_val_if_fail(net != NULL, -1);
	g_return_val_if_fail(req != NULL, -1);

	if (thread_sync_check(net->sync_init) == 0) {
		nugu_error("network is not started");
		return -1;
	}

	item = _request_item_new(REQUEST_REMOVE, req);
	if (!item)
		return -1;

	g_async_queue_push(net->requests, item);

	/* wakeup request using eventfd */
	written = write(net->wakeup_fd, &ev, sizeof(uint64_t));
	if (written != sizeof(uint64_t))
		nugu_error("write failed");

	/* wait for curl_multi_remove_handle() (maximum 5 secs) */
	if (thread_sync_wait_secs(item->sync, 5) != THREAD_SYNC_RESULT_OK) {
		nugu_error("http2_network_remove_request_sync() timeout");
		return -1;
	}

	_request_item_free(item);

	return 0;
}

int http2_network_start(HTTP2Network *net)
{
	int ret;

	g_return_val_if_fail(net != NULL, -1);

	if (net->running == 1)
		return 0;

	ret = pthread_create(&net->thread_id, NULL, _loop, net);
	if (ret < 0) {
		nugu_error("pthread_create() failed.");
		return -1;
	}

	ret = pthread_setname_np(net->thread_id, "http2");
	if (ret < 0)
		nugu_error("pthread_setname_np() failed");

	/* Wait for thread creation (maximum 5 secs) */
	if (thread_sync_wait_secs(net->sync_init, 5) != THREAD_SYNC_RESULT_OK) {
		nugu_error("thread creation timeout");
		return -1;
	}

	return 0;
}

void http2_network_enable_curl_log(HTTP2Network *net)
{
	g_return_if_fail(net != NULL);

	net->log = TRUE;
}

void http2_network_disable_curl_log(HTTP2Network *net)
{
	g_return_if_fail(net != NULL);

	net->log = FALSE;
}
