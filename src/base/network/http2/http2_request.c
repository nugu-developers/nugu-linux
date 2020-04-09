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
#include <unistd.h>

#include <glib.h>

#include "curl/curl.h"

#include "base/nugu_log.h"

#include "http2_request.h"

#define CT_JSON "Content-Type: application/json"
#define CT_OCTET "Content-Type: application/octet-stream"
#define CT_MULTIPART "Content-Type: multipart/form-data; boundary=%s"

#define SHOW_VERBOSE 0

struct _http2_request {
	CURL *easy;
	enum http2_result result;

	struct curl_slist *headers;
	enum http2_request_content_type type;
	char curl_errbuf[CURL_ERROR_SIZE];

	NuguBuffer *send_body;
	pthread_mutex_t lock_send_body;
	int send_body_closed;
	RequestSendCompleteCallback send_complete_cb;
	void *send_complete_cb_userdata;

	ResponseCodeCallback code_cb;
	void *code_cb_userdata;
	int code;

	NuguBuffer *response_header;
	ResponseHeaderCallback header_cb;
	void *header_cb_userdata;

	NuguBuffer *response_body;
	ResponseBodyCallback body_cb;
	void *body_cb_userdata;

	ResponseFinishCallback finish_cb;
	void *finish_cb_userdata;
	int finished;

	HTTP2DestroyCallback destroy_cb;
	void *destroy_cb_userdata;

	pthread_mutex_t lock_ref;
	int ref_count;

	/* Optional information */
	char *msg_id;
	char *dialog_id;
	char *profiling_contents;
};

static int _debug_callback(CURL *handle, curl_infotype type, char *data,
			   size_t size, void *userptr)
{
	int flag = 0;

	if (size > 0 && data[size - 1] == '\n') {
		data[size - 1] = '\0';
		flag = 1;
	}

	switch (type) {
	case CURLINFO_TEXT:
		nugu_log_print(NUGU_LOG_MODULE_NETWORK, NUGU_LOG_LEVEL_DEBUG,
			       NULL, NULL, -1, "[CURL] %s", data);
		break;
	case CURLINFO_HEADER_OUT:
		nugu_log_print(NUGU_LOG_MODULE_NETWORK, NUGU_LOG_LEVEL_DEBUG,
			       NULL, NULL, -1, "[CURL] Send header: %s", data);
		nugu_hexdump(NUGU_LOG_MODULE_NETWORK_TRACE, (uint8_t *)data,
			     size, NUGU_ANSI_COLOR_SEND, NUGU_ANSI_COLOR_NORMAL,
			     NUGU_LOG_MARK_SEND);
		break;
	case CURLINFO_DATA_OUT:
		nugu_log_print(NUGU_LOG_MODULE_NETWORK, NUGU_LOG_LEVEL_DEBUG,
			       NULL, NULL, -1, "[CURL] Send data: %d bytes",
			       size);
		nugu_hexdump(NUGU_LOG_MODULE_NETWORK_TRACE, (uint8_t *)data,
			     size, NUGU_ANSI_COLOR_SEND, NUGU_ANSI_COLOR_NORMAL,
			     NUGU_LOG_MARK_SEND);
		break;
	case CURLINFO_SSL_DATA_OUT:
		break;
	case CURLINFO_HEADER_IN:
		nugu_log_print(NUGU_LOG_MODULE_NETWORK, NUGU_LOG_LEVEL_DEBUG,
			       NULL, NULL, -1, "[CURL] Recv header: %s", data);
		nugu_hexdump(NUGU_LOG_MODULE_NETWORK_TRACE, (uint8_t *)data,
			     size, NUGU_ANSI_COLOR_RECV, NUGU_ANSI_COLOR_NORMAL,
			     NUGU_LOG_MARK_RECV);
		break;
	case CURLINFO_DATA_IN:
		nugu_log_print(NUGU_LOG_MODULE_NETWORK, NUGU_LOG_LEVEL_DEBUG,
			       NULL, NULL, -1, "[CURL] Recv data: %d bytes",
			       size);
		nugu_hexdump(NUGU_LOG_MODULE_NETWORK_TRACE, (uint8_t *)data,
			     size, NUGU_ANSI_COLOR_RECV, NUGU_ANSI_COLOR_NORMAL,
			     NUGU_LOG_MARK_RECV);
		break;
	case CURLINFO_SSL_DATA_IN:
		break;
	default:
		break;
	}

	if (flag)
		data[size - 1] = '\n';

	return 0;
}

static size_t _request_body_cb(char *buffer, size_t size, size_t nitems,
			       void *userdata)
{
	HTTP2Request *req = userdata;
	size_t length;

	if (!req->send_body)
		return 0;

	http2_request_lock_send_data(req);

	length = nugu_buffer_get_size(req->send_body);
	if (length >= size * nitems)
		length = size * nitems;
	else if (length == 0) {
		if (req->send_body_closed == 0) {
			/* Pause the current uploading */
			nugu_dbg("request paused until resume (req=%p)", req);
			http2_request_unlock_send_data(req);
			return CURL_READFUNC_PAUSE;
		}

		/* There is no more data to send. */
		http2_request_unlock_send_data(req);

		if (req->send_complete_cb)
			req->send_complete_cb(req,
					      req->send_complete_cb_userdata);

		return 0;
	}

	memcpy(buffer, nugu_buffer_peek(req->send_body), length);

	nugu_dbg("Sent req(%p) %d bytes", req, length);

	if (req->type == HTTP2_REQUEST_CONTENT_TYPE_JSON) {
		if (length < size * nitems)
			buffer[length] = '\0';

		nugu_log_protocol_send(NUGU_LOG_LEVEL_INFO, "Event (%p)\n%s",
				       req, buffer);

	} else if (req->type == HTTP2_REQUEST_CONTENT_TYPE_MULTIPART) {
		if (length < size * nitems)
			buffer[length] = '\0';

		nugu_log_protocol_send(NUGU_LOG_LEVEL_INFO,
				       "Event multipart (%p)\n%s", req, buffer);
	}

	nugu_buffer_shift_left(req->send_body, length);

	http2_request_unlock_send_data(req);

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

	if (req->code == -1) {
		long code = 0;

		/**
		 * The stored value will be zero if no server
		 * response code has been received.
		 */
		curl_easy_getinfo(req->easy, CURLINFO_RESPONSE_CODE, &code);
		if (code != 0) {
			req->code = (int)code;

			nugu_dbg("got response_code: %d", req->code);

			if (req->code_cb)
				req->code_cb(req, req->code,
					     req->code_cb_userdata);
		}
	}

	if (req->header_cb)
		req->header_cb(req, buffer, size, nmemb,
			       req->header_cb_userdata);
	else
		nugu_buffer_add(req->response_header, buffer, size * nmemb);

	return size * nmemb;
}

HTTP2Request *http2_request_new()
{
	struct _http2_request *req;

	req = calloc(1, sizeof(struct _http2_request));
	if (!req) {
		nugu_error_nomem();
		return NULL;
	}

	req->ref_count = 1;
	req->code = -1;
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
	pthread_mutex_init(&req->lock_send_body, NULL);

	nugu_dbg("request(%p) created", req);

	return req;
}

static void http2_request_free(HTTP2Request *req)
{
	g_return_if_fail(req != NULL);

	nugu_dbg("req(%p) destroy", req);

	if (req->destroy_cb)
		req->destroy_cb(req, req->destroy_cb_userdata);

	if (req->msg_id)
		free(req->msg_id);

	if (req->dialog_id)
		free(req->dialog_id);

	if (req->profiling_contents)
		free(req->profiling_contents);

	if (strlen(req->curl_errbuf) > 0)
		nugu_error("CURL ERROR: %s", req->curl_errbuf);

	if (req->send_body)
		nugu_buffer_free(req->send_body, 1);

	if (req->response_header)
		nugu_buffer_free(req->response_header, 1);

	if (req->response_body)
		nugu_buffer_free(req->response_body, 1);

	if (req->headers)
		curl_slist_free_all(req->headers);

	curl_easy_cleanup(req->easy);

	pthread_mutex_destroy(&req->lock_ref);
	pthread_mutex_destroy(&req->lock_send_body);

	memset(req, 0, sizeof(HTTP2Request));
	free(req);
}

int http2_request_ref(HTTP2Request *req)
{
	g_return_val_if_fail(req != NULL, -1);

	pthread_mutex_lock(&req->lock_ref);
	req->ref_count++;
	pthread_mutex_unlock(&req->lock_ref);

	return 0;
}

int http2_request_unref(HTTP2Request *req)
{
	g_return_val_if_fail(req != NULL, -1);

	pthread_mutex_lock(&req->lock_ref);
	req->ref_count--;
	if (req->ref_count > 0) {
		pthread_mutex_unlock(&req->lock_ref);
		return 0;
	}
	pthread_mutex_unlock(&req->lock_ref);

	http2_request_free(req);

	return 0;
}

int http2_request_set_result(HTTP2Request *req, enum http2_result result)
{
	g_return_val_if_fail(req != NULL, -1);

	req->result = result;

	return 0;
}

enum http2_result http2_request_get_result(HTTP2Request *req)
{
	g_return_val_if_fail(req != NULL, HTTP2_RESULT_UNKNOWN);

	return req->result;
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

int http2_request_close_send_data(HTTP2Request *req)
{
	g_return_val_if_fail(req != NULL, -1);

	req->send_body_closed = 1;

	return 0;
}

void http2_request_lock_send_data(HTTP2Request *req)
{
	g_return_if_fail(req != NULL);

	pthread_mutex_lock(&req->lock_send_body);
}

void http2_request_unlock_send_data(HTTP2Request *req)
{
	g_return_if_fail(req != NULL);

	pthread_mutex_unlock(&req->lock_send_body);
}

int http2_request_set_send_complete_callback(HTTP2Request *req,
					     RequestSendCompleteCallback cb,
					     void *userdata)
{
	g_return_val_if_fail(req != NULL, -1);

	req->send_complete_cb = cb;
	req->send_complete_cb_userdata = userdata;

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

const char *http2_request_peek_url(HTTP2Request *req)
{
	char *url = NULL;

	g_return_val_if_fail(req != NULL, NULL);

	curl_easy_getinfo(req->easy, CURLINFO_EFFECTIVE_URL, &url);

	return url;
}

int http2_request_set_content_type(HTTP2Request *req,
				   enum http2_request_content_type type,
				   const char *boundary)
{
	char *tmp;

	g_return_val_if_fail(req != NULL, -1);

	switch (type) {
	case HTTP2_REQUEST_CONTENT_TYPE_JSON:
		http2_request_add_header(req, CT_JSON);
		break;
	case HTTP2_REQUEST_CONTENT_TYPE_OCTET:
		http2_request_add_header(req, CT_OCTET);
		break;
	case HTTP2_REQUEST_CONTENT_TYPE_MULTIPART:
		tmp = g_strdup_printf(CT_MULTIPART, boundary);
		http2_request_add_header(req, tmp);
		g_free(tmp);
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

int http2_request_set_timeout(HTTP2Request *req, int timeout_secs)
{
	g_return_val_if_fail(req != NULL, -1);

	curl_easy_setopt(req->easy, CURLOPT_TIMEOUT, timeout_secs);

	return 0;
}

void *http2_request_get_handle(HTTP2Request *req)
{
	g_return_val_if_fail(req != NULL, NULL);

	return req->easy;
}

int http2_request_set_code_callback(HTTP2Request *req, ResponseCodeCallback cb,
				    void *userdata)
{
	g_return_val_if_fail(req != NULL, -1);

	req->code_cb = cb;
	req->code_cb_userdata = userdata;

	return 0;
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

int http2_request_set_destroy_callback(HTTP2Request *req,
				       HTTP2DestroyCallback cb, void *userdata)
{
	g_return_val_if_fail(req != NULL, -1);

	req->destroy_cb = cb;
	req->destroy_cb_userdata = userdata;

	return 0;
}

/* invoked in a thread loop */
void http2_request_emit_finished(HTTP2Request *req)
{
	g_return_if_fail(req != NULL);

	if (req->finished) {
		nugu_warn("already finished");
		return;
	}

	if (req->finish_cb)
		req->finish_cb(req, req->finish_cb_userdata);

	req->finished = 1;
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

int http2_request_set_msgid(HTTP2Request *req, const char *msgid)
{
	g_return_val_if_fail(req != NULL, -1);

	if (req->msg_id)
		free(req->msg_id);

	if (msgid)
		req->msg_id = strdup(msgid);
	else
		req->msg_id = NULL;

	return 0;
}

const char *http2_request_peek_msgid(HTTP2Request *req)
{
	g_return_val_if_fail(req != NULL, NULL);

	return req->msg_id;
}

int http2_request_set_dialogid(HTTP2Request *req, const char *dialogid)
{
	g_return_val_if_fail(req != NULL, -1);

	if (req->dialog_id)
		free(req->dialog_id);

	if (dialogid)
		req->dialog_id = strdup(dialogid);
	else
		req->dialog_id = NULL;

	return 0;
}

const char *http2_request_peek_dialogid(HTTP2Request *req)
{
	g_return_val_if_fail(req != NULL, NULL);

	return req->dialog_id;
}

int http2_request_set_profiling_contents(HTTP2Request *req,
					 const char *contents)
{
	g_return_val_if_fail(req != NULL, -1);

	if (req->profiling_contents)
		free(req->profiling_contents);

	if (contents)
		req->profiling_contents = strdup(contents);
	else
		req->profiling_contents = NULL;

	return 0;
}

const char *http2_request_peek_profiling_contents(HTTP2Request *req)
{
	g_return_val_if_fail(req != NULL, NULL);

	return req->profiling_contents;
}
