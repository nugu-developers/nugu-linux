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

#ifndef __AUDIO_PLAYER_LISTENER_H__
#define __AUDIO_PLAYER_LISTENER_H__

#include <interface/capability/audio_player_interface.hh>

#include "display_listener.hh"

using namespace NuguInterface;

class AudioPlayerListener : public IAudioPlayerListener, public DisplayListener {
public:
    AudioPlayerListener();
    virtual ~AudioPlayerListener() = default;

    void mediaStateChanged(AudioPlayerState state) override;
    void durationChanged(int duration) override;
    void positionChanged(int position) override;
    void favoriteChanged(bool favorite) override;
    void shuffleChanged(bool shuffle) override;
    void repeatChanged(RepeatType repeat) override;
    void requestContentCache(const std::string& key, const std::string& playurl) override;
    bool requestToGetCachedContent(const std::string& key, std::string& filepath) override;
};

#endif /* __AUDIO_PLAYER_LISTENER_H__ */
