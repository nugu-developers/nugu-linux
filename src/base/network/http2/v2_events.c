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
#include "base/nugu_uuid.h"
#include "base/nugu_prof.h"
#include "base/nugu_event.h"

#include "dg_types.h"

#include "directives_parser.h"
#include "threadsync.h"
#include "http2_request.h"
#include "v2_events.h"

#define CRLF "\r\n"
#define HYPHEN "--"

#define U_CRLF ((unsigned char *)CRLF)
#define U_HYPHEN ((unsigned char *)HYPHEN)

#define PART_HEADER_JSON                                                       \
	"Content-Disposition: form-data; name=\"event\"" CRLF                  \
	"Content-Type: application/json" CRLF CRLF

#define PART_HEADER_BINARY_FMT                                                 \
	"Content-Disposition: form-data; name=\"attachment\";"                 \
	" filename=\"%d;%s\"" CRLF                                             \
	"Content-Type: application/octet-stream" CRLF                          \
	"Content-Length: %zd" CRLF "Message-Id: %s" CRLF CRLF

struct _v2_events {
	HTTP2Request *req;
	unsigned char *boundary;
	size_t b_len;
	int sync;
	gboolean first_data;
	HTTP2Network *net;
	enum nugu_event_type type;
};

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
			free(event->dialog_id);
		if (event->msg_id)
			free(event->msg_id);
		free(event);
	}
}

static void _emit_directive_response(HTTP2Request *req, const char *json)
{
	struct equeue_data_event_response *event;

	event = calloc(1, sizeof(struct equeue_data_event_response));
	if (!event) {
		nugu_error_nomem();
		return;
	}

	if (http2_request_get_result(req) == HTTP2_RESULT_OK)
		event->success = 1;
	else
		event->success = 0;

	if (http2_request_peek_msgid(req))
		event->event_msg_id = g_strdup(http2_request_peek_msgid(req));

	if (http2_request_peek_dialogid(req))
		event->event_dialog_id =
			g_strdup(http2_request_peek_dialogid(req));

	if (json)
		event->json = g_strdup(json);

	if (nugu_equeue_push(NUGU_EQUEUE_TYPE_EVENT_RESPONSE, event) < 0) {
		nugu_error("nugu_equeue_push failed");

		if (event->event_dialog_id)
			free(event->event_dialog_id);
		if (event->event_msg_id)
			free(event->event_msg_id);
		if (event->json)
			free(event->json);
		free(event);
	}
}

/* invoked in a thread loop */
static void _on_code(HTTP2Request *req, int code, void *userdata)
{
	if (code != HTTP2_RESPONSE_OK) {
		nugu_error("code = %d", code);
		http2_request_set_header_callback(req, NULL, NULL);
		http2_request_set_body_callback(req, NULL, NULL);

		nugu_prof_mark_data(NUGU_PROF_TYPE_NETWORK_EVENT_FAILED,
				    http2_request_peek_dialogid(req),
				    http2_request_peek_msgid(req),
				    http2_request_peek_profiling_contents(req));

		if (code == HTTP2_RESPONSE_AUTHFAIL ||
		    code == HTTP2_RESPONSE_FORBIDDEN)
			nugu_equeue_push(NUGU_EQUEUE_TYPE_INVALID_TOKEN, NULL);

		return;
	}

	nugu_prof_mark_data(NUGU_PROF_TYPE_NETWORK_EVENT_RESPONSE,
			    http2_request_peek_dialogid(req),
			    http2_request_peek_msgid(req),
			    http2_request_peek_profiling_contents(req));

	_emit_send_result(code, req);
}

/* invoked in a thread loop */
static size_t _on_header(HTTP2Request *req, char *buffer, size_t size,
			 size_t nitems, void *userdata)
{
	int buffer_len = size * nitems;

	if (dir_parser_add_header(userdata, buffer, size * nitems) < 0)
		return buffer_len;

	return buffer_len;
}

/* invoked in a thread loop */
static size_t _on_body(HTTP2Request *req, char *buffer, size_t size,
		       size_t nitems, void *userdata)
{
	dir_parser_parse(userdata, buffer, size * nitems);

	return size * nitems;
}

/* invoked in a thread loop */
static void _on_finish(HTTP2Request *req, void *userdata)
{
	if (userdata)
		thread_sync_signal(userdata);

	if (http2_request_get_result(req) == HTTP2_RESULT_OK)
		return;

	if (http2_request_get_result(req) == HTTP2_RESULT_TIMEOUT) {
		nugu_error("receive timeout error");
		nugu_prof_mark_data(
			NUGU_PROF_TYPE_NETWORK_EVENT_DIRECTIVE_TIMEOUT,
			http2_request_peek_dialogid(req),
			http2_request_peek_msgid(req), NULL);
	}

	_emit_directive_response(req, NULL);
}

/* invoked in a thread loop */
static void _on_destroy(HTTP2Request *req, void *userdata)
{
	NuguBuffer *buf;

	if (!userdata)
		return;

	buf = dir_parser_get_json_buffer(userdata);
	if (buf) {
		dir_parser_set_json_buffer(userdata, NULL);

		nugu_buffer_free(buf, TRUE);
	}

	dir_parser_free(userdata);
}

/* invoked in a thread loop */
static void _directive_cb(DirParser *dp, NuguDirective *ndir, void *userdata)
{
	nugu_prof_mark_data(NUGU_PROF_TYPE_NETWORK_EVENT_DIRECTIVE_RESPONSE,
			    nugu_directive_peek_dialog_id(ndir),
			    nugu_directive_peek_msg_id(ndir), NULL);
}

static void _end_cb(DirParser *dp, void *userdata)
{
	NuguBuffer *buf;
	char *json;

	buf = dir_parser_get_json_buffer(dp);
	if (!buf)
		return;

	json = nugu_buffer_pop(buf, 0);
	if (json) {
		_emit_directive_response(userdata, json);
		free(json);
	}
}

V2Events *v2_events_new(const char *host, HTTP2Network *net, int is_sync,
			enum nugu_event_type type)
{
	char *tmp;
	struct _v2_events *event;
	int ret;
	unsigned char buf[8];
	char boundary[34] = "nugusdk.boundary.";
	DirParser *parser;
	NuguBuffer *buffer;

	g_return_val_if_fail(host != NULL, NULL);
	g_return_val_if_fail(net != NULL, NULL);

	event = calloc(1, sizeof(struct _v2_events));
	if (!event) {
		nugu_error_nomem();
		return NULL;
	}

	event->req = http2_request_new();
	if (!event->req) {
		nugu_error_nomem();
		v2_events_free(event);
		return NULL;
	}

	parser = dir_parser_new(DIR_PARSER_TYPE_EVENT_RESPONSE);
	if (!parser) {
		nugu_error_nomem();
		v2_events_free(event);
		return NULL;
	}

	tmp = g_strdup_printf(" (%p)", event->req);
	dir_parser_set_debug_message(parser, tmp);
	g_free(tmp);

	buffer = nugu_buffer_new(0);
	if (buffer)
		dir_parser_set_json_buffer(parser, buffer);

	dir_parser_set_directive_callback(parser, _directive_cb, event->req);
	dir_parser_set_end_callback(parser, _end_cb, event->req);

	nugu_uuid_fill_random(buf, sizeof(buf));
	nugu_uuid_convert_base16(buf, sizeof(buf), boundary + strlen(boundary),
				 sizeof(boundary));
	boundary[33] = '\0';

	event->boundary = (unsigned char *)g_strdup_printf("--%s", boundary);
	event->b_len = strlen((char *)event->boundary);
	event->sync = is_sync;
	event->net = net;
	event->type = type;
	event->first_data = TRUE;

	http2_request_set_method(event->req, HTTP2_REQUEST_METHOD_POST);
	http2_request_set_content_type(
		event->req, HTTP2_REQUEST_CONTENT_TYPE_MULTIPART, boundary);

	/* Set maximum timeout to 10 secs or 5 minutes */
	if (event->type == NUGU_EVENT_TYPE_DEFAULT)
		http2_request_set_timeout(event->req, 10);
	else if (event->type == NUGU_EVENT_TYPE_WITH_ATTACHMENT)
		http2_request_set_timeout(event->req, 5 * 60);

	tmp = g_strdup_printf("%s/v2/events", host);
	http2_request_set_url(event->req, tmp);
	g_free(tmp);

	http2_request_set_code_callback(event->req, _on_code, NULL);
	http2_request_set_header_callback(event->req, _on_header, parser);
	http2_request_set_body_callback(event->req, _on_body, parser);
	http2_request_set_finish_callback(event->req, _on_finish, NULL);
	http2_request_set_destroy_callback(event->req, _on_destroy, parser);

	ret = http2_network_add_request(event->net, event->req);
	if (ret < 0) {
		nugu_error("http2_network_add_request() failed: %d", ret);
		http2_request_unref(event->req);
		event->req = NULL;
		v2_events_free(event);
		return NULL;
	}

	return event;
}

void v2_events_free(V2Events *event)
{
	g_return_if_fail(event != NULL);

	if (event->boundary)
		free(event->boundary);

	if (event->req)
		http2_request_unref(event->req);

	memset(event, 0, sizeof(V2Events));
	free(event);
}

int v2_events_set_info(V2Events *event, const char *msg_id,
		       const char *dialog_id)
{
	g_return_val_if_fail(event != NULL, -1);

	http2_request_set_msgid(event->req, msg_id);
	http2_request_set_dialogid(event->req, dialog_id);

	return 0;
}

int v2_events_send_json(V2Events *event, const char *data, size_t length)
{
	g_return_val_if_fail(event != NULL, -1);

	http2_request_lock_send_data(event->req);

	http2_request_set_profiling_contents(event->req, data);

	/* Boundary for 1st multipart data */
	if (event->first_data) {
		http2_request_add_send_data(event->req, event->boundary,
					    event->b_len);
		http2_request_add_send_data(event->req, U_CRLF, 2);
		event->first_data = FALSE;
	}

	/* Body header */
	http2_request_add_send_data(event->req,
				    (unsigned char *)PART_HEADER_JSON,
				    strlen(PART_HEADER_JSON));

	/* Body */
	http2_request_add_send_data(event->req, (unsigned char *)data, length);
	http2_request_add_send_data(event->req, U_CRLF, 2);

	/* Boundary */
	http2_request_add_send_data(event->req, event->boundary, event->b_len);

	if (event->type == NUGU_EVENT_TYPE_DEFAULT)
		http2_request_add_send_data(event->req, U_HYPHEN, 2);

	http2_request_add_send_data(event->req, U_CRLF, 2);

	if (event->type == NUGU_EVENT_TYPE_DEFAULT)
		http2_request_close_send_data(event->req);

	http2_request_unlock_send_data(event->req);
	http2_network_resume_request(event->net, event->req);

	return 0;
}

int v2_events_send_binary(V2Events *event, const char *msgid, int seq,
			  int is_end, size_t length, unsigned char *data)
{
	char *part_header;

	g_return_val_if_fail(event != NULL, -1);

	part_header = g_strdup_printf(PART_HEADER_BINARY_FMT, seq,
				      (is_end == 1) ? "end" : "continued",
				      length, msgid);

	http2_request_lock_send_data(event->req);

	/* Boundary for 1st multipart data */
	if (event->first_data) {
		http2_request_add_send_data(event->req, event->boundary,
					    event->b_len);
		http2_request_add_send_data(event->req, U_CRLF, 2);
		event->first_data = FALSE;
	}

	/* Body header */
	http2_request_add_send_data(event->req, (unsigned char *)part_header,
				    strlen(part_header));
	g_free(part_header);

	/* Body */
	if (data != NULL && length > 0)
		http2_request_add_send_data(event->req, data, length);

	http2_request_add_send_data(event->req, U_CRLF, 2);

	/* Boundary */
	http2_request_add_send_data(event->req, event->boundary, event->b_len);

	if (is_end == 0)
		http2_request_add_send_data(event->req, U_CRLF, 2);
	else {
		/* End boundary */
		http2_request_add_send_data(event->req, U_HYPHEN, 2);
		http2_request_add_send_data(event->req, U_CRLF, 2);
		http2_request_close_send_data(event->req);
	}

	http2_request_unlock_send_data(event->req);
	http2_network_resume_request(event->net, event->req);

	return 0;
}

/* invoked in a thread loop */
static void _on_send_done(HTTP2Request *req, void *userdata)
{
	nugu_dbg("send all data (%p)", req);
}

int v2_events_send_done(V2Events *event)
{
	ThreadSync *sync = NULL;

	g_return_val_if_fail(event != NULL, -1);

	if (event->sync)
		sync = thread_sync_new();

	http2_request_set_finish_callback(event->req, _on_finish, sync);
	http2_request_set_send_complete_callback(event->req, _on_send_done,
						 event->net);

	http2_request_lock_send_data(event->req);

	/* End boundary */
	http2_request_add_send_data(event->req, event->boundary, event->b_len);
	http2_request_add_send_data(event->req, U_HYPHEN, 2);
	http2_request_add_send_data(event->req, U_CRLF, 2);

	http2_request_close_send_data(event->req);
	http2_request_unlock_send_data(event->req);
	http2_network_resume_request(event->net, event->req);

	if (sync) {
		thread_sync_wait_secs(sync, 5);
		thread_sync_free(sync);
	}

	return 0;
}
