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

#ifndef __HTTP2_REQUEST_H__
#define __HTTP2_REQUEST_H__

#include "base/nugu_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HTTP2_RESPONSE_OK 200
#define HTTP2_RESPONSE_AUTHFAIL 401
#define HTTP2_RESPONSE_FORBIDDEN 403
#define HTTP2_RESPONSE_ERROR 500

enum http2_request_method {
	HTTP2_REQUEST_METHOD_GET,
	HTTP2_REQUEST_METHOD_POST
};

enum http2_request_content_type {
	HTTP2_REQUEST_CONTENT_TYPE_UNKNOWN,
	HTTP2_REQUEST_CONTENT_TYPE_JSON,
	HTTP2_REQUEST_CONTENT_TYPE_OCTET,
	HTTP2_REQUEST_CONTENT_TYPE_MULTIPART
};

typedef struct _http2_request HTTP2Request;

typedef size_t (*ResponseHeaderCallback)(HTTP2Request *req, char *buffer,
					 size_t size, size_t nitems,
					 void *userdata);
typedef size_t (*ResponseBodyCallback)(HTTP2Request *req, char *buffer,
				       size_t size, size_t nitems,
				       void *userdata);
typedef void (*ResponseFinishCallback)(HTTP2Request *req, void *userdata);

typedef void (*HTTP2DestroyCallback)(HTTP2Request *req, void *userdata);

HTTP2Request *http2_request_new();

int http2_request_ref(HTTP2Request *req);
int http2_request_unref(HTTP2Request *req);

int http2_request_add_header(HTTP2Request *req, const char *header);

int http2_request_add_send_data(HTTP2Request *req, const unsigned char *data,
				size_t length);
int http2_request_close_send_data(HTTP2Request *req);
void http2_request_lock_send_data(HTTP2Request *req);
void http2_request_unlock_send_data(HTTP2Request *req);
int http2_request_resume(HTTP2Request *req);

int http2_request_set_method(HTTP2Request *req,
			     enum http2_request_method method);
int http2_request_set_url(HTTP2Request *req, const char *url);
int http2_request_set_content_type(HTTP2Request *req,
				   enum http2_request_content_type type,
				   const char *boundary);
int http2_request_set_connection_timeout(HTTP2Request *req, int timeout);
int http2_request_set_timeout(HTTP2Request *req, int timeout_secs);
int http2_request_set_useragent(HTTP2Request *req, const char *useragent);

void *http2_request_get_handle(HTTP2Request *req);

int http2_request_set_header_callback(HTTP2Request *req,
				      ResponseHeaderCallback cb,
				      void *userdata);
int http2_request_set_body_callback(HTTP2Request *req, ResponseBodyCallback cb,
				    void *userdata);
int http2_request_set_finish_callback(HTTP2Request *req,
				      ResponseFinishCallback cb,
				      void *userdata);
int http2_request_set_destroy_callback(HTTP2Request *req,
				       HTTP2DestroyCallback cb, void *userdata);

void http2_request_emit_completed(HTTP2Request *req);

NuguBuffer *http2_request_peek_response_body(HTTP2Request *req);
NuguBuffer *http2_request_peek_response_header(HTTP2Request *req);

int http2_request_get_response_code(HTTP2Request *req);

int http2_request_disable_verify_peer(HTTP2Request *req);

void http2_request_enable_curl_log(HTTP2Request *req);
void http2_request_disable_curl_log(HTTP2Request *req);

#ifdef __cplusplus
}
#endif

#endif
