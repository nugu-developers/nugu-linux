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

#ifndef __NUGU_ASR_AGENT_H__
#define __NUGU_ASR_AGENT_H__

#include <memory>
#include <vector>

#include "base/nugu_focus.h"
#include "capability/asr_interface.hh"
#include "clientkit/capability.hh"

namespace NuguCapability {

typedef struct expect_speech_attr {
    bool is_handle;
    std::string timeout;
    std::string session_id;
    std::string play_service_id;
    Json::Value domain_types;
} ExpectSpeechAttr;

class ASRAgent final : public Capability,
                       public IASRHandler,
                       public ISpeechRecognizerListener {
public:
    ASRAgent();
    virtual ~ASRAgent();

    void setAttribute(ASRAttribute&& attribute) override;
    void initialize() override;
    void deInitialize() override;
    void suspend() override;

    bool startRecognition() override;
    void stopRecognition() override;

    void parsingDirective(const char* dname, const char* message) override;
    void updateInfoForContext(Json::Value& ctx) override;
    void saveAllContextInfo();

    void receiveCommand(const std::string& from, const std::string& command, const std::string& param) override;
    void getProperty(const std::string& property, std::string& value) override;
    void getProperties(const std::string& property, std::list<std::string>& values) override;

    void checkResponseTimeout();
    void clearResponseTimeout();

    void sendEventRecognize(unsigned char* data, size_t length, bool is_end, EventResultCallback cb = nullptr);
    void sendEventResponseTimeout(EventResultCallback cb = nullptr);
    void sendEventListenTimeout(EventResultCallback cb = nullptr);
    void sendEventListenFailed(EventResultCallback cb = nullptr);
    void sendEventStopRecognize(EventResultCallback cb = nullptr);

    void setCapabilityListener(ICapabilityListener* clistener) override;
    void addListener(IASRListener* listener) override;
    void removeListener(IASRListener* listener) override;
    std::vector<IASRListener*> getListener();

    void resetExpectSpeechState();
    bool isExpectSpeechState();
    ListeningState getListeningState();

private:
    void sendEventCommon(const std::string& ename, EventResultCallback cb = nullptr);

    // implements ISpeechRecognizerListener
    void onListeningState(ListeningState state) override;
    void onRecordData(unsigned char* buf, int length) override;

    // parsing directive
    void parsingExpectSpeech(const char* message);
    void parsingNotifyResult(const char* message);

    void releaseASRFocus(bool is_cancel, ASRError error);

    ExpectSpeechAttr es_attr;
    CapabilityEvent* rec_event;
    INuguTimer* timer;
    std::unique_ptr<ISpeechRecognizer> speech_recognizer;
    std::string all_context_info;
    std::string dialog_id;
    IFocusListener* asr_focus_listener;
    IFocusListener* expect_focus_listener;
    std::vector<IASRListener*> asr_listeners;
    ListeningState prev_listening_state = ListeningState::DONE;

    // attribute
    std::string model_path;
    std::string epd_type;
    std::string asr_encoding;
    int response_timeout;
};

} // NuguCapability

#endif
