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

#include <string>

#include "nugu.h"

#include "base/nugu_equeue.h"
#include "base/nugu_log.h"
#include "base/nugu_plugin.h"

#include "capability/capability_factory.hh"
#include "capability/system_interface.hh"

#include "core/audio_recorder_manager.hh"
#include "core/capability_manager_helper.hh"

#include "nugu_client_impl.hh"

namespace NuguClientKit {

using namespace NuguCore;

NuguClientImpl::NuguClientImpl()
{
    nugu_equeue_initialize();

    nugu_core_container = std::unique_ptr<NuguCoreContainer>(new NuguCoreContainer());

    network_manager = std::unique_ptr<INetworkManager>(nugu_core_container->createNetworkManager());
    network_manager->addListener(this);
}

NuguClientImpl::~NuguClientImpl()
{
    CapabilityManagerHelper::destroyInstance();
    nugu_equeue_deinitialize();
}

void NuguClientImpl::setListener(INuguClientListener* listener)
{
    this->listener = listener;
}

INuguClientListener* NuguClientImpl::getListener()
{
    return listener;
}

void NuguClientImpl::registerCapability(const std::string& cname, std::pair<ICapabilityInterface*, ICapabilityListener*> capability)
{
    icapability_map[cname] = capability;
}

ICapabilityInterface* NuguClientImpl::getCapabilityHandler(const std::string& cname)
{
    if (icapability_map.empty()) {
        nugu_error("No Capability exist");
        return nullptr;
    }

    if (icapability_map.find(cname) != icapability_map.end())
        return icapability_map[cname].first;

    return nullptr;
}

INuguCoreContainer* NuguClientImpl::getNuguCoreContainer()
{
    return nugu_core_container.get();
}

int NuguClientImpl::create(void)
{
    if (createCapabilities() <= 0) {
        nugu_error("No Capabilities are created");
        return -1;
    }

    return 0;
}

int NuguClientImpl::createCapabilities(void)
{
    /*
     * [description]
     *  - icapability_map[CAPABILITY.type].first : ICapabilityInterface instance
     *  - icapability_map[CAPABILITY.type].second : ICapabilityListener instance
     */
    for (auto const& CAPABILITY : CapabilityFactory::getCapabilityList()) {
        // if user didn't add and it's not a required agent, skip to create
        if (icapability_map.find(CAPABILITY.name) == icapability_map.end() && !CAPABILITY.is_default)
            continue;

        if (!icapability_map[CAPABILITY.name].first)
            try {
                icapability_map[CAPABILITY.name].first = CAPABILITY.creator();
            } catch (std::exception& exp) {
                nugu_error(exp.what());
            }

        if (icapability_map[CAPABILITY.name].first) {
            // add general observer
            if (listener)
                icapability_map[CAPABILITY.name].first->registerObserver(listener);

            // set capability listener & handler each other
            if (icapability_map[CAPABILITY.name].second)
                icapability_map[CAPABILITY.name].first->setCapabilityListener(icapability_map[CAPABILITY.name].second);

            // set NuguCoreContainer
            icapability_map[CAPABILITY.name].first->setNuguCoreContainer(getNuguCoreContainer());

            std::string cname = icapability_map[CAPABILITY.name].first->getName();

            if (!cname.empty())
                CapabilityManagerHelper::addCapability(cname, icapability_map[CAPABILITY.name].first);
        }
    }

    return icapability_map.size();
}

bool NuguClientImpl::initialize(void)
{
    if (initialized) {
        nugu_info("It's already initialized.");
        return true;
    }

    if (nugu_plugin_load_directory(NUGU_PLUGIN_DIR) < 0) {
        nugu_error("Fail to load nugu_plugin ");
        return false;
    }

    nugu_plugin_initialize();

    AudioRecorderManager::getInstance();

    if (icapability_map.empty())
        create();

    // initialize capability
    for (auto const& icapability : icapability_map) {
        std::string cname = icapability.second.first->getName();
        nugu_dbg("'%s' capability initializing...", cname.c_str());
        icapability.second.first->initialize();
        nugu_dbg("'%s' capability initialized", cname.c_str());
    }

    if (listener)
        listener->onInitialized(this);

    initialized = true;

    return true;
}

void NuguClientImpl::deInitialize(void)
{
    ISystemHandler* sys_handler = dynamic_cast<ISystemHandler*>(getCapabilityHandler("System"));

    // Send a disconnect event indicating normal termination
    if (sys_handler)
        sys_handler->disconnect();

    // release capabilities
    for (auto& element : icapability_map) {
        std::string cname = element.second.first->getName();

        if (!cname.empty())
            CapabilityManagerHelper::removeCapability(cname);

        delete element.second.first;
    }

    icapability_map.clear();

    AudioRecorderManager::destroyInstance();

    // deinitialize core component
    nugu_plugin_deinitialize();

    nugu_dbg("NuguClientImpl deInitialize success.");
    initialized = false;
}

void NuguClientImpl::onStatusChanged(NetworkStatus status)
{
    if (status != NetworkStatus::CONNECTED)
        return;

    ISystemHandler* sys_handler = dynamic_cast<ISystemHandler*>(getCapabilityHandler("System"));

    if (sys_handler)
        sys_handler->synchronizeState();
}

INetworkManager* NuguClientImpl::getNetworkManager()
{
    return network_manager.get();
}

} // NuguClientKit
