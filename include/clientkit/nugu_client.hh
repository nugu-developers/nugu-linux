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

#include <clientkit/capability_interface.hh>
#include <clientkit/nugu_client_listener.hh>
#include <clientkit/nugu_core_container_interface.hh>

namespace NuguClientKit {

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
         * @brief Add capability instance. It could create from CapabilityFactory or by self as inhering ICapabilityInterface
         * @param[in] cap_instance capability interface
         * @return CapabilityBuilder object
         */
        CapabilityBuilder* add(ICapabilityInterface* cap_instance);

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
     * @brief Set the listener object
     * @param[in] clistener listener
     */
    void setListener(INuguClientListener* listener);

    /**
     * @brief Set wakeup word
     * @param[in] wakeup_word wakeup word text
     */
    void setWakeupWord(const std::string& wakeup_word);

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
     * @brief Get NuguCoreContainer object
     * @return INuguCoreContainer abstraction object of NuguCoreContainer
     */
    INuguCoreContainer* getNuguCoreContainer();

    /**
     * @brief Get CapabilityHandler object
     * @param[in] cname capability interface name
     * @return ICapabilityInterface if a feature agent has been created with the feature builder, otherwise NULL
     */
    ICapabilityInterface* getCapabilityHandler(const std::string& cname);

    /**
     * @brief Get NetworkManager object
     * @return INetworkManager if a feature agent has been created with the feature builder, otherwise NULL
     */
    INetworkManager* getNetworkManager();

private:
    std::unique_ptr<NuguClientImpl> impl;
    std::unique_ptr<CapabilityBuilder> cap_builder;
};

/**
 * @}
 */

} // NuguClientKit

#endif /* __NUGU_CLIENT_H__ */
