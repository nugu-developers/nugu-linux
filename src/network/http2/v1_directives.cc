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

#include "nugu_equeue.h"
#include "nugu_log.h"
#include "nugu_network_manager.h"
#include "json/json.h"

#include "dg_types.h"
#include "http2_request.h"
#include "multipart_parser.h"
#include "v1_directives.h"

#define FILTER_CONTENT_TYPE "content-type: multipart/related; boundary="
#define FILTER_BODY_CONTENT_TYPE "Content-Type: "
#define FILTER_JSON_TYPE "application/json"
#define FILTER_OPUS_TYPE "audio/opus"

enum content_type {
    CONTENT_TYPE_UNKNOWN,
    CONTENT_TYPE_JSON,
    CONTENT_TYPE_OPUS
};

enum kvstatus {
    KEY,
    VALUE_WAIT,
    VALUE
};

struct _v1_directives {
    char* url;

    MultipartParser* parser;
    HTTP2Network* network;

    enum content_type ctype;
    char* parent_msg_id;
    int is_end;
    size_t body_size;
    guint idle_src;
    int is_sync;
};

static void on_parsing_header(MultipartParser* parser, const char* data, size_t length, void* userdata)
{
    V1Directives* dir = (V1Directives*)userdata;

    const char* pos;
    const char* start = data;
    const char* end = data + length;
    int status = KEY;
    std::string key;
    std::string value;
    bool found = false;

    nugu_dbg("body header: %s", data);

    for (pos = data; pos < end; pos++) {
        switch (status) {
        case KEY:
            if (*pos == ':') {
                key.assign(start, pos - start);
                status = VALUE_WAIT;
            }
            break;
        case VALUE_WAIT:
            if (*pos == ' ')
                break;
            else {
                start = pos;
                status = VALUE;
            }
            break;
        case VALUE:
            if (*pos == '\r') {
                value.assign(start, pos - start);
            } else if (*pos == '\n') {
                status = KEY;
                start = pos + 1;
                found = true;
            }
            break;
        }

        if (found) {
            if (key.compare("Content-Type") == 0) {
                if (value.compare(0, strlen(FILTER_JSON_TYPE), FILTER_JSON_TYPE) == 0) {
                    dir->ctype = CONTENT_TYPE_JSON;
                } else if (value.compare(0, strlen(FILTER_OPUS_TYPE), FILTER_OPUS_TYPE) == 0) {
                    dir->ctype = CONTENT_TYPE_OPUS;
                } else {
                    dir->ctype = CONTENT_TYPE_UNKNOWN;
                    nugu_error("unknown");
                }
            } else if (key.compare("Content-Length") == 0) {
                sscanf(value.c_str(), "%zu", &dir->body_size);
            } else if (key.compare("Parent-Message-Id") == 0) {
                if (dir->parent_msg_id)
                    free(dir->parent_msg_id);
                dir->parent_msg_id = strdup(value.c_str());
            } else if (key.compare("Filename") == 0) {
                if (value.find("end") != std::string::npos)
                    dir->is_end = 1;
                else
                    dir->is_end = 0;
            }
            found = false;
            key.clear();
            value.clear();
        }
    }
}

static void _body_json(const char* data, size_t length)
{
    Json::Value root;
    Json::Value dir_list;
    Json::Reader reader;
    Json::StyledWriter writer;
    std::string dump;
    std::string group;
    char group_buf[32];

    if (!reader.parse(data, root)) {
        nugu_error("parsing error: '%s'", data);
        return;
    }

    dump = writer.write(root);

    dir_list = root["directives"];

    group = "{ \"directives\": [";
    for (Json::ArrayIndex i = 0; i < dir_list.size(); ++i) {
        Json::Value dir = dir_list[i];
        Json::Value h;

        h = dir["header"];

        if (i > 0)
            group.append(",");

        snprintf(group_buf, sizeof(group_buf), "\"%s.%s\"", h["namespace"].asCString(), h["name"].asCString());
        group.append(group_buf);
    }
    group.append("] }");

    if ((nugu_log_get_modules() & NUGU_LOG_MODULE_NETWORK_TRACE) != 0) {
        enum nugu_log_prefix back;

        back = nugu_log_get_prefix_fields();
        nugu_log_set_prefix_fields((nugu_log_prefix)(back & ~(NUGU_LOG_PREFIX_FILENAME | NUGU_LOG_PREFIX_LINE)));

        nugu_info("<-- Directives: group=%s\n%s", group.c_str(), dump.c_str());

        nugu_log_set_prefix_fields(back);
    }

    for (Json::ArrayIndex i = 0; i < dir_list.size(); ++i) {
        Json::Value dir = dir_list[i];
        Json::Value h;
        NuguDirective* ndir;
        std::string referrer;
        std::string p;

        h = dir["header"];

        p = writer.write(dir["payload"]);

        if (!h["referrerDialogRequestId"].empty())
            referrer = h["referrerDialogRequestId"].asString();

        ndir = nugu_directive_new(h["namespace"].asCString(),
            h["name"].asCString(), h["version"].asCString(),
            h["messageId"].asCString(), h["dialogRequestId"].asCString(),
            referrer.c_str(), p.c_str());
        if (!ndir)
            continue;

        nugu_equeue_push(NUGU_EQUEUE_TYPE_NEW_DIRECTIVE, ndir);
    }
}

static void _body_opus(const char* parent_msg_id, int is_end, const char* data, size_t length)
{
    struct equeue_data_attachment* item;

    item = (struct equeue_data_attachment*)malloc(sizeof(struct equeue_data_attachment));
    if (!item) {
        error_nomem();
        return;
    }

    item->parent_msg_id = strdup(parent_msg_id);
    item->media_type = strdup("audio/opus");
    item->is_end = is_end;
    item->length = length;
    item->data = (unsigned char*)malloc(length);
    memcpy(item->data, data, length);

    if ((nugu_log_get_modules() & NUGU_LOG_MODULE_NETWORK_TRACE) != 0) {
        enum nugu_log_prefix back;

        back = nugu_log_get_prefix_fields();
        nugu_log_set_prefix_fields((nugu_log_prefix)(back & ~(NUGU_LOG_PREFIX_FILENAME | NUGU_LOG_PREFIX_LINE)));

        nugu_info("<-- Attachment: parent=%s (%zd bytes, is_end=%d)", parent_msg_id, length, is_end);

        nugu_log_set_prefix_fields(back);
    }

    nugu_equeue_push(NUGU_EQUEUE_TYPE_NEW_ATTACHMENT, item);
}

static void _on_parsing_body(MultipartParser* parser, const char* data, size_t length, void* userdata)
{
    V1Directives* dir = (V1Directives*)userdata;

    if (dir->ctype == CONTENT_TYPE_JSON)
        _body_json(data, length);
    else if (dir->ctype == CONTENT_TYPE_OPUS)
        _body_opus(dir->parent_msg_id, dir->is_end, data, length);
}

/* invoked in a thread loop */
static size_t _on_body(HTTP2Request* req, char* buffer, size_t size, size_t nitems, void* userdata)
{
    V1Directives* dir = (V1Directives*)userdata;

    multipart_parser_parse(dir->parser, buffer, size * nitems, on_parsing_header, _on_parsing_body, userdata);

    return size * nitems;
}

/* invoked in a thread loop */
static size_t _on_header(HTTP2Request* req, char* buffer, size_t size, size_t nitems, void* userdata)
{
    V1Directives* dir = (V1Directives*)userdata;
    char* pos;
    int len;
    int buffer_len = size * nitems;
    int code;

    code = http2_request_get_response_code(req);
    if (code != HTTP2_RESPONSE_OK) {
        nugu_error("code = %d", code);
        http2_request_set_header_callback(req, NULL, NULL);
        http2_request_set_body_callback(req, NULL, NULL);
        http2_request_set_finish_callback(req, NULL, NULL);

        /* don't propagate the status event on sync call */
        if (dir->is_sync == 1)
            return buffer_len;

        if (code == HTTP2_RESPONSE_AUTHFAIL || code == HTTP2_RESPONSE_FORBIDDEN)
            nugu_equeue_push(NUGU_EQUEUE_TYPE_INVALID_TOKEN, NULL);
        else
            nugu_equeue_push(NUGU_EQUEUE_TYPE_SERVER_DISCONNECTED, NULL);

        return buffer_len;
    }

    if (strncmp(buffer, FILTER_CONTENT_TYPE, strlen(FILTER_CONTENT_TYPE)) != 0)
        return buffer_len;

    pos = strchr(buffer, '=');
    if (!pos)
        return buffer_len;

    pos++;
    len = (buffer_len - 2) - (pos - buffer);

    multipart_parser_set_boundary(dir->parser, pos, len);

    nugu_equeue_push(NUGU_EQUEUE_TYPE_SERVER_CONNECTED, NULL);

    return buffer_len;
}

static gboolean _re_establish(gpointer userdata)
{
    V1Directives* dir = (V1Directives*)userdata;

    nugu_dbg("re-establish");

    dir->idle_src = 0;

    /* Re-establish long-polling */
    v1_directives_establish(dir, dir->network);

    return FALSE;
}

/* invoked in a thread loop */
static void _on_finish(HTTP2Request* req, void* userdata)
{
    V1Directives* dir = (V1Directives*)userdata;
    int code;

    code = http2_request_get_response_code(req);
    if (code == HTTP2_RESPONSE_OK) {
        nugu_info("directive stream finished by server.");
        dir->is_sync = 0;
        dir->idle_src = g_idle_add(_re_establish, dir);
        return;
    }

    nugu_error("directive response code = %d", code);

    /* release cond_wait */
    if (dir->is_sync)
        http2_request_emit_response(req, HTTP2_REQUEST_SYNC_ITEM_HEADER);
    else {
        if (code == HTTP2_RESPONSE_AUTHFAIL || code == HTTP2_RESPONSE_FORBIDDEN)
            nugu_equeue_push(NUGU_EQUEUE_TYPE_INVALID_TOKEN, NULL);
        else
            nugu_equeue_push(NUGU_EQUEUE_TYPE_SERVER_DISCONNECTED, NULL);
    }

    return;
}

V1Directives* v1_directives_new(const char* host)
{
    struct _v1_directives* dir;

    g_return_val_if_fail(host != NULL, NULL);

    dir = (V1Directives*)calloc(1, sizeof(struct _v1_directives));
    if (!dir) {
        error_nomem();
        return NULL;
    }

    dir->url = g_strdup_printf("%s/v1/directives", host);

    return dir;
}

void v1_directives_free(V1Directives* dir)
{
    g_return_if_fail(dir != NULL);

    if (dir->parent_msg_id)
        free(dir->parent_msg_id);

    if (dir->idle_src)
        g_source_remove(dir->idle_src);

    if (dir->url)
        g_free(dir->url);

    if (dir->parser)
        multipart_parser_free(dir->parser);

    memset(dir, 0, sizeof(V1Directives));
    free(dir);
}

int v1_directives_establish(V1Directives* dir, HTTP2Network* net)
{
    HTTP2Request* req;
    int ret;
    int code;

    g_return_val_if_fail(dir != NULL, -1);
    g_return_val_if_fail(net != NULL, -1);

    if (dir->parser)
        multipart_parser_free(dir->parser);

    dir->network = net;
    dir->parser = multipart_parser_new();
    dir->ctype = CONTENT_TYPE_UNKNOWN;

    req = http2_request_new();
    http2_request_set_url(req, dir->url);
    http2_request_set_method(req, HTTP2_REQUEST_METHOD_GET);
    http2_request_set_header_callback(req, _on_header, dir);
    http2_request_set_body_callback(req, _on_body, dir);
    http2_request_set_finish_callback(req, _on_finish, dir);
    http2_request_set_connection_timeout(req, 0);
    http2_request_enable_curl_log(req);

    ret = http2_network_add_request(net, req);
    if (ret < 0)
        return ret;

    if (dir->is_sync == 0) {
        http2_request_unref(req);
        return ret;
    }

    /* blocking request */
    nugu_info("wait for directive response (%p)", req);
    http2_request_wait_response(req, HTTP2_REQUEST_SYNC_ITEM_HEADER);

    code = http2_request_get_response_code(req);
    if (code < 0) {
        nugu_error("failed(%d)", code);
        http2_request_unref(req);
        return code;
    }

    http2_request_unref(req);

    if (code != HTTP2_RESPONSE_OK) {
        nugu_error("error response(%d)", -code);
        return -code;
    }

    return ret;
}

int v1_directives_establish_sync(V1Directives* dir, HTTP2Network* net)
{
    int ret;

    dir->is_sync = 1;

    ret = v1_directives_establish(dir, net);
    if (ret < 0) {
        dir->is_sync = 0;
        return ret;
    }

    dir->is_sync = 0;

    return 0;
}
