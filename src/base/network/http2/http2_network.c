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
#include <errno.h>

#include <glib.h>

#ifdef HAVE_EVENTFD
#include <sys/eventfd.h>
#elif defined(_WIN32)
#include "base/nugu_winsock.h"
#else
#include <glib-unix.h>
#endif

#ifndef _WIN32
#include <unistd.h>
#else
typedef long ssize_t;
#endif

#include "curl/curl.h"

#include "base/nugu_log.h"
#include "base/nugu_network_manager.h"

#include "threadsync.h"
#include "http2_network.h"

enum request_type {
	REQUEST_ADD, /**< http2_network_add_request */
	REQUEST_REMOVE, /**< http2_network_remove_request */
	REQUEST_RESUME /**< http2_network_resume_request */
};

struct request_item {
	enum request_type type;
	int req_id;
};

struct _http2_network {
	int running;
	gboolean log;
	pthread_t thread_id;

	int req_ids;
	GHashTable *hash;
	pthread_mutex_t lock;

	const char *useragent;

	/*
	 * Communicate to thread context from gmainloop
	 * - eventfd only uses fds[0].
	 * - pipe is read with fds[0] and writes with fds[1].
	 */
	int wakeup_fds[2];
	GAsyncQueue *requests;

	/* Handled only within the thread context */
	CURLM *handle;
	GList *hlist;

	/* authorization header */
	gchar *token;

	/* Last-Asr-Event-Time header */
	gchar *last_asr;

	/* thread creation check */
	ThreadSync *sync_init;

#ifdef _WIN32
	NuguWinSocket *wsock;
#endif
};

static void _curl_code_to_result(HTTP2Request *req, CURLcode code)
{
	enum http2_result result;

	if (!req)
		return;

	switch (code) {
	case CURLE_OK:
		result = HTTP2_RESULT_OK;
		break;
	case CURLE_COULDNT_RESOLVE_PROXY:
		result = HTTP2_RESULT_PROXY_FAIL;
		break;
	case CURLE_COULDNT_RESOLVE_HOST:
		result = HTTP2_RESULT_DNS_FAIL;
		break;
	case CURLE_COULDNT_CONNECT:
		result = HTTP2_RESULT_CONNECT_FAIL;
		break;
	case CURLE_SSL_ENGINE_NOTFOUND:
	case CURLE_SSL_ENGINE_SETFAILED:
	case CURLE_SSL_CERTPROBLEM:
	case CURLE_SSL_CIPHER:
	case CURLE_PEER_FAILED_VERIFICATION:
	case CURLE_USE_SSL_FAILED:
	case CURLE_SSL_ENGINE_INITFAILED:
	case CURLE_SSL_CACERT_BADFILE:
	case CURLE_SSL_SHUTDOWN_FAILED:
	case CURLE_SSL_CRL_BADFILE:
	case CURLE_SSL_ISSUER_ERROR:
	case CURLE_SSL_PINNEDPUBKEYNOTMATCH:
	case CURLE_SSL_INVALIDCERTSTATUS:
		result = HTTP2_RESULT_SSL_FAIL;
		break;
	case CURLE_HTTP_RETURNED_ERROR:
		result = HTTP2_RESULT_HTTP_FAIL;
		break;
	case CURLE_HTTP2:
	case CURLE_HTTP2_STREAM:
		result = HTTP2_RESULT_HTTP2_FAIL;
		break;
	case CURLE_OPERATION_TIMEDOUT:
		result = HTTP2_RESULT_TIMEOUT;
		break;
	default:
		result = HTTP2_RESULT_UNKNOWN;
		break;
	}

	if (code != CURLE_OK)
		nugu_error("convert curl-%d error code to %d", code, result);

	http2_request_set_result(req, result);
}

static struct request_item *_request_item_new(enum request_type type,
					      int req_id)
{
	struct request_item *item;

	item = malloc(sizeof(struct request_item));
	if (!item) {
		nugu_error_nomem();
		return NULL;
	}

	item->type = type;
	item->req_id = req_id;

	return item;
}

static void _request_item_free(struct request_item *item)
{
	g_return_if_fail(item != NULL);

	free(item);
}

static int _process_add(HTTP2Network *net, struct request_item *item)
{
	CURLMcode rc;
	CURL *curl_h;
	HTTP2Request *h2req;

	pthread_mutex_lock(&net->lock);
	h2req = g_hash_table_lookup(net->hash, GINT_TO_POINTER(item->req_id));
	pthread_mutex_unlock(&net->lock);
	if (h2req == NULL) {
		nugu_error("invalid request_id(%d)", item->req_id);
		return -1;
	}

	curl_h = http2_request_get_handle(h2req);
	if (!curl_h) {
		nugu_error("invalid handle, req_id(%d)", item->req_id);
		return -1;
	}

	rc = curl_multi_add_handle(net->handle, curl_h);
	if (rc != CURLM_OK) {
		nugu_error("curl_multi_add_handle() failed. ret=%d", rc);
		return -1;
	}

	net->hlist = g_list_append(net->hlist, curl_h);

	return 0;
}

static int _process_remove(HTTP2Network *net, struct request_item *item)
{
	CURLMcode rc;
	CURL *curl_h;
	HTTP2Request *h2req;

	pthread_mutex_lock(&net->lock);
	h2req = g_hash_table_lookup(net->hash, GINT_TO_POINTER(item->req_id));
	pthread_mutex_unlock(&net->lock);
	if (h2req == NULL) {
		nugu_error("invalid request_id(%d)", item->req_id);
		return -1;
	}

	curl_h = http2_request_get_handle(h2req);
	if (!curl_h) {
		nugu_error("invalid handle, req_id(%d)", item->req_id);
		return -1;
	}

	rc = curl_multi_remove_handle(net->handle, curl_h);
	if (rc != CURLM_OK) {
		nugu_error("curl_multi_remove_handle() failed. ret=%d", rc);
		return -1;
	}

	net->hlist = g_list_remove(net->hlist, curl_h);

	pthread_mutex_lock(&net->lock);
	g_hash_table_remove(net->hash, GINT_TO_POINTER(item->req_id));
	pthread_mutex_unlock(&net->lock);

	nugu_dbg("removed req(%p) (code=%d)", h2req,
		 http2_request_get_response_code(h2req));
	http2_request_emit_finished((h2req));
	http2_request_unref(h2req);

	return 0;
}

static int _process_resume(HTTP2Network *net, struct request_item *item)
{
	CURL *curl_h;
	GList *cur;
	HTTP2Request *h2req;

	pthread_mutex_lock(&net->lock);
	h2req = g_hash_table_lookup(net->hash, GINT_TO_POINTER(item->req_id));
	pthread_mutex_unlock(&net->lock);
	if (h2req == NULL) {
		nugu_error("invalid request_id(%d)", item->req_id);
		return -1;
	}

	curl_h = http2_request_get_handle(h2req);
	if (!curl_h) {
		nugu_error("invalid handle, req_id(%d)", item->req_id);
		return -1;
	}

	cur = net->hlist;
	while (cur) {
		if (curl_h == cur->data) {
			nugu_dbg("resume the request (req=%p)", h2req);
			curl_easy_pause(curl_h, CURLPAUSE_CONT);
			return 0;
		}

		cur = cur->next;
	}

	nugu_error("can't find the request: %p (already completed?)", h2req);

	return -1;
}

static void _process_async_queue(HTTP2Network *net)
{
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
			_request_item_free(item);
		} else if (item->type == REQUEST_RESUME) {
			_process_resume(net, item);
			_request_item_free(item);
		}
	}
}

static void _process_completed(HTTP2Network *net, CURLMsg *curl_message)
{
	char *fake_p;
	HTTP2Request *req;
	int req_id;

	curl_easy_getinfo(curl_message->easy_handle, CURLINFO_PRIVATE, &fake_p);
	req = (HTTP2Request *)fake_p;

	_curl_code_to_result(req, curl_message->data.result);

	curl_multi_remove_handle(net->handle, curl_message->easy_handle);
	net->hlist = g_list_remove(net->hlist, curl_message->easy_handle);

	req_id = http2_request_get_id(req);
	pthread_mutex_lock(&net->lock);
	g_hash_table_remove(net->hash, GINT_TO_POINTER(req_id));
	pthread_mutex_unlock(&net->lock);

	nugu_dbg("completed req(%p), req_id(%d) (code=%d)", req, req_id,
		 http2_request_get_response_code(req));
	http2_request_emit_finished(req);
	http2_request_unref(req);
}

static void *_loop(void *data)
{
	HTTP2Network *net = data;
	CURLMcode mc;
	CURLMsg *curl_message;
	char *fake_p;
	int still_running = 0;
	int i;
#ifndef _WIN32
	struct curl_waitfd extra_fds[1];
	int numfds;
#endif

#ifdef HAVE_PTHREAD_SETNAME_NP
#ifdef __APPLE__
	i = pthread_setname_np("http2");
	if (i < 0)
		nugu_error("pthread_setname_np() failed");
#endif
#endif

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

			_process_completed(net, curl_message);
		}
#ifdef _WIN32
		if (nugu_winsock_check_for_data(net->wakeup_fds[0]) == 0) {
			char ev;

			if (nugu_winsock_read(net->wakeup_fds[0], &ev,
					      sizeof(ev)) < 0) {
				nugu_error("error read");
				continue;
			}
			_process_async_queue(net);
		}
#else
		/*
		 * wait for activity, timeout or "nothing"
		 *
		 * Refer: https://curl.haxx.se/libcurl/c/curl_multi_wait.html
		 */
		extra_fds[0].fd = net->wakeup_fds[0];
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
			ssize_t nread;
#ifdef HAVE_EVENTFD
			uint64_t ev = 0;
#else
			uint8_t ev = 0;
#endif
			nread = read(extra_fds[0].fd, &ev, sizeof(ev));
			if (nread == -1 || nread != sizeof(ev)) {
				nugu_error("read failed");
				break;
			}

			_process_async_queue(net);
		}
#endif
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
			curl_multi_remove_handle(net->handle, easy);
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

HTTP2Network *http2_network_new(void)
{
	struct _http2_network *net;
#if !defined(HAVE_EVENTFD) && !defined(_WIN32)
	GError *error = NULL;
#endif

	net = calloc(1, sizeof(struct _http2_network));
	if (!net) {
		nugu_error_nomem();
		return NULL;
	}

	net->wakeup_fds[0] = -1;
	net->wakeup_fds[1] = -1;

#ifdef _WIN32
	net->wsock = nugu_winsock_create();
	if (net->wsock == NULL) {
		nugu_error("failed to create window socket");
		free(net);
		return NULL;
	}
	net->wakeup_fds[0] =
		nugu_winsock_get_handle(net->wsock, NUGU_WINSOCKET_CLIENT);
	net->wakeup_fds[1] =
		nugu_winsock_get_handle(net->wsock, NUGU_WINSOCKET_SERVER);
#else
#ifdef HAVE_EVENTFD
	net->wakeup_fds[0] = eventfd(0, EFD_CLOEXEC);
	if (net->wakeup_fds[0] < 0) {
		nugu_error("eventfd() failed");
		free(net);
		return NULL;
	}
#else
	if (g_unix_open_pipe(net->wakeup_fds, FD_CLOEXEC, &error) == FALSE) {
		nugu_error("g_unix_open_pipe() failed: %s", error->message);
		g_error_free(error);
		free(net);
		return NULL;
	}
	nugu_dbg("pipe fds[0] = %d", net->wakeup_fds[0]);
	nugu_dbg("pipe fds[1] = %d", net->wakeup_fds[1]);
#endif
#endif

	net->requests =
		g_async_queue_new_full((GDestroyNotify)_request_item_free);
	net->useragent = nugu_network_manager_peek_useragent();
	net->sync_init = thread_sync_new();

	net->req_ids = 0;
	net->hash = g_hash_table_new(g_direct_hash, g_direct_equal);
	pthread_mutex_init(&net->lock, NULL);

	return net;
}

void http2_network_free(HTTP2Network *net)
{
	g_return_if_fail(net != NULL);

	if (net->running) {
		net->running = 0;
		http2_network_wakeup(net);
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

#ifdef _WIN32
	nugu_winsock_remove(net->wsock);
#else
	if (net->wakeup_fds[0] != -1)
		close(net->wakeup_fds[0]);
	if (net->wakeup_fds[1] != -1)
		close(net->wakeup_fds[1]);
#endif

	if (net->sync_init)
		thread_sync_free(net->sync_init);

	if (net->hash)
		g_hash_table_destroy(net->hash);

	if (net->token)
		g_free(net->token);
	if (net->last_asr)
		g_free(net->last_asr);

	memset(net, 0, sizeof(HTTP2Network));
	free(net);
}

int http2_network_set_token(HTTP2Network *net, const char *token)
{
	g_return_val_if_fail(net != NULL, -1);

	if (net->token)
		g_free(net->token);

	if (token)
		net->token = g_strdup_printf("authorization: Bearer %s", token);
	else
		net->token = NULL;

	return 0;
}

int http2_network_set_last_asr_time(HTTP2Network *net, const char *timestr)
{
	g_return_val_if_fail(net != NULL, -1);

	if (net->last_asr)
		g_free(net->last_asr);

	if (timestr)
		net->last_asr =
			g_strdup_printf("Last-Asr-Event-Time: %s", timestr);
	else
		net->last_asr = NULL;

	return 0;
}

int http2_network_wakeup(HTTP2Network *net)
{
	ssize_t written;

	g_return_val_if_fail(net != NULL, -1);

#ifdef _WIN32
	if (net->wakeup_fds[1] != -1) {
		char ev = '1';

		written =
			nugu_winsock_write(net->wakeup_fds[1], &ev, sizeof(ev));
		if (written != sizeof(ev))
			nugu_error("error wakeup");
	}
#else
	/* wakeup request using eventfd */
	if (net->wakeup_fds[1] == -1) {
		uint64_t ev = 1;

		written = write(net->wakeup_fds[0], &ev, sizeof(ev));
		if (written != sizeof(ev))
			nugu_error("write failed");
	} else {
		uint8_t ev = 1;

		written = write(net->wakeup_fds[1], &ev, sizeof(ev));
		if (written != sizeof(ev))
			nugu_error("write failed");
	}
#endif

	return 0;
}

int http2_network_add_request(HTTP2Network *net, HTTP2Request *req)
{
	struct request_item *item;
	int req_id = -1;

	g_return_val_if_fail(net != NULL, -1);
	g_return_val_if_fail(req != NULL, -1);

	if (thread_sync_check(net->sync_init) == 0) {
		nugu_error("network is not started");
		return req_id;
	}

	if (net->log)
		http2_request_enable_curl_log(req);

	if (net->token)
		http2_request_add_header(req, net->token);

	if (net->last_asr)
		http2_request_add_header(req, net->last_asr);

	http2_request_set_useragent(req, net->useragent);

	pthread_mutex_lock(&net->lock);
	net->req_ids++;
	req_id = net->req_ids;
	g_hash_table_insert(net->hash, GINT_TO_POINTER(req_id), req);
	pthread_mutex_unlock(&net->lock);

	http2_request_set_id(req, req_id);

	item = _request_item_new(REQUEST_ADD, req_id);
	if (!item)
		return req_id;

	http2_request_ref(req);

	g_async_queue_push(net->requests, item);
	http2_network_wakeup(net);

	return req_id;
}

int http2_network_remove_request(HTTP2Network *net, int request_id)
{
	struct request_item *item;

	g_return_val_if_fail(net != NULL, -1);
	g_return_val_if_fail(request_id > 0, -1);

	if (thread_sync_check(net->sync_init) == 0) {
		nugu_error("network is not started");
		return -1;
	}

	item = _request_item_new(REQUEST_REMOVE, request_id);
	if (!item)
		return -1;

	g_async_queue_push(net->requests, item);
	http2_network_wakeup(net);

	return 0;
}

int http2_network_resume_request(HTTP2Network *net, int request_id)
{
	struct request_item *item;

	g_return_val_if_fail(net != NULL, -1);
	g_return_val_if_fail(request_id > 0, -1);

	if (thread_sync_check(net->sync_init) == 0) {
		nugu_error("network is not started");
		return -1;
	}

	item = _request_item_new(REQUEST_RESUME, request_id);
	if (!item)
		return -1;

	g_async_queue_push(net->requests, item);
	http2_network_wakeup(net);

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

#ifdef HAVE_PTHREAD_SETNAME_NP
#ifndef __APPLE__
	ret = pthread_setname_np(net->thread_id, "http2");
	if (ret < 0)
		nugu_error("pthread_setname_np() failed");
#endif
#endif

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
