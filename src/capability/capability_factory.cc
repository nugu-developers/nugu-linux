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
#include "battery_agent.hh"
#include "bluetooth_agent.hh"
#include "chips_agent.hh"
#include "display_agent.hh"
#include "extension_agent.hh"
#include "location_agent.hh"
#include "message_agent.hh"
#include "mic_agent.hh"
#include "nudge_agent.hh"
#include "phone_call_agent.hh"
#include "routine_agent.hh"
#include "session_agent.hh"
#include "sound_agent.hh"
#include "speaker_agent.hh"
#include "system_agent.hh"
#include "text_agent.hh"
#include "tts_agent.hh"
#include "utility_agent.hh"

#include "capability/capability_factory.hh"

#define TEMPLATE_EXPLICIT_INSTANTIATION(agent, handler) \
    template NUGU_API handler* CapabilityFactory::makeCapability<agent, handler>(ICapabilityListener* listener)

namespace NuguCapability {

template <typename T, typename V>
V* CapabilityFactory::makeCapability(ICapabilityListener* listener)
{
    ICapabilityInterface* instance = new T();

    if (listener)
        instance->setCapabilityListener(listener);

    return dynamic_cast<V*>(instance);
}

TEMPLATE_EXPLICIT_INSTANTIATION(ASRAgent, IASRHandler);
TEMPLATE_EXPLICIT_INSTANTIATION(TTSAgent, ITTSHandler);
TEMPLATE_EXPLICIT_INSTANTIATION(AudioPlayerAgent, IAudioPlayerHandler);
TEMPLATE_EXPLICIT_INSTANTIATION(SystemAgent, ISystemHandler);
TEMPLATE_EXPLICIT_INSTANTIATION(TextAgent, ITextHandler);
TEMPLATE_EXPLICIT_INSTANTIATION(SpeakerAgent, ISpeakerHandler);
TEMPLATE_EXPLICIT_INSTANTIATION(MicAgent, IMicHandler);
TEMPLATE_EXPLICIT_INSTANTIATION(SoundAgent, ISoundHandler);
TEMPLATE_EXPLICIT_INSTANTIATION(SessionAgent, ISessionHandler);
TEMPLATE_EXPLICIT_INSTANTIATION(DisplayAgent, IDisplayHandler);
TEMPLATE_EXPLICIT_INSTANTIATION(UtilityAgent, IUtilityHandler);
TEMPLATE_EXPLICIT_INSTANTIATION(ExtensionAgent, IExtensionHandler);
TEMPLATE_EXPLICIT_INSTANTIATION(ChipsAgent, IChipsHandler);
TEMPLATE_EXPLICIT_INSTANTIATION(NudgeAgent, INudgeHandler);
TEMPLATE_EXPLICIT_INSTANTIATION(RoutineAgent, IRoutineHandler);
TEMPLATE_EXPLICIT_INSTANTIATION(PhoneCallAgent, IPhoneCallHandler);
TEMPLATE_EXPLICIT_INSTANTIATION(MessageAgent, IMessageHandler);
TEMPLATE_EXPLICIT_INSTANTIATION(BluetoothAgent, IBluetoothHandler);
TEMPLATE_EXPLICIT_INSTANTIATION(LocationAgent, ILocationHandler);
TEMPLATE_EXPLICIT_INSTANTIATION(BatteryAgent, IBatteryHandler);

} // NuguCapability
