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

#include <glib.h>
#include <stdlib.h>
#include <string.h>

#include "nugu_buffer.h"
#include "nugu_config.h"
#include "nugu_equeue.h"
#include "nugu_log.h"
#include "json/json.h"

#include "http2_request.h"
#include "v1_policies.h"

#include "dg_types.h"

static void _parse_servers(const Json::Value& root)
{
    Json::Value server_list;
    GList* servers = NULL;

    if (root["serverPolicies"].empty()) {
        nugu_error("can't find 'serverPolicies' attribute");
        nugu_equeue_push(NUGU_EQUEUE_TYPE_REGISTRY_FAILED, NULL);
        return;
    }

    server_list = root["serverPolicies"];
    if (server_list.size() == 0) {
        nugu_equeue_push(NUGU_EQUEUE_TYPE_REGISTRY_FAILED, NULL);
        return;
    }

    for (Json::ArrayIndex i = 0; i < server_list.size(); ++i) {
        struct dg_server_policy* server_item;
        Json::Value server = server_list[i];

        server_item = (struct dg_server_policy*)calloc(1, sizeof(struct dg_server_policy));
        if (!server_item) {
            error_nomem();
            break;
        }

        if (server["protocol"].isString()) {
            const char* tmp = server["protocol"].asCString();
            if (g_ascii_strcasecmp(tmp, "H2C") == 0)
                server_item->protocol = DG_PROTOCOL_H2C;
            else if (g_ascii_strcasecmp(tmp, "H2") == 0)
                server_item->protocol = DG_PROTOCOL_H2;
            else {
                nugu_error("unknown protocol: '%s' in item-%d", tmp, i);
                server_item->protocol = DG_PROTOCOL_UNKNOWN;
            }
        } else {
            nugu_error("can't find 'protocol' string attribute in item-%d", i);
            free(server_item);
            continue;
        }

        if (server["hostname"].isString()) {
            const char* tmp = server["hostname"].asCString();
            if (tmp) {
                int len = strlen(tmp);
                memcpy(server_item->hostname, tmp, (len > MAX_ADDRESS) ? MAX_ADDRESS : len);
            }
        } else {
            nugu_error("can't find 'hostname' string attribute in item-%d", i);
            free(server_item);
            continue;
        }

        if (server["address"].isString()) {
            const char* tmp = server["address"].asCString();
            if (tmp) {
                int len = strlen(tmp);
                memcpy(server_item->address, tmp, (len > MAX_ADDRESS) ? MAX_ADDRESS : len);
            }
        } else {
            nugu_error("can't find 'address' string attribute in item-%d", i);
            free(server_item);
            continue;
        }

        if (server["port"].isNumeric()) {
            server_item->port = server["port"].asInt();
        } else {
            nugu_error("can't find 'port' number attribute in item-%d", i);
            free(server_item);
            continue;
        }

        if (server["retryCountLimit"].isNumeric()) {
            server_item->retry_count_limit = server["retryCountLimit"].asInt();
        } else {
            nugu_error("can't find 'retryCountLimit' number attribute in item-%d", i);
            free(server_item);
            continue;
        }

        if (server["connectionTimeout"].isNumeric()) {
            server_item->connection_timeout_ms = server["connectionTimeout"].asInt();
        } else {
            nugu_error("can't find 'connectionTimeout' number attribute in item-%d", i);
            free(server_item);
            continue;
        }

        if (server["charge"].isString()) {
            if (g_ascii_strcasecmp(server["charge"].asCString(), "FREE") == 0)
                server_item->is_charge = 0;
            else
                server_item->is_charge = 1;
        } else {
            nugu_error("can't find 'charge' attribute in item-%d", i);
        }

        servers = g_list_append(servers, server_item);
    }

    if (g_list_length(servers) == 0) {
        nugu_error("no available servers");
        nugu_equeue_push(NUGU_EQUEUE_TYPE_REGISTRY_FAILED, NULL);
    } else {
        nugu_dbg("found %d servers", g_list_length(servers));
        nugu_equeue_push(NUGU_EQUEUE_TYPE_REGISTRY_SERVERS, servers);
    }
}

static int _parse_health_policy(const Json::Value& root)
{
    Json::Value health;
    struct dg_health_check_policy* policy;

    health = root["healthCheckPolicy"];
    if (health.empty()) {
        nugu_error("can't find 'healthCheckPolicy' attribute");
        nugu_equeue_push(NUGU_EQUEUE_TYPE_REGISTRY_FAILED, NULL);
        return -1;
    }

    policy = (struct dg_health_check_policy*)malloc(sizeof(struct dg_health_check_policy));
    if (!policy) {
        error_nomem();
        return -1;
    }

    policy->ttl_ms = health["ttl"].asInt();
    policy->ttl_max_ms = health["ttlMax"].asInt();
    policy->beta = health["beta"].asFloat();
    policy->retry_count_limit = health["retryCountLimit"].asInt();
    policy->retry_delay_ms = health["retryDelay"].asInt();
    policy->health_check_timeout_ms = health["healthCheckTimeout"].asInt();
    policy->accumulation_time_ms = health["accumulationTime"].asInt();

    nugu_equeue_push(NUGU_EQUEUE_TYPE_REGISTRY_HEALTH, policy);

    return 0;
}

static void _on_finish(HTTP2Request* req, void* userdata)
{
    int code;
    char* message = (char*)nugu_buffer_peek(http2_request_peek_response_body(req));
    Json::Value root;
    Json::Reader reader;
    std::string dump;
    Json::StyledWriter writer;

    code = http2_request_get_response_code(req);
    if (code != HTTP2_RESPONSE_OK) {
        nugu_error("failed(%d): %s", code, message);
        if (code == HTTP2_RESPONSE_AUTHFAIL)
            nugu_equeue_push(NUGU_EQUEUE_TYPE_INVALID_TOKEN, NULL);
        else
            nugu_equeue_push(NUGU_EQUEUE_TYPE_REGISTRY_FAILED, NULL);

        return;
    }

    if (!message) {
        nugu_error("no data");
        nugu_equeue_push(NUGU_EQUEUE_TYPE_REGISTRY_FAILED, NULL);
        return;
    }

    if (!reader.parse(message, root)) {
        nugu_error("parsing error: '%s'", message);
        nugu_equeue_push(NUGU_EQUEUE_TYPE_REGISTRY_FAILED, NULL);
        return;
    }

    dump = writer.write(root);
    nugu_dbg("Policy: %s", dump.c_str());

    if (_parse_health_policy(root) < 0)
        return;

    _parse_servers(root);
}

int v1_policies_get(HTTP2Network* net, const char* host)
{
    HTTP2Request* req;
    gchar* url;
    int ret;

    g_return_val_if_fail(net != NULL, -1);
    g_return_val_if_fail(host != NULL, -1);

    req = http2_request_new();

    url = g_strdup_printf("%s/v1/policies?protocol=H2", host);
    nugu_info("Get registry policy: %s", url);
    http2_request_set_url(req, url);
    g_free(url);

    http2_request_set_method(req, HTTP2_REQUEST_METHOD_GET);
    http2_request_set_finish_callback(req, _on_finish, NULL);

    ret = http2_network_add_request(net, req);
    if (ret < 0)
        return ret;

    http2_request_unref(req);

    return ret;
}
