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

#include <glib.h>

#include "base/nugu_log.h"
#include "base/nugu_equeue.h"
#include "base/nugu_uuid.h"

#include "dg_types.h"

#include "threadsync.h"
#include "http2_request.h"
#include "v1_events.h"

#define CRLF "\r\n"

#define PART_HEADER_JSON                                                       \
	"Content-Disposition: form-data; name=\"event\"" CRLF                  \
	"Content-Type: application/json" CRLF CRLF

#define PART_HEADER_BINARY_FMT                                                 \
	"Content-Disposition: form-data; name=\"attachment\";"                 \
	" filename=\"%d;%s\"" CRLF                                             \
	"Content-Type: application/octet-stream" CRLF                          \
	"Content-Length: %zd" CRLF CRLF

struct _v1_events {
	HTTP2Request *req;
	char *boundary;
	int sync;
};

static void _emit_send_result(int code, HTTP2Request *req)
{
	struct equeue_data_request_result *event;

	event = calloc(1, sizeof(struct equeue_data_request_result));
	if (!event) {
		nugu_error_nomem();
		return;
	}

	event->code = code;

	if (http2_request_peek_msgid(req))
		event->msg_id = strdup(http2_request_peek_msgid(req));

	if (http2_request_peek_dialogid(req))
		event->dialog_id = strdup(http2_request_peek_dialogid(req));

	if (code == HTTP2_RESPONSE_OK) {
		event->success = 1;
	} else {
		event->success = 0;
		nugu_error("event send failed: %d (msg=%s)", event->code,
			   event->msg_id);
	}

	if (nugu_equeue_push(NUGU_EQUEUE_TYPE_SEND_EVENT_RESULT, event) < 0) {
		nugu_error("nugu_equeue_push failed");

		if (event->dialog_id)
			free(event->dialog_id);
		if (event->msg_id)
			free(event->msg_id);
		free(event);
	}
}

/* invoked in a thread loop */
static void _on_finish(HTTP2Request *req, void *userdata)
{
	int code;

	if (userdata)
		thread_sync_signal(userdata);

	code = http2_request_get_response_code(req);

	_emit_send_result(code, req);

	if (code == HTTP2_RESPONSE_AUTHFAIL || code == HTTP2_RESPONSE_FORBIDDEN)
		nugu_equeue_push(NUGU_EQUEUE_TYPE_INVALID_TOKEN, NULL);
}

V1Events *v1_events_new(const char *host, HTTP2Network *net, int is_sync)
{
	char *tmp;
	struct _v1_events *event;
	int ret;
	unsigned char buf[8];
	char boundary[17];

	g_return_val_if_fail(host != NULL, NULL);
	g_return_val_if_fail(net != NULL, NULL);

	event = calloc(1, sizeof(struct _v1_events));
	if (!event) {
		nugu_error_nomem();
		return NULL;
	}

	nugu_uuid_fill_random(buf, sizeof(buf));
	nugu_uuid_convert_base16(buf, sizeof(buf), boundary, sizeof(boundary));
	boundary[16] = '\0';

	event->boundary = g_strdup_printf("%s--%s", CRLF, boundary);
	event->sync = is_sync;

	event->req = http2_request_new();
	http2_request_set_method(event->req, HTTP2_REQUEST_METHOD_POST);
	http2_request_set_content_type(
		event->req, HTTP2_REQUEST_CONTENT_TYPE_MULTIPART, boundary);

	/* Set maximum timeout to 20 seconds */
	http2_request_set_timeout(event->req, 20);

	tmp = g_strdup_printf("%s/v1/events", host);
	http2_request_set_url(event->req, tmp);
	g_free(tmp);

	http2_request_set_finish_callback(event->req, _on_finish, NULL);

	ret = http2_network_add_request(net, event->req);
	if (ret < 0) {
		nugu_error("http2_network_add_request() failed: %d", ret);
		http2_request_unref(event->req);
		event->req = NULL;
		v1_events_free(event);
		return NULL;
	}

	return event;
}

void v1_events_free(V1Events *event)
{
	g_return_if_fail(event != NULL);

	if (event->boundary)
		free(event->boundary);

	if (event->req)
		http2_request_unref(event->req);

	memset(event, 0, sizeof(V1Events));
	free(event);
}

int v1_events_set_info(V1Events *event, const char *msg_id,
		       const char *dialog_id)
{
	g_return_val_if_fail(event != NULL, -1);

	http2_request_set_msgid(event->req, msg_id);
	http2_request_set_dialogid(event->req, dialog_id);

	return 0;
}

int v1_events_send_json(V1Events *event, const char *data, size_t length)
{
	g_return_val_if_fail(event != NULL, -1);

	http2_request_lock_send_data(event->req);
	http2_request_add_send_data(event->req,
				    (unsigned char *)event->boundary,
				    strlen(event->boundary));
	http2_request_add_send_data(event->req, (unsigned char *)CRLF, 2);
	http2_request_add_send_data(event->req,
				    (unsigned char *)PART_HEADER_JSON,
				    strlen(PART_HEADER_JSON));
	http2_request_add_send_data(event->req, (const unsigned char *)data,
				    length);
	http2_request_add_send_data(event->req, (unsigned char *)CRLF, 2);
	http2_request_unlock_send_data(event->req);

	http2_request_resume(event->req);

	return 0;
}

int v1_events_send_binary(V1Events *event, int seq, int is_end, size_t length,
			  unsigned char *data)
{
	char *part_header;

	g_return_val_if_fail(event != NULL, -1);

	part_header =
		g_strdup_printf(PART_HEADER_BINARY_FMT, seq,
				(is_end == 1) ? "end" : "continued", length);

	http2_request_lock_send_data(event->req);
	http2_request_add_send_data(event->req,
				    (unsigned char *)event->boundary,
				    strlen(event->boundary));
	http2_request_add_send_data(event->req, (unsigned char *)CRLF, 2);
	http2_request_add_send_data(event->req, (unsigned char *)part_header,
				    strlen(part_header));

	if (data != NULL && length > 0) {
		http2_request_add_send_data(
			event->req, (const unsigned char *)data, length);
		http2_request_add_send_data(event->req, (unsigned char *)CRLF,
					    2);
	}

	http2_request_unlock_send_data(event->req);

	g_free(part_header);

	http2_request_resume(event->req);

	return 0;
}

int v1_events_send_done(V1Events *event)
{
	ThreadSync *sync = NULL;

	g_return_val_if_fail(event != NULL, -1);

	if (event->sync)
		sync = thread_sync_new();

	http2_request_set_finish_callback(event->req, _on_finish, sync);

	http2_request_lock_send_data(event->req);
	http2_request_add_send_data(event->req,
				    (unsigned char *)event->boundary,
				    strlen(event->boundary));
	http2_request_add_send_data(event->req, (unsigned char *)"--", 2);
	http2_request_add_send_data(event->req, (unsigned char *)CRLF, 2);
	http2_request_close_send_data(event->req);
	http2_request_unlock_send_data(event->req);

	http2_request_resume(event->req);

	if (sync) {
		thread_sync_wait_secs(sync, 5);
		thread_sync_free(sync);
	}

	return 0;
}
