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
#include "v1_ping.h"

struct _v1_ping {
	H2Manager *mgr;
	HTTP2Network *network;
	GatewayHealthPolicy policy;
	guint timer_src;
	char *host;
};

V1Ping *v1_ping_new(H2Manager *mgr, const GatewayHealthPolicy *policy)
{
	struct _v1_ping *ping;

	g_return_val_if_fail(mgr != NULL, NULL);
	g_return_val_if_fail(policy != NULL, NULL);

	ping = calloc(1, sizeof(struct _v1_ping));
	if (!ping) {
		error_nomem();
		return NULL;
	}

	ping->mgr = mgr;
	ping->host = g_strdup_printf("%s/v1/ping", h2manager_peek_host(mgr));
	memcpy(&ping->policy, policy, sizeof(GatewayHealthPolicy));

	return ping;
}

void v1_ping_free(V1Ping *ping)
{
	g_return_if_fail(ping != NULL);

	if (ping->timer_src)
		g_source_remove(ping->timer_src);

	if (ping->host)
		g_free(ping->host);

	memset(ping, 0, sizeof(V1Ping));
	free(ping);
}

static int _get_next_timeout(V1Ping *ping)
{
	int timeout;

	/**
	 * max(ttl_max * (0 < random() <= 1), ttl)
	 */
	timeout = ping->policy.ttl_max  * (random() / (float)RAND_MAX * 1.0f);
	if (timeout < ping->policy.ttl)
		timeout = ping->policy.ttl;

	/* Convert miliseconds to seconds (minimum 60 seconds) */
	timeout = timeout / 1000;
	if (timeout <= 0)
		timeout = 60;

	nugu_dbg("next ping timeout=%d", timeout);

	return timeout;
}

/* invoked in a thread loop */
static void _on_finish(HTTP2Request *req, void *userdata)
{
	int code;
	H2Manager *mgr = userdata;

	code = http2_request_get_response_code(req);
	if (code == HTTP2_RESPONSE_OK)
		return;

	nugu_error("ping send failed: %d", code);

	if (code == HTTP2_RESPONSE_AUTHFAIL || code == HTTP2_RESPONSE_FORBIDDEN)
		h2manager_set_status(mgr, H2_STATUS_TOKEN_FAILED);
	else
		h2manager_set_status(mgr, H2_STATUS_DISCONNECTED);
}

static gboolean _on_timeout(gpointer userdata)
{
	V1Ping *ping = userdata;
	HTTP2Request *req;
	int ret;

	nugu_dbg("Ping !");

	req = http2_request_new();
	http2_request_set_url(req, ping->host);
	http2_request_set_method(req, HTTP2_REQUEST_METHOD_GET);
	http2_request_set_content_type(req, HTTP2_REQUEST_CONTENT_TYPE_JSON);
	http2_request_set_finish_callback(req, _on_finish, ping->mgr);
	http2_request_enable_curl_log(req);

	/* Set maximum timeout to 5 seconds */
	http2_request_set_timeout(req, 5);

	ret = http2_network_add_request(ping->network, req);
	if (ret < 0)
		return FALSE;

	http2_request_unref(req);

	if (ret >= 0)
		ping->timer_src = g_timeout_add_seconds(_get_next_timeout(ping),
							_on_timeout, ping);

	return FALSE;
}

int v1_ping_establish(V1Ping *ping, HTTP2Network *net)
{
	g_return_val_if_fail(ping != NULL, -1);
	g_return_val_if_fail(net != NULL, -1);

	ping->network = net;
	ping->timer_src = g_timeout_add_seconds(_get_next_timeout(ping),
						_on_timeout, ping);

	return 0;
}
