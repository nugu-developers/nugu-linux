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

#include "http2_request.h"
#include "v1_ping.h"

struct _v1_ping {
	HTTP2Network *network;
	struct dg_health_check_policy policy;
	guint timer_src;
	char *url;
};

V1Ping *v1_ping_new(const char *host,
		    const struct dg_health_check_policy *policy)
{
	struct _v1_ping *ping;

	g_return_val_if_fail(host != NULL, NULL);
	g_return_val_if_fail(policy != NULL, NULL);

	ping = calloc(1, sizeof(struct _v1_ping));
	if (!ping) {
		nugu_error_nomem();
		return NULL;
	}

	ping->url = g_strdup_printf("%s/v1/ping", host);
	memcpy(&ping->policy, policy, sizeof(struct dg_health_check_policy));

	return ping;
}

void v1_ping_free(V1Ping *ping)
{
	g_return_if_fail(ping != NULL);

	if (ping->timer_src)
		g_source_remove(ping->timer_src);

	if (ping->url)
		g_free(ping->url);

	memset(ping, 0, sizeof(V1Ping));
	free(ping);
}

static int _get_next_timeout(V1Ping *ping)
{
	int timeout;

	/**
	 * max(ttl_max_ms * (0 < random() <= 1), retry_delay_ms)
	 */
	timeout = ping->policy.ttl_max_ms * (random() / (float)RAND_MAX * 1.0f);
	if (timeout < ping->policy.retry_delay_ms)
		timeout = ping->policy.retry_delay_ms;

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

	code = http2_request_get_response_code(req);
	if (code == HTTP2_RESPONSE_OK)
		return;

	nugu_error("ping send failed: %d", code);

	if (code == HTTP2_RESPONSE_AUTHFAIL || code == HTTP2_RESPONSE_FORBIDDEN)
		nugu_equeue_push(NUGU_EQUEUE_TYPE_INVALID_TOKEN, NULL);
	else
		nugu_equeue_push(NUGU_EQUEUE_TYPE_SEND_PING_FAILED, NULL);
}

static gboolean _on_timeout(gpointer userdata)
{
	V1Ping *ping = userdata;
	HTTP2Request *req;
	int ret;

	nugu_dbg("Ping !");

	req = http2_request_new();
	http2_request_set_url(req, ping->url);
	http2_request_set_method(req, HTTP2_REQUEST_METHOD_GET);
	http2_request_set_content_type(req, HTTP2_REQUEST_CONTENT_TYPE_JSON,
				       NULL);
	http2_request_set_finish_callback(req, _on_finish, ping);
	http2_request_enable_curl_log(req);

	/* Set maximum timeout */
	http2_request_set_timeout(req,
				  ping->policy.health_check_timeout_ms / 1000);

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
