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

#include "display_listener.hh"
#include <capability/audio_player_interface.hh>

using namespace NuguCapability;

class AudioPlayerListener : public IAudioPlayerListener,
                            public IAudioPlayerDisplayListener,
                            public DisplayListener {
public:
    AudioPlayerListener();
    virtual ~AudioPlayerListener() = default;

    // implements IAudioPlayerListener
    void mediaStateChanged(AudioPlayerState state, const std::string& dialog_id) override;
    void mediaEventReport(AudioPlayerEvent event, const std::string& dialog_id) override;
    void durationChanged(int duration) override;
    void positionChanged(int position) override;
    void favoriteChanged(bool favorite, const std::string& dialog_id) override;
    void shuffleChanged(bool shuffle, const std::string& dialog_id) override;
    void repeatChanged(RepeatType repeat, const std::string& dialog_id) override;
    void requestContentCache(const std::string& key, const std::string& playurl) override;
    bool requestToGetCachedContent(const std::string& key, std::string& filepath) override;
    bool onResume(const std::string& dialog_id) override;

    // implements IAudioPlayerDisplayListener
    bool requestLyricsPageAvailable(const std::string& id, bool& visible) override;
    bool showLyrics(const std::string& id) override;
    bool hideLyrics(const std::string& id) override;
    void updateMetaData(const std::string& id, const std::string& json_payload) override;

private:
    bool is_showing_lyrics = false;
};

#endif /* __AUDIO_PLAYER_LISTENER_H__ */
