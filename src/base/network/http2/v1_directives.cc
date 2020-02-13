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
#include <inttypes.h>

#include <glib.h>

#include "json/json.h"

#include "base/nugu_log.h"
#include "base/nugu_equeue.h"
#include "base/nugu_network_manager.h"
#include "base/nugu_directive.h"

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

struct _parser_data {
    enum content_type ctype;
    char* parent_msg_id;
    int is_end;
    size_t body_size;
};

struct _v1_directives {
    char* url;

    HTTP2Network* network;

    int connection_timeout_secs;
};

static struct _parser_data* parser_data_new(void)
{
    struct _parser_data* pdata;

    pdata = (struct _parser_data*)calloc(1, sizeof(struct _parser_data));
    if (!pdata) {
        error_nomem();
        return NULL;
    }

    return pdata;
}

static void parser_data_free(struct _parser_data* pdata)
{
    g_return_if_fail(pdata != NULL);

    if (pdata->parent_msg_id)
        free(pdata->parent_msg_id);

    free(pdata);
}

/* invoked in a thread loop */
static void on_parsing_header(MultipartParser* parser, const char* data, size_t length, void* userdata)
{
    struct _parser_data* pdata = (struct _parser_data*)multipart_parser_get_data(parser);
    const char* pos;
    const char* start = data;
    const char* end = data + length;
    int status = KEY;
    std::string key;
    std::string value;
    bool found = false;

    nugu_log_print(NUGU_LOG_MODULE_NETWORK_TRACE, NUGU_LOG_LEVEL_INFO, NULL, NULL, -1,
        "body header: %s", data);

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
                    pdata->ctype = CONTENT_TYPE_JSON;
                } else if (value.compare(0, strlen(FILTER_OPUS_TYPE), FILTER_OPUS_TYPE) == 0) {
                    pdata->ctype = CONTENT_TYPE_OPUS;
                } else {
                    pdata->ctype = CONTENT_TYPE_UNKNOWN;
                    nugu_error("unknown Content-Type");
                }
            } else if (key.compare("Content-Length") == 0) {
                pdata->body_size = (size_t)strtoumax(value.c_str(), nullptr, 10);
            } else if (key.compare("Parent-Message-Id") == 0) {
                if (pdata->parent_msg_id)
                    free(pdata->parent_msg_id);
                pdata->parent_msg_id = strdup(value.c_str());
            } else if (key.compare("Filename") == 0) {
                if (value.find("end") != std::string::npos)
                    pdata->is_end = 1;
                else
                    pdata->is_end = 0;
            }
            found = false;
            key.clear();
            value.clear();
        }
    }
}

/* invoked in a thread loop */
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
    nugu_log_protocol_recv(NUGU_LOG_LEVEL_INFO, "Directives\n%s", dump.c_str());

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

    nugu_dbg("group=%s", group.c_str());

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

        if (nugu_equeue_push(NUGU_EQUEUE_TYPE_NEW_DIRECTIVE, ndir) < 0) {
            nugu_error("nugu_equeue_push() failed.");
            nugu_directive_unref(ndir);
        }
    }
}

/* invoked in a thread loop */
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

    nugu_log_print(NUGU_LOG_MODULE_NETWORK_TRACE, NUGU_LOG_LEVEL_INFO, NULL, NULL, -1,
        "<-- Attachment: parent=%s (%zd bytes, is_end=%d)", parent_msg_id, length, is_end);

    nugu_equeue_push(NUGU_EQUEUE_TYPE_NEW_ATTACHMENT, item);
}

/* invoked in a thread loop */
static void _on_parsing_body(MultipartParser* parser, const char* data, size_t length, void* userdata)
{
    struct _parser_data* pdata = (struct _parser_data*)multipart_parser_get_data(parser);

    if (pdata->ctype == CONTENT_TYPE_JSON)
        _body_json(data, length);
    else if (pdata->ctype == CONTENT_TYPE_OPUS)
        _body_opus(pdata->parent_msg_id, pdata->is_end, data, length);
}

/* invoked in a thread loop */
static size_t _on_body(HTTP2Request* req, char* buffer, size_t size, size_t nitems, void* userdata)
{
    multipart_parser_parse((MultipartParser*)userdata, buffer, size * nitems, on_parsing_header, _on_parsing_body, NULL);

    return size * nitems;
}

/* invoked in a thread loop */
static size_t _on_header(HTTP2Request* req, char* buffer, size_t size, size_t nitems, void* userdata)
{
    MultipartParser* parser = (MultipartParser*)userdata;

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

    multipart_parser_set_boundary(parser, pos, len);

    nugu_equeue_push(NUGU_EQUEUE_TYPE_SERVER_CONNECTED, NULL);

    return buffer_len;
}

/* invoked in a thread loop */
static void _on_finish(HTTP2Request* req, void* userdata)
{
    int code;

    code = http2_request_get_response_code(req);
    if (code == HTTP2_RESPONSE_OK) {
        nugu_info("directive stream finished by server.");
        nugu_equeue_push(NUGU_EQUEUE_TYPE_DIRECTIVES_CLOSED, NULL);
        return;
    }

    nugu_error("directive response code = %d", code);

    if (code == HTTP2_RESPONSE_AUTHFAIL || code == HTTP2_RESPONSE_FORBIDDEN)
        nugu_equeue_push(NUGU_EQUEUE_TYPE_INVALID_TOKEN, NULL);
    else
        nugu_equeue_push(NUGU_EQUEUE_TYPE_SERVER_DISCONNECTED, NULL);

    return;
}

/* invoked in a thread loop */
static void _on_destroy(HTTP2Request* req, void* userdata)
{
    MultipartParser* parser = (MultipartParser*)userdata;
    struct _parser_data* pdata;

    if (!parser)
        return;

    pdata = (struct _parser_data*)multipart_parser_get_data(parser);
    if (pdata)
        parser_data_free(pdata);

    multipart_parser_free(parser);
}

V1Directives* v1_directives_new(const char* host, int connection_timeout_secs)
{
    struct _v1_directives* dir;

    g_return_val_if_fail(host != NULL, NULL);

    dir = (V1Directives*)calloc(1, sizeof(struct _v1_directives));
    if (!dir) {
        error_nomem();
        return NULL;
    }

    dir->url = g_strdup_printf("%s/v1/directives", host);
    dir->connection_timeout_secs = connection_timeout_secs;

    return dir;
}

void v1_directives_free(V1Directives* dir)
{
    g_return_if_fail(dir != NULL);

    if (dir->url)
        g_free(dir->url);

    memset(dir, 0, sizeof(V1Directives));
    free(dir);
}

int v1_directives_establish(V1Directives* dir, HTTP2Network* net)
{
    HTTP2Request* req;
    MultipartParser* parser;
    struct _parser_data* pdata;
    int ret;

    g_return_val_if_fail(dir != NULL, -1);
    g_return_val_if_fail(net != NULL, -1);

    dir->network = net;

    parser = multipart_parser_new();
    if (!parser) {
        error_nomem();
        return -1;
    }

    pdata = parser_data_new();
    if (!pdata) {
        error_nomem();
        multipart_parser_free(parser);
        return -1;
    }

    pdata->ctype = CONTENT_TYPE_UNKNOWN;

    multipart_parser_set_data(parser, pdata);

    req = http2_request_new();
    http2_request_set_url(req, dir->url);
    http2_request_set_method(req, HTTP2_REQUEST_METHOD_GET);
    http2_request_set_header_callback(req, _on_header, parser);
    http2_request_set_body_callback(req, _on_body, parser);
    http2_request_set_finish_callback(req, _on_finish, NULL);
    http2_request_set_destroy_callback(req, _on_destroy, parser);
    http2_request_set_connection_timeout(req, dir->connection_timeout_secs);
    http2_request_enable_curl_log(req);

    ret = http2_network_add_request(net, req);
    if (ret < 0) {
        nugu_error("http2_network_add_request() failed: %d", ret);
        http2_request_unref(req);
        parser_data_free(pdata);
        multipart_parser_free(parser);
        return ret;
    }

    http2_request_unref(req);

    return ret;
}
