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

#include "capability/asr_interface.hh"
#include "clientkit/capability.hh"

namespace NuguCapability {

typedef struct expect_speech_attr {
    bool is_handle;
    std::string dialog_id;
    std::string play_service_id;
    Json::Value domain_types;
    Json::Value asr_context;
} ExpectSpeechAttr;

class ASRAgent final : public Capability,
                       public IASRHandler,
                       public ISpeechRecognizerListener {
public:
    ASRAgent();
    virtual ~ASRAgent() = default;

    void setAttribute(ASRAttribute&& attribute) override;
    void initialize() override;
    void deInitialize() override;
    void suspend() override;

    void startRecognition(float power_noise, float power_speech, ASRInitiator initiator = ASRInitiator::TAP, AsrRecognizeCallback callback = nullptr) override;
    void startRecognition(ASRInitiator initiator = ASRInitiator::TAP, AsrRecognizeCallback callback = nullptr) override;
    void stopRecognition(bool cancel = false) override;

    void preprocessDirective(NuguDirective* ndir) override;
    void parsingDirective(const char* dname, const char* message) override;
    void cancelDirective(NuguDirective* ndir) override;
    void updateInfoForContext(Json::Value& ctx) override;

    bool receiveCommand(const std::string& from, const std::string& command, const std::string& param) override;
    bool getProperty(const std::string& property, std::string& value) override;
    bool getProperties(const std::string& property, std::list<std::string>& values) override;

    void setCapabilityListener(ICapabilityListener* clistener) override;
    void addListener(IASRListener* listener) override;
    void removeListener(IASRListener* listener) override;
    void setEpdAttribute(EpdAttribute&& attribute) override;
    EpdAttribute getEpdAttribute() override;
    std::vector<IASRListener*> getListener();

    void notifyEventResponse(const std::string& msg_id, const std::string& data, bool success) override;

private:
    // send event
    void sendEventCommon(const std::string& ename, EventResultCallback cb = nullptr, bool include_all_context = false);
    void sendEventRecognize(unsigned char* data, size_t length, bool is_end, EventResultCallback cb = nullptr);
    void sendEventResponseTimeout(EventResultCallback cb = nullptr);
    void sendEventListenTimeout(EventResultCallback cb = nullptr);
    void sendEventListenFailed(EventResultCallback cb = nullptr);
    void sendEventStopRecognize(EventResultCallback cb = nullptr);

    // parsing directive
    void parsingExpectSpeech(std::string&& dialog_id, const char* message);
    void parsingNotifyResult(const char* message);
    void parsingCancelRecognize(const char* message);
    void handleExpectSpeech();

    // implements ISpeechRecognizerListener
    void onListeningState(ListeningState state, const std::string& id) override;
    void onRecordData(unsigned char* buf, int length, bool is_end) override;

    void saveAllContextInfo();
    std::string getRecognizeDialogId();
    void setListeningId(const std::string& id);

    void checkResponseTimeout();
    void clearResponseTimeout();

    void cancelRecognition();
    void releaseASRFocus(bool is_cancel, ASRError error, bool release_focus = true);
    void releaseASRFocus(bool release_focus = true);
    void notifyASRErrorCancel(ASRError error, bool is_cancel = false);

    void executeOnForegroundAction(bool asr_user);
    void executeOnBackgroundAction(bool asr_user);
    void executeOnNoneAction(bool asr_user);

    ListeningState getListeningState();
    std::string getListeningStateStr(ListeningState state);
    void setASRState(ASRState state);
    ASRState getASRState();
    void resetExpectSpeechState();
    bool isExpectSpeechState();

    class FocusListener : public IFocusResourceListener {
    public:
        FocusListener(ASRAgent* asr_agent, IFocusManager* focus_manager, bool asr_user);
        FocusListener() = default;
        virtual ~FocusListener() = default;

        void onFocusChanged(FocusState state) override;

        ASRAgent* asr_agent = nullptr;
        IFocusManager* focus_manager = nullptr;
        FocusState focus_state = FocusState::NONE;
        bool asr_user = false;
    };

    const ASRInitiator NONE_INITIATOR = static_cast<ASRInitiator>(-1);

    ExpectSpeechAttr es_attr;
    CapabilityEvent* rec_event;
    INuguTimer* timer;
    std::unique_ptr<ISpeechRecognizer> speech_recognizer;
    std::string all_context_info;
    std::string dialog_id;
    int uniq;
    std::vector<IASRListener*> asr_listeners;
    ListeningState prev_listening_state;
    AsrRecognizeCallback rec_callback;
    ASRInitiator asr_initiator;
    ASRState cur_state;
    std::map<ASRInitiator, std::string> asr_initiator_texts;
    std::map<ASRState, std::string> asr_state_texts;
    std::string request_listening_id;
    bool asr_cancel;
    bool listen_timeout_fail_beep;
    std::string listen_timeout_event_msg_id;
    std::function<void(bool)> pending_release_focus;

    FocusListener* asr_user_listener;
    FocusListener* asr_dm_listener;

    float wakeup_power_noise;
    float wakeup_power_speech;

    // attribute
    std::string model_path;
    std::string epd_type;
    std::string asr_encoding;
    int response_timeout;
    EpdAttribute epd_attribute;
    EpdAttribute default_epd_attribute;
};

} // NuguCapability

#endif
