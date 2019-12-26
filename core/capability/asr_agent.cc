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

#include <string.h>

#include <chrono>

#include "base/nugu_log.h"
#include "interface/nugu_configuration.hh"

#include "asr_agent.hh"

namespace NuguCore {

static const std::string capability_version = "1.0";

class ASRFocusListener : public IFocusListener {
public:
    explicit ASRFocusListener(ASRAgent* agent, SpeechRecognizer* speech_recognizer);
    virtual ~ASRFocusListener();

    NuguFocusResult onFocus(void* event) override;
    NuguFocusResult onUnfocus(void* event) override;
    NuguFocusStealResult onStealRequest(void* event, NuguFocusType target_type) override;

private:
    ASRAgent* agent;
    SpeechRecognizer* speech_recognizer;
};

ASRFocusListener::ASRFocusListener(ASRAgent* agent, SpeechRecognizer* speech_recognizer)
    : agent(agent)
    , speech_recognizer(speech_recognizer)

{
    CapabilityManager::getInstance()->addFocus("asr", NUGU_FOCUS_TYPE_ASR, this);
}

ASRFocusListener::~ASRFocusListener()
{
    CapabilityManager::getInstance()->removeFocus("asr");
}

NuguFocusResult ASRFocusListener::onFocus(void* event)
{
    nugu_dbg("ASRFocusListener::onFocus");
    agent->saveAllContextInfo();

    speech_recognizer->startListening();

    if (agent->isExpectSpeechState()) {
        agent->resetExpectSpeechState();
        CapabilityManager::getInstance()->releaseFocus("expect");
    }

    return NUGU_FOCUS_OK;
}

NuguFocusResult ASRFocusListener::onUnfocus(void* event)
{
    nugu_dbg("ASRFocusListener::onUnfocus");
    speech_recognizer->stopListening();

    auto agent_listeners = agent->getListener();

    for (auto asr_listener : agent_listeners) {
        asr_listener->onState(ASRState::IDLE);
        agent->resetExpectSpeechState();
    }

    return NUGU_FOCUS_REMOVE;
}

NuguFocusStealResult ASRFocusListener::onStealRequest(void* event, NuguFocusType target_type)
{
    return NUGU_FOCUS_STEAL_ALLOW;
}

class ExpectFocusListener : public IFocusListener {
public:
    explicit ExpectFocusListener(ASRAgent* agent);
    virtual ~ExpectFocusListener();

    NuguFocusResult onFocus(void* event) override;
    NuguFocusResult onUnfocus(void* event) override;
    NuguFocusStealResult onStealRequest(void* event, NuguFocusType target_type) override;

private:
    ASRAgent* agent;
};

ExpectFocusListener::ExpectFocusListener(ASRAgent* agent)
    : agent(agent)
{
    CapabilityManager::getInstance()->addFocus("expect", NUGU_FOCUS_TYPE_ASR_EXPECT, this);
}

ExpectFocusListener::~ExpectFocusListener()
{
    CapabilityManager::getInstance()->removeFocus("expect");
}

NuguFocusResult ExpectFocusListener::onFocus(void* event)
{
    nugu_dbg("ExpectFocusListener::onFocus");
    agent->startRecognition();

    return NUGU_FOCUS_OK;
}

NuguFocusResult ExpectFocusListener::onUnfocus(void* event)
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
    : Capability(CapabilityType::ASR, capability_version)
    , es_attr({})
    , rec_event(nullptr)
    , timer(nullptr)
    , mic_off(false)
    , asr_focus_listener(nullptr)
    , expect_focus_listener(nullptr)
{
}

ASRAgent::~ASRAgent()
{
    if (timer) {
        nugu_timer_delete(timer);
        timer = nullptr;
    }

    if (rec_event) {
        delete rec_event;
        rec_event = nullptr;
    }

    delete asr_focus_listener;
    delete expect_focus_listener;
}

void ASRAgent::initialize()
{
    if (initialized) {
        nugu_info("It's already initialized.");
        return;
    }

    epd_type = NuguConfig::getValue(NuguConfig::Key::ASR_EPD_TYPE);
    asr_encoding = NuguConfig::getValue(NuguConfig::Key::ASR_ENCODING);

    speech_recognizer = std::unique_ptr<SpeechRecognizer>(new SpeechRecognizer());
    speech_recognizer->setListener(this);

    std::string timeout = NuguConfig::getValue(NuguConfig::Key::SERVER_RESPONSE_TIMEOUT_MSEC);
    timer = nugu_timer_new(std::stoi(timeout), 1);
    nugu_timer_set_callback(
        timer, [](void* userdata) {
            ASRAgent* asr = static_cast<ASRAgent*>(userdata);
            asr->sendEventResponseTimeout();
            asr->releaseASRFocus(false, ASRError::RESPONSE_TIMEOUT);
        },
        this);

    asr_focus_listener = new ASRFocusListener(this, this->speech_recognizer.get());
    expect_focus_listener = new ExpectFocusListener(this);

    initialized = true;
}

void ASRAgent::startRecognition()
{
    nugu_dbg("startRecognition()");
    CapabilityManager::getInstance()->requestFocus("asr", NULL);
}

void ASRAgent::stopRecognition()
{
    nugu_dbg("stopRecognition()");
    CapabilityManager::getInstance()->releaseFocus("asr");
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
    all_context_info = CapabilityManager::getInstance()->makeAllContextInfoStack();
}

void ASRAgent::receiveCommand(CapabilityType from, std::string command, const std::string& param)
{
    std::transform(command.begin(), command.end(), command.begin(), ::tolower);

    if (!command.compare("wakeup_detected")) {
        if (es_attr.is_handle) {
            CapabilityManager::getInstance()->releaseFocus("expect");
            es_attr = {};
        }
    } else if (!command.compare("releasefocus")) {
        if (from == CapabilityType::System) {
            if (dialog_id == param)
                CapabilityManager::getInstance()->releaseFocus("asr");
        } else {
            CapabilityManager::getInstance()->releaseFocus("asr");
        }
    }
}

void ASRAgent::getProperty(const std::string& property, std::string& value)
{
    if (property == "es.playServiceId") {
        value = es_attr.play_service_id;
    } else if (property == "es.property") {
        value = es_attr.property;
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
    nugu_timer_start(timer);
}

void ASRAgent::clearResponseTimeout()
{
    nugu_timer_stop(timer);
}

void ASRAgent::sendEventRecognize(unsigned char* data, size_t length, bool is_end)
{
    Json::StyledWriter writer;
    Json::Value root;
    std::string payload = "";

    if (!rec_event) {
        nugu_error("recognize event is not created");
        return;
    }

    root["codec"] = "SPEEX";
    root["property"] = "NORMAL";
    root["language"] = "KOR";
    root["endpointing"] = epd_type;
    root["encoding"] = asr_encoding;

    if (es_attr.is_handle) {
        root["sessionId"] = es_attr.session_id;
        root["playServiceId"] = es_attr.play_service_id;
        root["domainTypes"] = es_attr.domain_types;

        if (!es_attr.property.empty())
            root["property"] = es_attr.property;
    }
    payload = writer.write(root);

    dialog_id = rec_event->getDialogMessageId();
    if (!is_end && (length == 0 || data == nullptr))
        sendEvent(rec_event, all_context_info, payload);
    else
        sendAttachmentEvent(rec_event, is_end, length, data);
}

void ASRAgent::sendEventResponseTimeout()
{
    std::string ename = "ResponseTimeout";
    std::string payload = "";

    if (es_attr.is_handle) {
        Json::StyledWriter writer;
        Json::Value root;

        root["playServiceId"] = es_attr.play_service_id;
        payload = writer.write(root);
    }

    sendEvent(ename, getContextInfo(), payload);
}

void ASRAgent::sendEventListenTimeout()
{
    std::string ename = "ListenTimeout";
    std::string payload = "";

    if (es_attr.is_handle) {
        Json::StyledWriter writer;
        Json::Value root;

        root["playServiceId"] = es_attr.play_service_id;
        payload = writer.write(root);
    }

    sendEvent(ename, getContextInfo(), payload);
}

void ASRAgent::sendEventListenFailed()
{
    std::string ename = "ListenFailed";
    std::string payload = "";

    if (es_attr.is_handle) {
        Json::StyledWriter writer;
        Json::Value root;

        root["playServiceId"] = es_attr.play_service_id;
        payload = writer.write(root);
    }

    sendEvent(ename, getContextInfo(), payload);
}

void ASRAgent::sendEventStopRecognize()
{
    std::string ename = "StopRecognize";
    std::string payload = "";

    if (es_attr.is_handle) {
        Json::StyledWriter writer;
        Json::Value root;

        root["playServiceId"] = es_attr.play_service_id;
        payload = writer.write(root);
    }

    sendEvent(ename, getContextInfo(), payload);
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

    if (root["sessionId"].asString().empty()) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    es_attr.is_handle = true;
    es_attr.timeout = root["timeoutInMilliseconds"].asString();
    es_attr.session_id = root["sessionId"].asString();
    es_attr.play_service_id = root["playServiceId"].asString();
    es_attr.property = root["property"].asString();
    es_attr.domain_types = root["domainTypes"];

    nugu_dbg("Parsing ExpectSpeech directive");

    for (auto asr_listener : asr_listeners) {
        asr_listener->onState(ASRState::EXPECTING_SPEECH);
    }

    playsync_manager->setExpectSpeech(true);

    CapabilityManager::getInstance()->requestFocus("expect", NULL);
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
            asr_listener->onPartial(result);
        else if (state == "COMPLETE")
            asr_listener->onComplete(result);
        else if (state == "NONE")
            asr_listener->onNone();
        else if (state == "ERROR")
            asr_listener->onError(ASRError::RECOGNIZE_ERROR);
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

        playsync_manager->onMicOn();

        sendEventRecognize(NULL, 0, false);

        break;
    }
    case ListeningState::LISTENING:
        nugu_dbg("ListeningState::LISTENING");

        for (auto asr_listener : asr_listeners) {
            asr_listener->onState(ASRState::LISTENING);
        }

        break;
    case ListeningState::SPEECH_START:
        nugu_dbg("ListeningState::SPEECH_START");

        for (auto asr_listener : asr_listeners) {
            asr_listener->onState(ASRState::RECOGNIZING);
        }

        break;
    case ListeningState::SPEECH_END:
        nugu_dbg("ListeningState::SPEECH_END");
        nugu_info("speech end detected");

        speech_recognizer->stopListening();

        sendEventRecognize(NULL, 0, true);
        checkResponseTimeout();

        for (auto asr_listener : asr_listeners) {
            asr_listener->onState(ASRState::BUSY);
        }

        break;
    case ListeningState::TIMEOUT:
        nugu_dbg("ListeningState::TIMEOUT");
        nugu_info("time out");

        stopRecognition();
        sendEventListenTimeout();
        releaseASRFocus(false, ASRError::LISTEN_TIMEOUT);

        break;
    case ListeningState::FAILED:
        nugu_dbg("ListeningState::FAILED");

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
            asr_listener->onCancel();
        else
            asr_listener->onError(error);
    }

    CapabilityManager::getInstance()->releaseFocus("asr");
    playsync_manager->onASRError();
}

bool ASRAgent::isExpectSpeechState()
{
    if (es_attr.is_handle)
        return true;
    else
        return false;
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

} // NuguCore
