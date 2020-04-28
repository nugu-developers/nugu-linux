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

#ifndef __NUGU_INETWORK_MANAGER_INTERFACE_H__
#define __NUGU_INETWORK_MANAGER_INTERFACE_H__

#include <string>

namespace NuguClientKit {

/**
 * @file network_manager_interface.hh
 * @defgroup NetworkManagerInterface
 * @ingroup SDKNuguClientKit
 * @brief Network Manager interface
 *
 * The network manager controls the NUGU server connection
 * and disconnection and reports events for server connections.
 *
 * @{
 */

enum class NetworkStatus {
    DISCONNECTED, /**< Network disconnected */
    CONNECTED, /**< Network connected */
    CONNECTING /**< Connection in progress */
};

enum class NetworkError {
    TOKEN_ERROR, /**< Occurs when the issued token expires */
    UNKNOWN /**< Unknown */
};

/**
 * @brief network manager listener interface
 * @see INetworkManager
 */
class INetworkManagerListener {
public:
    virtual ~INetworkManagerListener() = default;

    /**
     * @brief Report the connection status with the NUGU server.
     * @param[in] status network status
     */
    virtual void onStatusChanged(NetworkStatus status);

    /**
     * @brief Report an error while communicating with the NUGU server.
     * @param[in] error network error
     */
    virtual void onError(NetworkError error);

    /**
     * @brief Report that an event has been sent to the server.
     * @param[in] ename event name
     * @param[in] msg_id event message id
     * @param[in] dialog_id event dialog request id
     * @param[in] referrer_id event referrer dialog request id
     */
    virtual void onEventSent(const char* ename, const char* msg_id, const char* dialog_id, const char* referrer_id);

    /**
     * @brief Report the result of sending an event from the server.
     * @param[in] msg_id event message id
     * @param[in] success event result
     * @param[in] code event result code (similar to http status code)
     */
    virtual void onEventSendResult(const char* msg_id, bool success, int code);

    /**
     * @brief Report the response data received from the server.
     * @param[in] msg_id event message id
     * @param[in] json response json data
     * @param[in] success event result
     */
    virtual void onEventResponse(const char* msg_id, const char* json, bool success);
};

/**
 * @brief network manager interface
 * @see INetworkManagerListener
 */
class INetworkManager {
public:
    virtual ~INetworkManager() = default;

    /**
     * @brief Add the Listener object
     * @param[in] listener listener object
     */
    virtual void addListener(INetworkManagerListener* listener) = 0;

    /**
     * @brief Remove the Listener object
     * @param[in] listener listener object
     */
    virtual void removeListener(INetworkManagerListener* listener) = 0;

    /**
     * @brief Request a connection with the NUGU server.
     * @return result
     * @retval 0 success
     * @retval -1 failure
     */
    virtual bool connect() = 0;

    /**
     * @brief Request a disconnection with the NUGU server.
     * @return result
     * @retval 0 success
     * @retval -1 failure
     */
    virtual bool disconnect() = 0;

    /**
     * @brief Set the access token value.
     * @param[in] token access token
     * @return result
     * @retval 0 success
     * @retval -1 failure
     */
    virtual bool setToken(std::string token) = 0;

    /**
     * @brief Set the device gateway registry url.
     * @param[in] registry url
     * @return result
     * @retval 0 success
     * @retval -1 failure
     */
    virtual bool setRegistryUrl(std::string url) = 0;

    /**
     * @brief Set the HTTP header UserAgent information.
     * @param[in] app_version application version (e.g. "0.1.0")
     * @param[in] additional_info additional information or ""
     * @return result
     * @retval 0 success
     * @retval -1 failure
     */
    virtual bool setUserAgent(std::string app_version, std::string additional_info = "") = 0;
};

/**
 * @}
 */

} // NuguClientKit

#endif /* __NUGU_INETWORK_MANAGER_INTERFACE_H__ */
