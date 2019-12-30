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

#include "capability/asr_agent.hh"
#include "capability/audio_player_agent.hh"
#include "capability/delegation_agent.hh"
#include "capability/display_agent.hh"
#include "capability/extension_agent.hh"
#include "capability/location_agent.hh"
#include "capability/system_agent.hh"
#include "capability/text_agent.hh"
#include "capability/tts_agent.hh"

#include "network_manager.hh"
#include "wakeup_handler.hh"

#include "capability_creator.hh"

namespace NuguCore {

// for restricting access to only this file
namespace {
    template <class T>
    ICapabilityInterface* create()
    {
        return new T;
    }
}

using CapabilityElement = CapabilityCreator::Element;

const std::list<CapabilityElement>& CapabilityCreator::getCapabilityList()
{
    static std::list<CapabilityElement> CAPABILITY_LIST {
    CapabilityElement { CapabilityType::ASR, true, &create<ASRAgent> },
    CapabilityElement { CapabilityType::TTS, true, &create<TTSAgent> },
    CapabilityElement { CapabilityType::AudioPlayer, true, &create<AudioPlayerAgent> },
    CapabilityElement { CapabilityType::System, true, &create<SystemAgent> },
    CapabilityElement { CapabilityType::Display, false, &create<DisplayAgent> },
    CapabilityElement { CapabilityType::Extension, false, &create<ExtensionAgent> },
    CapabilityElement { CapabilityType::Text, false, &create<TextAgent> },
    CapabilityElement { CapabilityType::Delegation, false, &create<DelegationAgent> },
    CapabilityElement { CapabilityType::Location, false, &create<LocationAgent> }
    };

    return CAPABILITY_LIST;
}

IWakeupHandler* CapabilityCreator::createWakeupHandler()
{
    return new WakeupHandler();
}

INetworkManager* CapabilityCreator::createNetworkManager()
{
    return new NetworkManager();
}

} // NuguCore
