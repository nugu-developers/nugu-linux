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
	STEP_SERVER_HANDOFF,
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
	[STEP_SERVER_CONNECTTED] = "STEP_SERVER_CONNECTTED",
	[STEP_SERVER_HANDOFF] = "STEP_SERVER_HANDOFF"
};

static const char * const _debug_status_strmap[] = {
	[NUGU_NETWORK_DISCONNECTED] = "NUGU_NETWORK_DISCONNECTED",
	[NUGU_NETWORK_CONNECTING] = "NUGU_NETWORK_CONNECTING",
	[NUGU_NETWORK_CONNECTED] = "NUGU_NETWORK_CONNECTED",
	[NUGU_NETWORK_TOKEN_ERROR] = "NUGU_NETWORK_TOKEN_ERROR",
};

struct _nugu_network {
	enum connection_step step;
	char *token;

	/* Registry */
	DGRegistry *registry;
	struct dg_health_check_policy policy;
	GList *server_list;
	const GList *serverinfo;

	/* Server */
	DGServer *server;

	/* Handoff */
	DGServer *handoff;
	NetworkManagerHandoffStatusCallback handoff_callback;
	void *handoff_callback_userdata;

	/* Status & Callback */
	NuguNetworkStatus cur_status;
	NetworkManagerStatusCallback status_callback;
	void *status_callback_userdata;
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

	if (nm->status_callback)
		nm->status_callback(nm->cur_status,
				    nm->status_callback_userdata);
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

static int _start_registry(NetworkManager *nm)
{
	nugu_info("start registry");

	nm->step = STEP_IDLE;
	nm->registry = dg_registry_new();

	if (dg_registry_request(nm->registry) < 0) {
		dg_registry_free(nm->registry);
		nm->registry = NULL;

		_update_status(nm, NUGU_NETWORK_DISCONNECTED);
		return -1;
	};

	_update_status(nm, NUGU_NETWORK_CONNECTING);
	return 0;
}

static void _log_server_info(NetworkManager *nm, DGServer *server)
{
	int pos = -1;
	int length = 0;

	if (server == nm->server) {
		GList *cur;

		cur = nm->server_list;
		for (; cur; cur = cur->next, length++) {
			if (cur == nm->serverinfo)
				pos = length + 1;
		}
	}

	if (dg_server_get_retry_count(server) == 0) {
		if (pos != -1)
			nugu_info("Try connect to [%d/%d] server", pos, length);
		else
			nugu_info("Try connect to handoff server");
	} else {
		if (pos != -1)
			nugu_info("Retry[%d/%d] connect to [%d/%d] server",
				  dg_server_get_retry_count(server),
				  dg_server_get_retry_count_limit(server), pos,
				  length);
		else
			nugu_info("Retry[%d/%d] connect to handoff server",
				  dg_server_get_retry_count(server),
				  dg_server_get_retry_count_limit(server));
	}
}

static void _try_connect_to_handoff(NetworkManager *nm)
{
	int success = 0;

	if (!nm->handoff)
		return;

	/* Retry to connect to handoff server */
	if (dg_server_is_retry_over(nm->handoff) == 0) {
		dg_server_increse_retry_count(nm->handoff);
		_log_server_info(nm, nm->handoff);

		if (dg_server_connect_async(nm->handoff) == 0) {
			nugu_dbg("request success");
			success = 1;
		} else {
			nugu_error("Server is unavailable.");
		}
	} else {
		nugu_error("retry count over");
	}

	if (success)
		return;

	dg_server_free(nm->handoff);
	nm->handoff = NULL;

	/* disconnect current server */
	dg_server_free(nm->server);
	nm->server = NULL;
	nm->serverinfo = NULL;

	if (nm->handoff_callback)
		nm->handoff_callback(NUGU_NETWORK_HANDOFF_FAILED,
				     nm->handoff_callback_userdata);

	/* restart from registry */
	_start_registry(nm);
}

static void _try_connect_to_servers(NetworkManager *nm)
{
	/* server already assigned. retry to connect */
	if (nm->server) {
		enum dg_server_type type;
		int success = 0;

		/* Retry to connect to current server */
		if (dg_server_is_retry_over(nm->server) == 0) {
			dg_server_increse_retry_count(nm->server);
			_log_server_info(nm, nm->server);

			if (dg_server_connect_async(nm->server) == 0) {
				nugu_dbg("request success");
				success = 1;
			} else {
				nugu_error("Server is unavailable.");
			}
		} else {
			nugu_error("retry count over");
		}

		if (success)
			return;

		type = dg_server_get_type(nm->server);

		/* forgot current server */
		nugu_dbg("forgot current server");
		dg_server_free(nm->server);
		nm->server = NULL;

		/* if disconnected from handoff server, start from registry */
		if (type == DG_SERVER_TYPE_HANDOFF) {
			_start_registry(nm);
			return;
		}
	}

	/* Determine which server candidates to connect to */
	if (nm->serverinfo == NULL) {
		nugu_dbg("start with first server in the list");
		nm->serverinfo = nm->server_list;
	} else {
		nugu_dbg("start with next server");
		nm->serverinfo = nm->serverinfo->next;
	}

	for (; nm->serverinfo; nm->serverinfo = nm->serverinfo->next) {
		nm->server = dg_server_new(nm->serverinfo->data);
		if (!nm->server) {
			nugu_error("dg_server_new() failed. try next server");
			continue;
		}

		_log_server_info(nm, nm->server);

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
		nugu_network_manager_disconnect();
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
		if (nm->handoff)
			_try_connect_to_handoff(nm);
		else {
			_update_status(nm, NUGU_NETWORK_CONNECTING);
			_try_connect_to_servers(nm);
		}
		break;

	case STEP_SERVER_CONNECTTED:
		if (nm->handoff) {
			nugu_info("handoff success. replace the server");
			dg_server_free(nm->server);
			nm->serverinfo = NULL;
			nm->server = nm->handoff;
			nm->handoff = NULL;

			if (nm->handoff_callback)
				nm->handoff_callback(
					NUGU_NETWORK_HANDOFF_SUCCESS,
					nm->handoff_callback_userdata);
		}
		dg_server_start_health_check(nm->server, &(nm->policy));
		dg_server_reset_retry_count(nm->server);
		_update_status(nm, NUGU_NETWORK_CONNECTED);
		break;

	case STEP_SERVER_FAILED:
		nugu_dbg("retry connection");
		_process_connecting(nm, STEP_SERVER_CONNECTING);
		break;

	case STEP_SERVER_HANDOFF:
		/*
		 * In case of handoff failure, it is necessary to start from
		 * the registry instead of the next server in the existing
		 * server list, so release all registry information.
		 */
		if (nm->server_list) {
			g_list_free_full(nm->server_list, free);
			nm->server_list = NULL;
		}
		nm->serverinfo = NULL;
		break;

	default:
		nugu_warn("unhandled step: %d", new_step);
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

	nm->cur_status = NUGU_NETWORK_DISCONNECTED;
	nm->step = STEP_IDLE;

	return nm;
}

static void nugu_network_manager_free(NetworkManager *nm)
{
	g_return_if_fail(nm != NULL);

	if (nm->server_list)
		g_list_free_full(nm->server_list, free);

	if (nm->token)
		free(nm->token);

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

	return _start_registry(_network);
}

EXPORT_API int nugu_network_manager_disconnect(void)
{
	g_return_val_if_fail(_network != NULL, -1);

	if (_network->cur_status == NUGU_NETWORK_DISCONNECTED) {
		nugu_dbg("already disconnected");
		return 0;
	}

	nugu_info("disconnecting: current step is %d", _network->step);

	_network->serverinfo = NULL;

	if (_network->server) {
		dg_server_free(_network->server);
		_network->server = NULL;
	}

	if (_network->handoff) {
		dg_server_free(_network->handoff);
		_network->handoff = NULL;
	}

	if (_network->registry) {
		dg_registry_free(_network->registry);
		_network->registry = NULL;
	}

	_update_status(_network, NUGU_NETWORK_DISCONNECTED);

	return 0;
}

EXPORT_API int
nugu_network_manager_set_status_callback(NetworkManagerStatusCallback callback,
					 void *userdata)
{
	if (!_network)
		return -1;

	_network->status_callback = callback;
	_network->status_callback_userdata = userdata;

	return 0;
}

EXPORT_API int nugu_network_manager_set_handoff_status_callback(
	NetworkManagerHandoffStatusCallback callback, void *userdata)
{
	if (!_network)
		return -1;

	_network->handoff_callback = callback;
	_network->handoff_callback_userdata = userdata;

	return 0;
}

EXPORT_API NuguNetworkStatus nugu_network_manager_get_status(void)
{
	return _network->cur_status;
}

EXPORT_API int nugu_network_manager_send_event(NuguEvent *nev)
{
	g_return_val_if_fail(nev != NULL, -1);

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
	g_return_val_if_fail(nev != NULL, -1);

	if (!_network) {
		nugu_error("network manager not initialized");
		return -1;
	}

	if (!_network->server) {
		nugu_error("not connected");
		return -1;
	}

	return dg_server_send_attachment(_network->server, nev, is_end, length,
					 data);
}

EXPORT_API int
nugu_network_manager_handoff(const NuguNetworkServerPolicy *policy)
{
	g_return_val_if_fail(policy != NULL, -1);

	if (!_network) {
		nugu_error("network manager not initialized");
		return -1;
	}

	if (_network->handoff)
		dg_server_free(_network->handoff);

	_network->handoff = dg_server_new(policy);
	if (!_network->handoff) {
		nugu_error("dg_server_new() failed.");
		return -1;
	}

	dg_server_set_type(_network->handoff, DG_SERVER_TYPE_HANDOFF);

	_log_server_info(_network, _network->handoff);

	if (dg_server_connect_async(_network->handoff) < 0) {
		dg_server_free(_network->handoff);
		_network->handoff = NULL;
		nugu_error("Server is unavailable.");
		return -1;
	}

	_update_step(_network, STEP_SERVER_HANDOFF);

	return 0;
}

int nugu_network_manager_set_token(const char *token)
{
	if (!_network) {
		nugu_error("network manager not initialized");
		return -1;
	}

	if (!token) {
		nugu_error("token is NULL");
		return -1;
	}

	if (_network->token)
		free(_network->token);

	_network->token = strdup(token);

	return 0;
}

const char *nugu_network_manager_peek_token(void)
{
	if (!_network) {
		nugu_error("network manager not initialized");
		return NULL;
	}

	return _network->token;
}
