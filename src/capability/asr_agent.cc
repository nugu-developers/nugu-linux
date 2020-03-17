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

#include <chrono>
#include <string.h>

#include "asr_agent.hh"
#include "base/nugu_log.h"

namespace NuguCapability {

static const char* CAPABILITY_NAME = "ASR";
static const char* CAPABILITY_VERSION = "1.1";

class ASRFocusListener : public IFocusListener {
public:
    explicit ASRFocusListener(ASRAgent* agent, ISpeechRecognizer* speech_recognizer);
    virtual ~ASRFocusListener();

    void onFocus(void* event) override;
    NuguFocusResult onUnfocus(void* event, NuguUnFocusMode mode) override;
    NuguFocusStealResult onStealRequest(void* event, NuguFocusType target_type) override;

private:
    ASRAgent* agent;
    ISpeechRecognizer* speech_recognizer;
};

ASRFocusListener::ASRFocusListener(ASRAgent* agent, ISpeechRecognizer* speech_recognizer)
    : agent(agent)
    , speech_recognizer(speech_recognizer)

{
    agent->getCapabilityHelper()->addFocus("asr", NUGU_FOCUS_TYPE_ASR, this);
}

ASRFocusListener::~ASRFocusListener()
{
    agent->getCapabilityHelper()->removeFocus("asr");
}

void ASRFocusListener::onFocus(void* event)
{
    nugu_dbg("ASRFocusListener::onFocus");
    speech_recognizer->startListening();

    if (agent->isExpectSpeechState()) {
        agent->resetExpectSpeechState();
        agent->getCapabilityHelper()->releaseFocus("expect");
    }
}

NuguFocusResult ASRFocusListener::onUnfocus(void* event, NuguUnFocusMode mode)
{
    nugu_dbg("ASRFocusListener::onUnfocus");
    speech_recognizer->stopListening();

    auto agent_listeners = agent->getListener();

    for (auto asr_listener : agent_listeners) {
        asr_listener->onState(ASRState::IDLE, agent->getRecognizeDialogId());
        agent->resetExpectSpeechState();
    }

    return NUGU_FOCUS_REMOVE;
}

NuguFocusStealResult ASRFocusListener::onStealRequest(void* event, NuguFocusType target_type)
{
    return (agent->getListeningState() == ListeningState::DONE) ? NUGU_FOCUS_STEAL_ALLOW : NUGU_FOCUS_STEAL_REJECT;
}

class ExpectFocusListener : public IFocusListener {
public:
    explicit ExpectFocusListener(ASRAgent* agent);
    virtual ~ExpectFocusListener();

    void onFocus(void* event) override;
    NuguFocusResult onUnfocus(void* event, NuguUnFocusMode mode) override;
    NuguFocusStealResult onStealRequest(void* event, NuguFocusType target_type) override;

private:
    ASRAgent* agent;
};

ExpectFocusListener::ExpectFocusListener(ASRAgent* agent)
    : agent(agent)
{
    agent->getCapabilityHelper()->addFocus("expect", NUGU_FOCUS_TYPE_ASR_EXPECT, this);
}

ExpectFocusListener::~ExpectFocusListener()
{
    agent->getCapabilityHelper()->removeFocus("expect");
}

void ExpectFocusListener::onFocus(void* event)
{
    nugu_dbg("ExpectFocusListener::onFocus");
    agent->startRecognition();
}

NuguFocusResult ExpectFocusListener::onUnfocus(void* event, NuguUnFocusMode mode)
{
    nugu_dbg("ExpectFocusListener::onUnfocus");

    return NUGU_FOCUS_REMOVE;
}

NuguFocusStealResult ExpectFocusListener::onStealRequest(void* event, NuguFocusType target_type)
{
    nugu_dbg("ExpectFocusListener::onStealRequest");

    return NUGU_FOCUS_STEAL_ALLOW;
}

ASRAgent::ASRAgent()
    : Capability(CAPABILITY_NAME, CAPABILITY_VERSION)
    , es_attr({})
    , rec_event(nullptr)
    , timer(nullptr)
    , asr_focus_listener(nullptr)
    , expect_focus_listener(nullptr)
    , model_path("")
    , epd_type(NUGU_ASR_EPD_TYPE)
    , asr_encoding(NUGU_ASR_ENCODING)
    , response_timeout(NUGU_SERVER_RESPONSE_TIMEOUT_SEC)
{
}

ASRAgent::~ASRAgent()
{
}

void ASRAgent::setAttribute(ASRAttribute&& attribute)
{
    // It's not check validation, just bypass to SpeechRecognizer.
    model_path = attribute.model_path;

    if (!attribute.epd_type.empty())
        epd_type = attribute.epd_type;

    if (!attribute.asr_encoding.empty())
        asr_encoding = attribute.asr_encoding;

    if (attribute.response_timeout > 0)
        response_timeout = attribute.response_timeout;
}

void ASRAgent::initialize()
{
    if (initialized) {
        nugu_info("It's already initialized.");
        return;
    }

    speech_recognizer = std::unique_ptr<ISpeechRecognizer>(core_container->createSpeechRecognizer(model_path));
    speech_recognizer->setListener(this);

    timer = core_container->createNuguTimer();
    timer->setInterval(response_timeout);
    timer->setCallback([&](int count, int repeat) {
        sendEventResponseTimeout();
        releaseASRFocus(false, ASRError::RESPONSE_TIMEOUT);
    });

    asr_focus_listener = new ASRFocusListener(this, this->speech_recognizer.get());
    expect_focus_listener = new ExpectFocusListener(this);

    initialized = true;
}

void ASRAgent::deInitialize()
{
    if (initialized == false) {
        nugu_info("already de-initialized");
        return;
    }

    if (timer) {
        delete timer;
        timer = nullptr;
    }

    if (rec_event) {
        delete rec_event;
        rec_event = nullptr;
    }

    delete asr_focus_listener;
    delete expect_focus_listener;

    speech_recognizer.reset();

    initialized = false;
}

void ASRAgent::suspend()
{
    stopRecognition();
}

void ASRAgent::startRecognition(AsrRecognizeCallback callback)
{
    nugu_dbg("startRecognition()");

    if (callback != nullptr)
        rec_callback = std::move(callback);

    if (speech_recognizer->isMute())
        return;

    saveAllContextInfo();
    capa_helper->requestFocus("asr", NULL);
}

void ASRAgent::stopRecognition()
{
    nugu_dbg("stopRecognition()");

    if (rec_event)
        sendEventStopRecognize();

    capa_helper->releaseFocus("asr");
}

void ASRAgent::parsingDirective(const char* dname, const char* message)
{
    if (!strcmp(dname, "ExpectSpeech"))
        parsingExpectSpeech(message);
    else if (!strcmp(dname, "NotifyResult"))
        parsingNotifyResult(message);
    else
        nugu_warn("%s[%s] is not support %s directive", getName().c_str(), getVersion().c_str(), dname);
}

void ASRAgent::updateInfoForContext(Json::Value& ctx)
{
    Json::Value asr;

    asr["version"] = getVersion();
    ctx[getName()] = asr;
}

void ASRAgent::saveAllContextInfo()
{
    all_context_info = capa_helper->makeAllContextInfoStack();
}

void ASRAgent::receiveCommand(const std::string& from, const std::string& command, const std::string& param)
{
    std::string convert_command;
    convert_command.resize(command.size());
    std::transform(command.cbegin(), command.cend(), convert_command.begin(), ::tolower);

    if (!convert_command.compare("wakeup_detected")) {
        if (es_attr.is_handle) {
            capa_helper->releaseFocus("expect");
            es_attr = {};
        }
    } else if (!convert_command.compare("releasefocus")) {
        if (from == "System" || from == "CapabilityManager") {
            if (dialog_id == param)
                capa_helper->releaseFocus("asr");
        } else {
            capa_helper->releaseFocus("asr");
        }
    }
}

void ASRAgent::getProperty(const std::string& property, std::string& value)
{
    if (property == "es.playServiceId") {
        value = es_attr.play_service_id;
    } else if (property == "es.sessionId") {
        value = es_attr.session_id;
    } else {
        value = "";
    }
}

void ASRAgent::getProperties(const std::string& property, std::list<std::string>& values)
{
    if (property == "es.domainTypes") {
        for (int i = 0; i < (int)es_attr.domain_types.size(); i++) {
            values.emplace_back(es_attr.domain_types[i].asString());
        }
    } else {
        values.clear();
    }
}

void ASRAgent::checkResponseTimeout()
{
    if (timer)
        timer->start();
}

void ASRAgent::clearResponseTimeout()
{
    if (timer)
        timer->stop();
}

std::string ASRAgent::getRecognizeDialogId()
{
    return dialog_id;
}

void ASRAgent::sendEventRecognize(unsigned char* data, size_t length, bool is_end, EventResultCallback cb)
{
    Json::StyledWriter writer;
    Json::Value root;
    std::string payload = "";

    if (!rec_event) {
        nugu_error("recognize event is not created");
        return;
    }

    root["codec"] = "SPEEX";
    root["language"] = "KOR";
    root["endpointing"] = epd_type;
    root["encoding"] = asr_encoding;

    if (es_attr.is_handle) {
        root["sessionId"] = es_attr.session_id;
        if (es_attr.play_service_id.size())
            root["playServiceId"] = es_attr.play_service_id;
        if (!es_attr.domain_types.empty())
            root["domainTypes"] = es_attr.domain_types;
        if (!es_attr.asr_context.empty())
            root["asrContext"] = es_attr.asr_context;
    }
    payload = writer.write(root);

    dialog_id = rec_event->getDialogMessageId();
    if (!is_end && (length == 0 || data == nullptr))
        sendEvent(rec_event, all_context_info, payload, std::move(cb));
    else
        sendAttachmentEvent(rec_event, is_end, length, data);
}

void ASRAgent::sendEventResponseTimeout(EventResultCallback cb)
{
    sendEventCommon("ResponseTimeout", std::move(cb));
}

void ASRAgent::sendEventListenTimeout(EventResultCallback cb)
{
    sendEventCommon("ListenTimeout", std::move(cb));
}

void ASRAgent::sendEventListenFailed(EventResultCallback cb)
{
    sendEventCommon("ListenFailed", std::move(cb));
}

void ASRAgent::sendEventStopRecognize(EventResultCallback cb)
{
    sendEventCommon("StopRecognize", std::move(cb));
}

void ASRAgent::sendEventCommon(const std::string& ename, EventResultCallback cb)
{
    std::string payload = "";

    if (es_attr.is_handle) {
        Json::StyledWriter writer;
        Json::Value root;

        root["playServiceId"] = es_attr.play_service_id;
        payload = writer.write(root);
    }

    sendEvent(ename, getContextInfo(), payload, std::move(cb));
}

void ASRAgent::setCapabilityListener(ICapabilityListener* clistener)
{
    if (clistener)
        addListener(dynamic_cast<IASRListener*>(clistener));
}

void ASRAgent::addListener(IASRListener* listener)
{
    if (listener && std::find(asr_listeners.begin(), asr_listeners.end(), listener) == asr_listeners.end()) {
        asr_listeners.emplace_back(listener);
    }
}

void ASRAgent::removeListener(IASRListener* listener)
{
    auto iterator = std::find(asr_listeners.begin(), asr_listeners.end(), listener);

    if (iterator != asr_listeners.end())
        asr_listeners.erase(iterator);
}

long ASRAgent::getEpdSilenceInterval()
{
    return static_cast<long>(speech_recognizer ? speech_recognizer->getEpdPauseLength() : 0);
}

std::vector<IASRListener*> ASRAgent::getListener()
{
    return asr_listeners;
}

void ASRAgent::parsingExpectSpeech(const char* message)
{
    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    if (root["sessionId"].asString().empty() || root["timeoutInMilliseconds"].empty()) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    es_attr.is_handle = true;
    es_attr.timeout = root["timeoutInMilliseconds"].asString();
    es_attr.session_id = root["sessionId"].asString();
    es_attr.play_service_id = root["playServiceId"].asString();
    es_attr.domain_types = root["domainTypes"];
    es_attr.asr_context = root["asrContext"];

    nugu_dbg("Parsing ExpectSpeech directive");

    for (auto asr_listener : asr_listeners) {
        asr_listener->onState(ASRState::EXPECTING_SPEECH, getRecognizeDialogId());
    }

    playsync_manager->setExpectSpeech(true);

    capa_helper->requestFocus("expect", NULL);
}

void ASRAgent::parsingNotifyResult(const char* message)
{
    Json::Value root;
    Json::Reader reader;
    std::string result;
    std::string state;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    result = root["result"].asString();
    state = root["state"].asString();

    if (state.size() == 0) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    clearResponseTimeout();

    for (auto asr_listener : asr_listeners) {
        if (state == "PARTIAL")
            asr_listener->onPartial(result, getRecognizeDialogId());
        else if (state == "COMPLETE")
            asr_listener->onComplete(result, getRecognizeDialogId());
        else if (state == "NONE")
            asr_listener->onNone(getRecognizeDialogId());
    }

    if (state == "ERROR") {
        nugu_error("NotifyResult.state is ERROR");
        releaseASRFocus(false, ASRError::RECOGNIZE_ERROR);
    }
}

void ASRAgent::onListeningState(ListeningState state)
{
    switch (state) {
    case ListeningState::READY: {
        nugu_dbg("ListeningState::READY");

        clearResponseTimeout();

        if (rec_event)
            delete rec_event;

        rec_event = new CapabilityEvent("Recognize", this);
        rec_event->setType(NUGU_EVENT_TYPE_WITH_ATTACHMENT);

        playsync_manager->holdContext();

        sendEventRecognize(NULL, 0, false, [&](const std::string& ename, const std::string& msg_id, const std::string& dialog_id, bool success, int code) {
            nugu_dbg("receive %s.%s(%s) result %d(code:%d)", getName().c_str(), ename.c_str(), msg_id.c_str(), success, code);
        });

        std::string id = getRecognizeDialogId();
        nugu_dbg("user request dialog id: %s", id.c_str());
        if (rec_callback != nullptr && id.size())
            rec_callback(id);
        break;
    }
    case ListeningState::LISTENING:
        nugu_dbg("ListeningState::LISTENING");

        for (auto asr_listener : asr_listeners) {
            asr_listener->onState(ASRState::LISTENING, getRecognizeDialogId());
        }

        break;
    case ListeningState::SPEECH_START:
        nugu_dbg("ListeningState::SPEECH_START");

        for (auto asr_listener : asr_listeners) {
            asr_listener->onState(ASRState::RECOGNIZING, getRecognizeDialogId());
        }

        break;
    case ListeningState::SPEECH_END:
        nugu_dbg("ListeningState::SPEECH_END");
        nugu_info("speech end detected");

        speech_recognizer->stopListening();

        sendEventRecognize(NULL, 0, true);
        checkResponseTimeout();

        for (auto asr_listener : asr_listeners) {
            asr_listener->onState(ASRState::BUSY, getRecognizeDialogId());
        }

        break;
    case ListeningState::TIMEOUT:
        nugu_dbg("ListeningState::TIMEOUT");
        nugu_info("time out");

        if (rec_event)
            rec_event->forceClose();

        stopRecognition();
        sendEventListenTimeout();
        releaseASRFocus(false, ASRError::LISTEN_TIMEOUT);

        break;
    case ListeningState::FAILED:
        nugu_dbg("ListeningState::FAILED");

        if (rec_event)
            rec_event->forceClose();

        stopRecognition();
        releaseASRFocus(false, ASRError::LISTEN_FAILED);

        break;
    case ListeningState::DONE:
        nugu_dbg("ListeningState::DONE");

        if (rec_event) {
            delete rec_event;
            rec_event = nullptr;
        }

        // it consider cancel by user
        if (prev_listening_state == ListeningState::READY
            || prev_listening_state == ListeningState::LISTENING
            || prev_listening_state == ListeningState::SPEECH_START) {
            releaseASRFocus(true, ASRError::UNKNOWN);
        }

        break;
    }

    prev_listening_state = state;
}

void ASRAgent::releaseASRFocus(bool is_cancel, ASRError error)
{
    for (auto asr_listener : asr_listeners) {
        if (is_cancel)
            asr_listener->onCancel(getRecognizeDialogId());
        else
            asr_listener->onError(error, getRecognizeDialogId());
    }

    capa_helper->releaseFocus("asr");
    playsync_manager->onASRError();
}

bool ASRAgent::isExpectSpeechState()
{
    if (es_attr.is_handle)
        return true;
    else
        return false;
}

ListeningState ASRAgent::getListeningState()
{
    return prev_listening_state;
}

void ASRAgent::resetExpectSpeechState()
{
    if (es_attr.is_handle)
        es_attr = {};
}

/*
 * The callback is invoked in the thread context.
 */
void ASRAgent::onRecordData(unsigned char* buf, int length)
{
    sendEventRecognize((unsigned char*)buf, length, false);
}

} // NuguCapability
