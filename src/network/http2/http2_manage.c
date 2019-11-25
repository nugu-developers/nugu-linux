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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <glib.h>

#include "nugu_config.h"
#include "nugu_log.h"

#include "http2_manage.h"
#include "http2_network.h"
#include "gateway_registry.h"
#include "v1_directives.h"
#include "v1_event.h"
#include "v1_event_attachment.h"
#include "v1_ping.h"
#include "nugu_network_manager.h"

struct _h2_manager {
	HTTP2Network *network;

	V1Directives *directives;
	V1Ping *ping;

	char *token;
	char *host;

	int tid;
	enum h2manager_status status;
	H2ManagerStatusCallback callback;
	void *callback_userdata;
	pthread_cond_t status_cond;
	pthread_mutex_t status_lock;

	int emit_in_idle;
	guint idle_source;

	GatewayHealthPolicy policy;
	GList *servers;

	pthread_t thread_id;
};

static int _h2manager_network_start(H2Manager *manager)
{
	g_return_val_if_fail(manager != NULL, -1);

	if (manager->network) {
		nugu_dbg("network already started");
		return 0;
	}

	manager->network = http2_network_new();
	if (!manager->network)
		return -1;

	http2_network_set_token(manager->network, manager->token);
	http2_network_enable_curl_log(manager->network);
	http2_network_start(manager->network);

	return 0;
}

static int _h2manager_network_stop(H2Manager *manager, int with_status)
{
	g_return_val_if_fail(manager != NULL, -1);

	if (!manager->network) {
		nugu_dbg("network already stopped");
		return 0;
	}

	http2_network_free(manager->network);
	manager->network = NULL;

	if (with_status)
		h2manager_set_status(manager, H2_STATUS_DISCONNECTED);

	return 0;
}

static int _h2manager_establish_directive(H2Manager *manager)
{
	int ret;

	if (manager->directives)
		v1_directives_free(manager->directives);

	manager->directives = v1_directives_new(manager);

	ret = v1_directives_establish_sync(manager->directives,
					   manager->network);
	if (ret < 0) {
		v1_directives_free(manager->directives);
		manager->directives = NULL;
	}

	return ret;
}

static void _h2manager_establish_ping(H2Manager *manager)
{
	if (manager->ping)
		v1_ping_free(manager->ping);

	manager->ping = v1_ping_new(manager, &(manager->policy));
	v1_ping_establish(manager->ping, manager->network);
}

static int _h2manager_connect_to_server(H2Manager *manager, const char *host)
{
	int ret;

	g_return_val_if_fail(manager != NULL, -1);
	g_return_val_if_fail(host != NULL, -1);

	nugu_info("Try connect to %s", host);

	ret = _h2manager_network_start(manager);
	if (ret < 0)
		return -1;

	if (manager->host) {
		free(manager->host);
		manager->host = NULL;
	}

	manager->host = strdup(host);

	ret = _h2manager_establish_directive(manager);
	if (ret < 0) {
		_h2manager_network_stop(manager, 0);

		if (ret == -HTTP2_RESPONSE_AUTHFAIL ||
		    ret == -HTTP2_RESPONSE_FORBIDDEN)
			h2manager_set_status(manager, H2_STATUS_TOKEN_FAILED);

		return ret;
	}

	http2_network_disable_curl_log(manager->network);
	h2manager_set_status(manager, H2_STATUS_CONNECTED);
	_h2manager_establish_ping(manager);

	return 0;
}

static void *_connect_in_thread(void *userdata)
{
	H2Manager *manager = userdata;
	const GList *cur;
	int ret;

	for (cur = manager->servers; cur; cur = cur->next) {
		GatewayServer *item = cur->data;
		char *host = NULL;

		if (!item->protocol)
			continue;

		if (g_ascii_strcasecmp(item->protocol, "H2") == 0)
			host = g_strdup_printf("https://%s:%d", item->hostname,
					       item->port);
		else if (g_ascii_strcasecmp(item->protocol, "H2C") == 0)
			host = g_strdup_printf("http://%s:%d", item->hostname,
					       item->port);
		else {
			nugu_dbg("not supported protocol: %s", item->protocol);
			continue;
		}

		if (!host)
			continue;

		ret = _h2manager_connect_to_server(manager, host);
		if (ret < 0) {
			g_free(host);

			if (ret == -HTTP2_RESPONSE_AUTHFAIL ||
			    ret == -HTTP2_RESPONSE_FORBIDDEN) {
				nugu_dbg("token error. skip next hosts");
				break;
			} else
				continue;
		}

		nugu_info("Connected to %s", host);
		g_free(host);

		break;
	}

	nugu_info("connect thread finished");

	if (manager->status == H2_STATUS_CONNECTING)
		h2manager_set_status(manager, H2_STATUS_DISCONNECTED);

	return NULL;
}

static int _h2manager_connect_to_servers(H2Manager *manager)
{
	int ret;

	g_return_val_if_fail(manager != NULL, -1);
	g_return_val_if_fail(manager->servers != NULL, -1);

	ret = pthread_create(&manager->thread_id, NULL, _connect_in_thread,
			     manager);
	if (ret < 0) {
		nugu_error("pthread_create() failed.");
		return -1;
	}

	ret = pthread_setname_np(manager->thread_id, "connection");
	if (ret < 0)
		nugu_error("pthread_setname_np() failed");

	return 0;
}

static void on_policy(GatewayRegistry *registry, int response_code,
		      void *userdata)
{
	H2Manager *manager = userdata;
	int ret;

	if (response_code < 0) {
		nugu_error("network fail. (%d)", response_code);
		gateway_registry_free(registry);
		_h2manager_network_stop(manager, 1);
		return;
	} else if (response_code != HTTP2_RESPONSE_OK) {
		nugu_error("request fail. (%d)", response_code);
		gateway_registry_free(registry);
		h2manager_set_status(manager, H2_STATUS_TOKEN_FAILED);
		_h2manager_network_stop(manager, 0);
		return;
	}

	ret = gateway_registry_get_health_policy(registry, &(manager->policy));
	if (ret < 0) {
		nugu_error("health policy error");
		gateway_registry_free(registry);
		_h2manager_network_stop(manager, 1);
		return;
	}

	nugu_dbg("health_check_timeout: %d",
		 manager->policy.health_check_timeout);

	if (manager->servers)
		gateway_registry_server_list_free(manager->servers);

	manager->servers = gateway_registry_get_servers(registry);
	if (!manager->servers) {
		nugu_error("no servers");
		gateway_registry_free(registry);
		_h2manager_network_stop(manager, 1);
		return;
	}

	gateway_registry_free(registry);
	_h2manager_network_stop(manager, 0);
	_h2manager_connect_to_servers(manager);
}

H2Manager *h2manager_new(void)
{
	H2Manager *manager;
	const char *token_value;

	manager = calloc(1, sizeof(H2Manager));
	if (!manager) {
		error_nomem();
		return NULL;
	}

	manager->status = H2_STATUS_READY;
	manager->tid = (pid_t)syscall(SYS_gettid);

	token_value = nugu_config_get(NUGU_CONFIG_KEY_TOKEN);
	if (token_value)
		manager->token = g_strdup(token_value);

	nugu_info("created");

	return manager;
}

void h2manager_free(H2Manager *manager)
{
	g_return_if_fail(manager != NULL);

	nugu_dbg("destroy h2manager");

	if (manager->idle_source) {
		g_source_remove(manager->idle_source);
		manager->idle_source = 0;
	}

	if (manager->servers) {
		gateway_registry_server_list_free(manager->servers);
		manager->servers = NULL;
	}

	if (manager->ping) {
		v1_ping_free(manager->ping);
		manager->ping = NULL;
	}

	if (manager->directives) {
		v1_directives_free(manager->directives);
		manager->directives = NULL;
	}

	if (manager->thread_id)
		pthread_join(manager->thread_id, NULL);

	if (manager->host)
		g_free(manager->host);

	if (manager->token)
		g_free(manager->token);

	nugu_dbg("destroyed");

	memset(manager, 0, sizeof(H2Manager));
	free(manager);
}

const char *h2manager_peek_host(H2Manager *manager)
{
	g_return_val_if_fail(manager != NULL, NULL);

	return manager->host;
}

int h2manager_set_status_callback(H2Manager *manager,
				  H2ManagerStatusCallback callback,
				  void *userdata)
{
	g_return_val_if_fail(manager != NULL, -1);

	manager->callback = callback;
	manager->callback_userdata = userdata;

	return 0;
}

static gboolean _emit_callback_in_idle(H2Manager *manager)
{
	nugu_dbg("emit callback - idle");

	pthread_mutex_lock(&manager->status_lock);
	manager->emit_in_idle = 1;
	pthread_cond_signal(&manager->status_cond);
	pthread_mutex_unlock(&manager->status_lock);

	manager->idle_source = 0;

	if (manager->callback)
		manager->callback(manager->callback_userdata);

	nugu_dbg("emit callback - idle done");

	return FALSE;
}

int h2manager_set_status(H2Manager *manager, enum h2manager_status status)
{
	int tid;
	struct timespec spec;
	int wait_status = 0;

	g_return_val_if_fail(manager != NULL, -1);

	pthread_mutex_lock(&manager->status_lock);

	nugu_dbg("set h2manager status to %d", status);

	/* Prevent event emit for same status */
	if (manager->status == status) {
		pthread_mutex_unlock(&manager->status_lock);
		return 0;
	}

	manager->status = status;
	pthread_mutex_unlock(&manager->status_lock);

	/* Set from same thread */
	tid = (pid_t)syscall(SYS_gettid);
	if (tid == manager->tid) {
		nugu_dbg("emit callback");

		if (manager->callback)
			manager->callback(manager->callback_userdata);

		nugu_dbg("emit callback - done");
		return 0;
	}

	pthread_mutex_lock(&manager->status_lock);
	manager->emit_in_idle = 0;
	pthread_mutex_unlock(&manager->status_lock);

	nugu_dbg("request to emit callback at idle");
	manager->idle_source =
		g_idle_add((GSourceFunc)_emit_callback_in_idle, manager);

	clock_gettime(CLOCK_REALTIME, &spec);
	spec.tv_sec = spec.tv_sec + 5;

	pthread_mutex_lock(&manager->status_lock);
	if (manager->emit_in_idle == 0)
		wait_status = pthread_cond_timedwait(
			&manager->status_cond, &manager->status_lock, &spec);
	pthread_mutex_unlock(&manager->status_lock);

	if (wait_status == ETIMEDOUT)
		nugu_info("timeout");

	return 0;
}

enum h2manager_status h2manager_get_status(H2Manager *manager)
{
	int status;

	g_return_val_if_fail(manager != NULL, H2_STATUS_ERROR);

	pthread_mutex_lock(&manager->status_lock);
	status = manager->status;
	pthread_mutex_unlock(&manager->status_lock);

	return status;
}

int h2manager_connect(H2Manager *manager)
{
	GatewayRegistry *registry;
	int ret;

	g_return_val_if_fail(manager != NULL, -1);

	if (manager->status == H2_STATUS_CONNECTING ||
	    manager->status == H2_STATUS_CONNECTED) {
		nugu_dbg("already connecting or connected");
		return 0;
	}

	h2manager_set_status(manager, H2_STATUS_CONNECTING);
	_h2manager_network_start(manager);

	registry = gateway_registry_new();
	if (!registry) {
		_h2manager_network_stop(manager, 1);
		return -1;
	}

	gateway_registry_set_callback(registry, on_policy, manager);

	nugu_info("Try connect to Registry server");

	ret = gateway_registry_policy(registry, manager->network);
	if (ret < 0) {
		nugu_error("get policy failed: %d", ret);
		gateway_registry_free(registry);
		_h2manager_network_stop(manager, 1);
		return ret;
	}

	nugu_info("Success to GET request for policy");

	return 0;
}

int h2manager_disconnect(H2Manager *manager)
{
	g_return_val_if_fail(manager != NULL, -1);

	nugu_dbg("destroy network");

	_h2manager_network_stop(manager, 1);

	nugu_dbg("destroy ping, directives");

	if (manager->ping) {
		v1_ping_free(manager->ping);
		manager->ping = NULL;
	}

	if (manager->directives) {
		v1_directives_free(manager->directives);
		manager->directives = NULL;
	}

	return 0;
}

int h2manager_send_event(H2Manager *manager, const char *json)
{
	V1Event *e;

	if (h2manager_get_status(manager) != H2_STATUS_CONNECTED) {
		nugu_error("http2 network is not ready(%d)",
			   h2manager_get_status(manager));
		return -1;
	}

	e = v1_event_new(manager);
	if (!e)
		return -1;

	v1_event_set_json(e, json, strlen(json));

	v1_event_send_with_free(e, manager->network);

	return 0;
}

int h2manager_send_event_attachment(H2Manager *manager, const char *name_space,
				    const char *name, const char *version,
				    const char *parent_msg_id,
				    const char *msg_id, const char *dialog_id,
				    int seq, int is_end, size_t length,
				    unsigned char *data)
{
	V1EventAttachment *ea;

	if (h2manager_get_status(manager) != H2_STATUS_CONNECTED) {
		nugu_error("http2 network is not ready(%d)",
			   h2manager_get_status(manager));
		return -1;
	}

	ea = v1_event_attachment_new(manager->host);
	if (!ea)
		return -1;

	if (data != NULL && length > 0)
		v1_event_attachment_set_data(ea, data, length);
	v1_event_attachment_set_query(ea, name_space, name, version,
				      parent_msg_id, msg_id, dialog_id, seq,
				      is_end);
	v1_event_attachment_send_with_free(ea, manager->network);

	return 0;
}
