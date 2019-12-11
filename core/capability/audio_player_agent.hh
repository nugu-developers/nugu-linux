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

#ifndef __NUGU_AUDIO_PLAYER_AGENT_H__
#define __NUGU_AUDIO_PLAYER_AGENT_H__

#include "base/nugu_focus.h"
#include "interface/capability/audio_player_interface.hh"
#include "interface/capability/display_interface.hh"
#include "interface/media_player_interface.hh"

#include "capability.hh"
#include "capability_manager.hh"
#include "display_render_assembly.hh"

namespace NuguCore {

using namespace NuguInterface;

class AudioPlayerAgent : public Capability,
                         public IMediaPlayerListener,
                         public IFocusListener,
                         public IAudioPlayerHandler,
                         public DisplayRenderAssembly<AudioPlayerAgent> {
public:
    enum PlaybackError {
        MEDIA_ERROR_UNKNOWN,
        MEDIA_ERROR_INVALID_REQUEST,
        MEDIA_ERROR_SERVICE_UNAVAILABLE,
        MEDIA_ERROR_INTERNAL_SERVER_ERROR,
        MEDIA_ERROR_INTERNAL_DEVICE_ERROR
    };

public:
    AudioPlayerAgent();
    virtual ~AudioPlayerAgent();
    void initialize() override;

    void parsingDirective(const char* dname, const char* message) override;
    void updateInfoForContext(Json::Value& ctx) override;
    void receiveCommand(CapabilityType from, std::string command, const std::string& param) override;
    void setCapabilityListener(ICapabilityListener* clistener) override;

    void addListener(IAudioPlayerListener* listener) override;
    void removeListener(IAudioPlayerListener* listener) override;
    void play() override;
    void stop() override;
    void next() override;
    void prev() override;
    void pause() override;
    void resume() override;
    void seek(int mesc) override;

    void sendEventPlaybackStarted();
    void sendEventPlaybackFinished();
    void sendEventPlaybackStopped();
    void sendEventPlaybackPaused();
    void sendEventPlaybackResumed();
    void sendEventPlaybackFailed(PlaybackError err, const std::string& reason);
    void sendEventProgressReportDelayElapsed();
    void sendEventProgressReportIntervalElapsed();
    void sendEventByDisplayInterface(const std::string& command);

    void mediaStateChanged(MediaPlayerState state);
    void mediaEventReport(MediaPlayerEvent event);
    void mediaFinished();
    void mediaLoaded();

    void mediaChanged(const std::string& url) override;
    void durationChanged(int duration);
    void positionChanged(int position);
    void volumeChanged(int volume);
    void muteChanged(int mute);

    // implement DisplayRenderAssembly
    void onElementSelected(const std::string& item_token);

private:
    void sendEventCommon(std::string ename);

    AudioPlayerState audioPlayerState();

    void parsingPlay(const char* message);
    void parsingPause(const char* message);
    void parsingStop(const char* message);

    std::string playbackError(PlaybackError error);
    std::string playerActivity(AudioPlayerState state);

    NuguFocusResult onFocus(NuguFocusResource rsrc, void* event) override;
    NuguFocusResult onUnfocus(NuguFocusResource rsrc, void* event) override;
    NuguFocusStealResult onStealRequest(NuguFocusResource rsrc, void* event, NuguFocusType target_type) override;

    IMediaPlayer* player;
    AudioPlayerState cur_aplayer_state;
    AudioPlayerState prev_aplayer_state;
    bool is_paused;
    std::string ps_id;
    long report_delay_time;
    long report_interval_time;
    std::string cur_token;
    std::string pre_ref_dialog_id;
    bool is_finished;
    std::vector<IAudioPlayerListener*> aplayer_listeners;
};

} // NuguCore

#endif
