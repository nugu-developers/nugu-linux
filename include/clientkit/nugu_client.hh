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

#ifndef __NUGU_CLIENT_H__
#define __NUGU_CLIENT_H__

#include <memory>
#include <string>

#include <capability/capability_interface.hh>
#include <interface/media_player_interface.hh>
#include <interface/network_manager_interface.hh>
#include <interface/nugu_configuration.hh>
#include <interface/wakeup_interface.hh>

#include <clientkit/nugu_client_listener.hh>

namespace NuguClientKit {

using namespace NuguInterface;

/**
 * @file nugu_client.hh
 * @defgroup NuguClient NuguClient
 * @ingroup SDKNuguClientKit
 * @brief Nugu Client
 *
 * The nugu client has access to the capability builder, config management,
 * and handler for each capability agent, making it easier to use the nugu sdk.
 *
 * @{
 */

class NuguClientImpl;

/**
 * @brief NUGU Client
 * @see INuguClientListener
 */
class NuguClient {
public:
    /**
     * @brief CapabilityBuilder
     */
    class CapabilityBuilder {
    public:
        explicit CapabilityBuilder(NuguClientImpl* client_impl);
        virtual ~CapabilityBuilder();

        /**
         * @brief Add capabilities to create in the capabilitybuilder. Capabilitybuilder support to insert customized capability agents.
         * @param[in] capability agent name
         * @param[in] clistener capability listener
         * @param[in] cap_instance capability interface
         * @return CapabilityBuilder object
         */
        CapabilityBuilder* add(const std::string& cname,
            ICapabilityListener* clistener = nullptr,
            ICapabilityInterface* cap_instance = nullptr);

        /**
         * @brief Construct with capabilities added to CapabilityBuilder.
         * @return true if construct success, otherwise false
         */
        bool construct();

    private:
        NuguClientImpl* client_impl = nullptr;
    };

    NuguClient();
    ~NuguClient();

    /**
     * @brief Set config to change nugu sdk internal behavior
     * @param[in] key config key
     * @param[in] value config value
     */
    void setConfig(NuguConfig::Key key, const std::string& value);

    /**
     * @brief Set the listener object
     * @param[in] clistener listener
     */
    void setListener(INuguClientListener* listener);

    /**
     * @brief Get CapabilityBuilder object
     * @return CapabilityBuilder object
     */
    CapabilityBuilder* getCapabilityBuilder();

    /**
     * @brief Request nugu sdk initialization
     */
    bool initialize(void);

    /**
     * @brief Request nugu sdk deinitialization
     */
    void deInitialize(void);

    /**
     * @brief Get WakeupHandler object
     * @return IWakeupHandler if a feature agent has been created with the feature builder, otherwise NULL
     */
    IWakeupHandler* getWakeupHandler();

    /**
     * @brief Get CapabilityHandler object
     * @return ICapabilityInterface if a feature agent has been created with the feature builder, otherwise NULL
     */
    ICapabilityInterface* getCapabilityHandler(const std::string& cname);

    /**
     * @brief Get NetworkManager object
     * @return INetworkManager if a feature agent has been created with the feature builder, otherwise NULL
     */
    INetworkManager* getNetworkManager();

    /**
     * @brief Get new media player object
     * @return IMediaPlayer if media player is created, otherwise nullptr
     */
    IMediaPlayer* createMediaPlayer();

private:
    std::unique_ptr<NuguClientImpl> impl;
    std::unique_ptr<CapabilityBuilder> cap_builder;
};

/**
 * @}
 */

} // NuguClientKit

#endif /* __NUGU_CLIENT_H__ */
