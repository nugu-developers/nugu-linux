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
#include "capability/system_interface.hh"

#include "nugu_client_impl.hh"

namespace NuguClientKit {

using namespace NuguCore;
using namespace NuguCapability;

NuguClientImpl::NuguClientImpl()
{
    nugu_equeue_initialize();

    nugu_core_container = std::unique_ptr<NuguCoreContainer>(new NuguCoreContainer());

    network_manager = std::unique_ptr<INetworkManager>(nugu_core_container->createNetworkManager());
    network_manager->addListener(this);
}

NuguClientImpl::~NuguClientImpl()
{
    nugu_core_container->destroyInstance();

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

void NuguClientImpl::setWakeupWord(const std::string& wakeup_word)
{
    if (!wakeup_word.empty())
        nugu_core_container->setWakeupWord(wakeup_word);
}

void NuguClientImpl::registerCapability(ICapabilityInterface* capability)
{
    if (!capability) {
        nugu_warn("The Capability instance is not exist.");
        return;
    }

    std::string cname = capability->getName();

    if (cname.empty()) {
        nugu_warn("The Capability name is not set.");
        return;
    }

    icapability_map.emplace(cname, capability);
}

ICapabilityInterface* NuguClientImpl::getCapabilityHandler(const std::string& cname)
{
    if (icapability_map.empty()) {
        nugu_error("No Capability exist");
        return nullptr;
    }

    if (icapability_map.find(cname) != icapability_map.end())
        return icapability_map.at(cname);

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
    for (auto const& capability : icapability_map) {
        if (capability.first.empty() || !capability.second)
            continue;

        // set NuguCoreContainer
        capability.second->setNuguCoreContainer(getNuguCoreContainer());

        // add capability to CapabilityManager
        nugu_core_container->addCapability(capability.first, capability.second);
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
        nugu_error("Fail to load nugu_plugin");
    }

    nugu_plugin_initialize();

    nugu_core_container->createAudioRecorderManager();

    if (icapability_map.empty())
        create();

    // initialize capability
    for (auto const& capability : icapability_map) {
        std::string cname = capability.second->getName();
        nugu_dbg("'%s' capability initializing...", cname.c_str());

        capability.second->initialize();
        nugu_dbg("'%s' capability initialized", cname.c_str());
    }

    if (listener)
        listener->onInitialized(this);

    initialized = true;

    return true;
}

void NuguClientImpl::deInitialize(void)
{
    // release capabilities
    for (auto& capability : icapability_map) {
        std::string cname = capability.second->getName();
        nugu_dbg("'%s' capability de-initializing...", cname.c_str());

        if (!cname.empty())
            nugu_core_container->removeCapability(cname);

        capability.second->deInitialize();
        nugu_dbg("'%s' capability de-initialized", cname.c_str());
    }

    icapability_map.clear();

    nugu_core_container->destoryAudioRecorderManager();

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

void NuguClientImpl::onEventResult(const char* msg_id, bool success, int code)
{
    nugu_core_container->getCapabilityHelper()->notifyEventResult(msg_id, success, code);
}

INetworkManager* NuguClientImpl::getNetworkManager()
{
    return network_manager.get();
}

} // NuguClientKit
