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

void SpeakerListener::requestSetVolume(SpeakerType type, int volume, bool linear)
{
    std::cout << "[Speaker] type: " << (int)type << ", volume: " << volume << ", linear: " << linear << std::endl;
    bool success = false;

    if (type == SpeakerType::NUGU && nugu_speaker_volume)
        success = nugu_speaker_volume(volume);

    if (speaker_handler) {
        if (success)
            speaker_handler->informVolumeSucceeded(type, volume);
        else
            speaker_handler->informVolumeFailed(type, volume);
    }
}

void SpeakerListener::requestSetMute(SpeakerType type, bool mute)
{
    std::cout << "[Speaker] type: " << (int)type << ", mute: " << mute << std::endl;
    bool success = false;

    if (type == SpeakerType::NUGU && nugu_speaker_mute)
        success = nugu_speaker_mute(mute);

    if (speaker_handler) {
        if (success)
            speaker_handler->informMuteSucceeded(type, mute);
        else
            speaker_handler->informMuteFailed(type, mute);
    }
}

void SpeakerListener::setSpeakerHandler(ISpeakerHandler* speaker)
{
    speaker_handler = speaker;
}

void SpeakerListener::setVolumeNuguSpeaker(nugu_volume_func vns)
{
    nugu_speaker_volume = std::move(vns);
}

void SpeakerListener::setMuteNuguSpeaker(nugu_mute_func mns)
{
    nugu_speaker_mute = std::move(mns);
}