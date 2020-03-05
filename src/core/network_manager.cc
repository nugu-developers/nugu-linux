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

#include <algorithm>

#include "base/nugu_log.h"
#include "base/nugu_network_manager.h"

#include "network_manager.hh"

namespace NuguCore {

static void _status(NuguNetworkStatus status, void* userdata)
{
    NetworkManager* instance = static_cast<NetworkManager*>(userdata);
    auto listeners = instance->getListener();

    // send nugu network disconnected info to user for inducing checking auth info
    if (status == NUGU_NETWORK_DISCONNECTED) {
        nugu_info("Network disconnected");

        for (auto listener : listeners) {
            listener->onStatusChanged(NetworkStatus::DISCONNECTED);
        }
    } else if (status == NUGU_NETWORK_CONNECTED) {
        nugu_info("Network connected");

        for (auto listener : listeners) {
            listener->onStatusChanged(NetworkStatus::CONNECTED);
        }
    } else if (status == NUGU_NETWORK_CONNECTING) {
        nugu_info("Network connecting");

        for (auto listener : listeners) {
            listener->onStatusChanged(NetworkStatus::CONNECTING);
        }
    } else if (status == NUGU_NETWORK_TOKEN_ERROR) {
        nugu_error("Network token error");

        for (auto listener : listeners) {
            listener->onError(NetworkError::TOKEN_ERROR);
        }
    } else {
        nugu_error("Network unknown error");
        for (auto listener : listeners) {
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
    auto listeners = instance->getListener();
    for (auto listener : listeners) {
        const char* ename = nugu_event_peek_name(event);
        const char* msg_id = nugu_event_peek_msg_id(event);
        const char* dialog_id = nugu_event_peek_dialog_id(event);
        const char* referrer_id = nugu_event_peek_referrer_id(event);

        listener->onEventSent(ename, msg_id, dialog_id, referrer_id);
    }
}

static void eventResultCallback(int success, const char* msg_id, const char* dialog_id, int code, void* userdata)
{
    if (msg_id == NULL || dialog_id == NULL || userdata == NULL) {
        nugu_error("parameter is null value");
        return;
    }

    NetworkManager* instance = static_cast<NetworkManager*>(userdata);
    auto listeners = instance->getListener();
    for (auto listener : listeners) {
        bool result = success == 1 ? true : false;
        listener->onEventResult(msg_id, result, code);
    }
}

NetworkManager::NetworkManager()
{
    nugu_network_manager_initialize();
    nugu_network_manager_set_status_callback(_status, this);
    nugu_network_manager_set_event_send_notify_callback(eventSendNotifyCallback, this);
    nugu_network_manager_set_event_result_callback(eventResultCallback, this);
}

NetworkManager::~NetworkManager()
{
    nugu_network_manager_set_status_callback(NULL, NULL);
    nugu_network_manager_set_event_send_notify_callback(NULL, NULL);
    nugu_network_manager_set_event_result_callback(NULL, NULL);
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
    if (nugu_network_manager_set_token(token.c_str()) < 0) {
        nugu_error("network set token failed");
        return false;
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

bool NetworkManager::setUserAgent(std::string uagent)
{
    if (nugu_network_manager_set_useragent(uagent.c_str()) < 0) {
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
