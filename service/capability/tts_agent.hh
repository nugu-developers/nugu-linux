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

#include <functional>

#include <core/nugu_decoder.h>
#include <core/nugu_focus.h>
#include <core/nugu_pcm.h>
#include <interface/capability/tts_interface.hh>

#include "capability.hh"
#include "capability_manager.hh"

namespace NuguCore {

using namespace NuguInterface;

class TTSAgent : public Capability,
                 public ITTSHandler,
                 public IFocusListener {
public:
    TTSAgent();
    virtual ~TTSAgent();
    void initialize() override;

    void processDirective(NuguDirective* ndir) override;
    void updateInfoForContext(Json::Value& ctx) override;
    std::string getContextInfo();
    void receiveCommand(CapabilityType from, std::string command, std::string param) override;
    void receiveCommandAll(std::string command, std::string param) override;

    void stopTTS() override;
    void requestTTS(std::string text, std::string play_service_id) override;

    void sendEventSpeechStarted(std::string token);
    void sendEventSpeechFinished(std::string token);
    void sendEventSpeechStopped(std::string token);
    void sendEventSpeechPlay(std::string token, std::string text, std::string play_service_id = "");
    void setCapabilityListener(ICapabilityListener* clistener) override;

    static void pcmStatusCallback(enum nugu_media_status status, void* userdata);
    static void pcmEventCallback(enum nugu_media_event event, void* userdata);
    static void directiveDataCallback(NuguDirective* ndir, void* userdata);
    static void getAttachmentData(NuguDirective* ndir, void* userdata);

    std::string cur_token;
    int speak_status;
    bool finish;

private:
    void sendEventCommon(std::string ename, std::string token);
    // parsing directive
    void parsingSpeak(const char* message, NuguDirective* ndir);
    void parsingStop(const char* message);

    void startTTS(NuguDirective* ndir);
    NuguFocusResult onFocus(NuguFocusResource rsrc, void* event);
    NuguFocusResult onUnfocus(NuguFocusResource rsrc, void* event);
    NuguFocusStealResult onStealRequest(NuguFocusResource rsrc, void* event, NuguFocusType target_type);

    NuguDirective* speak_dir;
    NuguPcm* pcm;
    NuguDecoder* decoder;

    std::string ps_id;
    ITTSListener* tts_listener;
};

} // NuguCore

#endif
