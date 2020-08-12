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

#include "base/nugu_log.h"
#include "base/nugu_prof.h"

#include "asr_agent.hh"

namespace NuguCapability {

static const char* CAPABILITY_NAME = "ASR";
static const char* CAPABILITY_VERSION = "1.3";

ASRAgent::ASRAgent()
    : Capability(CAPABILITY_NAME, CAPABILITY_VERSION)
    , es_attr({})
    , rec_event(nullptr)
    , timer(nullptr)
    , uniq(0)
    , focus_state(FocusState::NONE)
    , cur_state(ASRState::IDLE)
    , asr_cancel(false)
    , model_path("")
    , epd_type(NUGU_ASR_EPD_TYPE)
    , asr_encoding(NUGU_ASR_ENCODING)
    , response_timeout(NUGU_SERVER_RESPONSE_TIMEOUT_SEC)
    , epd_attribute({})
    , wakeup_power_noise(0)
    , wakeup_power_speech(0)
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

    // It just bypass, because the validation is checked in createSpeechRecognizer().
    epd_attribute.epd_timeout = attribute.epd_timeout;
    epd_attribute.epd_max_duration = attribute.epd_max_duration;
}

void ASRAgent::initialize()
{
    if (initialized) {
        nugu_info("It's already initialized.");
        return;
    }

    speech_recognizer = std::unique_ptr<ISpeechRecognizer>(core_container->createSpeechRecognizer(model_path, epd_attribute));
    speech_recognizer->setListener(this);

    timer = core_container->createNuguTimer();
    timer->setInterval(response_timeout * NUGU_TIMER_UNIT_SEC);
    timer->setCallback([&](int count, int repeat) {
        sendEventResponseTimeout();
        releaseASRFocus(false, ASRError::RESPONSE_TIMEOUT);
    });

    addReferrerEvents("Recognize", "ExpectSpeech");
    addReferrerEvents("ResponseTimeout", "ASRAgent.sendEventRecognize");
    addReferrerEvents("ListenTimeout", "ASRAgent.sendEventRecognize");
    addReferrerEvents("RecognizeResult", "ASRAgent.sendEventRecognize");
    addReferrerEvents("StopRecognize", "ASRAgent.sendEventRecognize");
    addReferrerEvents("ListenFailed", "ASRAgent.sendEventRecognize");

    addBlockingPolicy("ExpectSpeech", { BlockingMedium::AUDIO, true });

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

    speech_recognizer.reset();

    initialized = false;
}

void ASRAgent::suspend()
{
    stopRecognition();
}

void ASRAgent::startRecognition(float power_noise, float power_speech, AsrRecognizeCallback callback)
{
    wakeup_power_noise = power_noise;
    wakeup_power_speech = power_speech;
    startRecognition(std::move(callback));
}

void ASRAgent::startRecognition(AsrRecognizeCallback callback)
{
    if (callback)
        rec_callback = std::move(callback);

    if (speech_recognizer->isMute()) {
        nugu_warn("recorder is mute state");
        if (rec_callback)
            rec_callback("");
        return;
    }

    focus_manager->requestFocus(DIALOG_FOCUS_TYPE, CAPABILITY_NAME, this);
    asr_cancel = false;
}

void ASRAgent::stopRecognition()
{
    nugu_dbg("stopRecognition()");
    focus_manager->unholdFocus(DIALOG_FOCUS_TYPE);
    focus_manager->releaseFocus(DIALOG_FOCUS_TYPE, CAPABILITY_NAME);
    asr_cancel = false;
}

void ASRAgent::cancelRecognition()
{
    nugu_dbg("cancelRecognition()");
    asr_cancel = true;
    speech_recognizer->stopListening();
}

void ASRAgent::onFocusChanged(FocusState state)
{
    nugu_info("Focus Changed(%s -> %s)", focus_manager->getStateString(focus_state).c_str(), focus_manager->getStateString(state).c_str());

    switch (state) {
    case FocusState::FOREGROUND: {
        playstack_manager->stopHolding();

        saveAllContextInfo();

        std::string id = "id#" + std::to_string(uniq++);
        setListeningId(id);
        speech_recognizer->startListening(id);
    } break;
    case FocusState::BACKGROUND:
        focus_manager->releaseFocus(DIALOG_FOCUS_TYPE, CAPABILITY_NAME);
        break;
    case FocusState::NONE:
        playstack_manager->resetHolding();

        speech_recognizer->stopListening();

        if (getASRState() == ASRState::LISTENING
            || getASRState() == ASRState::RECOGNIZING
            || getASRState() == ASRState::EXPECTING_SPEECH)
            sendEventStopRecognize();

        if (getASRState() == ASRState::BUSY)
            setASRState(ASRState::IDLE);

        resetExpectSpeechState();
        break;
    }
    focus_state = state;
}

void ASRAgent::preprocessDirective(NuguDirective* ndir)
{
    const char* dname;
    const char* message;

    if (!ndir
        || !(dname = nugu_directive_peek_name(ndir))
        || !(message = nugu_directive_peek_json(ndir))) {
        nugu_error("The directive info is not exist.");
        return;
    }

    if (!strcmp(dname, "ExpectSpeech"))
        parsingExpectSpeech(std::string(nugu_directive_peek_dialog_id(ndir)), message);
}

void ASRAgent::parsingDirective(const char* dname, const char* message)
{
    if (!strcmp(dname, "ExpectSpeech"))
        handleExpectSpeech();
    else if (!strcmp(dname, "NotifyResult"))
        parsingNotifyResult(message);
    else if (!strcmp(dname, "CancelRecognize"))
        parsingCancelRecognize(message);
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
    all_context_info = capa_helper->makeAllContextInfo();
}

bool ASRAgent::receiveCommand(const std::string& from, const std::string& command, const std::string& param)
{
    std::string convert_command;
    convert_command.resize(command.size());
    std::transform(command.cbegin(), command.cend(), convert_command.begin(), ::tolower);

    nugu_dbg("receive command(%s) from %s", command.c_str(), from.c_str());

    if (convert_command == "releasefocus") {
        if (param.find("TTS") == std::string::npos) {
            nugu_info("Focus(type: %s) Release", DIALOG_FOCUS_TYPE);
            stopRecognition();
        }
    } else if (convert_command == "cancel") {
        cancelRecognition();
    } else {
        nugu_error("invalid command: %s", command.c_str());
        return false;
    }

    return true;
}

bool ASRAgent::getProperty(const std::string& property, std::string& value)
{
    value = "";

    if (property == "es.playServiceId") {
        value = es_attr.play_service_id;
    } else if (property == "es.asrContext") {
        if (!es_attr.asr_context.empty()) {
            Json::StyledWriter writer;
            value = writer.write(es_attr.asr_context);
        }
    } else {
        nugu_error("invalid property: %s", property.c_str());
        return false;
    }

    return true;
}

bool ASRAgent::getProperties(const std::string& property, std::list<std::string>& values)
{
    values.clear();

    if (property == "es.domainTypes") {
        for (int i = 0; i < (int)es_attr.domain_types.size(); i++) {
            values.emplace_back(es_attr.domain_types[i].asString());
        }
    } else {
        nugu_error("invalid property: %s", property.c_str());
        return false;
    }

    return true;
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

    if (wakeup_power_noise && wakeup_power_speech) {
        root["wakeup"]["power"]["noise"] = wakeup_power_noise;
        root["wakeup"]["power"]["speech"] = wakeup_power_speech;
    }

    if (es_attr.is_handle) {
        if (es_attr.play_service_id.size())
            root["playServiceId"] = es_attr.play_service_id;
        if (!es_attr.domain_types.empty())
            root["domainTypes"] = es_attr.domain_types;
        if (!es_attr.asr_context.empty())
            root["asrContext"] = es_attr.asr_context;
    } else {
        // reset referrer dialog request id
        setReferrerDialogRequestId("ExpectSpeech", "");
    }
    payload = writer.write(root);

    dialog_id = rec_event->getDialogRequestId();
    setReferrerDialogRequestId("ASRAgent.sendEventRecognize", dialog_id);

    if (!is_end && (length == 0 || data == nullptr)) {
        nugu_prof_mark_data(NUGU_PROF_TYPE_ASR_RECOGNIZE,
            dialog_id.c_str(),
            rec_event->getMessageId().c_str(),
            payload.c_str());

        sendEvent(rec_event, all_context_info, payload, std::move(cb));
    } else
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

void ASRAgent::parsingExpectSpeech(std::string&& dialog_id, const char* message)
{
    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    if (root["timeoutInMilliseconds"].empty()) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    es_attr.is_handle = true;
    es_attr.timeout = root["timeoutInMilliseconds"].asString();
    es_attr.play_service_id = root["playServiceId"].asString();
    es_attr.domain_types = root["domainTypes"];
    es_attr.asr_context = root["asrContext"];
    es_attr.dialog_id = dialog_id;

    session_manager->activate(es_attr.dialog_id);
    interaction_control_manager->start(InteractionMode::MULTI_TURN, getName());
    focus_manager->holdFocus(DIALOG_FOCUS_TYPE);
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

    nugu_prof_mark(NUGU_PROF_TYPE_ASR_RESULT);

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

void ASRAgent::parsingCancelRecognize(const char* message)
{
    Json::Value root;
    Json::Reader reader;
    std::string cause;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    cause = root["cause"].asString();
    if (cause.size() == 0) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    clearResponseTimeout();
    nugu_warn("CancelRecognize.cause => %s", cause.c_str());

    if (cause == "WAKEUP_POWER")
        releaseASRFocus(true, ASRError::UNKNOWN, true);
}

void ASRAgent::handleExpectSpeech()
{
    if (es_attr.is_handle) {
        setASRState(ASRState::EXPECTING_SPEECH);
        focus_manager->requestFocus(DIALOG_FOCUS_TYPE, CAPABILITY_NAME, this);
        asr_cancel = false;
    }
}

void ASRAgent::onListeningState(ListeningState state, const std::string& id)
{
    switch (state) {
    case ListeningState::READY:
        nugu_dbg("[%s] ListeningState::READY", id.c_str());
        clearResponseTimeout();
        break;
    case ListeningState::LISTENING: {
        nugu_dbg("[%s] ListeningState::LISTENING", id.c_str());

        if (rec_event)
            delete rec_event;

        rec_event = new CapabilityEvent("Recognize", this);
        rec_event->setType(NUGU_EVENT_TYPE_WITH_ATTACHMENT);

        sendEventRecognize(NULL, 0, false,
            [&](const std::string& ename, const std::string& msg_id, const std::string& dialog_id, bool success, int code) {
                nugu_dbg("receive %s.%s(%s) result %d(code:%d)", getName().c_str(), ename.c_str(), msg_id.c_str(), success, code);
            });

        wakeup_power_noise = 0;
        wakeup_power_speech = 0;

        std::string id = getRecognizeDialogId();
        nugu_dbg("user request dialog id: %s", id.c_str());

        if (rec_callback && !id.empty())
            rec_callback(id);

        setASRState(ASRState::LISTENING);
        break;
    }
    case ListeningState::SPEECH_START:
        nugu_dbg("[%s] ListeningState::SPEECH_START", id.c_str());
        setASRState(ASRState::RECOGNIZING);
        break;
    case ListeningState::SPEECH_END:
        nugu_dbg("[%s] ListeningState::SPEECH_END", id.c_str());
        break;
    case ListeningState::TIMEOUT:
        nugu_dbg("[%s] ListeningState::TIMEOUT", id.c_str());
        break;
    case ListeningState::FAILED:
        nugu_dbg("[%s] ListeningState::FAILED", id.c_str());
        break;
    case ListeningState::DONE:
        nugu_dbg("[%s] ListeningState::DONE", id.c_str());

        if (asr_cancel) {
            nugu_dbg("ASR cancel");
            setASRState(ASRState::IDLE);
            if (rec_event) {
                rec_event->forceClose();
                delete rec_event;
                rec_event = nullptr;
            }
            break;
        }

        if (prev_listening_state == ListeningState::READY || prev_listening_state == ListeningState::LISTENING) {
            nugu_dbg("PrevListeningState::%s", prev_listening_state == ListeningState::READY ? "READY" : "LISTENING");
            releaseASRFocus(true, ASRError::UNKNOWN, (request_listening_id == id));
        } else if (prev_listening_state == ListeningState::SPEECH_END) {
            nugu_dbg("PrevListeningState::SPEECH_END");
            sendEventRecognize(NULL, 0, true);
            checkResponseTimeout();
            setASRState(ASRState::BUSY);
        } else if (prev_listening_state == ListeningState::TIMEOUT) {
            nugu_dbg("PrevListeningState::TIMEOUT");
            if (rec_event)
                rec_event->forceClose();

            sendEventListenTimeout();
            releaseASRFocus(false, ASRError::LISTEN_TIMEOUT, (request_listening_id == id));
        } else if (prev_listening_state == ListeningState::FAILED) {
            nugu_dbg("PrevListeningState::FAILED");
            if (rec_event)
                rec_event->forceClose();
            releaseASRFocus(false, ASRError::LISTEN_FAILED, (request_listening_id == id));
        }

        if (rec_event) {
            delete rec_event;
            rec_event = nullptr;
        }

        if (prev_listening_state != ListeningState::SPEECH_END)
            setASRState(ASRState::IDLE);
        break;
    }

    prev_listening_state = state;
}

void ASRAgent::releaseASRFocus(bool is_cancel, ASRError error, bool release_focus)
{
    for (auto asr_listener : asr_listeners) {
        if (is_cancel)
            asr_listener->onCancel(getRecognizeDialogId());
        else
            asr_listener->onError(error, getRecognizeDialogId());
    }

    if (release_focus) {
        nugu_dbg("request to release focus");
        focus_manager->releaseFocus(DIALOG_FOCUS_TYPE, CAPABILITY_NAME);
    }
}

bool ASRAgent::isExpectSpeechState()
{
    return es_attr.is_handle;
}

ListeningState ASRAgent::getListeningState()
{
    return prev_listening_state;
}

void ASRAgent::resetExpectSpeechState()
{
    if (!es_attr.dialog_id.empty())
        session_manager->deactivate(es_attr.dialog_id);

    if (es_attr.is_handle)
        es_attr = {};

    interaction_control_manager->finish(InteractionMode::MULTI_TURN, getName());
}

/*
 * The callback is invoked in the thread context.
 */
void ASRAgent::onRecordData(unsigned char* buf, int length)
{
    sendEventRecognize((unsigned char*)buf, length, false);
}

void ASRAgent::setASRState(ASRState state)
{
    if (state == cur_state)
        return;

    nugu_info("[asr state] %d => %d", cur_state, state);

    cur_state = state;

    for (auto asr_listener : asr_listeners)
        asr_listener->onState(state, getRecognizeDialogId());
}

ASRState ASRAgent::getASRState()
{
    return cur_state;
}

void ASRAgent::setListeningId(const std::string& id)
{
    request_listening_id = id;
    nugu_dbg("startListening with new id(%s)", request_listening_id.c_str());
}

void ASRAgent::notifyEventResponse(const std::string& msg_id, const std::string& data, bool success)
{
    // release focus when there are no focus stealer like TTS
    if (data.find("ASR") != std::string::npos && data.find("NotifyResult") != std::string::npos
        && data.find("TTS") == std::string::npos) {
        nugu_info("Focus(type: %s) Release", DIALOG_FOCUS_TYPE);
        stopRecognition();
    }
}

} // NuguCapability
