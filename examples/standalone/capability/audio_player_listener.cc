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

#include "audio_player_listener.hh"
#include "nugu_log.h"

AudioPlayerListener::AudioPlayerListener()
{
    listener_name = "[AudioPlayer]";
}

void AudioPlayerListener::mediaStateChanged(AudioPlayerState state)
{
    std::cout << "[AudioPlayer] ";
    switch (state) {
    case AudioPlayerState::IDLE:
        std::cout << "IDLE\n";
        break;
    case AudioPlayerState::PLAYING:
        std::cout << "PLAYING\n";
        break;
    case AudioPlayerState::STOPPED:
        std::cout << "STOPPED\n";
        break;
    case AudioPlayerState::PAUSED:
        std::cout << "PAUSED\n";
        break;
    case AudioPlayerState::FINISHED:
        std::cout << "FINISHED\n";
        break;
    }
}

void AudioPlayerListener::durationChanged(int duration)
{
    nugu_info("[AudioPlayer] - %d", duration);
}

void AudioPlayerListener::positionChanged(int position)
{
    nugu_info("[AudioPlayer] - %d", position);
}