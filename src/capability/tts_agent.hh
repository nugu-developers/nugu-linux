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

#include "base/nugu_decoder.h"
#include "base/nugu_focus.h"
#include "base/nugu_pcm.h"
#include "capability/tts_interface.hh"
#include "clientkit/capability.hh"

namespace NuguCapability {

class TTSAgent final : public Capability,
                       public ITTSHandler,
                       public IFocusListener {
public:
    TTSAgent();
    virtual ~TTSAgent();

    void setAttribute(TTSAttribute&& attribute) override;
    void initialize() override;
    void deInitialize() override;
    void suspend() override;

    void parsingDirective(const char* dname, const char* message) override;
    void updateInfoForContext(Json::Value& ctx) override;

    void stopTTS() override;
    void requestTTS(const std::string& text, const std::string& play_service_id) override;
    bool setVolume(int volume) override;

    void sendEventSpeechStarted(const std::string& token, EventResultCallback cb = nullptr);
    void sendEventSpeechFinished(const std::string& token, EventResultCallback cb = nullptr);
    void sendEventSpeechStopped(const std::string& token, EventResultCallback cb = nullptr);
    void sendEventSpeechPlay(const std::string& token, const std::string& text, const std::string& play_service_id = "", EventResultCallback cb = nullptr);
    void setCapabilityListener(ICapabilityListener* clistener) override;

    static void pcmStatusCallback(enum nugu_media_status status, void* userdata);
    static void pcmEventCallback(enum nugu_media_event event, void* userdata);
    static void directiveDataCallback(NuguDirective* ndir, void* userdata);
    static void getAttachmentData(NuguDirective* ndir, void* userdata);

    std::string cur_token;
    int speak_status;
    bool finish;

private:
    void sendEventCommon(const std::string& ename, const std::string& token, EventResultCallback cb = nullptr);
    // parsing directive
    void parsingSpeak(const char* message);
    void parsingStop(const char* message);

    void startTTS(NuguDirective* ndir);
    void onFocus(void* event) override;
    NuguFocusResult onUnfocus(void* event, NuguUnFocusMode mode) override;
    NuguFocusStealResult onStealRequest(void* event, NuguFocusType target_type) override;

    NuguDirective* speak_dir;
    NuguPcm* pcm;
    NuguDecoder* decoder;

    std::string dialog_id;
    std::string ps_id;
    std::string playstackctl_ps_id;
    ITTSListener* tts_listener;

    // attribute
    std::string tts_engine;
};

} // NuguCapability

#endif
