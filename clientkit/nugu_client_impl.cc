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

#include "interface/capability/system_interface.hh"

#include "core/audio_recorder_manager.hh"
#include "core/capability_creator.hh"
#include "core/capability_manager_helper.hh"
#include "core/media_player.hh"

#include "nugu_client_impl.hh"

namespace NuguClientKit {

using namespace NuguCore;

NuguClientImpl::NuguClientImpl()
{
    nugu_equeue_initialize();
    NuguConfig::loadDefaultValue();

    network_manager = std::unique_ptr<INetworkManager>(CapabilityCreator::createNetworkManager());
    network_manager->addListener(this);
}

NuguClientImpl::~NuguClientImpl()
{
    CapabilityManagerHelper::destroyInstance();
    nugu_equeue_deinitialize();
}

void NuguClientImpl::setConfig(NuguConfig::Key key, const std::string& value)
{
    NuguConfig::setValue(key, value);
}

void NuguClientImpl::setListener(INuguClientListener* listener)
{
    this->listener = listener;
}

INuguClientListener* NuguClientImpl::getListener()
{
    return listener;
}

void NuguClientImpl::registerCapability(const CapabilityType ctype, std::pair<ICapabilityInterface*, ICapabilityListener*> capability)
{
    icapability_map[ctype] = capability;
}

ICapabilityInterface* NuguClientImpl::getCapabilityHandler(const CapabilityType ctype)
{
    if (icapability_map.empty()) {
        nugu_error("No Capability exist");
        return nullptr;
    }

    if (icapability_map.find(ctype) != icapability_map.end())
        return icapability_map[ctype].first;

    return nullptr;
}

IWakeupHandler* NuguClientImpl::getWakeupHandler()
{
    if (!wakeup_handler)
        wakeup_handler = CapabilityCreator::createWakeupHandler();

    return wakeup_handler;
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
    for (auto const& CAPABILITY : CapabilityCreator::getCapabilityList()) {
        // if user didn't add and it's not a required agent, skip to create
        if (icapability_map.find(CAPABILITY.type) == icapability_map.end() && !CAPABILITY.is_default)
            continue;

        if (!icapability_map[CAPABILITY.type].first)
            try {
                icapability_map[CAPABILITY.type].first = CAPABILITY.creator();
            } catch (std::exception& exp) {
                nugu_error(exp.what());
            }

        if (icapability_map[CAPABILITY.type].first) {
            // add general observer
            if (listener)
                icapability_map[CAPABILITY.type].first->registerObserver(listener);

            // set capability listener & handler each other
            if (icapability_map[CAPABILITY.type].second)
                icapability_map[CAPABILITY.type].first->setCapabilityListener(icapability_map[CAPABILITY.type].second);

            std::string cname = icapability_map[CAPABILITY.type].first->getTypeName(CAPABILITY.type);

            if (!cname.empty())
                CapabilityManagerHelper::addCapability(cname, icapability_map[CAPABILITY.type].first);
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
        std::string cname = icapability.second.first->getTypeName(icapability.second.first->getType());
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
    ISystemHandler* sys_handler = dynamic_cast<ISystemHandler*>(getCapabilityHandler(CapabilityType::System));

    // Send a disconnect event indicating normal termination
    if (sys_handler)
        sys_handler->disconnect();

    // release capabilities
    for (auto& element : icapability_map) {
        std::string cname = element.second.first->getTypeName(element.first);

        if (!cname.empty())
            CapabilityManagerHelper::removeCapability(cname);

        delete element.second.first;
    }

    icapability_map.clear();

    if (wakeup_handler) {
        delete wakeup_handler;
        wakeup_handler = nullptr;
    }

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

    ISystemHandler* sys_handler = dynamic_cast<ISystemHandler*>(getCapabilityHandler(CapabilityType::System));

    if (sys_handler)
        sys_handler->synchronizeState();
}

INetworkManager* NuguClientImpl::getNetworkManager()
{
    return network_manager.get();
}

IMediaPlayer* NuguClientImpl::createMediaPlayer()
{
    if (!initialized) {
        nugu_info("NUGU media player could use after initialize");
        return nullptr;
    }
    return new MediaPlayer();
}

} // NuguClientKit
