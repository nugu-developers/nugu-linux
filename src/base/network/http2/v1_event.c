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

#include "dg_types.h"

#include "http2_request.h"
#include "v1_event.h"

struct _v1_event {
	HTTP2Request *req;
};

V1Event *v1_event_new(const char *host)
{
	char *tmp;
	struct _v1_event *event;

	g_return_val_if_fail(host != NULL, NULL);

	event = calloc(1, sizeof(struct _v1_event));
	if (!event) {
		nugu_error_nomem();
		return NULL;
	}

	event->req = http2_request_new();
	http2_request_set_method(event->req, HTTP2_REQUEST_METHOD_POST);
	http2_request_set_content_type(event->req,
				       HTTP2_REQUEST_CONTENT_TYPE_JSON, NULL);

	/* Set maximum timeout to 5 seconds */
	http2_request_set_timeout(event->req, 5);

	tmp = g_strdup_printf("%s/v1/event", host);
	http2_request_set_url(event->req, tmp);
	g_free(tmp);

	return event;
}

void v1_event_free(V1Event *event)
{
	g_return_if_fail(event != NULL);

	if (event->req)
		http2_request_unref(event->req);

	memset(event, 0, sizeof(V1Event));
	free(event);
}

int v1_event_set_json(V1Event *event, const char *data, size_t length)
{
	g_return_val_if_fail(event != NULL, -1);

	if (http2_request_add_send_data(event->req, (const unsigned char *)data,
					length) < 0)
		return -1;

	return http2_request_close_send_data(event->req);
}

int v1_event_set_info(V1Event *event, const char *msg_id, const char *dialog_id)
{
	g_return_val_if_fail(event != NULL, -1);

	http2_request_set_msgid(event->req, msg_id);
	http2_request_set_dialogid(event->req, dialog_id);

	return 0;
}

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
		event->msg_id = g_strdup(http2_request_peek_msgid(req));

	if (http2_request_peek_dialogid(req))
		event->dialog_id = g_strdup(http2_request_peek_dialogid(req));

	if (code == HTTP2_RESPONSE_OK) {
		event->success = 1;
	} else {
		event->success = 0;
		nugu_error("event send failed: %d (msg=%s)", event->code,
			   event->msg_id);
	}

	if (nugu_equeue_push(NUGU_EQUEUE_TYPE_EVENT_SEND_RESULT, event) < 0) {
		nugu_error("nugu_equeue_push failed");

		if (event->dialog_id)
			g_free(event->dialog_id);
		if (event->msg_id)
			g_free(event->msg_id);
		free(event);
	}
}

/* invoked in a thread loop */
static void _on_finish(HTTP2Request *req, void *userdata)
{
	int code;

	code = http2_request_get_response_code(req);

	_emit_send_result(code, req);

	if (code == HTTP2_RESPONSE_AUTHFAIL || code == HTTP2_RESPONSE_FORBIDDEN)
		nugu_equeue_push(NUGU_EQUEUE_TYPE_INVALID_TOKEN, NULL);
}

int v1_event_send_with_free(V1Event *event, HTTP2Network *net)
{
	int ret;

	g_return_val_if_fail(event != NULL, -1);
	g_return_val_if_fail(net != NULL, -1);

	http2_request_set_finish_callback(event->req, _on_finish, NULL);

	ret = http2_network_add_request(net, event->req);
	if (ret < 0)
		return ret;

	http2_request_unref(event->req);
	event->req = NULL;

	v1_event_free(event);

	return ret;
}
