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

#ifndef __NUGU_NETWORK_MANAGER_INTERFACE_H__
#define __NUGU_NETWORK_MANAGER_INTERFACE_H__

#include <nugu.h>
#include <string>
#include <base/nugu_event.h>

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
    CONNECTING, /**< Connection in progress */
    READY, /**< Network ready for ondemand connection type */
    CONNECTED /**< Network connected */
};

enum class NetworkError {
    FAILED, /**< Failed to connect to all servers */
    TOKEN_ERROR, /**< Occurs when the issued token expires */
    UNKNOWN /**< Unknown */
};

/**
 * @brief network manager listener interface
 * @see INetworkManager
 */
class NUGU_API INetworkManagerListener {
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
     * @brief Report that the event will be sent to the server.
     * @param[in] nev NuguEvent object
     */
    virtual void onEventSend(NuguEvent *nev);

    /**
     * @brief Report that the attachment will be sent to the server.
     * @param[in] nev NuguEvent object
     */
    virtual void onEventAttachmentSend(NuguEvent *nev, int seq, bool is_end, size_t length, unsigned char *data);

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
     * @param[in] data response raw data (format json)
     * @param[in] success event result
     */
    virtual void onEventResponse(const char* msg_id, const char* data, bool success);
};

/**
 * @brief network manager interface
 * @see INetworkManagerListener
 */
class NUGU_API INetworkManager {
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
     * @brief Set the access token value. The connection type is automatically set through token analysis.
     * @param[in] token access token
     * @return result
     * @retval 0 success
     * @retval -1 failure
     */
    virtual bool setToken(const std::string& token) = 0;

    /**
     * @brief Set the device gateway registry url.
     * @param[in] url registry url
     * @return result
     * @retval 0 success
     * @retval -1 failure
     */
    virtual bool setRegistryUrl(const std::string& url) = 0;

    /**
     * @brief Set the HTTP header UserAgent information.
     * @param[in] app_version application version (e.g. "0.1.0")
     * @param[in] additional_info additional information or ""
     * @return result
     * @retval 0 success
     * @retval -1 failure
     */
    virtual bool setUserAgent(const std::string& app_version, const std::string& additional_info = "") = 0;
};

/**
 * @}
 */

} // NuguClientKit

#endif /* __NUGU_INETWORK_MANAGER_INTERFACE_H__ */
