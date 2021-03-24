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

#include <capability/capability_factory.hh>

#include "capability_collection.hh"

namespace {
template <typename T, typename V>
std::unique_ptr<V> makeCapability(ICapabilityListener* listener)
{
    return std::unique_ptr<V>(CapabilityFactory::makeCapability<T, V>(listener));
}

template <typename T, typename... Ts>
std::unique_ptr<T> make_unique(Ts&&... params)
{
    return std::unique_ptr<T>(new T(std::forward<Ts>(params)...));
}
}

CapabilityCollection::CapabilityCollection()
    : speech_operator(make_unique<SpeechOperator>())
{
    composeCapabilityFactory();
}

SpeechOperator* CapabilityCollection::getSpeechOperator()
{
    return speech_operator.get();
}

void CapabilityCollection::composeCapabilityFactory()
{
    factories.emplace("System", [&]() {
        if (!system_handler) {
            system_listener = make_unique<SystemListener>();
            system_handler = makeCapability<SystemAgent, ISystemHandler>(system_listener.get());
        }

        return system_handler.get();
    });
    factories.emplace("ASR", [&]() {
        if (!asr_handler) {
            asr_handler = makeCapability<ASRAgent, IASRHandler>(speech_operator->getASRListener());
            speech_operator->setASRHandler(asr_handler.get());
        }

        return asr_handler.get();
    });
    factories.emplace("TTS", [&]() {
        if (!tts_handler) {
            tts_listener = make_unique<TTSListener>();
            tts_handler = makeCapability<TTSAgent, ITTSHandler>(tts_listener.get());
        }

        return tts_handler.get();
    });
    factories.emplace("AudioPlayer", [&]() {
        if (!audio_player_handler) {
            aplayer_listener = make_unique<AudioPlayerListener>();
            audio_player_handler = makeCapability<AudioPlayerAgent, IAudioPlayerHandler>(aplayer_listener.get());
        }

        return audio_player_handler.get();
    });
    factories.emplace("Text", [&]() {
        if (!text_handler) {
            text_listener = make_unique<TextListener>();
            text_handler = makeCapability<TextAgent, ITextHandler>(text_listener.get());
        }

        return text_handler.get();
    });
    factories.emplace("Speaker", [&]() {
        if (!speaker_handler) {
            speaker_listener = make_unique<SpeakerListener>();
            speaker_handler = makeCapability<SpeakerAgent, ISpeakerHandler>(speaker_listener.get());
            composeSpeakerInterface();
        }

        return speaker_handler.get();
    });
    factories.emplace("Mic", [&]() {
        if (!mic_handler) {
            mic_listener = make_unique<MicListener>();
            mic_handler = makeCapability<MicAgent, IMicHandler>(mic_listener.get());
            mic_handler->enable();
        }

        return mic_handler.get();
    });
    factories.emplace("Sound", [&]() {
        if (!sound_handler) {
            sound_listener = make_unique<SoundListener>();
            sound_handler = makeCapability<SoundAgent, ISoundHandler>(sound_listener.get());
            sound_listener->setSoundHandler(sound_handler.get());
        }

        return sound_handler.get();
    });
    factories.emplace("Session", [&]() {
        if (!session_handler) {
            session_listener = make_unique<SessionListener>();
            session_handler = makeCapability<SessionAgent, ISessionHandler>(session_listener.get());
        }

        return session_handler.get();
    });
    factories.emplace("Display", [&]() {
        if (!display_handler) {
            display_listener = make_unique<DisplayListener>();
            display_handler = makeCapability<DisplayAgent, IDisplayHandler>(display_listener.get());
            display_listener->setDisplayHandler(display_handler.get());
        }

        return display_handler.get();
    });
    factories.emplace("Utility", [&]() {
        if (!utility_handler)
            utility_handler = makeCapability<UtilityAgent, IUtilityHandler>(nullptr);

        return utility_handler.get();
    });
    factories.emplace("Extension", [&]() {
        if (!extension_handler) {
            extension_listener = make_unique<ExtensionListener>();
            extension_handler = makeCapability<ExtensionAgent, IExtensionHandler>(extension_listener.get());
            extension_listener->setExtensionHandler(extension_handler.get());
        }

        return extension_handler.get();
    });
    factories.emplace("Chips", [&]() {
        if (!chips_handler) {
            chips_listener = make_unique<ChipsListener>();
            chips_handler = makeCapability<ChipsAgent, IChipsHandler>(chips_listener.get());
        }

        return chips_handler.get();
    });
}

void CapabilityCollection::composeSpeakerInterface()
{
    if (!speaker_handler || !speaker_listener)
        return;

    // compose SpeakerInfo
    std::map<SpeakerType, SpeakerInfo> speakers {
        { SpeakerType::NUGU, makeSpeakerInfo(SpeakerType::NUGU, 0, true) },
        { SpeakerType::MUSIC, makeSpeakerInfo(SpeakerType::MUSIC) },
        { SpeakerType::RINGTON, makeSpeakerInfo(SpeakerType::RINGTON) },
        { SpeakerType::CALL, makeSpeakerInfo(SpeakerType::CALL) },
        { SpeakerType::NOTIFICATION, makeSpeakerInfo(SpeakerType::NOTIFICATION) },
        { SpeakerType::ALARM, makeSpeakerInfo(SpeakerType::ALARM) },
        { SpeakerType::VOICE_COMMAND, makeSpeakerInfo(SpeakerType::VOICE_COMMAND) },
        { SpeakerType::NAVIGATION, makeSpeakerInfo(SpeakerType::NAVIGATION) },
        { SpeakerType::SYSTEM_SOUND, makeSpeakerInfo(SpeakerType::SYSTEM_SOUND) },
    };

    speaker_handler->setSpeakerInfo(speakers);
    speaker_listener->setSpeakerHandler(speaker_handler.get());
    speaker_listener->setVolumeNuguSpeakerCallback([&](int volume) {
        if (tts_handler && !tts_handler->setVolume(volume))
            return false;
        if (audio_player_handler && !audio_player_handler->setVolume(volume))
            return false;
        return true;
    });
    speaker_listener->setMuteNuguSpeakerCallback([&](bool mute) {
        if (!tts_handler)
            return false;
        tts_handler->stopTTS();
        if (audio_player_handler && !audio_player_handler->setMute(mute))
            return false;
        return true;
    });
}

SpeakerInfo CapabilityCollection::makeSpeakerInfo(SpeakerType type, int muted, bool can_control)
{
    SpeakerInfo nugu_speaker;
    nugu_speaker.type = type;
    nugu_speaker.mute = muted;
    nugu_speaker.can_control = can_control;

    return nugu_speaker;
}
