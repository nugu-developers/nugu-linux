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

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "njson/njson.h"

#include "base/nugu_equeue.h"
#include "base/nugu_log.h"
#include "base/nugu_prof.h"

#include "dg_types.h"

#include "directives_parser.h"
#include "http2_request.h"
#include "multipart_parser.h"

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

struct _dir_parser {
    MultipartParser* m_parser;

    enum dir_parser_type type;
    enum content_type ctype;
    char* parent_msg_id;
    int seq;
    int is_end;
    size_t body_size;

    DirParserDirectiveCallback directive_cb;
    void* directive_cb_userdata;

    DirParserJsonCallback json_cb;
    void* json_cb_userdata;
    NuguBuffer* json_buffer;

    DirParserEndCallback end_cb;
    void* end_cb_userdata;

    char* debug_msg;
};

static void on_parsing_header(MultipartParser* parser, const char* data,
    size_t length, void* userdata)
{
    struct _dir_parser* dp;
    const char* pos;
    const char* start = data;
    const char* end = data + length;
    int status = KEY;
    std::string key;
    std::string value;
    bool found = false;

    dp = (struct _dir_parser*)multipart_parser_get_data(parser);
    if (!dp)
        return;

    nugu_log_print(NUGU_LOG_MODULE_NETWORK_TRACE, NUGU_LOG_LEVEL_INFO, NULL,
        NULL, -1, "body header: %s", data);

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

        if (found == false)
            continue;

        if (key.compare("Content-Type") == 0) {
            if (value.compare(0, strlen(FILTER_JSON_TYPE), FILTER_JSON_TYPE) == 0) {
                dp->ctype = CONTENT_TYPE_JSON;
            } else if (value.compare(0, strlen(FILTER_OPUS_TYPE), FILTER_OPUS_TYPE) == 0) {
                dp->ctype = CONTENT_TYPE_OPUS;
            } else {
                dp->ctype = CONTENT_TYPE_UNKNOWN;
                nugu_error("unknown Content-Type");
            }
        } else if (key.compare("Content-Length") == 0) {
            dp->body_size = (size_t)strtoumax(value.c_str(), nullptr, 10);
        } else if (key.compare("Parent-Message-Id") == 0) {
            if (dp->parent_msg_id)
                free(dp->parent_msg_id);
            dp->parent_msg_id = g_strdup(value.c_str());
        } else if (key.compare("Filename") == 0) {
            dp->seq = std::stoi(value);
            if (value.find("end") != std::string::npos)
                dp->is_end = 1;
            else
                dp->is_end = 0;
        }

        found = false;
        key.clear();
        value.clear();
    }
}

static void _body_json(DirParser* dp, const char* data, size_t length)
{
    NJson::Value root;
    NJson::Value dir_list;
    NJson::ArrayIndex list_size;
    NJson::Reader reader;
    NJson::StyledWriter writer;
    std::string group;
    char group_buf[32];

    if (!reader.parse(data, root)) {
        nugu_error("parsing error: '%s'", data);
        return;
    }

    if (dp->json_buffer) {
        if (nugu_buffer_get_size(dp->json_buffer) == 0)
            nugu_buffer_add(dp->json_buffer, "[", 1);
        else
            nugu_buffer_add(dp->json_buffer, ",", 1);

        nugu_buffer_add(dp->json_buffer, data, length);
    }

    if (dp->json_cb)
        dp->json_cb(dp, data, dp->json_cb_userdata);

    if ((nugu_log_get_modules() & NUGU_LOG_MODULE_PROTOCOL) != 0) {
        std::string dump;
        int limit;

        dump = writer.write(root);

        limit = nugu_log_get_protocol_line_limit();
        if (limit > 0 && dump.length() > (size_t)limit) {
            dump.resize(limit);
            dump.append("<...too long...>");
        }

        nugu_log_protocol_recv(NUGU_LOG_LEVEL_INFO, "Directives%s\n%s",
            (dp->debug_msg) ? dp->debug_msg : "", dump.c_str());
    }

    dir_list = root["directives"];
    list_size = dir_list.size();

    group = "{ \"directives\": [";
    for (NJson::ArrayIndex i = 0; i < list_size; ++i) {
        NJson::Value dir = dir_list[i];
        NJson::Value h;

        h = dir["header"];

        if (i > 0)
            group.append(",");

        snprintf(group_buf, sizeof(group_buf), "\"%s.%s\"",
            h["namespace"].asCString(), h["name"].asCString());
        group.append(group_buf);
    }
    group.append("] }");

    if (list_size > 1)
        nugu_dbg("group=%s", group.c_str());

    for (NJson::ArrayIndex i = 0; i < list_size; ++i) {
        NJson::Value dir = dir_list[i];
        NJson::Value h;
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
            referrer.c_str(), p.c_str(), group.c_str());
        if (!ndir)
            continue;

        if (dp->directive_cb)
            dp->directive_cb(dp, ndir, dp->directive_cb_userdata);

        if (nugu_equeue_push(NUGU_EQUEUE_TYPE_NEW_DIRECTIVE, ndir) < 0) {
            nugu_error("nugu_equeue_push() failed.");
            nugu_directive_unref(ndir);
        }
    }
}

static void _body_opus(DirParser* dp, const char* parent_msg_id, int seq,
    int is_end, const char* data, size_t length)
{
    struct equeue_data_attachment* item;

    item = (struct equeue_data_attachment*)malloc(sizeof(struct equeue_data_attachment));
    if (!item) {
        nugu_error_nomem();
        return;
    }

    item->parent_msg_id = g_strdup(parent_msg_id);
    item->media_type = g_strdup("audio/opus");
    item->seq = seq;
    item->is_end = is_end;
    item->length = length;
    item->data = (unsigned char*)malloc(length);
    memcpy(item->data, data, length);

    if (item->seq == 0)
        nugu_prof_mark(NUGU_PROF_TYPE_TTS_NET_FIRST_ATTACHMENT);

    nugu_log_print(NUGU_LOG_MODULE_NETWORK, NUGU_LOG_LEVEL_DEBUG, NULL,
        NULL, -1, "<-- Attachment: parent=%s (%zd bytes, seq=%d, is_end=%d)%s",
        parent_msg_id, length, seq, is_end,
        (dp->debug_msg) ? dp->debug_msg : "");

    nugu_equeue_push(NUGU_EQUEUE_TYPE_NEW_ATTACHMENT, item);
}

static void _on_parsing_body(MultipartParser* parser, const char* data,
    size_t length, void* userdata)
{
    struct _dir_parser* dp;

    dp = (struct _dir_parser*)multipart_parser_get_data(parser);
    if (!dp)
        return;

    if (dp->ctype == CONTENT_TYPE_JSON)
        _body_json(dp, data, length);
    else if (dp->ctype == CONTENT_TYPE_OPUS)
        _body_opus(dp, dp->parent_msg_id, dp->seq, dp->is_end, data, length);
}

static void _on_parsing_end_boundary(MultipartParser* parser, void* userdata)
{
    struct _dir_parser* dp;

    dp = (struct _dir_parser*)multipart_parser_get_data(parser);
    if (!dp)
        return;

    if (dp->json_buffer) {
        if (nugu_buffer_get_size(dp->json_buffer) > 0)
            nugu_buffer_add(dp->json_buffer, "]", 1);
    }

    if (dp->end_cb)
        dp->end_cb(dp, dp->end_cb_userdata);
}

DirParser* dir_parser_new(enum dir_parser_type type)
{
    DirParser* dp;

    dp = (struct _dir_parser*)calloc(1, sizeof(struct _dir_parser));
    if (!dp) {
        nugu_error_nomem();
        return NULL;
    }

    dp->m_parser = multipart_parser_new();
    if (!dp->m_parser) {
        nugu_error_nomem();
        free(dp);
        return NULL;
    }

    dp->type = type;
    dp->ctype = CONTENT_TYPE_UNKNOWN;
    dp->debug_msg = NULL;

    multipart_parser_set_data(dp->m_parser, dp);

    return dp;
}

void dir_parser_free(DirParser* dp)
{
    g_return_if_fail(dp != NULL);

    multipart_parser_free(dp->m_parser);

    if (dp->debug_msg)
        free(dp->debug_msg);

    if (dp->parent_msg_id)
        free(dp->parent_msg_id);

    free(dp);
}

int dir_parser_add_header(DirParser* dp, const char* buffer,
    size_t buffer_len)
{
    const char* pos;

    g_return_val_if_fail(dp != NULL, -1);
    g_return_val_if_fail(buffer != NULL, -1);

    if (buffer_len == 0)
        return -1;

    if (strncmp(buffer, FILTER_CONTENT_TYPE, strlen(FILTER_CONTENT_TYPE)) != 0)
        return -1;

    pos = buffer + strlen(FILTER_CONTENT_TYPE);

    /* strlen(pos) - 2('\r\n') */
    multipart_parser_set_boundary(dp->m_parser, pos, strlen(pos) - 2);

    return 0;
}

int dir_parser_parse(DirParser* dp, const char* buffer, size_t buffer_len)
{
    g_return_val_if_fail(dp != NULL, -1);
    g_return_val_if_fail(buffer != NULL, -1);

    if (buffer_len == 0)
        return 0;

    multipart_parser_parse(dp->m_parser, buffer, buffer_len,
        on_parsing_header, _on_parsing_body, _on_parsing_end_boundary, NULL);

    return 0;
}

int dir_parser_set_debug_message(DirParser* dp, const char* msg)
{
    g_return_val_if_fail(dp != NULL, -1);

    if (dp->debug_msg) {
        free(dp->debug_msg);
        dp->debug_msg = NULL;
    }

    if (msg)
        dp->debug_msg = g_strdup(msg);

    return 0;
}

int dir_parser_set_directive_callback(DirParser* dp, DirParserDirectiveCallback cb,
    void* userdata)
{
    g_return_val_if_fail(dp != NULL, -1);

    dp->directive_cb = cb;
    dp->directive_cb_userdata = userdata;

    return 0;
}

int dir_parser_set_json_callback(DirParser* dp, DirParserJsonCallback cb,
    void* userdata)
{
    g_return_val_if_fail(dp != NULL, -1);

    dp->json_cb = cb;
    dp->json_cb_userdata = userdata;

    return 0;
}

int dir_parser_set_end_callback(DirParser* dp, DirParserEndCallback cb,
    void* userdata)
{
    g_return_val_if_fail(dp != NULL, -1);

    dp->end_cb = cb;
    dp->end_cb_userdata = userdata;

    return 0;
}

int dir_parser_set_json_buffer(DirParser* dp, NuguBuffer* buf)
{
    g_return_val_if_fail(dp != NULL, -1);

    dp->json_buffer = buf;

    return 0;
}

NuguBuffer* dir_parser_get_json_buffer(DirParser* dp)
{
    g_return_val_if_fail(dp != NULL, NULL);

    return dp->json_buffer;
}
