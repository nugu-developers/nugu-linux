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

namespace NuguInterface {

/**
 * @file network_manager_interface.hh
 * @defgroup NetworkManagerInterface
 * @ingroup SDKNuguInterface
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
    UNKNOWN /**< UNKNOWN */
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
     */
    virtual void onStatusChanged(NetworkStatus status);

    /**
     * @brief Report an error while communicating with the NUGU server.
     */
    virtual void onError(NetworkError error);
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
     */
    virtual bool connect() = 0;

    /**
     * @brief Request a disconnection with the NUGU server.
     */
    virtual bool disconnect() = 0;

    /**
     * @brief Set the access token value.
     */
    virtual bool setToken(std::string token) = 0;
};

/**
 * @}
 */

} // NuguInterface

#endif /* __NUGU_INETWORK_MANAGER_INTERFACE_H__ */
