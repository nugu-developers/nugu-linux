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

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

#include "nugu_log.h"
#include "http2_request.h"

#define SHOW_VERBOSE 0

struct _http2_request {
	CURL *easy;
	struct curl_slist *headers;
	enum http2_request_content_type type;
	char curl_errbuf[CURL_ERROR_SIZE];

	NuguBuffer *response_header;
	NuguBuffer *response_body;
	NuguBuffer *send_body;

	ResponseHeaderCallback header_cb;
	void *header_cb_userdata;

	ResponseBodyCallback body_cb;
	void *body_cb_userdata;

	ResponseFinishCallback finish_cb;
	void *finish_cb_userdata;

	int header_received;
	pthread_cond_t cond_header, cond_finish;

	int finished;
	pthread_mutex_t lock_header, lock_finish;

	pthread_mutex_t lock_ref;
	int ref_count;
};

static int _debug_callback(CURL *handle, curl_infotype type, char *data,
			   size_t size, void *userptr)
{
	enum nugu_log_prefix back;
	int flag = 0;

	back = nugu_log_get_prefix_fields();
	nugu_log_set_prefix_fields((enum nugu_log_prefix)(
		back & ~(NUGU_LOG_PREFIX_FILENAME | NUGU_LOG_PREFIX_LINE)));

	if (size > 0 && data[size - 1] == '\n') {
		data[size - 1] = '\0';
		flag = 1;
	}

	switch (type) {
	case CURLINFO_TEXT:
		nugu_dbg("[CURL] %s", data);
		break;
	case CURLINFO_HEADER_OUT:
		nugu_dbg("[CURL] Send header: %s", data);
		break;
	case CURLINFO_DATA_OUT:
		nugu_dbg("[CURL] Send data: %d bytes", size);
		break;
	case CURLINFO_SSL_DATA_OUT:
		break;
	case CURLINFO_HEADER_IN:
		nugu_dbg("[CURL] Recv header: %s", data);
		break;
	case CURLINFO_DATA_IN:
		nugu_dbg("[CURL] Recv data: %d bytes", size);
		break;
	case CURLINFO_SSL_DATA_IN:
		break;
	default:
		break;
	}

	if (flag)
		data[size - 1] = '\n';

	nugu_log_set_prefix_fields(back);

	return 0;
}

static size_t _request_body_cb(char *buffer, size_t size, size_t nitems,
			       void *userdata)
{
	HTTP2Request *req = userdata;
	size_t length;

	if (!req->send_body)
		return 0;

	length = nugu_buffer_get_size(req->send_body);
	if (length >= size * nitems)
		length = size * nitems;
	else if (length == 0)
		return 0;

	memcpy(buffer, nugu_buffer_peek(req->send_body), length);
	if ((nugu_log_get_modules() & NUGU_LOG_MODULE_NETWORK_TRACE) != 0) {
		enum nugu_log_prefix back;

		back = nugu_log_get_prefix_fields();
		nugu_log_set_prefix_fields(back & ~(NUGU_LOG_PREFIX_FILENAME |
						    NUGU_LOG_PREFIX_LINE));

		if (req->type == HTTP2_REQUEST_CONTENT_TYPE_JSON) {
			if (length < size * nitems)
				buffer[length] = '\0';
			nugu_info("--> Sent req(%p) %d bytes (json)\n%s", req,
				  length, buffer);
		} else {
			nugu_info("--> Sent req(%p) %d bytes", req, length);
		}

		nugu_log_set_prefix_fields(back);
	}

	nugu_buffer_shift_left(req->send_body, length);

	return length;
}

static size_t _response_body_cb(char *ptr, size_t size, size_t nmemb,
				void *userdata)
{
	HTTP2Request *req = userdata;

	if (req->body_cb)
		req->body_cb(req, ptr, size, nmemb, req->body_cb_userdata);
	else
		nugu_buffer_add(req->response_body, ptr, size * nmemb);

	return size * nmemb;
}

static size_t _response_header_cb(char *buffer, size_t size, size_t nmemb,
				  void *userdata)
{
	HTTP2Request *req = userdata;

	if (req->header_cb)
		req->header_cb(req, buffer, size, nmemb,
			       req->header_cb_userdata);
	else
		nugu_buffer_add(req->response_header, buffer, size * nmemb);

	http2_request_emit_response(req, HTTP2_REQUEST_SYNC_ITEM_HEADER);

	return size * nmemb;
}

HTTP2Request *http2_request_new()
{
	struct _http2_request *req;

	req = calloc(1, sizeof(struct _http2_request));
	if (!req) {
		error_nomem();
		return NULL;
	}

	req->ref_count = 1;
	req->response_header = nugu_buffer_new(0);
	req->response_body = nugu_buffer_new(0);
	req->send_body = nugu_buffer_new(0);

	req->easy = curl_easy_init();

	curl_easy_setopt(req->easy, CURLOPT_HTTP_VERSION,
			 CURL_HTTP_VERSION_2_0);
	curl_easy_setopt(req->easy, CURLOPT_ERRORBUFFER, req->curl_errbuf);
	curl_easy_setopt(req->easy, CURLOPT_DEBUGFUNCTION, _debug_callback);

	curl_easy_setopt(req->easy, CURLOPT_PRIVATE, req);
	curl_easy_setopt(req->easy, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(req->easy, CURLOPT_AUTOREFERER, 1L);

	curl_easy_setopt(req->easy, CURLOPT_WRITEFUNCTION, _response_body_cb);
	curl_easy_setopt(req->easy, CURLOPT_WRITEDATA, req);
	curl_easy_setopt(req->easy, CURLOPT_HEADERFUNCTION,
			 _response_header_cb);
	curl_easy_setopt(req->easy, CURLOPT_HEADERDATA, req);

	pthread_mutex_init(&req->lock_ref, NULL);
	pthread_mutex_init(&req->lock_header, NULL);
	pthread_mutex_init(&req->lock_finish, NULL);
	pthread_cond_init(&req->cond_header, NULL);
	pthread_cond_init(&req->cond_finish, NULL);

	return req;
}

void http2_request_free(HTTP2Request *req)
{
	g_return_if_fail(req != NULL);

	nugu_dbg("req(%p) destroy", req);

	if (strlen(req->curl_errbuf) > 0)
		nugu_error("CURL ERROR: %s", req->curl_errbuf);

	if (req->send_body)
		nugu_buffer_free(req->send_body, TRUE);

	if (req->response_header)
		nugu_buffer_free(req->response_header, TRUE);

	if (req->response_body)
		nugu_buffer_free(req->response_body, TRUE);

	if (req->headers)
		curl_slist_free_all(req->headers);

	curl_easy_cleanup(req->easy);

	pthread_mutex_destroy(&req->lock_ref);
	pthread_mutex_destroy(&req->lock_header);
	pthread_mutex_destroy(&req->lock_finish);
	pthread_cond_destroy(&req->cond_header);
	pthread_cond_destroy(&req->cond_finish);

	memset(req, 0, sizeof(HTTP2Request));
	free(req);
}

int http2_request_ref(HTTP2Request *req)
{
	g_return_val_if_fail(req != NULL, -1);

	pthread_mutex_lock(&req->lock_ref);
	req->ref_count++;
	nugu_dbg("req(%p) count=%d", req, req->ref_count);
	pthread_mutex_unlock(&req->lock_ref);

	return 0;
}

int http2_request_unref(HTTP2Request *req)
{
	g_return_val_if_fail(req != NULL, -1);

	pthread_mutex_lock(&req->lock_ref);
	req->ref_count--;
	nugu_dbg("req(%p) count=%d", req, req->ref_count);
	if (req->ref_count > 0) {
		pthread_mutex_unlock(&req->lock_ref);
		return 0;
	}
	pthread_mutex_unlock(&req->lock_ref);

	http2_request_free(req);

	return 0;
}

int http2_request_add_header(HTTP2Request *req, const char *header)
{
	g_return_val_if_fail(req != NULL, -1);
	g_return_val_if_fail(header != NULL, -1);

	req->headers = curl_slist_append(req->headers, header);
	if (!req->headers) {
		nugu_error("curl_slist_append() failed");
		return -1;
	}

	curl_easy_setopt(req->easy, CURLOPT_HTTPHEADER, req->headers);

	return 0;
}

int http2_request_add_send_data(HTTP2Request *req, const unsigned char *data,
				size_t length)
{
	g_return_val_if_fail(req != NULL, -1);

	nugu_buffer_add(req->send_body, data, length);

	return 0;
}

int http2_request_set_method(HTTP2Request *req,
			     enum http2_request_method method)
{
	g_return_val_if_fail(req != NULL, -1);

	if (method == HTTP2_REQUEST_METHOD_POST) {
		curl_easy_setopt(req->easy, CURLOPT_POST, 1L);
		curl_easy_setopt(req->easy, CURLOPT_READFUNCTION,
				 _request_body_cb);
		curl_easy_setopt(req->easy, CURLOPT_READDATA, req);
	}

	return 0;
}

int http2_request_set_url(HTTP2Request *req, const char *url)
{
	g_return_val_if_fail(req != NULL, -1);
	g_return_val_if_fail(url != NULL, -1);

	curl_easy_setopt(req->easy, CURLOPT_URL, url);

	if (strncmp(url, "http://", 7) == 0) {
		nugu_dbg("use HTTP/2 over clean TCP");
		curl_easy_setopt(req->easy, CURLOPT_HTTP_VERSION,
				 CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE);
	}

	return 0;
}

int http2_request_set_content_type(HTTP2Request *req,
				   enum http2_request_content_type type)
{
	g_return_val_if_fail(req != NULL, -1);

	switch (type) {
	case HTTP2_REQUEST_CONTENT_TYPE_JSON:
		http2_request_add_header(req, "Content-Type: application/json");
		break;
	case HTTP2_REQUEST_CONTENT_TYPE_OCTET:
		http2_request_add_header(
			req, "Content-Type: application/octet-stream");
		break;
	case HTTP2_REQUEST_CONTENT_TYPE_UNKNOWN:
		break;
	default:
		return -1;
	}

	req->type = type;

	return 0;
}

int http2_request_set_useragent(HTTP2Request *req, const char *useragent)
{
	g_return_val_if_fail(req != NULL, -1);
	g_return_val_if_fail(useragent != NULL, -1);

	curl_easy_setopt(req->easy, CURLOPT_USERAGENT, useragent);

	return 0;
}

int http2_request_set_connection_timeout(HTTP2Request *req, int timeout)
{
	g_return_val_if_fail(req != NULL, -1);

	curl_easy_setopt(req->easy, CURLOPT_CONNECTTIMEOUT, timeout);

	return 0;
}

int http2_request_set_timeout(HTTP2Request *req, int timeout)
{
	g_return_val_if_fail(req != NULL, -1);

	curl_easy_setopt(req->easy, CURLOPT_TIMEOUT, timeout);

	return 0;
}

CURL *http2_request_get_handle(HTTP2Request *req)
{
	g_return_val_if_fail(req != NULL, NULL);

	return req->easy;
}

int http2_request_set_header_callback(HTTP2Request *req,
				      ResponseHeaderCallback cb, void *userdata)
{
	g_return_val_if_fail(req != NULL, -1);

	req->header_cb = cb;
	req->header_cb_userdata = userdata;

	return 0;
}

int http2_request_set_body_callback(HTTP2Request *req, ResponseBodyCallback cb,
				    void *userdata)
{
	g_return_val_if_fail(req != NULL, -1);

	req->body_cb = cb;
	req->body_cb_userdata = userdata;

	return 0;
}

int http2_request_set_finish_callback(HTTP2Request *req,
				      ResponseFinishCallback cb, void *userdata)
{
	g_return_val_if_fail(req != NULL, -1);

	req->finish_cb = cb;
	req->finish_cb_userdata = userdata;

	return 0;
}

void http2_request_wait_response(HTTP2Request *req,
				 enum http2_request_sync_item item)
{
	struct timespec spec;
	int status = 0;

	g_return_if_fail(req != NULL);

	/* Wait maximum 5 secs */
	clock_gettime(CLOCK_REALTIME, &spec);
	spec.tv_sec = spec.tv_sec + 5;

	if (item == HTTP2_REQUEST_SYNC_ITEM_HEADER) {
		pthread_mutex_lock(&req->lock_header);
		if (req->header_received == 0)
			status = pthread_cond_timedwait(
				&req->cond_header, &req->lock_header, &spec);
		pthread_mutex_unlock(&req->lock_header);
	} else if (item == HTTP2_REQUEST_SYNC_ITEM_FINISH) {
		pthread_mutex_lock(&req->lock_finish);
		if (req->finished == 0)
			status = pthread_cond_timedwait(
				&req->cond_finish, &req->lock_finish, &spec);
		pthread_mutex_unlock(&req->lock_finish);
	}

	if (status == ETIMEDOUT) {
		nugu_info("timeout. clear the callbacks");
		http2_request_set_header_callback(req, NULL, NULL);
		http2_request_set_body_callback(req, NULL, NULL);
		http2_request_set_finish_callback(req, NULL, NULL);
	}
}

void http2_request_emit_response(HTTP2Request *req,
				 enum http2_request_sync_item item)
{
	g_return_if_fail(req != NULL);

	if (item == HTTP2_REQUEST_SYNC_ITEM_HEADER) {
		pthread_mutex_lock(&req->lock_header);
		req->header_received = 1;
		pthread_cond_signal(&req->cond_header);
		pthread_mutex_unlock(&req->lock_header);
	} else if (item == HTTP2_REQUEST_SYNC_ITEM_FINISH) {
		pthread_mutex_lock(&req->lock_finish);
		req->finished = 1;
		pthread_cond_signal(&req->cond_finish);
		pthread_mutex_unlock(&req->lock_finish);
	}
}

/* invoked in a thread loop */
void http2_request_emit_completed(HTTP2Request *req)
{
	g_return_if_fail(req != NULL);

	if (req->finish_cb)
		req->finish_cb(req, req->finish_cb_userdata);

	http2_request_emit_response(req, HTTP2_REQUEST_SYNC_ITEM_FINISH);
}

NuguBuffer *http2_request_peek_response_body(HTTP2Request *req)
{
	g_return_val_if_fail(req != NULL, NULL);

	return req->response_body;
}

NuguBuffer *http2_request_peek_response_header(HTTP2Request *req)
{
	g_return_val_if_fail(req != NULL, NULL);

	return req->response_header;
}

int http2_request_get_response_code(HTTP2Request *req)
{
	long response_code = -1;

	g_return_val_if_fail(req != NULL, -1);

	curl_easy_getinfo(req->easy, CURLINFO_RESPONSE_CODE, &response_code);
	if (response_code == 0)
		return -1;

	return (int)response_code;
}

int http2_request_disable_verify_peer(HTTP2Request *req)
{
	g_return_val_if_fail(req != NULL, -1);

	curl_easy_setopt(req->easy, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(req->easy, CURLOPT_SSL_VERIFYPEER, 0L);

	return 0;
}

void http2_request_enable_curl_log(HTTP2Request *req)
{
	g_return_if_fail(req != NULL);

	curl_easy_setopt(req->easy, CURLOPT_VERBOSE, 1L);
}

void http2_request_disable_curl_log(HTTP2Request *req)
{
	g_return_if_fail(req != NULL);

	curl_easy_setopt(req->easy, CURLOPT_VERBOSE, 0L);
}
