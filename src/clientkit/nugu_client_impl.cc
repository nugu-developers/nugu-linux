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
#include "base/nugu_prof.h"

#include "nugu_client_impl.hh"

namespace NuguClientKit {

using namespace NuguCore;

NuguClientImpl::NuguClientImpl()
    : nugu_core_container(std::unique_ptr<NuguCoreContainer>(new NuguCoreContainer()))
    , speech_recognizer_aggregator(std::unique_ptr<SpeechRecognizerAggregator>(new SpeechRecognizerAggregator()))
{
    nugu_info("NUGU SDK v%s", NUGU_VERSION);

    nugu_prof_clear();
    nugu_prof_mark(NUGU_PROF_TYPE_SDK_CREATED);

    nugu_equeue_initialize();

    network_manager = std::unique_ptr<INetworkManager>(nugu_core_container->createNetworkManager());
    network_manager->addListener(nugu_core_container->getNetworkManagerListener());

    capa_helper = nugu_core_container->getCapabilityHelper();

    auto session_manager(capa_helper->getSessionManager());
    dialog_ux_state_aggregator = std::unique_ptr<DialogUXStateAggregator>(new DialogUXStateAggregator(session_manager));
}

NuguClientImpl::~NuguClientImpl()
{
    nugu_core_container->destroyAudioRecorderManager();
    nugu_core_container->destroyInstance();

    if (plugin_loaded)
        unloadPlugins();

    nugu_equeue_deinitialize();
}

void NuguClientImpl::setWakeupWord(const std::string& wakeup_word)
{
    if (!wakeup_word.empty())
        nugu_core_container->setWakeupWord(wakeup_word);
}

void NuguClientImpl::setWakeupModel(const WakeupModelFile& model_file)
{
    wakeup_model_file = model_file;
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

void NuguClientImpl::addDialogUXStateListener(IDialogUXStateAggregatorListener* listener)
{
    if (listener)
        dialog_ux_state_aggregator->addListener(listener);
}

void NuguClientImpl::removeDialogUXStateListener(IDialogUXStateAggregatorListener* listener)
{
    if (listener)
        dialog_ux_state_aggregator->removeListener(listener);
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

INetworkManager* NuguClientImpl::getNetworkManager()
{
    return network_manager.get();
}

IFocusManager* NuguClientImpl::getFocusManager()
{
    return capa_helper->getFocusManager();
}

ISpeechRecognizerAggregator* NuguClientImpl::getSpeechRecognizerAggregator()
{
    return speech_recognizer_aggregator.get();
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

bool NuguClientImpl::loadPlugins(const std::string& path)
{
    int result;

    if (plugin_loaded) {
        nugu_info("plugin already loaded.");
        return true;
    }

    nugu_prof_mark(NUGU_PROF_TYPE_SDK_PLUGIN_INIT_START);

    if (path.size() > 0) {
        nugu_dbg("load plugins from directory(%s)", path.c_str());
        result = nugu_plugin_load_directory(path.c_str());
    } else {
        nugu_dbg("load plugins from default directory(%s)", NUGU_PLUGIN_DIR);
        result = nugu_plugin_load_directory(NUGU_PLUGIN_DIR);
    }

    if (result < 0) {
        nugu_error("Fail to load directory");
        return false;
    }

    plugin_loaded = true;

    if (result == 0) {
        nugu_info("no plugins");
        nugu_prof_mark(NUGU_PROF_TYPE_SDK_PLUGIN_INIT_DONE);
        return true;
    }

    result = nugu_plugin_initialize();
    nugu_info("loaded %d plugins", result);
    nugu_prof_mark(NUGU_PROF_TYPE_SDK_PLUGIN_INIT_DONE);

    return true;
}

void NuguClientImpl::unloadPlugins(void)
{
    if (!plugin_loaded) {
        nugu_info("plugin already unloaded");
        return;
    }

    nugu_plugin_deinitialize();

    plugin_loaded = false;
}

bool NuguClientImpl::initialize(void)
{
    if (initialized) {
        nugu_info("It's already initialized.");
        return true;
    }

    if (!plugin_loaded)
        loadPlugins();

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

    registerDialogUXStateAggregator();
    setupSpeechRecognizerAggregator();

    nugu_info("initialized %d capabilities", icapability_map.size());

    nugu_prof_mark(NUGU_PROF_TYPE_SDK_INIT_DONE);

    initialized = true;

    return true;
}

void NuguClientImpl::deInitialize(void)
{
    if (!initialized) {
        nugu_info("already de-initialized");
        return;
    }

    // deinitialize capabilities
    for (auto& capability : icapability_map) {
        std::string cname = capability.second->getName();
        nugu_dbg("'%s' capability de-initializing...", cname.c_str());

        capability.second->deInitialize();

        nugu_dbg("'%s' capability de-initialized", cname.c_str());
    }

    unregisterDialogUXStateAggregator();
    speech_recognizer_aggregator->reset();
    nugu_core_container->resetInstance();

    nugu_dbg("NuguClientImpl deInitialize success.");

    initialized = false;
}

void NuguClientImpl::registerDialogUXStateAggregator()
{
    addDialogUXStateAggregator<IASRHandler*>("ASR");
    addDialogUXStateAggregator<ITTSHandler*>("TTS");
    addDialogUXStateAggregator<IChipsHandler*>("Chips");

    capa_helper->getInteractionControlManager()->addListener(dialog_ux_state_aggregator.get());
}

void NuguClientImpl::unregisterDialogUXStateAggregator()
{
    removeDialogUXStateAggregator<IASRHandler*>("ASR");
    removeDialogUXStateAggregator<ITTSHandler*>("TTS");
    removeDialogUXStateAggregator<IChipsHandler*>("Chips");

    capa_helper->getInteractionControlManager()->removeListener(dialog_ux_state_aggregator.get());
}

void NuguClientImpl::setupSpeechRecognizerAggregator()
{
    // setup only if WakeupHandler is not exist as considering user setup case
    if (!wakeup_model_file.net.empty() && !wakeup_model_file.search.empty() && !speech_recognizer_aggregator->getWakeupHandler())
        speech_recognizer_aggregator->setWakeupHandler(std::shared_ptr<IWakeupHandler>(nugu_core_container->createWakeupHandler(wakeup_model_file)));

    speech_recognizer_aggregator->setASRHandler(dynamic_cast<IASRHandler*>(icapability_map.at("ASR")));
}

template <typename H>
void NuguClientImpl::addDialogUXStateAggregator(std::string&& cname)
{
    try {
        auto handler(dynamic_cast<H>(icapability_map.at(cname)));

        if (handler)
            handler->addListener(dialog_ux_state_aggregator.get());
    } catch (std::out_of_range& exception) {
        // skip silently
    }
}

template <typename H>
void NuguClientImpl::removeDialogUXStateAggregator(std::string&& cname)
{
    try {
        auto handler(dynamic_cast<H>(icapability_map.at(cname)));

        if (handler)
            handler->removeListener(dialog_ux_state_aggregator.get());
    } catch (std::out_of_range& exception) {
        // skip silently
    }
}

} // NuguClientKit
