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

#ifndef __NUGU_TTS_AGENT_H__
#define __NUGU_TTS_AGENT_H__

#include "capability/tts_interface.hh"
#include "clientkit/capability.hh"

namespace NuguCapability {

class TTSAgent final : public Capability,
                       public ITTSHandler,
                       public IMediaPlayerListener,
                       public IFocusResourceListener,
                       public IPlaySyncManagerListener {
public:
    TTSAgent();
    virtual ~TTSAgent() = default;

    void setAttribute(TTSAttribute&& attribute) override;
    void initialize() override;
    void deInitialize() override;
    void suspend() override;

    void preprocessDirective(NuguDirective* ndir) override;
    void parsingDirective(const char* dname, const char* message) override;
    void cancelDirective(NuguDirective* ndir) override;
    void updateInfoForContext(Json::Value& ctx) override;

    void stopTTS() override;
    std::string requestTTS(const std::string& text, const std::string& play_service_id, const std::string& referrer_id = "") override;
    bool setVolume(int volume) override;

    void onFocusChanged(FocusState state) override;
    void executeOnForegroundAction();

    void sendEventSpeechStarted(const std::string& token, EventResultCallback cb = nullptr);
    void sendEventSpeechFinished(const std::string& token, EventResultCallback cb = nullptr);
    void sendEventSpeechStopped(const std::string& token, EventResultCallback cb = nullptr);
    std::string sendEventSpeechPlay(const std::string& token, const std::string& text, const std::string& play_service_id = "", EventResultCallback cb = nullptr);
    void setCapabilityListener(ICapabilityListener* clistener) override;
    void addListener(ITTSListener* listener) override;
    void removeListener(ITTSListener* listener) override;

    static void directiveDataCallback(NuguDirective* ndir, int seq, void* userdata);
    static void getAttachmentData(NuguDirective* ndir, int seq, void* userdata);

    // implements IPlaySyncManagerListener
    void onSyncState(const std::string& ps_id, PlaySyncState state, void* extra_data) override;
    void onDataChanged(const std::string& ps_id, std::pair<void*, void*> extra_datas) override;

private:
    void sendEventCommon(CapabilityEvent* event, const std::string& token, EventResultCallback cb = nullptr);
    // parsing directive
    void parsingSpeak(const char* message);
    void parsingStop(const char* message);
    void postProcessDirective(bool is_cancel = false);
    void sendClearNudgeCommand(NuguDirective* ndir);

    void checkAndUpdateVolume();
    void mediaStateChanged(MediaPlayerState state) override;
    void mediaEventReport(MediaPlayerEvent event) override;
    void mediaChanged(const std::string& url) override;
    void durationChanged(int duration) override;
    void positionChanged(int position) override;
    void volumeChanged(int volume) override;
    void muteChanged(int mute) override;

    ITTSPlayer* player;
    MediaPlayerState cur_state;
    FocusState focus_state;
    std::string cur_token;
    bool is_prehandling;
    bool is_finished;
    bool is_stopped_by_explicit;
    bool volume_update;
    int volume;

    NuguDirective* speak_dir;

    std::string dialog_id;
    std::string ps_id;
    std::string playstackctl_ps_id;
    std::vector<ITTSListener*> tts_listeners;

    // attribute
    std::string tts_engine;
};

} // NuguCapability

#endif
