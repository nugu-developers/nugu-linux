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
#include "display_agent.hh"
#include "mic_agent.hh"
#include "session_agent.hh"
#include "sound_agent.hh"
#include "speaker_agent.hh"
#include "system_agent.hh"
#include "text_agent.hh"
#include "tts_agent.hh"

#include "capability/capability_factory.hh"

namespace NuguCapability {

template <typename T, typename V>
V* CapabilityFactory::makeCapability(ICapabilityListener* listener)
{
    ICapabilityInterface* instance = new T();

    if (listener)
        instance->setCapabilityListener(listener);

    return dynamic_cast<V*>(instance);
}

template IASRHandler* CapabilityFactory::makeCapability<ASRAgent, IASRHandler>(ICapabilityListener* listener);
template ITTSHandler* CapabilityFactory::makeCapability<TTSAgent, ITTSHandler>(ICapabilityListener* listener);
template IAudioPlayerHandler* CapabilityFactory::makeCapability<AudioPlayerAgent, IAudioPlayerHandler>(ICapabilityListener* listener);
template ISystemHandler* CapabilityFactory::makeCapability<SystemAgent, ISystemHandler>(ICapabilityListener* listener);
template ITextHandler* CapabilityFactory::makeCapability<TextAgent, ITextHandler>(ICapabilityListener* listener);
template ISpeakerHandler* CapabilityFactory::makeCapability<SpeakerAgent, ISpeakerHandler>(ICapabilityListener* listener);
template IMicHandler* CapabilityFactory::makeCapability<MicAgent, IMicHandler>(ICapabilityListener* listener);
template ISoundHandler* CapabilityFactory::makeCapability<SoundAgent, ISoundHandler>(ICapabilityListener* listener);
template ISessionHandler* CapabilityFactory::makeCapability<SessionAgent, ISessionHandler>(ICapabilityListener* listener);
template IDisplayHandler* CapabilityFactory::makeCapability<DisplayAgent, IDisplayHandler>(ICapabilityListener* listener);

} // NuguCapability
