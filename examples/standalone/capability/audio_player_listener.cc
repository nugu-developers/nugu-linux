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

#include "audio_player_listener.hh"

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
    std::cout << "[AudioPlayer] - duration: " << duration << std::endl;
}

void AudioPlayerListener::positionChanged(int position)
{
    std::cout << "[AudioPlayer] - position: " << position << std::endl;
}

void AudioPlayerListener::favoriteChanged(bool favorite)
{
    std::cout << "[AudioPlayer] - favorite: " << favorite << std::endl;
}

void AudioPlayerListener::shuffleChanged(bool shuffle)
{
    std::cout << "[AudioPlayer] - shuffle: " << shuffle << std::endl;
}

void AudioPlayerListener::repeatChanged(RepeatType repeat)
{
    std::cout << "[AudioPlayer] - repeat: " << (int)repeat << std::endl;
}

void AudioPlayerListener::requestContentCache(const std::string& key, const std::string& playurl)
{
    std::cout << "[AudioPlayer] request to cache content - key: " << key << ", playurl: " << playurl << std::endl;
}

bool AudioPlayerListener::requestToGetCachedContent(const std::string& key, std::string& filepath)
{
    std::cout << "[AudioPlayer] request to get cached content - key: " << key << std::endl;
    return false;
}
