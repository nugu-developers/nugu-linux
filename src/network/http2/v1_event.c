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

#include "nugu_log.h"
#include "http2_request.h"
#include "v1_event.h"

struct _v1_event {
	H2Manager *mgr;
	HTTP2Request *req;
};

V1Event *v1_event_new(H2Manager *mgr)
{
	char *tmp;
	struct _v1_event *event;

	g_return_val_if_fail(mgr != NULL, NULL);

	event = calloc(1, sizeof(struct _v1_event));
	if (!event) {
		error_nomem();
		return NULL;
	}

	event->mgr = mgr;
	event->req = http2_request_new();
	http2_request_set_method(event->req, HTTP2_REQUEST_METHOD_POST);
	http2_request_set_content_type(event->req,
				       HTTP2_REQUEST_CONTENT_TYPE_JSON);

	/* Set maximum timeout to 5 seconds */
	http2_request_set_timeout(event->req, 5);

	tmp = g_strdup_printf("%s/v1/event", h2manager_peek_host(mgr));
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

	return http2_request_add_send_data(event->req,
					   (const unsigned char *)data, length);
}

/* invoked in a thread loop */
static void _on_finish(HTTP2Request *req, void *userdata)
{
	int code;
	H2Manager *mgr = userdata;

	code = http2_request_get_response_code(req);
	if (code == HTTP2_RESPONSE_OK)
		return;

	nugu_error("event send failed: %d", code);

	if (code == HTTP2_RESPONSE_AUTHFAIL || code == HTTP2_RESPONSE_FORBIDDEN)
		h2manager_set_status(mgr, H2_STATUS_TOKEN_FAILED);
	else
		h2manager_set_status(mgr, H2_STATUS_DISCONNECTED);
}

int v1_event_send_with_free(V1Event *event, HTTP2Network *net)
{
	int ret;

	g_return_val_if_fail(event != NULL, -1);
	g_return_val_if_fail(net != NULL, -1);

	http2_request_set_finish_callback(event->req, _on_finish, event->mgr);

	ret = http2_network_add_request(net, event->req);
	if (ret < 0)
		return ret;

	http2_request_unref(event->req);
	event->req = NULL;

	v1_event_free(event);

	return ret;
}
