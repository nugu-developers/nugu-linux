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
#include "nugu_log.h"
#include "nugu_plugin.h"

namespace NuguClientKit {

using namespace NuguCore;

NuguClientImpl::NuguClientImpl()
{
    nugu_config_initialize();
    network_manager = std::unique_ptr<INetworkManager>(CapabilityCreator::createNetworkManager());
    network_manager->addListener(this);
}

NuguClientImpl::~NuguClientImpl()
{
    CapabilityManagerHelper::destroyInstance();
    nugu_config_deinitialize();
}

void NuguClientImpl::setAccessToken(std::string access_token)
{
    if (access_token.empty()) {
        nugu_error("There are no access token");
        return;
    }

    nugu_config_set(NUGU_CONFIG_KEY_TOKEN, access_token.c_str());
}

void NuguClientImpl::setConfig(const std::string& key, const std::string& value)
{
    config_map[key] = value;
}

void NuguClientImpl::setConfigs(std::map<std::string, std::string> cfgs)
{
    if (cfgs.empty())
        return;

    config_map.insert(cfgs.begin(), cfgs.end());
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

ICapabilityHandler* NuguClientImpl::getCapabilityHandler(const CapabilityType ctype)
{
    if (icapability_map.empty()) {
        nugu_error("No Capability exist");
        return nullptr;
    }

    if (icapability_map.find(ctype) != icapability_map.end())
        return dynamic_cast<ICapabilityHandler*>(icapability_map[ctype].first);

    return nullptr;
}

IWakeupHandler* NuguClientImpl::getWakeupHandler()
{
    if (!wakeup_handler)
        wakeup_handler = CapabilityCreator::createWakeupHandler();

    return wakeup_handler;
}

void NuguClientImpl::setConfiguration(void)
{
    const std::map<std::string, std::string> default_config_map = NuguConfig::getDefaultValues(config_map);
    config_map.insert(default_config_map.cbegin(), default_config_map.cend());

    for (auto config : config_map) {
        nugu_config_set(config.first.c_str(), config.second.c_str());
    }
}

int NuguClientImpl::create(void)
{
    setConfiguration();

    if (createCapabilities() <= 0) {
        nugu_error("No Capabilities are created");
        return -1;
    }

    return 0;
}

int NuguClientImpl::createCapabilities(void)
{
    // [description]
    // icapability_map[CAPBILITY].first : ICapabilityInterface instance
    // icapability_map[CAPBILITY].second : ICapabilityListener instance

    for (auto const& CAPBILITY : CapabilityCreator::CAPABILITY_LIST) {
        if (icapability_map.find(CAPBILITY) == icapability_map.end() || !icapability_map[CAPBILITY].first) {
            try {
                icapability_map[CAPBILITY].first = CapabilityCreator::createCapability(CAPBILITY);
            } catch (std::exception& exp) {
                nugu_error(exp.what());
            }
        }

        if (icapability_map[CAPBILITY].first) {
            // add general observer
            if (listener)
                icapability_map[CAPBILITY].first->registerObserver(listener);

            // set capability listener & handler each other
            if (icapability_map[CAPBILITY].second) {
                icapability_map[CAPBILITY].first->setCapabilityListener(icapability_map[CAPBILITY].second);
                icapability_map[CAPBILITY].second->registerCapabilityHandler(dynamic_cast<ICapabilityHandler*>(icapability_map[CAPBILITY].first));
            }

            std::string cname = icapability_map[CAPBILITY].first->getTypeName(CAPBILITY);

            if (!cname.empty())
                CapabilityManagerHelper::addCapability(cname, icapability_map[CAPBILITY].first);
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

    if (icapability_map.empty())
        create();

    // initialize capability
    for (auto const& icapability : icapability_map) {
        icapability.second.first->initialize();
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

    // deinitialize core component
    nugu_plugin_deinitialize();

    nugu_dbg("NuguClientImpl deInitialize succeess.");
    initialized = false;
}

void NuguClientImpl::onConnected()
{
    ISystemHandler* sys_handler = dynamic_cast<ISystemHandler*>(getCapabilityHandler(CapabilityType::System));

    if (sys_handler)
        sys_handler->synchronizeState();
}

INetworkManager* NuguClientImpl::getNetworkManager()
{
    return network_manager.get();
}
} // NuguClientKit
