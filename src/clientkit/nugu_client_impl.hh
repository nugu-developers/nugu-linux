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

#ifndef __NUGU_CLIENT_IMPL_H__
#define __NUGU_CLIENT_IMPL_H__

#include <map>
#include <memory>

#include "clientkit/capability_interface.hh"
#include "clientkit/dialog_ux_state_aggregator.hh"
#include "clientkit/speech_recognizer_aggregator.hh"
#include "core/nugu_core_container.hh"

namespace NuguClientKit {

using namespace NuguCore;

class NuguClientImpl {
public:
    NuguClientImpl();
    virtual ~NuguClientImpl();

    void setWakeupWord(const std::string& wakeup_word);
    void setWakeupModel(const WakeupModelFile& model_file);
    void registerCapability(ICapabilityInterface* capability);
    void addDialogUXStateListener(IDialogUXStateAggregatorListener* listener);
    void removeDialogUXStateListener(IDialogUXStateAggregatorListener* listener);
    int create(void);
    bool loadPlugins(const std::string& path = "");
    void unloadPlugins(void);
    bool initialize(void);
    void deInitialize(void);

    ICapabilityInterface* getCapabilityHandler(const std::string& cname);
    INuguCoreContainer* getNuguCoreContainer();
    INetworkManager* getNetworkManager();
    IFocusManager* getFocusManager();
    ISpeechRecognizerAggregator* getSpeechRecognizerAggregator();

private:
    int createCapabilities(void);
    void registerDialogUXStateAggregator();
    void unregisterDialogUXStateAggregator();
    void setupSpeechRecognizerAggregator();

    template <typename H>
    void addDialogUXStateAggregator(std::string&& cname);

    template <typename H>
    void removeDialogUXStateAggregator(std::string&& cname);

    using CapabilityMap = std::map<std::string, ICapabilityInterface*>;

    CapabilityMap icapability_map;
    std::unique_ptr<INetworkManager> network_manager = nullptr;
    std::unique_ptr<NuguCoreContainer> nugu_core_container = nullptr;
    std::unique_ptr<DialogUXStateAggregator> dialog_ux_state_aggregator = nullptr;
    std::unique_ptr<SpeechRecognizerAggregator> speech_recognizer_aggregator = nullptr;
    ICapabilityHelper* capa_helper = nullptr;
    bool initialized = false;
    bool plugin_loaded = false;
    WakeupModelFile wakeup_model_file {};
};

} // NuguClientKit

#endif /* __NUGU_CLIENT_IMPL_H__ */
