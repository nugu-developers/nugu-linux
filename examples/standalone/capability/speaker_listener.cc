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

#include <iostream>

#include "speaker_listener.hh"
#include "speaker_status.hh"

void SpeakerListener::requestSetVolume(const std::string& ps_id, SpeakerType type, int volume, bool linear)
{
    std::cout << "[Speaker] type: " << (int)type << ", volume: " << volume << ", linear: " << linear << std::endl;
    bool success = false;

    if (type == SpeakerType::NUGU && nugu_speaker_volume) {
        success = nugu_speaker_volume(volume);

        if (success)
            SpeakerStatus::getInstance()->setNUGUVolume(volume);
    }

    if (speaker_handler) {
        speaker_handler->informVolumeChanged(type, volume);
        speaker_handler->sendEventVolumeChanged(ps_id, success);
    }
}

void SpeakerListener::requestSetMute(const std::string& ps_id, SpeakerType type, bool mute)
{
    std::cout << "[Speaker] type: " << (int)type << ", mute: " << mute << std::endl;
    bool success = false;

    if (type == SpeakerType::NUGU && nugu_speaker_mute) {
        success = nugu_speaker_mute(mute);

        if (success)
            SpeakerStatus::getInstance()->setSpeakerMute(mute);
    }

    if (speaker_handler) {
        speaker_handler->informMuteChanged(type, mute);
        speaker_handler->sendEventMuteChanged(ps_id, success);
    }
}

void SpeakerListener::setSpeakerHandler(ISpeakerHandler* speaker)
{
    speaker_handler = speaker;
}

void SpeakerListener::setVolumeNuguSpeakerCallback(nugu_volume_func vns)
{
    nugu_speaker_volume = std::move(vns);
}

void SpeakerListener::setMuteNuguSpeakerCallback(nugu_mute_func mns)
{
    nugu_speaker_mute = std::move(mns);
}

auto SpeakerListener::getNuguSpeakerVolumeControl() -> decltype(nugu_speaker_volume)
{
    return nugu_speaker_volume;
}

auto SpeakerListener::getNuguSpeakerMuteControl() -> decltype(nugu_speaker_mute)
{
    return nugu_speaker_mute;
}
