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
#include <glib.h>

#include <algorithm>

#include "json/json.h"

#include "base/nugu_log.h"
#include "base/nugu_network_manager.h"

#include "network_manager.hh"

namespace NuguCore {

static void _status(NuguNetworkStatus status, void* userdata)
{
    NetworkManager* instance = static_cast<NetworkManager*>(userdata);
    const auto& listeners = instance->getListener();

    // send nugu network disconnected info to user for inducing checking auth info
    if (status == NUGU_NETWORK_DISCONNECTED) {
        nugu_info("Network disconnected");

        for (const auto& listener : listeners) {
            listener->onStatusChanged(NetworkStatus::DISCONNECTED);
        }
    } else if (status == NUGU_NETWORK_CONNECTING) {
        nugu_info("Network connecting");

        for (const auto& listener : listeners) {
            listener->onStatusChanged(NetworkStatus::CONNECTING);
        }
    } else if (status == NUGU_NETWORK_READY) {
        nugu_info("Network ready");

        for (const auto& listener : listeners) {
            listener->onStatusChanged(NetworkStatus::READY);
        }
    } else if (status == NUGU_NETWORK_CONNECTED) {
        nugu_info("Network connected");

        for (const auto& listener : listeners) {
            listener->onStatusChanged(NetworkStatus::CONNECTED);
        }
    } else if (status == NUGU_NETWORK_TOKEN_ERROR) {
        nugu_error("Network token error");

        for (const auto& listener : listeners) {
            listener->onError(NetworkError::TOKEN_ERROR);
        }
    } else {
        nugu_error("Network unknown error");
        for (const auto& listener : listeners) {
            listener->onError(NetworkError::UNKNOWN);
        }
    }
}

static void eventSendNotifyCallback(NuguEvent* event, void* userdata)
{
    if (event == NULL || userdata == NULL) {
        nugu_error("parameter is null value");
        return;
    }

    NetworkManager* instance = static_cast<NetworkManager*>(userdata);
    const auto& listeners = instance->getListener();
    for (const auto& listener : listeners)
        listener->onEventSend(event);
}

static void eventDataSendNotifyCallback(NuguEvent* event, int is_end, size_t length, unsigned char* data, void* userdata)
{
    if (event == NULL || userdata == NULL) {
        nugu_error("parameter is null value");
        return;
    }

    NetworkManager* instance = static_cast<NetworkManager*>(userdata);
    const auto& listeners = instance->getListener();
    for (const auto& listener : listeners)
        listener->onEventAttachmentSend(event, nugu_event_get_seq(event),
            (is_end == 1) ? true : false, length, data);
}

static void eventSendResultCallback(int success, const char* msg_id, const char* dialog_id, int code, void* userdata)
{
    if (msg_id == NULL || dialog_id == NULL || userdata == NULL) {
        nugu_error("parameter is null value");
        return;
    }

    NetworkManager* instance = static_cast<NetworkManager*>(userdata);
    const auto& listeners = instance->getListener();
    for (const auto& listener : listeners) {
        bool result = success == 1 ? true : false;
        listener->onEventSendResult(msg_id, result, code);
    }
}

static void eventResponseCallback(int success,
    const char* event_msg_id, const char* event_dialog_id,
    const char* json, void* userdata)
{
    if (event_msg_id == NULL || event_dialog_id == NULL || userdata == NULL) {
        nugu_error("parameter is null value");
        return;
    }

    NetworkManager* instance = static_cast<NetworkManager*>(userdata);
    const auto& listeners = instance->getListener();
    for (const auto& listener : listeners) {
        bool result = success == 1 ? true : false;
        listener->onEventResponse(event_msg_id, json, result);
    }
}

NetworkManager::NetworkManager()
{
    nugu_network_manager_initialize();
    nugu_network_manager_set_status_callback(_status, this);
    nugu_network_manager_set_event_send_notify_callback(eventSendNotifyCallback, this);
    nugu_network_manager_set_event_data_send_notify_callback(eventDataSendNotifyCallback, this);
    nugu_network_manager_set_event_result_callback(eventSendResultCallback, this);
    nugu_network_manager_set_event_response_callback(eventResponseCallback, this);
}

NetworkManager::~NetworkManager()
{
    nugu_network_manager_set_status_callback(NULL, NULL);
    nugu_network_manager_set_event_send_notify_callback(NULL, NULL);
    nugu_network_manager_set_event_data_send_notify_callback(NULL, NULL);
    nugu_network_manager_set_event_result_callback(NULL, NULL);
    nugu_network_manager_set_event_response_callback(NULL, this);
    nugu_network_manager_deinitialize();

    listeners.clear();
}

void NetworkManager::addListener(INetworkManagerListener* listener)
{
    if (listener && std::find(listeners.begin(), listeners.end(), listener) == listeners.end())
        listeners.emplace_back(listener);
}

void NetworkManager::removeListener(INetworkManagerListener* listener)
{
    auto iterator = std::find(listeners.begin(), listeners.end(), listener);

    if (iterator != listeners.end())
        listeners.erase(iterator);
}

std::vector<INetworkManagerListener*> NetworkManager::getListener()
{
    return listeners;
}

bool NetworkManager::setToken(std::string token)
{
    /* JWT format consist of 3 parts (header, payload, signature) */
    gchar **jwt = g_strsplit(token.c_str(), ".", -1);
    if (jwt == NULL) {
        nugu_error("invalid JWT token format");
        return false;
    }

    if (jwt[0] == NULL || jwt[1] == NULL) {
        nugu_error("invalid JWT token format");
        g_strfreev(jwt);
        return false;
    }

    /* add optional padding('==') for base64 decoding */
    gchar *tmp = g_strdup_printf("%s==", jwt[1]);

    g_strfreev(jwt);

    gsize out_len = 0;
    guchar *payload = g_base64_decode(tmp, &out_len);
    g_free(tmp);

    if (payload == NULL || out_len <= 0) {
        nugu_error("invalid token (decoding error)");
        return false;
    }

    /* generate null-terminated string from decoding result */
    gchar *json_text = (gchar *)g_malloc(out_len + 1);
    memcpy(json_text, payload, out_len);
    json_text[out_len] = 0;
    g_free(payload);

    nugu_dbg("JWT payload: %s", json_text);

    /* find the 'device:S.I.D.' from the scope */
    Json::Reader reader;
    Json::Value root;

    if (!reader.parse(json_text, root)) {
        nugu_error("Payload parsing error");
        g_free(json_text);
        return false;
    }

    g_free(json_text);

    bool is_connection_oriented = false;

    Json::Value scope = root["scope"];
    if (scope.isArray()) {
        Json::ArrayIndex scope_len = scope.size();
        for (Json::ArrayIndex i = 0; i < scope_len; ++i) {
            nugu_dbg("scope[%d] = %s", i, scope[i].asCString());
            if (scope[i].asString() == "device:S.I.D.") {
                is_connection_oriented = true;
                break;
            }
        }
    }

    if (nugu_network_manager_set_token(token.c_str()) < 0) {
        nugu_error("network set token failed");
        return false;
    }

    if (is_connection_oriented) {
        nugu_dbg("Connection oriented!");
        nugu_network_manager_set_connection_type(NUGU_NETWORK_CONNECTION_ORIENTED);
    } else {
        nugu_dbg("Connection ondemand! (connect when sending an event)");
        nugu_network_manager_set_connection_type(NUGU_NETWORK_CONNECTION_ONDEMAND);
    }

    return true;
}

bool NetworkManager::setRegistryUrl(std::string url)
{
    if (nugu_network_manager_set_registry_url(url.c_str()) < 0) {
        nugu_error("network set registry url failed");
        return false;
    }

    return true;
}

bool NetworkManager::setUserAgent(std::string app_version, std::string additional_info)
{
    if (nugu_network_manager_set_useragent(app_version.c_str(), additional_info.c_str()) < 0) {
        nugu_error("network set useragent failed");
        return false;
    }

    return true;
}

bool NetworkManager::connect()
{
    if (nugu_network_manager_connect() < 0) {
        nugu_error("network connect failed");
        return false;
    }

    return true;
}

bool NetworkManager::disconnect()
{
    if (nugu_network_manager_disconnect() < 0) {
        nugu_error("network disconnect failed");
        return false;
    }

    return true;
}

} // NuguCore
