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

void AudioPlayerListener::mediaStateChanged(AudioPlayerState state, const std::string& dialog_id)
{
    std::cout << "[AudioPlayer][id:" << dialog_id << "] ";
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

void AudioPlayerListener::mediaEventReport(AudioPlayerEvent event, const std::string& dialog_id)
{
    std::cout << "[AudioPlayer][id:" << dialog_id << "] ";
    switch (event) {
    case AudioPlayerEvent::UNDERRUN:
        std::cout << "UNDERRUN\n";
        break;
    case AudioPlayerEvent::LOAD_FAILED:
        std::cout << "LOAD_FAILED\n";
        break;
    case AudioPlayerEvent::LOAD_DONE:
        std::cout << "LOAD_DONE\n";
        break;
    case AudioPlayerEvent::INVALID_URL:
        std::cout << "INVALID_URL\n";
        break;
    default:
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

void AudioPlayerListener::favoriteChanged(bool favorite, const std::string& dialog_id)
{
    std::cout << "[AudioPlayer][id:" << dialog_id << "] - favorite: " << favorite << std::endl;
}

void AudioPlayerListener::shuffleChanged(bool shuffle, const std::string& dialog_id)
{
    std::cout << "[AudioPlayer][id:" << dialog_id << "] - shuffle: " << shuffle << std::endl;
}

void AudioPlayerListener::repeatChanged(RepeatType repeat, const std::string& dialog_id)
{
    std::cout << "[AudioPlayer][id:" << dialog_id << "] - repeat: " << (int)repeat << std::endl;
}

void AudioPlayerListener::requestContentCache(const std::string& key, const std::string& playurl)
{
    std::cout << "[AudioPlayer] request to cache content:\n"
              << "  - key: " << key << std::endl
              << "  - playurl: " << playurl << std::endl;
}

bool AudioPlayerListener::requestToGetCachedContent(const std::string& key, std::string& filepath)
{
    return false;
}

void AudioPlayerListener::renderDisplay(const std::string& id, const std::string& type, const std::string& json_payload, const std::string& dialog_id)
{
    std::cout << "[AudioPlayer][id:" << id << "] renderDisplay - type: " << type << std::endl;
}

bool AudioPlayerListener::clearDisplay(const std::string& id, bool unconditionally)
{
    std::cout << "[AudioPlayer][id:" << id << "] clearDisplay - unconditionally: " << (int)unconditionally << std::endl;
    return false;
}

void AudioPlayerListener::controlDisplay(const std::string& id, ControlType type, ControlDirection direction)
{
    std::cout << "[AudioPlayer][id:" << id << "] controlDisplay" << std::endl;
}

void AudioPlayerListener::updateDisplay(const std::string& id, const std::string& json_payload)
{
    std::cout << "[AudioPlayer][id:" << id << "] updateDisplay" << std::endl;
}

bool AudioPlayerListener::requestLyricsPageAvailable(const std::string& id, bool& visible)
{
    std::cout << "[AudioPlayer] requestLyricsPageAvailable" << std::endl;
    return false;
}

bool AudioPlayerListener::showLyrics(const std::string& id)
{
    std::cout << "[AudioPlayer] showLyrics" << std::endl;
    return false;
}

bool AudioPlayerListener::hideLyrics(const std::string& id)
{
    std::cout << "[AudioPlayer] hideLyrics" << std::endl;
    return false;
}

void AudioPlayerListener::updateMetaData(const std::string& id, const std::string& json_payload)
{
    std::cout << "[AudioPlayer][id:" << id << "] updateMetaData - json: " << json_payload << std::endl;
}