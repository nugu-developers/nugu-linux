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

#include <capability/utility_interface.hh>

#include "audio_player_listener.hh"
#include "display_listener.hh"
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

    template <typename T>
    T* getCapability(const std::string& capability_name)
    {
        return dynamic_cast<T*>(factories[capability_name]());
    }

    SpeechOperator* getSpeechOperator();

private:
    void composeCapabilityFactory();
    SpeakerInfo makeSpeakerInfo(SpeakerType type, int muted = NUGU_SPEAKER_UNABLE_CONTROL, bool can_control = false);

    // Capability instance
    std::shared_ptr<ISystemHandler> system_handler = nullptr;
    std::shared_ptr<IASRHandler> asr_handler = nullptr;
    std::shared_ptr<ITTSHandler> tts_handler = nullptr;
    std::shared_ptr<IAudioPlayerHandler> audio_player_handler = nullptr;
    std::shared_ptr<ITextHandler> text_handler = nullptr;
    std::shared_ptr<ISpeakerHandler> speaker_handler = nullptr;
    std::shared_ptr<IMicHandler> mic_handler = nullptr;
    std::shared_ptr<ISoundHandler> sound_handler = nullptr;
    std::shared_ptr<ISessionHandler> session_handler = nullptr;
    std::shared_ptr<IDisplayHandler> display_handler = nullptr;
    std::shared_ptr<IUtilityHandler> utility_handler = nullptr;

    // Capability listener
    std::shared_ptr<SpeechOperator> speech_operator = nullptr;
    std::shared_ptr<TTSListener> tts_listener = nullptr;
    std::shared_ptr<AudioPlayerListener> aplayer_listener = nullptr;
    std::shared_ptr<SystemListener> system_listener = nullptr;
    std::shared_ptr<TextListener> text_listener = nullptr;
    std::shared_ptr<SpeakerListener> speaker_listener = nullptr;
    std::shared_ptr<MicListener> mic_listener = nullptr;
    std::shared_ptr<SoundListener> sound_listener = nullptr;
    std::shared_ptr<SessionListener> session_listener = nullptr;
    std::shared_ptr<DisplayListener> display_listener = nullptr;

    std::map<std::string, std::function<ICapabilityInterface*()>> factories;
};

#endif /* __NUGU_CAPABILITY_COLLECTION_H__ */
