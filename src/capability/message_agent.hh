/*
 * Copyright (c) 2021 SK Telecom Co., Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or` implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __MESSAGE_AGENT_H__
#define __MESSAGE_AGENT_H__

#include <vector>

#include "capability/audio_player_interface.hh"
#include "capability/message_interface.hh"

namespace NuguCapability {

class MessageAgent final : public Capability,
                           public IMediaPlayerListener,
                           public IFocusResourceListener,
                           public IMessageHandler {
public:
    MessageAgent();
    virtual ~MessageAgent() = default;

    void initialize() override;
    void deInitialize() override;

    void setCapabilityListener(ICapabilityListener* clistener) override;
    void updateInfoForContext(Json::Value& ctx) override;
    void parsingDirective(const char* dname, const char* message) override;

    static void directiveDataCallback(NuguDirective* ndir, int seq, void* userdata);
    static void getAttachmentData(NuguDirective* ndir, void* userdata);

    void stopTTS() override;
    void candidatesListed(const std::string& payload) override;
    void sendMessageSucceeded(const std::string& payload) override;
    void sendMessageFailed(const std::string& payload) override;
    void getMessageSucceeded(const std::string& payload) override;
    void getMessageFailed(const std::string& payload) override;

private:
    void parsingSendCandidates(const char* message);
    void parsingSendMessage(const char* message);
    void parsingGetMessage(const char* message);
    void parsingReadMessage(const char* message);

    void sendEventReadMessageFinished(EventResultCallback cb = nullptr);

    // implements IMediaPlayerListener
    void mediaStateChanged(MediaPlayerState state) override;
    void mediaEventReport(MediaPlayerEvent event) override;
    void mediaChanged(const std::string& url) override;
    void durationChanged(int duration) override;
    void positionChanged(int position) override;
    void volumeChanged(int volume) override;
    void muteChanged(int mute) override;

    // implements IFocusResourceListener
    void onFocusChanged(FocusState state) override;

    void executeOnForegroundAction();
    std::string getCurrentTTSState();

    IMessageListener* message_listener = nullptr;
    NuguDirective* speak_dir = nullptr;
    ITTSPlayer* tts_player = nullptr;
    MediaPlayerState cur_state = MediaPlayerState::IDLE;
    FocusState focus_state = FocusState::NONE;
    std::string tts_token;
    std::string tts_ps_id;
    bool is_finished = false;
    std::string template_data;
};

} // NuguCapability

#endif /* __MESSAGE_AGENT_H__ */
