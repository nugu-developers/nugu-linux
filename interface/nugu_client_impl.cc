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

#include <interface/capability/system_interface.hh>
#include <interface/nugu_configuration.hh>

#include "capability_creator.hh"
#include "capability_manager_helper.hh"
#include "nugu_client_impl.hh"
#include "nugu_config.h"
#include "nugu_equeue.h"
#include "nugu_log.h"
#include "nugu_plugin.h"
#include "media_player.hh"
#include "audio_recorder_manager.hh"

namespace NuguClientKit {

using namespace NuguCore;

NuguClientImpl::NuguClientImpl()
{
    nugu_config_initialize();
    nugu_equeue_initialize();
    setDefaultConfigs();

    network_manager = std::unique_ptr<INetworkManager>(CapabilityCreator::createNetworkManager());
    network_manager->addListener(this);
}

NuguClientImpl::~NuguClientImpl()
{
    CapabilityManagerHelper::destroyInstance();
    nugu_equeue_deinitialize();
    nugu_config_deinitialize();
}

void NuguClientImpl::setConfig(const std::string& key, const std::string& value)
{
    if (config_env_map.find(key) == config_env_map.end()) {
        config_map[key] = value;
        nugu_config_set(key.c_str(), value.c_str());
    }
}

void NuguClientImpl::setConfigs(std::map<std::string, std::string>& cfgs)
{
    if (cfgs.empty())
        return;

    for (auto config : cfgs)
        config_map[config.first] = config.second;

    for (auto config : config_env_map)
        config_map[config.first] = config.second;

    for (auto config : config_map)
        nugu_config_set(config.first.c_str(), config.second.c_str());
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

void NuguClientImpl::setDefaultConfigs(void)
{
    const std::map<std::string, std::string> default_config_map = NuguConfig::getDefaultValues();

    for (auto config : default_config_map)
        config_map[config.first] = config.second;

    for (auto config : config_env_map)
        config_map[config.first] = config.second;

    for (auto config : config_map)
        nugu_config_set(config.first.c_str(), config.second.c_str());
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
     *  - icapability_map[CAPBILITY.type].first : ICapabilityInterface instance
     *  - icapability_map[CAPBILITY.type].second : ICapabilityListener instance
     */
    for (auto const& CAPBILITY : CapabilityCreator::CAPABILITY_LIST) {
        // if user didn't add and it's not a required agent, skip to create
        if (icapability_map.find(CAPBILITY.type) == icapability_map.end() && !CAPBILITY.is_default)
            continue;

        if (!icapability_map[CAPBILITY.type].first)
            try {
                icapability_map[CAPBILITY.type].first = CAPBILITY.creator();
            } catch (std::exception& exp) {
                nugu_error(exp.what());
            }

        if (icapability_map[CAPBILITY.type].first) {
            // add general observer
            if (listener)
                icapability_map[CAPBILITY.type].first->registerObserver(listener);

            // set capability listener & handler each other
            if (icapability_map[CAPBILITY.type].second)
                icapability_map[CAPBILITY.type].first->setCapabilityListener(icapability_map[CAPBILITY.type].second);

            std::string cname = icapability_map[CAPBILITY.type].first->getTypeName(CAPBILITY.type);

            if (!cname.empty())
                CapabilityManagerHelper::addCapability(cname, icapability_map[CAPBILITY.type].first);
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

    if (nugu_plugin_load_directory(CONFIG_PLUGIN_PATH) < 0) {
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

    nugu_dbg("NuguClientImpl deInitialize succeess.");
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
