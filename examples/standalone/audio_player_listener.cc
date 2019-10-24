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

void AudioPlayerListener::renderDisplay(const std::string& type, const std::string& json)
{
    nugu_info("got received to render display template");
}
void AudioPlayerListener::clearDisplay()
{
    nugu_info("got received to clear display template");
}
void AudioPlayerListener::mediaStateChanged(AudioPlayerState state)
{
    nugu_info("%s - %d", __FUNCTION__, (int)state);
}
void AudioPlayerListener::durationChanged(int duration)
{
    nugu_info("%s - %d", __FUNCTION__, duration);
}
void AudioPlayerListener::positionChanged(int position)
{
    nugu_info("%s - %d", __FUNCTION__, position);
}
