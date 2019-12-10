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

#include <stdlib.h>
#include <string.h>

#include "nugu_log.h"
#include "nugu_event.h"
#include "nugu_uuid.h"

#include "dg_server.h"

#include "http2/http2_network.h"
#include "http2/v1_directives.h"
#include "http2/v1_event.h"
#include "http2/v1_event_attachment.h"
#include "http2/v1_ping.h"
#include "http2/v1_policies.h"

struct _dg_server {
	HTTP2Network *net;
	V1Directives *directives;
	V1Ping *ping;

	int retry_count;
	gchar *host;
	NuguNetworkServerPolicy policy;
	enum dg_server_type type;
};

DGServer *dg_server_new(const NuguNetworkServerPolicy *policy)
{
	struct _dg_server *server;
	gchar *host;

	g_return_val_if_fail(policy != NULL, NULL);

	switch (policy->protocol) {
	case NUGU_NETWORK_PROTOCOL_H2:
		host = g_strdup_printf("https://%s:%d", policy->hostname,
				       policy->port);
		break;
	case NUGU_NETWORK_PROTOCOL_H2C:
		host = g_strdup_printf("http://%s:%d", policy->hostname,
				       policy->port);
		break;
	default:
		nugu_error("not supported protocol: %d", policy->protocol);
		return NULL;
	}

	nugu_dbg("server address: %s", host);

	server = calloc(1, sizeof(struct _dg_server));
	if (!server) {
		error_nomem();
		g_free(host);
		return NULL;
	}

	server->net = http2_network_new();
	if (!server->net) {
		free(server);
		g_free(host);
		return NULL;
	}

	memcpy(&(server->policy), policy, sizeof(NuguNetworkServerPolicy));
	server->host = host;
	server->type = DG_SERVER_TYPE_NORMAL;
	server->retry_count = 0;

	http2_network_set_token(server->net, nugu_network_manager_peek_token());
	http2_network_enable_curl_log(server->net);
	http2_network_start(server->net);

	return server;
}

void dg_server_free(DGServer *server)
{
	g_return_if_fail(server != NULL);

	g_free(server->host);

	if (server->directives)
		v1_directives_free(server->directives);

	if (server->ping)
		v1_ping_free(server->ping);

	http2_network_free(server->net);

	memset(server, 0, sizeof(struct _dg_server));
	free(server);
}

int dg_server_set_type(DGServer *server, enum dg_server_type type)
{
	g_return_val_if_fail(server != NULL, -1);

	server->type = type;

	return 0;
}

enum dg_server_type dg_server_get_type(DGServer *server)
{
	g_return_val_if_fail(server != NULL, DG_SERVER_TYPE_NORMAL);

	return server->type;
}

int dg_server_connect_async(DGServer *server)
{
	int ret;

	g_return_val_if_fail(server != NULL, -1);

	if (server->directives)
		v1_directives_free(server->directives);

	server->directives = v1_directives_new(
		server->host, server->policy.connection_timeout_ms / 1000);

	ret = v1_directives_establish(server->directives, server->net);
	if (ret < 0) {
		v1_directives_free(server->directives);
		server->directives = NULL;
	}

	return ret;
}

int dg_server_start_health_check(DGServer *server,
				 const struct dg_health_check_policy *policy)
{
	g_return_val_if_fail(server != NULL, -1);
	g_return_val_if_fail(policy != NULL, -1);

	if (server->ping)
		v1_ping_free(server->ping);

	server->ping = v1_ping_new(server->host, policy);
	v1_ping_establish(server->ping, server->net);

	return 0;
}

unsigned int dg_server_get_retry_count(DGServer *server)
{
	g_return_val_if_fail(server != NULL, -1);

	return server->retry_count;
}

unsigned int dg_server_get_retry_count_limit(DGServer *server)
{
	g_return_val_if_fail(server != NULL, -1);

	return server->policy.retry_count_limit;
}

int dg_server_is_retry_over(DGServer *server)
{
	g_return_val_if_fail(server != NULL, -1);

	if (server->retry_count >= server->policy.retry_count_limit)
		return 1;

	return 0;
}

void dg_server_increse_retry_count(DGServer *server)
{
	g_return_if_fail(server != NULL);

	server->retry_count++;
}

void dg_server_reset_retry_count(DGServer *server)
{
	g_return_if_fail(server != NULL);

	server->retry_count = 0;
}

int dg_server_send_event(DGServer *server, NuguEvent *nev)
{
	char *payload;
	V1Event *e;

	payload = nugu_event_generate_payload(nev);
	if (!payload)
		return -1;

	e = v1_event_new(server->host);
	if (!e) {
		g_free(payload);
		return -1;
	}

	v1_event_set_json(e, payload, strlen(payload));
	g_free(payload);

	v1_event_send_with_free(e, server->net);

	return 0;
}

int dg_server_send_attachment(DGServer *server, NuguEvent *nev, int is_end,
			      size_t length, unsigned char *data)
{
	char *msg_id;
	int ret = -1;
	V1EventAttachment *ea;

	g_return_val_if_fail(server != NULL, -1);
	g_return_val_if_fail(nev != NULL, -1);

	ea = v1_event_attachment_new(server->host);
	if (!ea)
		return -1;

	msg_id = nugu_uuid_generate_short();

	if (data != NULL && length > 0)
		v1_event_attachment_set_data(ea, data, length);

	v1_event_attachment_set_query(ea, nugu_event_peek_namespace(nev),
				      nugu_event_peek_name(nev),
				      nugu_event_peek_version(nev),
				      nugu_event_peek_msg_id(nev), msg_id,
				      nugu_event_peek_dialog_id(nev),
				      nugu_event_get_seq(nev), is_end);

	free(msg_id);

	ret = v1_event_attachment_send_with_free(ea, server->net);
	if (ret < 0)
		return ret;

	nugu_event_increase_seq(nev);

	return ret;
}
