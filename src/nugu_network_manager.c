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

#include "http2_manage.h"

struct _nugu_network {
	NuguNetworkStatus cur_status;
	NetworkManagerCallback callback;
	void *callback_userdata;

	guint idle_source;

	H2Manager *h2;

	pthread_mutex_t lock;
};

typedef struct _nugu_network NetworkManager;

static NetworkManager *_network;

EXPORT_API int nugu_network_manager_send_event(NuguEvent *nev)
{
	char *payload;
	int ret;

	if (!_network) {
		nugu_error("network manager not initialized");
		return -1;
	}

	if (nugu_event_peek_context(nev) == NULL) {
		nugu_error("context must not null");
		return -1;
	}

	payload = nugu_event_generate_payload(nev);
	if (!payload)
		return -1;

	ret = h2manager_send_event(_network->h2, payload);
	free(payload);

	return ret;
}

EXPORT_API int nugu_network_manager_send_event_data(NuguEvent *nev, int is_end,
						    size_t length,
						    unsigned char *data)
{
	int ret = -1;
	char *msg_id;

	if (!_network) {
		nugu_error("network manager not initialized");
		return -1;
	}

	msg_id = nugu_uuid_generate_short();

	ret = h2manager_send_event_attachment(
		_network->h2, nugu_event_peek_namespace(nev),
		nugu_event_peek_name(nev), nugu_event_peek_version(nev),
		nugu_event_peek_msg_id(nev), msg_id,
		nugu_event_peek_dialog_id(nev), nugu_event_get_seq(nev), is_end,
		length, data);

	free(msg_id);
	if (ret < 0)
		return ret;

	nugu_event_increase_seq(nev);

	return 0;
}

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

static NetworkManager *nugu_network_manager_new(void)
{
	NetworkManager *nm;

	nm = calloc(1, sizeof(NetworkManager));
	if (!nm) {
		error_nomem();
		return NULL;
	}

	nugu_equeue_set_handler(NUGU_EQUEUE_TYPE_NEW_DIRECTIVE, on_directive,
				NULL, nm);

	nugu_equeue_set_handler(NUGU_EQUEUE_TYPE_NEW_ATTACHMENT, on_attachment,
				on_destroy_attachment, nm);

	nm->cur_status = NUGU_NETWORK_UNKNOWN;

	return nm;
}

static void nugu_network_manager_free(NetworkManager *nm)
{
	g_return_if_fail(nm != NULL);

	if (nm->idle_source > 0)
		g_source_remove(nm->idle_source);

	memset(nm, 0, sizeof(NetworkManager));
	free(nm);
}

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

static void _emit_event(NuguNetworkStatus status)
{
	if (_network->cur_status == status) {
		nugu_dbg("ignore same status event(%d)", status);
		return;
	}

	nugu_dbg("propagate event(%d)", status);

	_network->cur_status = status;

	if (_network->callback)
		_network->callback(_network->callback_userdata);

	nugu_dbg("propagate event - done");
}

static gboolean _disconnect_network_in_idle(gpointer userdata)
{
	NuguNetworkStatus status;

	if (!_network) {
		nugu_dbg("already disconnected");
		return FALSE;
	}

	status = nugu_network_manager_get_status();

	nugu_dbg("disconnect current network at idle (status=%d)", status);
	nugu_network_manager_disconnect();

	_network->idle_source = 0;

	return FALSE;
}

static void _status_callback(void *userdata)
{
	enum h2manager_status h2_status;
	NuguNetworkStatus status;

	h2_status = h2manager_get_status(_network->h2);
	if (h2_status == H2_STATUS_CONNECTING)
		return;
	else if (h2_status == H2_STATUS_READY)
		return;

	status = nugu_network_manager_get_status();
	switch (status) {
	case NUGU_NETWORK_DISCONNECTED:
		nugu_dbg("disconnect current network (status=%d)", status);
		nugu_network_manager_disconnect();
		break;
	case NUGU_NETWORK_TOKEN_ERROR:
		nugu_dbg("request to disconnect the network at idle");
		_network->idle_source =
			g_idle_add(_disconnect_network_in_idle, NULL);
		break;
	default:
		nugu_dbg("status = %d", status);
		break;
	}

	_emit_event(status);
}

EXPORT_API int nugu_network_manager_connect(void)
{
	if (!_network)
		return -1;

	if (_network->h2) {
		nugu_dbg("already connected");
		return 0;
	}

	_network->cur_status = NUGU_NETWORK_UNKNOWN;
	_network->h2 = h2manager_new();
	if (!_network->h2)
		return -1;

	h2manager_set_status_callback(_network->h2, _status_callback, _network);

	return h2manager_connect(_network->h2);
}

EXPORT_API int nugu_network_manager_disconnect(void)
{
	int event_emit = 0;

	if (!_network)
		return -1;

	if (!_network->h2) {
		nugu_dbg("already disconnected");
		return 0;
	}

	if (_network->cur_status != NUGU_NETWORK_DISCONNECTED) {
		nugu_dbg("current network status = %d", _network->cur_status);
		_network->cur_status = NUGU_NETWORK_DISCONNECTED;
		event_emit = 1;
	}

	h2manager_set_status_callback(_network->h2, NULL, NULL);
	h2manager_disconnect(_network->h2);
	h2manager_free(_network->h2);
	_network->h2 = NULL;

	if (event_emit && _network->callback) {
		nugu_dbg("propagate event");
		_network->callback(_network->callback_userdata);
		nugu_dbg("propagate event - done");
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
	enum h2manager_status status;

	if (!_network)
		return NUGU_NETWORK_DISCONNECTED;

	if (!_network->h2)
		return NUGU_NETWORK_DISCONNECTED;

	status = h2manager_get_status(_network->h2);

	switch (status) {
	case H2_STATUS_ERROR:
		return NUGU_NETWORK_DISCONNECTED;
	case H2_STATUS_READY:
		return NUGU_NETWORK_DISCONNECTED;
	case H2_STATUS_CONNECTING:
		return NUGU_NETWORK_DISCONNECTED;
	case H2_STATUS_DISCONNECTED:
		return NUGU_NETWORK_DISCONNECTED;
	case H2_STATUS_CONNECTED:
		return NUGU_NETWORK_CONNECTED;
	case H2_STATUS_TOKEN_FAILED:
		return NUGU_NETWORK_TOKEN_ERROR;
	default:
		break;
	}

	return NUGU_NETWORK_DISCONNECTED;
}
