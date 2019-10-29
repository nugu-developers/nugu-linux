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
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <interface/nugu_configuration.hh>

#include "asr_agent.hh"
#include "nugu_config.h"
#include "nugu_log.h"

namespace NuguCore {

static const std::string capability_version = "1.0";

class ASRFocusListener : public IFocusListener {
public:
    explicit ASRFocusListener(ASRAgent* agent, SpeechRecognizer* speech_recognizer);
    virtual ~ASRFocusListener();

    NuguFocusResult onFocus(NuguFocusResource rsrc, void* event) override;
    NuguFocusResult onUnfocus(NuguFocusResource rsrc, void* event) override;
    NuguFocusStealResult onStealRequest(NuguFocusResource rsrc, void* event, NuguFocusType target_type) override;

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

NuguFocusResult ASRFocusListener::onFocus(NuguFocusResource rsrc, void* event)
{

    if (rsrc == NUGU_FOCUS_RESOURCE_MIC) {
        agent->saveAllContextInfo();
        if (agent->isExpectSpeechState()) {
            agent->resetExpectSpeechState();
            CapabilityManager::getInstance()->releaseFocus("expect", NUGU_FOCUS_RESOURCE_SPK);
        }
        CapabilityManager::getInstance()->requestFocus("asr", NUGU_FOCUS_RESOURCE_SPK, NULL);
    } else if (rsrc == NUGU_FOCUS_RESOURCE_SPK) {
        speech_recognizer->startListening();
    }

    return NUGU_FOCUS_OK;
}

NuguFocusResult ASRFocusListener::onUnfocus(NuguFocusResource rsrc, void* event)
{
    if (rsrc == NUGU_FOCUS_RESOURCE_MIC) {
        speech_recognizer->stopListening();
    } else if (rsrc == NUGU_FOCUS_RESOURCE_SPK) {
        auto agent_listeners = agent->getListener();

        for (auto asr_listener : agent_listeners) {
            asr_listener->onState(ASRState::IDLE);
            agent->resetExpectSpeechState();
        }
    }

    return NUGU_FOCUS_REMOVE;
}

NuguFocusStealResult ASRFocusListener::onStealRequest(NuguFocusResource rsrc, void* event, NuguFocusType target_type)
{
    if (rsrc == NUGU_FOCUS_RESOURCE_MIC)
        return NUGU_FOCUS_STEAL_REJECT;

    return NUGU_FOCUS_STEAL_ALLOW;
}

class ExpectFocusListener : public IFocusListener {
public:
    explicit ExpectFocusListener(ASRAgent* agent);
    virtual ~ExpectFocusListener();

    NuguFocusResult onFocus(NuguFocusResource rsrc, void* event) override;
    NuguFocusResult onUnfocus(NuguFocusResource rsrc, void* event) override;
    NuguFocusStealResult onStealRequest(NuguFocusResource rsrc, void* event, NuguFocusType target_type) override;

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

NuguFocusResult ExpectFocusListener::onFocus(NuguFocusResource rsrc, void* event)
{
    agent->startRecognition();

    return NUGU_FOCUS_OK;
}

NuguFocusResult ExpectFocusListener::onUnfocus(NuguFocusResource rsrc, void* event)
{
    return NUGU_FOCUS_REMOVE;
}

NuguFocusStealResult ExpectFocusListener::onStealRequest(NuguFocusResource rsrc, void* event, NuguFocusType target_type)
{
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
        nugu_event_free(rec_event);
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

    epd_type = nugu_config_get(NuguConfig::Key::ASR_EPD_TYPE.c_str());
    asr_encoding = nugu_config_get(NuguConfig::Key::ASR_ENCODING.c_str());

    speech_recognizer = std::unique_ptr<SpeechRecognizer>(new SpeechRecognizer());
    speech_recognizer->setListener(this);

    std::string timeout = nugu_config_get(NuguConfig::Key::SERVER_RESPONSE_TIMEOUT_MSEC.c_str());
    timer = nugu_timer_new(std::stoi(timeout), 1);
    nugu_timer_set_callback(
        timer, [](void* userdata) {
            ASRAgent* asr = static_cast<ASRAgent*>(userdata);
            asr->sendEventResponseTimeout();
            asr->releaseASRSpeakFocus(false, ASRError::RESPONSE_TIMEOUT);
        },
        this);

    asr_focus_listener = new ASRFocusListener(this, this->speech_recognizer.get());
    expect_focus_listener = new ExpectFocusListener(this);

    initialized = true;
}

void ASRAgent::startRecognition()
{
    CapabilityManager::getInstance()->requestFocus("asr", NUGU_FOCUS_RESOURCE_MIC, NULL);
}

void ASRAgent::stopRecognition()
{
    CapabilityManager::getInstance()->releaseFocus("asr", NUGU_FOCUS_RESOURCE_MIC);
}

void ASRAgent::processDirective(NuguDirective* ndir)
{
    const char* dname;
    const char* message;

    message = nugu_directive_peek_json(ndir);
    dname = nugu_directive_peek_name(ndir);

    if (!message || !dname) {
        nugu_error("directive message is not correct");
        destoryDirective(ndir);
        return;
    }

    // directive name check
    if (!strcmp(dname, "ExpectSpeech"))
        parsingExpectSpeech(message);
    else if (!strcmp(dname, "NotifyResult"))
        parsingNotifyResult(message);
    else
        nugu_warn("%s[%s] is not support %s directive", getName().c_str(), getVersion().c_str(), dname);

    destoryDirective(ndir);
}

void ASRAgent::updateInfoForContext(Json::Value& ctx)
{
    Json::Value asr;

    asr["version"] = getVersion();
    ctx[getName()] = asr;
}

std::string ASRAgent::getContextInfo()
{
    Json::Value ctx;
    CapabilityManager* cmanager = CapabilityManager::getInstance();

    updateInfoForContext(ctx);
    return cmanager->makeContextInfo(ctx);
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
            CapabilityManager::getInstance()->releaseFocus("expect", NUGU_FOCUS_RESOURCE_SPK);
            es_attr = {};
        }
    } else if (!command.compare("releasefocus")) {
        if (from == CapabilityType::System) {
            if (dialog_id == param)
                CapabilityManager::getInstance()->releaseFocus("asr", NUGU_FOCUS_RESOURCE_SPK);
        } else {
            CapabilityManager::getInstance()->releaseFocus("asr", NUGU_FOCUS_RESOURCE_SPK);
        }
    }
}

void ASRAgent::getProperty(const std::string& property, std::string& value)
{
    if (property == "playServiceId") {
        value = es_attr.play_service_id;
    } else if (property == "property") {
        value = es_attr.property;
    } else if (property == "sessionId") {
        value = es_attr.session_id;
    } else {
        value = "";
    }
}

void ASRAgent::getProperties(const std::string& property, std::list<std::string>& values)
{
    if (property == "domainTypes") {
        for (int i = 0; i < (int)es_attr.domain_types.size(); i++) {
            values.push_back(es_attr.domain_types[i].asString());
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
    std::string event_json;

    if (!rec_event) {
        nugu_error("recorder event is not created");
        return;
    }

    dialog_id = nugu_event_peek_dialog_id(rec_event);

    nugu_event_set_context(rec_event, all_context_info.c_str());

    root["codec"] = "SPEEX";
    root["property"] = "NORMAL";
    root["language"] = "KOR";
    root["endpointing"] = epd_type;
    root["encoding"] = asr_encoding;

    if (es_attr.is_handle) {
        root["sessionId"] = es_attr.session_id;
        root["playServiceId"] = es_attr.play_service_id;
        root["domainTypes"] = es_attr.domain_types;

        if (!es_attr.property.empty()) {
            root["property"] = es_attr.property;
        }
    }

    event_json = writer.write(root);

    nugu_event_set_json(rec_event, event_json.c_str());

    sendEvent(rec_event, is_end, length, data);
}

void ASRAgent::sendEventResponseTimeout()
{
    NuguEvent* event;

    event = nugu_event_new(getName().c_str(), "ResponseTimeout",
        getVersion().c_str());

    nugu_event_set_context(event, getContextInfo().c_str());

    if (es_attr.is_handle) {
        Json::StyledWriter writer;
        Json::Value root;
        std::string event_json;

        root["playServiceId"] = es_attr.play_service_id;
        event_json = writer.write(root);
        nugu_event_set_json(event, event_json.c_str());
    }

    sendEvent(event);
    nugu_event_free(event);
}

void ASRAgent::sendEventListenTimeout()
{
    NuguEvent* event;

    event = nugu_event_new(getName().c_str(), "ListenTimeout",
        getVersion().c_str());

    nugu_event_set_context(event, getContextInfo().c_str());

    if (es_attr.is_handle) {
        Json::StyledWriter writer;
        Json::Value root;
        std::string event_json;

        root["playServiceId"] = es_attr.play_service_id;
        event_json = writer.write(root);
        nugu_event_set_json(event, event_json.c_str());
    }

    sendEvent(event);
    nugu_event_free(event);
}

void ASRAgent::sendEventListenFailed()
{
    NuguEvent* event;

    event = nugu_event_new(getName().c_str(), "ListenFailed",
        getVersion().c_str());

    nugu_event_set_context(event, getContextInfo().c_str());

    if (es_attr.is_handle) {
        Json::StyledWriter writer;
        Json::Value root;
        std::string event_json;

        root["playServiceId"] = es_attr.play_service_id;
        event_json = writer.write(root);
        nugu_event_set_json(event, event_json.c_str());
    }

    sendEvent(event);
    nugu_event_free(event);
}

void ASRAgent::sendEventStopRecognize()
{
    NuguEvent* event;

    event = nugu_event_new(getName().c_str(), "StopRecognize",
        getVersion().c_str());

    nugu_event_set_context(event, getContextInfo().c_str());

    if (es_attr.is_handle) {
        Json::StyledWriter writer;
        Json::Value root;
        std::string event_json;

        root["playServiceId"] = es_attr.play_service_id;
        event_json = writer.write(root);
        nugu_event_set_json(event, event_json.c_str());
    }

    sendEvent(event);
    nugu_event_free(event);
}

void ASRAgent::setCapabilityListener(ICapabilityListener* clistener)
{
    if (clistener)
        addListener(dynamic_cast<IASRListener*>(clistener));
}

void ASRAgent::addListener(IASRListener* listener)
{
    if (listener && std::find(asr_listeners.begin(), asr_listeners.end(), listener) == asr_listeners.end()) {
        asr_listeners.push_back(listener);
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

    for (auto asr_listener : asr_listeners) {
        asr_listener->onState(ASRState::EXPECTING_SPEECH);
    }

    playsync_manager->setExpectSpeech(true);

    CapabilityManager::getInstance()->requestFocus("expect", NUGU_FOCUS_RESOURCE_SPK, NULL);
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
            nugu_event_free(rec_event);

        rec_event = nugu_event_new(getName().c_str(), "Recognize", getVersion().c_str());

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

        stopRecognition();
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
        releaseASRSpeakFocus(false, ASRError::LISTEN_TIMEOUT);

        break;
    case ListeningState::FAILED:
        nugu_dbg("ListeningState::FAILED");

        stopRecognition();
        releaseASRSpeakFocus(false, ASRError::LISTEN_FAILED);

        break;
    case ListeningState::DONE:
        nugu_dbg("ListeningState::DONE");

        if (rec_event) {
            nugu_event_free(rec_event);
            rec_event = NULL;
        }

        // it consider cancel by user
        if (prev_listening_state == ListeningState::READY
            || prev_listening_state == ListeningState::LISTENING
            || prev_listening_state == ListeningState::SPEECH_START) {

            releaseASRSpeakFocus(true, ASRError::UNKNOWN);
        }

        break;
    }

    prev_listening_state = state;
}

void ASRAgent::releaseASRSpeakFocus(bool is_cancel, ASRError error)
{
    for (auto asr_listener : asr_listeners) {
        if (is_cancel)
            asr_listener->onCancel();
        else
            asr_listener->onError(error);
    }

    CapabilityManager::getInstance()->releaseFocus("asr", NUGU_FOCUS_RESOURCE_SPK);
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
