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

#include "asr_agent.hh"
#include "audio_player_agent.hh"
#include "delegation_agent.hh"
#include "display_agent.hh"
#include "extension_agent.hh"
#include "location_agent.hh"
#include "mic_agent.hh"
#include "speaker_agent.hh"
#include "system_agent.hh"
#include "text_agent.hh"
#include "tts_agent.hh"

#include "capability/capability_factory.hh"

namespace NuguCapability {

template <typename T, typename V, typename... Ts>
V* CapabilityFactory::makeCapability(Ts&&... params)
{
    return dynamic_cast<V*>(new T(std::forward<Ts>(params)...));
}

template IASRHandler* CapabilityFactory::makeCapability<ASRAgent, IASRHandler>();
template ITTSHandler* CapabilityFactory::makeCapability<TTSAgent, ITTSHandler>();
template IAudioPlayerHandler* CapabilityFactory::makeCapability<AudioPlayerAgent, IAudioPlayerHandler>();
template ISystemHandler* CapabilityFactory::makeCapability<SystemAgent, ISystemHandler>();
template IDisplayHandler* CapabilityFactory::makeCapability<DisplayAgent, IDisplayHandler>();
template IExtensionHandler* CapabilityFactory::makeCapability<ExtensionAgent, IExtensionHandler>();
template ITextHandler* CapabilityFactory::makeCapability<TextAgent, ITextHandler>();
template IDelegationHandler* CapabilityFactory::makeCapability<DelegationAgent, IDelegationHandler>();
template ICapabilityInterface* CapabilityFactory::makeCapability<LocationAgent, ICapabilityInterface>();
template ISpeakerHandler* CapabilityFactory::makeCapability<SpeakerAgent, ISpeakerHandler>();
template IMicHandler* CapabilityFactory::makeCapability<MicAgent, IMicHandler>();

} // NuguCapability
