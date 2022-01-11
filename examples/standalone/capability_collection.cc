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
    factories = {
        { "System", [&] {
             return setupCapabilityInstance<SystemAgent>("System", system_listener, system_handler);
         } },
        { "ASR", [&] {
             return setupCapabilityInstance<ASRAgent>("ASR", speech_operator, asr_handler);
         } },
        { "TTS", [&] {
             return setupCapabilityInstance<TTSAgent>("TTS", tts_listener, tts_handler);
         } },
        { "AudioPlayer", [&] {
             return setupCapabilityInstance<AudioPlayerAgent>("AudioPlayer", aplayer_listener, audio_player_handler, [&] {
                 audio_player_handler->setDisplayListener(aplayer_listener.get());
             });
         } },
        { "Text", [&] {
             return setupCapabilityInstance<TextAgent>("Text", text_listener, text_handler);
         } },
        { "Speaker", [&] {
             return setupCapabilityInstance<SpeakerAgent>("Speaker", speaker_listener, speaker_handler, [&] {
                 composeSpeakerInterface();
             });
         } },
        { "Mic", [&] {
             return setupCapabilityInstance<MicAgent>("Mic", mic_listener, mic_handler, [&] {
                 mic_handler->enable();
             });
         } },
        { "Sound", [&] {
             return setupCapabilityInstance<SoundAgent>("Sound", sound_listener, sound_handler);
         } },
        { "Session", [&] {
             return setupCapabilityInstance<SessionAgent>("Session", session_listener, session_handler);
         } },
        { "Display", [&] {
             return setupCapabilityInstance<DisplayAgent>("Display", display_listener, display_handler);
         } },
        { "Utility", [&] {
             return setupCapabilityInstance<UtilityAgent>("Utility", utility_handler);
         } },
        { "Extension", [&] {
             return setupCapabilityInstance<ExtensionAgent>("Extension", extension_listener, extension_handler);
         } },
        { "Chips", [&] {
             return setupCapabilityInstance<ChipsAgent>("Chips", chips_listener, chips_handler);
         } },
        { "Nudge", [&] {
             return setupCapabilityInstance<NudgeAgent>("Nudge", nudge_handler);
         } },
        { "Routine", [&] {
             return setupCapabilityInstance<RoutineAgent>("Routine", routine_handler);
         } },
        { "Bluetooth", [&] {
             return setupCapabilityInstance<BluetoothAgent>("Bluetooth", bluetooth_listener, bluetooth_handler);
         } },
        { "Location", [&] {
             return setupCapabilityInstance<LocationAgent>("Location", location_listener, location_handler);
         } }
    };
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
        if (tts_handler && !tts_handler->setMute(mute))
            return false;

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

template <typename A, typename HT>
ICapabilityInterface* CapabilityCollection::setupCapabilityInstance(std::string&& name, std::unique_ptr<HT>& handler)
{
    if (!handler)
        handler = makeCapability<A, HT>(nullptr);

    return handler.get();
}

template <typename A, typename LT, typename HT>
ICapabilityInterface* CapabilityCollection::setupCapabilityInstance(std::string&& name, std::unique_ptr<LT>& listener, std::unique_ptr<HT>& handler, std::function<void()>&& post_action)
{
    if (!handler) {
        if (!listener)
            listener = make_unique<LT>();

        handler = makeCapability<A, HT>(listener.get());
        listener->setCapabilityHandler(handler.get());
        capability_listeners.emplace(name, listener.get());

        if (post_action)
            post_action();
    }

    return handler.get();
}
