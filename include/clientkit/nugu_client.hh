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
#include <clientkit/nugu_core_container_interface.hh>

namespace NuguClientKit {

/**
 * @file nugu_client.hh
 * @defgroup NuguClient NuguClient
 * @ingroup SDKNuguClientKit
 * @brief Nugu Client
 *
 * The NUGU client has access to the capability builder, config management,
 * and handler for each capability agent, making it easier to use the NUGU SDK.
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
        friend class NuguClient;

        explicit CapabilityBuilder(NuguClientImpl* client_impl);
        virtual ~CapabilityBuilder() = default;

        NuguClientImpl* client_impl = nullptr;
    };

    NuguClient();
    ~NuguClient();

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
     * @brief Request NUGU SDK to load all plugins from directory.
     * If this function is not called directly, it is called automatically by initialize().
     * @param[in] path Plugin directory path. If empty, the default directory is used.
     */
    bool loadPlugins(const std::string& path = "");

    /**
     * @brief Request NUGU SDK to unload all plugins
     */
    void unloadPlugins(void);

    /**
     * @brief Request NUGU SDK initialization
     */
    bool initialize(void);

    /**
     * @brief Request NUGU SDK deinitialization
     */
    void deInitialize(void);

    /**
     * @brief Get NuguCoreContainer object
     * @return INuguCoreContainer abstraction object of NuguCoreContainer
     */
    INuguCoreContainer* getNuguCoreContainer();

    /**
     * @brief Get NetworkManager object
     * @return INetworkManager if a feature agent has been created with the feature builder, otherwise NULL
     */
    INetworkManager* getNetworkManager();

    /**
     * @brief Get instance of CapabilityAgent
     * @return ICapabilityInterface the instance which is related to requested CapabilityAgent, otherwise nullptr
     */
    ICapabilityInterface* getCapabilityHandler(const std::string& cname);

private:
    std::unique_ptr<NuguClientImpl> impl;
    CapabilityBuilder* cap_builder;
};

/**
 * @}
 */

} // NuguClientKit

#endif /* __NUGU_CLIENT_H__ */
