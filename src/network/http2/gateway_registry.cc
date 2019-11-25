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

#include "nugu_buffer.h"
#include "nugu_config.h"
#include "nugu_log.h"
#include "json/json.h"

#include "gateway_registry.h"
#include "http2_network.h"
#include "http2_request.h"

#define GATEWAY_REGISTRY_URL_FORMAT "%s/v1/policies?protocol=H2"

struct _gateway_registry {
    char* body;
    char* host;
    GatewayRegistryCallback callback;
    void* callback_userdata;
    HTTP2Request* req;
    guint idle_source;
};

static void _free_server_item(gpointer data)
{
    GatewayServer* server_item = (GatewayServer*)data;

    if (!server_item)
        return;

    g_free(server_item->protocol);
    g_free(server_item->hostname);
    g_free(server_item->address);
    g_free(server_item->charge);
    free(server_item);
}

void gateway_registry_server_list_free(GList* servers)
{
    g_list_free_full(servers, _free_server_item);
}

GatewayRegistry* gateway_registry_new(void)
{
    struct _gateway_registry* registry;

    const char* gateway_registry_dns = nugu_config_get(NUGU_CONFIG_KEY_GATEWAY_REGISTRY_DNS);

    if (!gateway_registry_dns) {
        nugu_error("Gateway registry dns is not set.");
        return NULL;
    }

    registry = (GatewayRegistry*)calloc(1, sizeof(struct _gateway_registry));
    if (!registry) {
        error_nomem();
        return NULL;
    }

    registry->host = g_strdup_printf(GATEWAY_REGISTRY_URL_FORMAT, gateway_registry_dns);

    return registry;
}

void gateway_registry_free(GatewayRegistry* registry)
{
    g_return_if_fail(registry != NULL);

    if (registry->idle_source)
        g_source_remove(registry->idle_source);

    if (registry->body)
        free(registry->body);

    if (registry->host)
        free(registry->host);

    if (registry->req)
        http2_request_unref(registry->req);

    memset(registry, 0, sizeof(GatewayRegistry));
    free(registry);
}

int gateway_registry_set_callback(GatewayRegistry* registry,
    GatewayRegistryCallback callback, void* userdata)
{
    g_return_val_if_fail(registry != NULL, -1);

    registry->callback = callback;
    registry->callback_userdata = userdata;

    return 0;
}

static gboolean _callback_in_idle(gpointer data)
{
    GatewayRegistry* registry = (GatewayRegistry*)data;

    if (registry->callback)
        registry->callback(registry,
            http2_request_get_response_code(registry->req),
            registry->callback_userdata);

    return FALSE;
}

static void _on_finish(HTTP2Request* req, void* userdata)
{
    GatewayRegistry* registry = (GatewayRegistry*)userdata;
    char* message = (char*)nugu_buffer_peek(http2_request_peek_response_body(req));
    int code;

    code = http2_request_get_response_code(registry->req);
    if (code == HTTP2_RESPONSE_OK) {
        if (message)
            registry->body = strdup(message);
    } else {
        nugu_error("failed(%d): %s", code, message);
    }

    registry->idle_source = g_idle_add(_callback_in_idle, registry);
}

int gateway_registry_policy(GatewayRegistry* registry, HTTP2Network* net)
{
    int ret;

    g_return_val_if_fail(registry != NULL, -1);
    g_return_val_if_fail(net != NULL, -1);
    g_return_val_if_fail(registry->req == NULL, -1);

    registry->req = http2_request_new();
    http2_request_set_url(registry->req, registry->host);
    http2_request_set_method(registry->req, HTTP2_REQUEST_METHOD_GET);
    http2_request_set_finish_callback(registry->req, _on_finish, registry);

    nugu_dbg("Registry Policy: %s", registry->host);
    ret = http2_network_add_request(net, registry->req);
    if (ret < 0)
        return ret;

    return ret;
}

GList* gateway_registry_get_servers(GatewayRegistry* registry)
{
    g_return_val_if_fail(registry != NULL, NULL);

    Json::Value root;
    Json::Value health;
    Json::Reader reader;
    Json::Value server_list;
    Json::StyledWriter writer;
    GList* servers = NULL;

    if (!registry->body)
        return NULL;

    if (!reader.parse(registry->body, root)) {
        nugu_error("parsing error: '%s'", registry->body);
        return NULL;
    }

    server_list = root["serverPolicies"];
    for (Json::ArrayIndex i = 0; i < server_list.size(); ++i) {
        GatewayServer* server_item;
        Json::Value server = server_list[i];

        server_item = (GatewayServer*)malloc(sizeof(GatewayServer));
        server_item->protocol = g_strdup(server["protocol"].asCString());
        server_item->hostname = g_strdup(server["hostname"].asCString());
        server_item->address = g_strdup(server["address"].asCString());
        server_item->port = server["port"].asInt();
        server_item->retry_count_limit = server["retryCountLimit"].asInt();
        server_item->connection_timeout = server["connectionTimeout"].asInt();
        server_item->charge = g_strdup(server["charge"].asCString());
        servers = g_list_append(servers, server_item);
    }

    return servers;
}

int gateway_registry_get_health_policy(GatewayRegistry* registry,
    GatewayHealthPolicy* policy)
{
    Json::Value root;
    Json::Value health;
    Json::Reader reader;
    Json::Value server_list;
    Json::StyledWriter writer;
    std::string dump;

    g_return_val_if_fail(registry != NULL, -1);
    g_return_val_if_fail(policy != NULL, -1);

    if (!registry->body)
        return -1;

    if (!reader.parse(registry->body, root)) {
        nugu_error("parsing error: '%s'", registry->body);
        return -1;
    }

    dump = writer.write(root);
    nugu_dbg("Policy: %s", dump.c_str());

    health = root["healthCheckPolicy"];

    policy->ttl = health["ttl"].asInt();
    policy->ttl_max = health["ttlMax"].asInt();
    policy->beta = health["beta"].asFloat();
    policy->retry_count_limit = health["retryCountLimit"].asInt();
    policy->retry_delay = health["retryDelay"].asInt();
    policy->health_check_timeout = health["healthCheckTimeout"].asInt();
    policy->accumulation_time = health["accumulationTime"].asInt();

    return 0;
}
