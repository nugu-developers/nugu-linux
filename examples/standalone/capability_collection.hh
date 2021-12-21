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

#ifndef __NUGU_CAPABILITY_COLLECTION_H__
#define __NUGU_CAPABILITY_COLLECTION_H__

#include <functional>
#include <map>
#include <memory>

#include <capability/nudge_interface.hh>
#include <capability/routine_interface.hh>
#include <capability/utility_interface.hh>

#include "audio_player_listener.hh"
#include "bluetooth_listener.hh"
#include "chips_listener.hh"
#include "display_listener.hh"
#include "extension_listener.hh"
#include "mic_listener.hh"
#include "session_listener.hh"
#include "sound_listener.hh"
#include "speaker_listener.hh"
#include "speech_operator.hh"
#include "system_listener.hh"
#include "text_listener.hh"
#include "tts_listener.hh"

using namespace NuguClientKit;

class CapabilityCollection {
public:
    CapabilityCollection();
    virtual ~CapabilityCollection() = default;

    template <typename T = ICapabilityInterface>
    T* getCapability(const std::string& capability_name)
    {
        try {
            return dynamic_cast<T*>(factories.at(capability_name)());
        } catch (std::out_of_range& exception) {
            return nullptr;
        }
    }

    template <typename T = ICapabilityListener>
    T* getCapabilityListener(const std::string& capability_name)
    {
        try {
            return dynamic_cast<T*>(capability_listeners.at(capability_name));
        } catch (std::out_of_range& exception) {
            return nullptr;
        }
    }

    SpeechOperator* getSpeechOperator();

private:
    void composeCapabilityFactory();
    void composeSpeakerInterface();
    SpeakerInfo makeSpeakerInfo(SpeakerType type, int muted = NUGU_SPEAKER_UNABLE_CONTROL, bool can_control = false);

    // Capability instance
    std::unique_ptr<ISystemHandler> system_handler = nullptr;
    std::unique_ptr<IASRHandler> asr_handler = nullptr;
    std::unique_ptr<ITTSHandler> tts_handler = nullptr;
    std::unique_ptr<IAudioPlayerHandler> audio_player_handler = nullptr;
    std::unique_ptr<ITextHandler> text_handler = nullptr;
    std::unique_ptr<ISpeakerHandler> speaker_handler = nullptr;
    std::unique_ptr<IMicHandler> mic_handler = nullptr;
    std::unique_ptr<ISoundHandler> sound_handler = nullptr;
    std::unique_ptr<ISessionHandler> session_handler = nullptr;
    std::unique_ptr<IDisplayHandler> display_handler = nullptr;
    std::unique_ptr<IUtilityHandler> utility_handler = nullptr;
    std::unique_ptr<IExtensionHandler> extension_handler = nullptr;
    std::unique_ptr<IChipsHandler> chips_handler = nullptr;
    std::unique_ptr<INudgeHandler> nudge_handler = nullptr;
    std::unique_ptr<IRoutineHandler> routine_handler = nullptr;
    std::unique_ptr<IBluetoothHandler> bluetooth_handler = nullptr;

    // Capability listener
    std::unique_ptr<SpeechOperator> speech_operator = nullptr;
    std::unique_ptr<TTSListener> tts_listener = nullptr;
    std::unique_ptr<AudioPlayerListener> aplayer_listener = nullptr;
    std::unique_ptr<SystemListener> system_listener = nullptr;
    std::unique_ptr<TextListener> text_listener = nullptr;
    std::unique_ptr<SpeakerListener> speaker_listener = nullptr;
    std::unique_ptr<MicListener> mic_listener = nullptr;
    std::unique_ptr<SoundListener> sound_listener = nullptr;
    std::unique_ptr<SessionListener> session_listener = nullptr;
    std::unique_ptr<DisplayListener> display_listener = nullptr;
    std::unique_ptr<ExtensionListener> extension_listener = nullptr;
    std::unique_ptr<ChipsListener> chips_listener = nullptr;
    std::unique_ptr<BluetoothListener> bluetooth_listener = nullptr;

    std::map<std::string, std::function<ICapabilityInterface*()>> factories;
    std::map<std::string, ICapabilityListener*> capability_listeners;
};

#endif /* __NUGU_CAPABILITY_COLLECTION_H__ */
