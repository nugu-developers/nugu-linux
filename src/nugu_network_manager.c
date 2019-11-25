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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <glib.h>
#include <unistd.h>
#include <string.h>

#include "nugu_log.h"
#include "nugu_uuid.h"
#include "nugu_equeue.h"
#include "nugu_network_manager.h"
#include "nugu_directive_sequencer.h"

#include "dg_registry.h"
#include "dg_server.h"
#include "dg_types.h"

enum connection_step {
	STEP_IDLE, /**< Idle */
	STEP_INVALID_TOKEN,
	STEP_DISCONNECTING,
	STEP_REGISTRY_FAILED,
	STEP_REGISTRY_DONE,
	STEP_SERVER_CONNECTING,
	STEP_SERVER_FAILED,
	STEP_SERVER_CONNECTTED,
	STEP_MAX
};

static const char * const _debug_connection_step[] = {
	[STEP_IDLE] = "STEP_IDLE", /**< IDLE */
	[STEP_INVALID_TOKEN] = "STEP_INVALID_TOKEN",
	[STEP_DISCONNECTING] = "STEP_DISCONNECTING",
	[STEP_REGISTRY_FAILED] = "STEP_REGISTRY_FAILED",
	[STEP_REGISTRY_DONE] = "STEP_REGISTRY_DONE",
	[STEP_SERVER_CONNECTING] = "STEP_SERVER_CONNECTING",
	[STEP_SERVER_FAILED] = "STEP_SERVER_FAILED",
	[STEP_SERVER_CONNECTTED] = "STEP_SERVER_CONNECTTED"
};

static const char * const _debug_status_strmap[] = {
	[NUGU_NETWORK_UNKNOWN] = "NUGU_NETWORK_UNKNOWN", /**< Unknown */
	[NUGU_NETWORK_CONNECTING] = "NUGU_NETWORK_CONNECTING",
	[NUGU_NETWORK_DISCONNECTED] = "NUGU_NETWORK_DISCONNECTED",
	[NUGU_NETWORK_CONNECTED] = "NUGU_NETWORK_CONNECTED",
	[NUGU_NETWORK_TOKEN_ERROR] = "NUGU_NETWORK_TOKEN_ERROR",
};

struct _nugu_network {
	enum connection_step step;

	/* Registry */
	DGRegistry *registry;
	struct dg_health_check_policy policy;
	GList *server_list;
	const GList *serverinfo;

	/* Server */
	DGServer *server;
	int retry_count;

	/* Status & Callback */
	NuguNetworkStatus cur_status;
	NetworkManagerCallback callback;
	void *callback_userdata;
};

typedef struct _nugu_network NetworkManager;

static void on_directive(enum nugu_equeue_type type, void *data, void *userdata)
{
	nugu_dirseq_push(data);
}

static void on_attachment(enum nugu_equeue_type type, void *data,
			  void *userdata)
{
	NuguDirective *ndir;
	struct equeue_data_attachment *item = data;

	ndir = nugu_dirseq_find_by_msgid(item->parent_msg_id);
	if (!ndir)
		return;

	if (item->is_end)
		nugu_directive_close_data(ndir);

	nugu_directive_set_media_type(ndir, item->media_type);
	nugu_directive_add_data(ndir, item->length, item->data);
}

static void on_destroy_attachment(void *data)
{
	struct equeue_data_attachment *item = data;

	if (item->data)
		free(item->data);
	if (item->parent_msg_id)
		free(item->parent_msg_id);
	if (item->media_type)
		free(item->media_type);

	free(item);
}

static void _update_status(NetworkManager *nm, NuguNetworkStatus new_status)
{
	if (nm->cur_status == new_status) {
		nugu_dbg("ignore same status: %s (%d)",
			 _debug_status_strmap[new_status], new_status);
		return;
	}

	nugu_info("Network status: %s (%d) -> %s (%d)",
		  _debug_status_strmap[nm->cur_status], nm->cur_status,
		  _debug_status_strmap[new_status], new_status);

	nm->cur_status = new_status;

	if (nm->callback)
		nm->callback(nm->callback_userdata);
}

static int _update_step(NetworkManager *nm, enum connection_step new_step)
{
	if (new_step == nm->step) {
		nugu_dbg("ignore same step: %s (%d)",
			 _debug_connection_step[new_step], new_step);
		return -1;
	}

	nugu_dbg("Network connection step: %s (%d) -> %s (%d)",
		 _debug_connection_step[nm->step], nm->step,
		 _debug_connection_step[new_step], new_step);

	nm->step = new_step;

	return 0;
}

static void _log_server_info(NetworkManager *nm, int retry_count_limit)
{
	int pos = 0;
	int length = 0;
	GList *cur;

	cur = nm->server_list;
	for (; cur; cur = cur->next, length++) {
		if (cur == nm->serverinfo)
			pos = length + 1;
	}

	if (nm->retry_count == 0)
		nugu_info("Try connect to [%d/%d] server", pos, length);
	else {
		nugu_info("Retry[%d/%d] connect to [%d/%d] server",
			  nm->retry_count, retry_count_limit, pos, length);
	}
}

static void _try_connect_to_servers(NetworkManager *nm)
{
	/* server already assigned. retry to connect */
	if (nm->server) {
		const struct dg_server_policy *policy = nm->serverinfo->data;

		/* Retry to connect to current server */
		if (nm->retry_count < policy->retry_count_limit) {
			nm->retry_count++;
			_log_server_info(nm, policy->retry_count_limit);

			if (dg_server_connect_async(nm->server) == 0)
				return;
		} else {
			nugu_error("retry count over");
		}

		/* forgot current server */
		nugu_dbg("forgot current server");
		dg_server_free(nm->server);
		nm->server = NULL;
		nm->retry_count = 0;
	}

	/* Determine which server candidates to connect to */
	if (nm->serverinfo == NULL)
		nm->serverinfo = nm->server_list;
	else
		nm->serverinfo = nm->serverinfo->next;

	for (; nm->serverinfo; nm->serverinfo = nm->serverinfo->next) {
		_log_server_info(nm, 0);

		nm->retry_count = 0;

		nm->server = dg_server_new(nm->serverinfo->data);
		if (!nm->server) {
			nugu_error("dg_server_new() failed. try next server");
			continue;
		}

		if (dg_server_connect_async(nm->server) < 0) {
			dg_server_free(nm->server);
			nm->server = NULL;
			nugu_error("Server is unavailable. try next server");
			continue;
		}

		return;
	}

	nugu_error("fail to connect all servers");
	nm->serverinfo = NULL;
	_update_status(nm, NUGU_NETWORK_DISCONNECTED);
}

static void _process_connecting(NetworkManager *nm,
				enum connection_step new_step)
{
	g_return_if_fail(nm != NULL);

	if (_update_step(nm, new_step) < 0)
		return;

	switch (new_step) {
	case STEP_INVALID_TOKEN:
		_update_status(nm, NUGU_NETWORK_TOKEN_ERROR);
		break;

	case STEP_REGISTRY_FAILED:
		if (nm->registry) {
			dg_registry_free(nm->registry);
			nm->registry = NULL;
		}

		/* No cached server-list */
		if (nm->server_list == NULL) {
			nugu_error("registry failed. no cached server-list");
			_update_status(nm, NUGU_NETWORK_DISCONNECTED);
			break;
		}

		nugu_warn("registry failed. use cached server-list");
		_process_connecting(nm, STEP_SERVER_CONNECTING);
		break;

	case STEP_REGISTRY_DONE:
		if (nm->registry) {
			dg_registry_free(nm->registry);
			nm->registry = NULL;
		}
		_process_connecting(nm, STEP_SERVER_CONNECTING);
		break;

	case STEP_SERVER_CONNECTING:
		_update_status(nm, NUGU_NETWORK_CONNECTING);
		_try_connect_to_servers(nm);
		break;

	case STEP_SERVER_CONNECTTED:
		dg_server_start_health_check(nm->server, &(nm->policy));
		nm->retry_count = 0;
		_update_status(nm, NUGU_NETWORK_CONNECTED);
		break;

	case STEP_SERVER_FAILED:
		nugu_dbg("retry connection");
		_process_connecting(nm, STEP_SERVER_CONNECTING);
		break;

	default:
		break;
	}
}

static void on_receive_registry_health(enum nugu_equeue_type type, void *data,
				       void *userdata)
{
	NetworkManager *nm = userdata;
	struct dg_health_check_policy *policy = data;

	g_return_if_fail(policy != NULL);

	memcpy(&(nm->policy), policy, sizeof(struct dg_health_check_policy));
}

static void on_receive_registry_servers(enum nugu_equeue_type type, void *data,
					void *userdata)
{
	NetworkManager *nm = userdata;

	if (nm->server_list)
		g_list_free_full(nm->server_list, free);

	nm->server_list = data;
	nm->serverinfo = NULL;

	_process_connecting(userdata, STEP_REGISTRY_DONE);
}

static void on_event(enum nugu_equeue_type type, void *data, void *userdata)
{
	switch (type) {
	case NUGU_EQUEUE_TYPE_INVALID_TOKEN:
		nugu_dbg("received invalid token event");
		_process_connecting(userdata, STEP_INVALID_TOKEN);
		break;
	case NUGU_EQUEUE_TYPE_REGISTRY_FAILED:
		nugu_dbg("received registry failed event");
		_process_connecting(userdata, STEP_REGISTRY_FAILED);
		break;
	case NUGU_EQUEUE_TYPE_SEND_PING_FAILED:
		nugu_dbg("received send ping failed event");
		_process_connecting(userdata, STEP_SERVER_FAILED);
		break;
	case NUGU_EQUEUE_TYPE_SERVER_DISCONNECTED:
		nugu_dbg("received server disconnected event");
		_process_connecting(userdata, STEP_SERVER_FAILED);
		break;
	case NUGU_EQUEUE_TYPE_SERVER_CONNECTED:
		nugu_dbg("received server connected event");
		_process_connecting(userdata, STEP_SERVER_CONNECTTED);
		break;
	default:
		nugu_error("unhandled event: %d", type);
		break;
	}
}

static NetworkManager *nugu_network_manager_new(void)
{
	NetworkManager *nm;

	nm = calloc(1, sizeof(NetworkManager));
	if (!nm) {
		error_nomem();
		return NULL;
	}

	/* Received message from server */
	nugu_equeue_set_handler(NUGU_EQUEUE_TYPE_NEW_DIRECTIVE, on_directive,
				NULL, nm);
	nugu_equeue_set_handler(NUGU_EQUEUE_TYPE_NEW_ATTACHMENT, on_attachment,
				on_destroy_attachment, nm);

	/* Received registry policy */
	nugu_equeue_set_handler(NUGU_EQUEUE_TYPE_REGISTRY_HEALTH,
				on_receive_registry_health, free, nm);
	nugu_equeue_set_handler(NUGU_EQUEUE_TYPE_REGISTRY_SERVERS,
				on_receive_registry_servers, NULL, nm);

	/* Handler for simple event */
	nugu_equeue_set_handler(NUGU_EQUEUE_TYPE_REGISTRY_FAILED, on_event,
				NULL, nm);
	nugu_equeue_set_handler(NUGU_EQUEUE_TYPE_INVALID_TOKEN, on_event, NULL,
				nm);
	nugu_equeue_set_handler(NUGU_EQUEUE_TYPE_SEND_PING_FAILED, on_event,
				NULL, nm);
	nugu_equeue_set_handler(NUGU_EQUEUE_TYPE_SERVER_DISCONNECTED, on_event,
				NULL, nm);
	nugu_equeue_set_handler(NUGU_EQUEUE_TYPE_SERVER_CONNECTED, on_event,
				NULL, nm);

	nm->cur_status = NUGU_NETWORK_UNKNOWN;
	nm->step = STEP_IDLE;

	return nm;
}

static void nugu_network_manager_free(NetworkManager *nm)
{
	g_return_if_fail(nm != NULL);

	if (nm->server_list)
		g_list_free_full(nm->server_list, free);

	memset(nm, 0, sizeof(NetworkManager));
	free(nm);
}

static NetworkManager *_network;

EXPORT_API int nugu_network_manager_initialize(void)
{
	struct timespec spec;

	if (_network) {
		nugu_dbg("already initialized");
		return 0;
	}

	clock_gettime(CLOCK_REALTIME, &spec);
	srandom(spec.tv_nsec ^ spec.tv_sec);

	_network = nugu_network_manager_new();
	if (!_network)
		return -1;

	return 0;
}

EXPORT_API void nugu_network_manager_deinitialize(void)
{
	if (!_network)
		return;

	nugu_network_manager_disconnect();
	nugu_network_manager_free(_network);
	_network = NULL;

	nugu_dirseq_deinitialize();
}

EXPORT_API int nugu_network_manager_connect(void)
{
	g_return_val_if_fail(_network != NULL, -1);

	if (_network->cur_status == NUGU_NETWORK_CONNECTING) {
		nugu_dbg("connection in progress");
		return 0;
	}

	if (_network->cur_status == NUGU_NETWORK_CONNECTED) {
		nugu_dbg("already connected");
		return 0;
	}

	_network->step = STEP_IDLE;
	_network->registry = dg_registry_new();

	if (dg_registry_request(_network->registry) < 0) {
		dg_registry_free(_network->registry);
		_network->registry = NULL;
		_update_status(_network, NUGU_NETWORK_DISCONNECTED);
		return -1;
	};

	_update_status(_network, NUGU_NETWORK_CONNECTING);

	return 0;
}

EXPORT_API int nugu_network_manager_disconnect(void)
{
	g_return_val_if_fail(_network != NULL, -1);

	if (_network->cur_status == NUGU_NETWORK_DISCONNECTED) {
		nugu_dbg("already disconnected");
		return 0;
	}

	nugu_info("disconnecting: current step is %d", _network->step);

	if (_network->server) {
		dg_server_free(_network->server);
		_network->server = NULL;
	}

	if (_network->registry) {
		dg_registry_free(_network->registry);
		_network->registry = NULL;
	}

	return 0;
}

EXPORT_API int
nugu_network_manager_set_callback(NetworkManagerCallback callback,
				  void *userdata)
{
	if (!_network)
		return -1;

	_network->callback = callback;
	_network->callback_userdata = userdata;

	return 0;
}

EXPORT_API NuguNetworkStatus nugu_network_manager_get_status(void)
{
	return _network->cur_status;
}

EXPORT_API int nugu_network_manager_send_event(NuguEvent *nev)
{
	if (!_network) {
		nugu_error("network manager not initialized");
		return -1;
	}

	if (!_network->server) {
		nugu_error("not connected");
		return -1;
	}

	if (nugu_event_peek_context(nev) == NULL) {
		nugu_error("context must not null");
		return -1;
	}

	return dg_server_send_event(_network->server, nev);
}

EXPORT_API int nugu_network_manager_send_event_data(NuguEvent *nev, int is_end,
						    size_t length,
						    unsigned char *data)
{
	if (!_network) {
		nugu_error("network manager not initialized");
		return -1;
	}

	return dg_server_send_attachment(_network->server, nev, is_end, length,
					 data);
}
