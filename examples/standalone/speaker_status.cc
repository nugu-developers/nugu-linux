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

#include "speaker_status.hh"

SpeakerStatus* SpeakerStatus::instance = nullptr;

SpeakerStatus* SpeakerStatus::getInstance()
{
    if (!instance)
        instance = new SpeakerStatus();

    return instance;
}

void SpeakerStatus::destroyInstance()
{
    if (instance) {
        delete instance;
        instance = nullptr;
    }
}

void SpeakerStatus::setDefaultValues(DefaultValues&& values)
{
    std::tie(mic_mute, speaker_mute, nugu_volume) = values;
}

bool SpeakerStatus::isMicMute()
{
    return mic_mute;
}

bool SpeakerStatus::isSpeakerMute()
{
    return speaker_mute;
}

void SpeakerStatus::setMicMute(bool mute)
{
    mic_mute = mute;
}

void SpeakerStatus::setSpeakerMute(bool mute)
{
    speaker_mute = mute;
}

void SpeakerStatus::setNUGUVolume(int volume)
{
    nugu_volume = volume;
}

int SpeakerStatus::getNUGUVolume()
{
    return nugu_volume;
}
