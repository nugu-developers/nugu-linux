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

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "base/nugu_equeue.h"
#include "base/nugu_log.h"
#include "base/nugu_network_manager.h"

#include "directives_parser.h"
#include "http2_request.h"
#include "v1_directives.h"

struct _v1_directives {
	char *url;

	HTTP2Network *network;

	int connection_timeout_secs;
};

/* invoked in a thread loop */
static size_t _on_body(HTTP2Request *req, char *buffer, size_t size,
		       size_t nitems, void *userdata)
{
	dir_parser_parse(userdata, buffer, size * nitems);

	return size * nitems;
}

/* invoked in a thread loop */
static size_t _on_header(HTTP2Request *req, char *buffer, size_t size,
			 size_t nitems, void *userdata)
{
	int buffer_len = size * nitems;
	int code;

	code = http2_request_get_response_code(req);
	if (code != HTTP2_RESPONSE_OK) {
		nugu_error("code = %d", code);
		http2_request_set_header_callback(req, NULL, NULL);
		http2_request_set_body_callback(req, NULL, NULL);
		http2_request_set_finish_callback(req, NULL, NULL);

		if (code == HTTP2_RESPONSE_AUTHFAIL ||
		    code == HTTP2_RESPONSE_FORBIDDEN)
			nugu_equeue_push(NUGU_EQUEUE_TYPE_INVALID_TOKEN, NULL);
		else
			nugu_equeue_push(NUGU_EQUEUE_TYPE_SERVER_DISCONNECTED,
					 NULL);

		return buffer_len;
	}

	if (dir_parser_add_header(userdata, buffer, size * nitems) < 0)
		return buffer_len;

	nugu_equeue_push(NUGU_EQUEUE_TYPE_SERVER_CONNECTED, NULL);

	return buffer_len;
}

/* invoked in a thread loop */
static void _on_finish(HTTP2Request *req, void *userdata)
{
	int code;

	code = http2_request_get_response_code(req);
	if (code == HTTP2_RESPONSE_OK) {
		nugu_info("directive stream finished by server.");
		nugu_equeue_push(NUGU_EQUEUE_TYPE_DIRECTIVES_CLOSED, NULL);
		return;
	}

	nugu_error("directive response code = %d", code);

	if (code == HTTP2_RESPONSE_AUTHFAIL || code == HTTP2_RESPONSE_FORBIDDEN)
		nugu_equeue_push(NUGU_EQUEUE_TYPE_INVALID_TOKEN, NULL);
	else
		nugu_equeue_push(NUGU_EQUEUE_TYPE_SERVER_DISCONNECTED, NULL);
}

/* invoked in a thread loop */
static void _on_destroy(HTTP2Request *req, void *userdata)
{
	if (!userdata)
		return;

	dir_parser_free(userdata);
}

V1Directives *v1_directives_new(const char *host, int api_version,
				int connection_timeout_secs)
{
	struct _v1_directives *dir;

	g_return_val_if_fail(host != NULL, NULL);
	g_return_val_if_fail(api_version == 1 || api_version == 2, NULL);

	dir = (V1Directives *)calloc(1, sizeof(struct _v1_directives));
	if (!dir) {
		nugu_error_nomem();
		return NULL;
	}

	dir->url = g_strdup_printf("%s/v%d/directives", host, api_version);
	if (!dir->url) {
		nugu_error_nomem();
		free(dir);
		return NULL;
	}

	dir->connection_timeout_secs = connection_timeout_secs;

	return dir;
}

void v1_directives_free(V1Directives *dir)
{
	g_return_if_fail(dir != NULL);

	if (dir->url)
		g_free(dir->url);

	memset(dir, 0, sizeof(V1Directives));
	free(dir);
}

int v1_directives_establish(V1Directives *dir, HTTP2Network *net)
{
	HTTP2Request *req;
	DirParser *parser;
	int ret;

	g_return_val_if_fail(dir != NULL, -1);
	g_return_val_if_fail(net != NULL, -1);

	parser = dir_parser_new();
	if (!parser) {
		nugu_error_nomem();
		return -1;
	}

	req = http2_request_new();
	if (!req) {
		dir_parser_free(parser);
		nugu_error_nomem();
		return -1;
	}

	dir->network = net;

	http2_request_set_url(req, dir->url);
	http2_request_set_method(req, HTTP2_REQUEST_METHOD_GET);
	http2_request_set_header_callback(req, _on_header, parser);
	http2_request_set_body_callback(req, _on_body, parser);
	http2_request_set_finish_callback(req, _on_finish, NULL);
	http2_request_set_destroy_callback(req, _on_destroy, parser);
	http2_request_set_connection_timeout(req, dir->connection_timeout_secs);
	http2_request_enable_curl_log(req);

	ret = http2_network_add_request(net, req);
	if (ret < 0) {
		nugu_error("http2_network_add_request() failed: %d", ret);
		http2_request_unref(req);
		dir_parser_free(parser);
		return ret;
	}

	http2_request_unref(req);

	return ret;
}
