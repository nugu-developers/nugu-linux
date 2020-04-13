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
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include "curl/curl.h"

#include "nugu.h"

#include "base/nugu_log.h"
#include "base/nugu_uuid.h"
#include "base/nugu_equeue.h"
#include "base/nugu_directive_sequencer.h"
#include "base/nugu_network_manager.h"
#include "base/nugu_prof.h"

#include "network/dg_registry.h"
#include "network/dg_server.h"
#include "network/dg_types.h"

enum connection_step {
	STEP_IDLE, /**< Idle */
	STEP_INVALID_TOKEN,
	STEP_DISCONNECTING,
	STEP_REGISTRY_FAILED,
	STEP_REGISTRY_DONE,
	STEP_SERVER_CONNECTING,
	STEP_SERVER_FAILED,
	STEP_SERVER_CONNECTED,
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
	[STEP_SERVER_CONNECTED] = "STEP_SERVER_CONNECTED",
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
	char *useragent;

	/* Registry */
	char *registry_url;
	DGRegistry *registry;
	struct dg_health_check_policy policy;
	GList *server_list;
	const GList *serverinfo;

	/* Server */
	DGServer *server;
	struct timespec ts_connected;

	/* Handoff */
	DGServer *handoff;
	NuguNetworkManagerHandoffStatusCallback handoff_callback;
	void *handoff_callback_userdata;

	/* Event send notify */
	NuguNetworkManagerEventSendNotifyCallback event_send_callback;
	void *event_send_callback_userdata;

	/* Event result */
	NuguNetworkManagerEventResultCallback event_result_callback;
	void *event_result_callback_userdata;

	/* Status & Callback */
	NuguNetworkStatus cur_status;
	NuguNetworkManagerStatusCallback status_callback;
	void *status_callback_userdata;
};

typedef struct _nugu_network NetworkManager;

static void on_directive(enum nugu_equeue_type type, void *data, void *userdata)
{
	nugu_directive_ref(data);
	nugu_dirseq_push(data);
}

static void on_destroy_directive(void *data)
{
	nugu_directive_unref(data);
}

static void on_attachment(enum nugu_equeue_type type, void *data,
			  void *userdata)
{
	NuguDirective *ndir;
	struct equeue_data_attachment *item = data;

	ndir = nugu_dirseq_find_by_msgid(item->parent_msg_id);
	if (!ndir)
		return;

	if (nugu_directive_get_data_size(ndir) == 0)
		nugu_prof_mark_data(NUGU_PROF_TYPE_TTS_FIRST_ATTACHMENT,
				    nugu_directive_peek_dialog_id(ndir),
				    nugu_directive_peek_msg_id(ndir), NULL);

	if (item->is_end) {
		nugu_prof_mark_data(NUGU_PROF_TYPE_TTS_LAST_ATTACHMENT,
				    nugu_directive_peek_dialog_id(ndir),
				    nugu_directive_peek_msg_id(ndir), NULL);

		nugu_directive_close_data(ndir);
	}

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

static void on_destroy_event_send_result(void *data)
{
	struct equeue_data_request_result *item = data;

	if (item->msg_id)
		free(item->msg_id);

	if (item->dialog_id)
		free(item->dialog_id);

	free(item);
}

static void on_event_send_result(enum nugu_equeue_type type, void *data,
				 void *userdata)
{
	struct equeue_data_request_result *item = data;
	NetworkManager *nm = userdata;

	if (item->success == 0)
		nugu_error("event send failed: msg_id=%s, code=%d",
			   item->msg_id, item->code);

	if (nm->event_result_callback == NULL)
		return;

	nm->event_result_callback(item->success, item->msg_id, item->dialog_id,
				  item->code,
				  nm->event_result_callback_userdata);
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

	if (nm->status_callback == NULL)
		return;

	nm->status_callback(nm->cur_status, nm->status_callback_userdata);
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

	nugu_prof_mark(NUGU_PROF_TYPE_NETWORK_REGISTRY_REQUEST);

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
		nugu_prof_mark(NUGU_PROF_TYPE_NETWORK_SERVER_ESTABLISH_REQUEST);
		if (nm->handoff)
			_try_connect_to_handoff(nm);
		else {
			_update_status(nm, NUGU_NETWORK_CONNECTING);
			_try_connect_to_servers(nm);
		}
		break;

	case STEP_SERVER_CONNECTED:
		if (nm->handoff) {
			nugu_info("connected to handoff server");
			if (nm->handoff_callback)
				nm->handoff_callback(
					NUGU_NETWORK_HANDOFF_READY,
					nm->handoff_callback_userdata);

			nugu_info("remove current server");
			dg_server_free(nm->server);

			nugu_info("change to handoff server");
			nm->serverinfo = NULL;
			nm->server = nm->handoff;
			nm->handoff = NULL;

			if (nm->handoff_callback)
				nm->handoff_callback(
					NUGU_NETWORK_HANDOFF_COMPLETED,
					nm->handoff_callback_userdata);
		}
		dg_server_start_health_check(nm->server, &(nm->policy));
		dg_server_reset_retry_count(nm->server);
		_update_status(nm, NUGU_NETWORK_CONNECTED);
		break;

	case STEP_SERVER_FAILED:
		nugu_prof_mark(NUGU_PROF_TYPE_NETWORK_SERVER_ESTABLISH_FAILED);
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

	nugu_prof_mark(NUGU_PROF_TYPE_NETWORK_REGISTRY_RESPONSE);

	if (nm->server_list)
		g_list_free_full(nm->server_list, free);

	nm->server_list = data;
	nm->serverinfo = NULL;

	_process_connecting(userdata, STEP_REGISTRY_DONE);
}

static void on_directives_closed(enum nugu_equeue_type type, void *data,
				 void *userdata)
{
	NetworkManager *nm = userdata;
	struct timespec spec;

	nugu_prof_mark(NUGU_PROF_TYPE_NETWORK_DIRECTIVES_CLOSED);

	if (nm->server == NULL) {
		nugu_info("no connected servers");
		return;
	}

	clock_gettime(CLOCK_REALTIME, &spec);

	if (spec.tv_sec - nm->ts_connected.tv_sec == 0) {
		nugu_error(
			"Connection was finished immediately on the server.");

		/* forgot current server */
		nugu_dbg("forgot current server");
		dg_server_free(nm->server);
		nm->server = NULL;

		_process_connecting(nm, STEP_SERVER_CONNECTING);
		return;
	}

	nugu_info("re-establish directives connection");
	dg_server_connect_async(nm->server);
}

static void on_event(enum nugu_equeue_type type, void *data, void *userdata)
{
	NetworkManager *nm = userdata;

	switch (type) {
	case NUGU_EQUEUE_TYPE_INVALID_TOKEN:
		nugu_dbg("received invalid token event");
		nugu_prof_mark(NUGU_PROF_TYPE_NETWORK_INVALID_TOKEN);
		_process_connecting(userdata, STEP_INVALID_TOKEN);
		break;
	case NUGU_EQUEUE_TYPE_REGISTRY_FAILED:
		nugu_dbg("received registry failed event");
		nugu_prof_mark(NUGU_PROF_TYPE_NETWORK_REGISTRY_FAILED);
		_process_connecting(userdata, STEP_REGISTRY_FAILED);
		break;
	case NUGU_EQUEUE_TYPE_SEND_PING_FAILED:
		nugu_dbg("received send ping failed event");
		nugu_prof_mark(NUGU_PROF_TYPE_NETWORK_PING_FAILED);
		dg_server_stop_health_check(nm->server);
		_process_connecting(userdata, STEP_SERVER_FAILED);
		break;
	case NUGU_EQUEUE_TYPE_SERVER_DISCONNECTED:
		nugu_dbg("received server disconnected event");
		_process_connecting(userdata, STEP_SERVER_FAILED);
		break;
	case NUGU_EQUEUE_TYPE_SERVER_CONNECTED:
		nugu_dbg("received server connected event");
		nugu_prof_mark(
			NUGU_PROF_TYPE_NETWORK_SERVER_ESTABLISH_RESPONSE);
		nugu_prof_mark(NUGU_PROF_TYPE_NETWORK_CONNECTED);
		clock_gettime(CLOCK_REALTIME, &nm->ts_connected);
		_process_connecting(userdata, STEP_SERVER_CONNECTED);
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
		nugu_error_nomem();
		return NULL;
	}

	/* Received message from server */
	nugu_equeue_set_handler(NUGU_EQUEUE_TYPE_NEW_DIRECTIVE, on_directive,
				on_destroy_directive, nm);
	nugu_equeue_set_handler(NUGU_EQUEUE_TYPE_NEW_ATTACHMENT, on_attachment,
				on_destroy_attachment, nm);

	/* Result of sending event request */
	nugu_equeue_set_handler(NUGU_EQUEUE_TYPE_SEND_EVENT_RESULT,
				on_event_send_result,
				on_destroy_event_send_result, nm);

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

	/* Long polling stream closed by server */
	nugu_equeue_set_handler(NUGU_EQUEUE_TYPE_DIRECTIVES_CLOSED,
				on_directives_closed, NULL, nm);

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

	if (nm->registry_url)
		free(nm->registry_url);

	if (nm->useragent)
		free(nm->useragent);

	memset(nm, 0, sizeof(NetworkManager));
	free(nm);
}

static NetworkManager *_network;

EXPORT_API int nugu_network_manager_initialize(void)
{
	struct timespec spec;
	curl_version_info_data *cinfo;

	if (_network) {
		nugu_dbg("already initialized");
		return 0;
	}

	curl_global_init(CURL_GLOBAL_DEFAULT);

	cinfo = curl_version_info(CURLVERSION_NOW);
	if (cinfo) {
		nugu_dbg("curl %s (%s), nghttp2_version=%s, ssl_version=%s",
			 cinfo->version, cinfo->host, cinfo->nghttp2_version,
			 cinfo->ssl_version);

		if (cinfo->protocols) {
			const char *const *proto;

			nugu_dbg("Supported protocols: ");
			for (proto = cinfo->protocols; *proto; ++proto)
				nugu_dbg("  <%s>", *proto);
		}
	}

	clock_gettime(CLOCK_REALTIME, &spec);
	srandom(spec.tv_nsec ^ spec.tv_sec);

	_network = nugu_network_manager_new();
	if (!_network)
		return -1;

	nugu_network_manager_set_registry_url(NUGU_REGISTRY_URL);
	nugu_network_manager_set_useragent(NUGU_USERAGENT);

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

	curl_global_cleanup();

	nugu_info("network manager de-initialized");
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

	nugu_prof_mark(NUGU_PROF_TYPE_NETWORK_CONNECT_REQUEST);

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

EXPORT_API int nugu_network_manager_set_status_callback(
	NuguNetworkManagerStatusCallback callback, void *userdata)
{
	if (!_network)
		return -1;

	_network->status_callback = callback;
	_network->status_callback_userdata = userdata;

	return 0;
}

EXPORT_API int nugu_network_manager_set_handoff_status_callback(
	NuguNetworkManagerHandoffStatusCallback callback, void *userdata)
{
	if (!_network)
		return -1;

	_network->handoff_callback = callback;
	_network->handoff_callback_userdata = userdata;

	return 0;
}

EXPORT_API int nugu_network_manager_set_event_send_notify_callback(
	NuguNetworkManagerEventSendNotifyCallback callback, void *userdata)
{
	if (!_network)
		return -1;

	_network->event_send_callback = callback;
	_network->event_send_callback_userdata = userdata;

	return 0;
}

EXPORT_API int nugu_network_manager_set_event_result_callback(
	NuguNetworkManagerEventResultCallback callback, void *userdata)
{
	if (!_network)
		return -1;

	_network->event_result_callback = callback;
	_network->event_result_callback_userdata = userdata;

	return 0;
}

EXPORT_API NuguNetworkStatus nugu_network_manager_get_status(void)
{
	return _network->cur_status;
}

EXPORT_API int nugu_network_manager_send_event(NuguEvent *nev, int is_sync)
{
	int ret;

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

	if (is_sync && nugu_event_get_type(nev) != NUGU_EVENT_TYPE_DEFAULT) {
		nugu_error("sync option supports only default type event");
		return -1;
	}

	ret = dg_server_send_event(_network->server, nev, is_sync);
	if (ret < 0) {
		nugu_error("event send failed: %d", ret);
		return -1;
	}

	if (_network->event_send_callback)
		_network->event_send_callback(
			nev, _network->event_send_callback_userdata);

	return 0;
}

EXPORT_API int nugu_network_manager_force_close_event(NuguEvent *nev)
{
	g_return_val_if_fail(nev != NULL, -1);

	if (nugu_event_get_type(nev) == NUGU_EVENT_TYPE_DEFAULT) {
		nugu_error("not supported event type");
		return -1;
	}

	if (!_network) {
		nugu_error("network manager not initialized");
		return -1;
	}

	if (!_network->server) {
		nugu_error("not connected");
		return -1;
	}

	return dg_server_force_close_event(_network->server, nev);
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

EXPORT_API int nugu_network_manager_set_token(const char *token)
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

EXPORT_API const char *nugu_network_manager_peek_token(void)
{
	if (!_network) {
		nugu_error("network manager not initialized");
		return NULL;
	}

	return _network->token;
}

EXPORT_API int nugu_network_manager_set_registry_url(const char *urlname)
{
#ifdef NUGU_ENV_NETWORK_REGISTRY_SERVER
	char *override_value;
#endif

	if (!_network) {
		nugu_error("network manager not initialized");
		return -1;
	}

	if (!urlname) {
		nugu_error("urlname is NULL");
		return -1;
	}

	if (_network->registry_url)
		free(_network->registry_url);

#ifdef NUGU_ENV_NETWORK_REGISTRY_SERVER
	override_value = getenv(NUGU_ENV_NETWORK_REGISTRY_SERVER);
	if (override_value)
		_network->registry_url = strdup(override_value);
	else
		_network->registry_url = strdup(urlname);
#else
	_network->registry_url = strdup(urlname);
#endif

	return 0;
}

EXPORT_API const char *nugu_network_manager_peek_registry_url(void)
{
	if (!_network) {
		nugu_error("network manager not initialized");
		return NULL;
	}

	return _network->registry_url;
}

EXPORT_API int nugu_network_manager_set_useragent(const char *uagent)
{
#ifdef NUGU_ENV_NETWORK_USERAGENT
	char *override_value;
#endif

	if (!_network) {
		nugu_error("network manager not initialized");
		return -1;
	}

	if (!uagent) {
		nugu_error("urlname is NULL");
		return -1;
	}

	if (_network->useragent)
		free(_network->useragent);

#ifdef NUGU_ENV_NETWORK_USERAGENT
	override_value = getenv(NUGU_ENV_NETWORK_USERAGENT);
	if (override_value)
		_network->useragent = strdup(override_value);
	else
		_network->useragent = strdup(uagent);
#else
	_network->useragent = strdup(uagent);
#endif

	return 0;
}

EXPORT_API const char *nugu_network_manager_peek_useragent(void)
{
	if (!_network) {
		nugu_error("network manager not initialized");
		return NULL;
	}

	return _network->useragent;
}
