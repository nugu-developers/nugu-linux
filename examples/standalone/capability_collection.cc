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
std::shared_ptr<V> makeCapability(ICapabilityListener* listener)
{
    return std::shared_ptr<V>(CapabilityFactory::makeCapability<T, V>(listener));
}
}

CapabilityCollection::CapabilityCollection()
    : speech_operator(std::make_shared<SpeechOperator>())
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
            system_listener = std::make_shared<SystemListener>();
            system_handler = makeCapability<SystemAgent, ISystemHandler>(system_listener.get());
        }

        return system_handler.get();
    });
    factories.emplace("ASR", [&]() {
        if (!asr_handler)
            asr_handler = makeCapability<ASRAgent, IASRHandler>(speech_operator->getASRListener());

        speech_operator->setASRHandler(asr_handler.get());

        return asr_handler.get();
    });
    factories.emplace("TTS", [&]() {
        if (!tts_handler) {
            tts_listener = std::make_shared<TTSListener>();
            tts_handler = makeCapability<TTSAgent, ITTSHandler>(tts_listener.get());
        }

        return tts_handler.get();
    });
    factories.emplace("AudioPlayer", [&]() {
        if (!audio_player_handler) {
            aplayer_listener = std::make_shared<AudioPlayerListener>();
            audio_player_handler = makeCapability<AudioPlayerAgent, IAudioPlayerHandler>(aplayer_listener.get());
        }

        return audio_player_handler.get();
    });
    factories.emplace("Text", [&]() {
        if (!text_handler) {
            text_listener = std::make_shared<TextListener>();
            text_handler = makeCapability<TextAgent, ITextHandler>(text_listener.get());
        }

        return text_handler.get();
    });
    factories.emplace("Speaker", [&]() {
        if (!speaker_handler) {
            speaker_listener = std::make_shared<SpeakerListener>();
            speaker_handler = makeCapability<SpeakerAgent, ISpeakerHandler>(speaker_listener.get());

            // compose SpeakerInfo
            SpeakerInfo nugu_speaker = makeSpeakerInfo(SpeakerType::NUGU, true);
            SpeakerInfo call_speaker = makeSpeakerInfo(SpeakerType::CALL);
            SpeakerInfo alarm_speaker = makeSpeakerInfo(SpeakerType::ALARM);
            SpeakerInfo external_speaker = makeSpeakerInfo(SpeakerType::EXTERNAL);

            std::map<SpeakerType, SpeakerInfo*> speakers = {
                { SpeakerType::NUGU, &nugu_speaker },
                { SpeakerType::CALL, &call_speaker },
                { SpeakerType::ALARM, &alarm_speaker },
                { SpeakerType::EXTERNAL, &external_speaker },
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

        return speaker_handler.get();
    });
    factories.emplace("Mic", [&]() {
        if (!mic_handler) {
            mic_listener = std::make_shared<MicListener>();
            mic_handler = makeCapability<MicAgent, IMicHandler>(mic_listener.get());
            mic_handler->enable();
        }

        return mic_handler.get();
    });
}

SpeakerInfo CapabilityCollection::makeSpeakerInfo(SpeakerType type, bool can_control)
{
    SpeakerInfo nugu_speaker;
    nugu_speaker.type = type;
    nugu_speaker.can_control = can_control;

    return nugu_speaker;
}