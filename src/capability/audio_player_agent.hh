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

#include <map>

#include "capability/audio_player_interface.hh"
#include "capability/display_interface.hh"
#include "clientkit/capability.hh"
#include "display_render_helper.hh"

namespace NuguCapability {

using namespace NuguClientKit;

class AudioPlayerAgent final : public Capability,
                               public IMediaPlayerListener,
                               public IAudioPlayerHandler,
                               public IFocusResourceListener,
                               public IPlaySyncManagerListener {
public:
    enum PlaybackError {
        MEDIA_ERROR_UNKNOWN,
        MEDIA_ERROR_INVALID_REQUEST,
        MEDIA_ERROR_SERVICE_UNAVAILABLE,
        MEDIA_ERROR_INTERNAL_SERVER_ERROR,
        MEDIA_ERROR_INTERNAL_DEVICE_ERROR
    };

    using RenderInfo = std::tuple<std::string, std::string, std::string>;

public:
    AudioPlayerAgent();
    virtual ~AudioPlayerAgent() = default;
    void initialize() override;
    void deInitialize() override;
    void suspend() override;
    void restore() override;

    void preprocessDirective(NuguDirective* ndir) override;
    void parsingDirective(const char* dname, const char* message) override;
    void updateInfoForContext(Json::Value& ctx) override;
    bool receiveCommand(const std::string& from, const std::string& command, const std::string& param) override;
    void receiveCommandAll(const std::string& command, const std::string& param) override;
    void setCapabilityListener(ICapabilityListener* clistener) override;

    // implements IAudioPlayerHandler
    void addListener(IAudioPlayerListener* listener) override;
    void removeListener(IAudioPlayerListener* listener) override;
    std::string play() override;
    std::string stop(bool direct_access) override;
    std::string next() override;
    std::string prev() override;
    std::string pause(bool direct_access) override;
    std::string resume(bool direct_access) override;
    void seek(int msec) override;
    std::string requestFavoriteCommand(bool current_favorite) override;
    std::string requestRepeatCommand(RepeatType current_repeat) override;
    std::string requestShuffleCommand(bool current_shuffle) override;
    bool setVolume(int volume) override;
    bool setMute(bool mute) override;

    void displayRendered(const std::string& id) override;
    void displayCleared(const std::string& id) override;
    void elementSelected(const std::string& id, const std::string& item_token, const std::string& postback) override;
    void informControlResult(const std::string& id, ControlType type, ControlDirection direction) override;
    void setListener(IDisplayListener* listener) override;
    void removeListener(IDisplayListener* listener) override;
    void stopRenderingTimer(const std::string& id) override;

    void mediaStateChanged(MediaPlayerState state) override;
    void mediaEventReport(MediaPlayerEvent event) override;

    void mediaChanged(const std::string& url) override;
    void durationChanged(int duration) override;
    void positionChanged(int position) override;
    void volumeChanged(int volume) override;
    void muteChanged(int mute) override;

    // implements IPlaySyncManagerListener, IFocusResourceListener
    void onFocusChanged(FocusState state) override;
    void onSyncState(const std::string& ps_id, PlaySyncState state, void* extra_data) override;
    void onDataChanged(const std::string& ps_id, std::pair<void*, void*> extra_datas) override;

private:
    std::string sendEventCommon(const std::string& ename, EventResultCallback cb = nullptr, bool include_all_context = false);
    void sendEventPlaybackStarted(EventResultCallback cb = nullptr);
    void sendEventPlaybackFinished(EventResultCallback cb = nullptr);
    void sendEventPlaybackStopped(EventResultCallback cb = nullptr);
    void sendEventPlaybackPaused(EventResultCallback cb = nullptr);
    void sendEventPlaybackResumed(EventResultCallback cb = nullptr);
    void sendEventPlaybackFailed(PlaybackError err, const std::string& reason, EventResultCallback cb = nullptr);
    void sendEventProgressReportDelayElapsed(EventResultCallback cb = nullptr);
    void sendEventProgressReportIntervalElapsed(EventResultCallback cb = nullptr);
    std::string sendEventByMediaPlayerControl(const std::string& command, EventResultCallback cb = nullptr);
    void sendEventShowLyricsSucceeded(EventResultCallback cb = nullptr);
    void sendEventShowLyricsFailed(EventResultCallback cb = nullptr);
    void sendEventHideLyricsSucceeded(EventResultCallback cb = nullptr);
    void sendEventHideLyricsFailed(EventResultCallback cb = nullptr);
    void sendEventControlLyricsPageSucceeded(const std::string& direction, EventResultCallback cb = nullptr);
    void sendEventControlLyricsPageFailed(const std::string& direction, EventResultCallback cb = nullptr);
    void sendEventByRequestOthersDirective(const std::string& dname, EventResultCallback cb = nullptr);
    void sendEventRequestPlayCommandIssued(const std::string& dname, const std::string& payload, EventResultCallback cb = nullptr);
    void sendEventRequestCommandFailed(const std::string& play_service_id, const std::string& type, const std::string& reason, EventResultCallback cb = nullptr);
    void notifyEventResponse(const std::string& msg_id, const std::string& data, bool success) override;

    bool isContentCached(const std::string& key, std::string& playurl);
    void parsingPlay(const char* message);
    void parsingPause(const char* message);
    void parsingStop(const char* message);
    void parsingUpdateMetadata(const char* message);
    void parsingShowLyrics(const char* message);
    void parsingHideLyrics(const char* message);
    void parsingControlLyricsPage(const char* message);
    void parsingRequestPlayCommand(const char* dname, const char* message);
    void parsingRequestOthersCommand(const char* dname, const char* message);
    DisplayRenderInfo* composeRenderInfo(NuguDirective* ndir, const char* message);

    void clearContext();
    void checkAndUpdateVolume();
    bool hasToSkipForegroundAction();
    void executeOnForegroundAction();
    void executeOnBackgroundAction();

    std::string playbackError(PlaybackError error);
    std::string playerActivity(AudioPlayerState state);

    static void directiveDataCallback(NuguDirective* ndir, int seq, void* userdata);
    static void getAttachmentData(NuguDirective* ndir, void* userdata);

    const unsigned int PAUSE_CONTEXT_HOLD_TIME = 60 * 10;

    IMediaPlayer* cur_player;
    IMediaPlayer* media_player;
    ITTSPlayer* tts_player;
    NuguDirective* speak_dir;
    FocusState focus_state;
    bool is_tts_activate;
    bool stop_reason_by_play_another;
    bool has_play_directive;
    std::string play_directive_dialog_id;
    bool receive_new_play_directive;
    bool suspended_stop_policy;
    bool skip_intermediate_foreground_focus;

    AudioPlayerState cur_aplayer_state;
    AudioPlayerState prev_aplayer_state;
    bool is_paused;
    bool is_paused_by_unfocus;
    std::string ps_id;
    long report_delay_time;
    long report_interval_time;
    std::string cur_token;
    std::string cur_url;
    std::string pre_ref_dialog_id;
    std::string cur_dialog_id;
    bool is_finished;
    bool volume_update;
    int volume;
    std::string template_id;
    std::unique_ptr<DisplayRenderHelper> render_helper;
    std::vector<IAudioPlayerListener*> aplayer_listeners;
    IAudioPlayerDisplayListener* display_listener;
};

} // NuguCapability

#endif
