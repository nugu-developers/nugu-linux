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
#include "text_agent.hh"

namespace NuguCapability {

static const char* CAPABILITY_NAME = "Text";
static const char* CAPABILITY_VERSION = "1.5";

TextAgent::TextAgent()
    : Capability(CAPABILITY_NAME, CAPABILITY_VERSION)
    , text_listener(nullptr)
    , response_timeout(NUGU_SERVER_RESPONSE_TIMEOUT_SEC)
{
}

void TextAgent::setAttribute(TextAttribute&& attribute)
{
    if (attribute.response_timeout > 0)
        response_timeout = attribute.response_timeout;
}

void TextAgent::initialize()
{
    if (initialized) {
        nugu_info("It's already initialized.");
        return;
    }

    Capability::initialize();

    cur_state = TextState::IDLE;
    cur_dialog_id = "";
    dir_groups = "";
    interaction_mode = InteractionMode::NONE;
    handle_interaction_control = false;

    timer = core_container->createNuguTimer(true);
    timer->setInterval(response_timeout * NUGU_TIMER_UNIT_SEC);
    timer->setCallback([&]() {
        finishInteractionControl();
        notifyResponseTimeout();
    });

    timer_msec = core_container->createNuguTimer(true);
    timer_msec->setInterval(response_timeout);
    timer_msec->setCallback([&]() {
        if (!cur_dialog_id.size()) {
            nugu_warn("cur_dialog_id is cleared...");
            return;
        }

        if (text_listener)
            text_listener->onState(cur_state, cur_dialog_id);
    });
    addReferrerEvents("TextInput", "TextSource");

    initialized = true;
}

void TextAgent::deInitialize()
{
    if (timer) {
        delete timer;
        timer = nullptr;
    }

    if (timer_msec) {
        delete timer_msec;
        timer_msec = nullptr;
    }

    initialized = false;
}

void TextAgent::parsingDirective(const char* dname, const char* message)
{
    nugu_dbg("message: %s", message);

    // directive name check
    if (!strcmp(dname, "TextSource"))
        parsingTextSource(message);
    else if (!strcmp(dname, "TextRedirect"))
        parsingTextRedirect(message);
    else {
        nugu_warn("%s[%s] is not support %s directive",
            getName().c_str(), getVersion().c_str(), dname);
    }
}

void TextAgent::updateInfoForContext(Json::Value& ctx)
{
    Json::Value text;

    text["version"] = getVersion();

    ctx[getName()] = text;
}

void TextAgent::receiveCommandAll(const std::string& command, const std::string& param)
{
    std::string convert_command;
    convert_command.resize(command.size());
    std::transform(command.cbegin(), command.cend(), convert_command.begin(), ::tolower);

    if (convert_command == "receive_directive_group") {
        dir_groups = param;
    } else if (convert_command == "directive_dialog_id" && param == cur_dialog_id) {
        nugu_dbg("process receive command => directive_dialog_id(%s)", param.c_str());

        if (timer)
            timer->stop();

        if (text_listener)
            text_listener->onComplete(cur_dialog_id);

        cur_state = TextState::IDLE;
        if (text_listener)
            text_listener->onState(cur_state, cur_dialog_id);

        capa_helper->sendCommand("Text", "ASR", "releaseFocus", dir_groups);
        cur_dialog_id = "";

        if (dir_groups.find("TTS") == std::string::npos && dir_groups.find("AudioPlayer") == std::string::npos)
            playsync_manager->resetHolding();
    }
}

void TextAgent::setCapabilityListener(ICapabilityListener* clistener)
{
    if (clistener)
        text_listener = dynamic_cast<ITextListener*>(clistener);
}

std::string TextAgent::requestTextInput(const std::string& text, const std::string& token, bool include_dialog_attribute)
{
    nugu_dbg("receive text interface : %s from user app", text.c_str());

    if (cur_state == TextState::BUSY) {
        nugu_warn("already request nugu service to the server");
        return "";
    }

    if (timer)
        timer->start();

    if (timer_msec)
        timer_msec->start();

    cur_state = TextState::BUSY;

    sendEventTextInput({ text, token }, include_dialog_attribute);

    nugu_dbg("user request id: %s", cur_dialog_id.c_str());
    if (text_listener)
        text_listener->onState(cur_state, cur_dialog_id);

    // for relaying multi-turn states until text request is finished, if ASR is multi-turn
    if (!handle_interaction_control && interaction_control_manager->isMultiTurnActive())
        startInteractionControl(InteractionMode::MULTI_TURN);

    capa_helper->sendCommand("Text", "ASR", "cancel", "");

    return cur_dialog_id;
}

void TextAgent::notifyResponseTimeout()
{
    if (text_listener)
        text_listener->onError(TextError::RESPONSE_TIMEOUT, cur_dialog_id);

    cur_state = TextState::IDLE;
    if (text_listener)
        text_listener->onState(cur_state, cur_dialog_id);
}

void TextAgent::sendEventTextInput(const TextInputParam& text_input_param, bool include_dialog_attribute, EventResultCallback cb)
{
    CapabilityEvent event("TextInput", this);
    Json::FastWriter writer;
    Json::Value root;

    root["text"] = text_input_param.text;

    if (!text_input_param.token.empty())
        root["token"] = text_input_param.token;

    if (!text_input_param.ps_id.empty())
        root["playServiceId"] = text_input_param.ps_id;
    else if (include_dialog_attribute) {
        std::string ps_id = "";
        std::string asr_context = "";
        std::list<std::string> domainTypes;

        capa_helper->getCapabilityProperty("ASR", "es.playServiceId", ps_id);
        capa_helper->getCapabilityProperty("ASR", "es.asrContext", asr_context);
        capa_helper->getCapabilityProperties("ASR", "es.domainTypes", domainTypes);

        if (ps_id.size())
            root["playServiceId"] = ps_id;
        if (asr_context.size())
            root["asrContext"] = asr_context;
        if (domainTypes.size()) {
            while (!domainTypes.empty()) {
                root["domainTypes"].append(domainTypes.front());
                domainTypes.pop_front();
            }
        }
    }

    cur_dialog_id = event.getDialogRequestId();
    playsync_manager->stopHolding();

    sendEvent(&event, capa_helper->makeAllContextInfo(), writer.write(root), std::move(cb));
}

void TextAgent::sendEventTextSourceFailed(const TextInputParam& text_input_param, EventResultCallback cb)
{
    sendEventFailed("TextSourceFailed", text_input_param, std::move(cb));
}

void TextAgent::sendEventTextRedirectFailed(const TextInputParam& text_input_param, EventResultCallback cb)
{
    sendEventFailed("TextRedirectFailed", text_input_param, std::move(cb));
}

void TextAgent::sendEventFailed(std::string&& event_name, const TextInputParam& text_input_param, EventResultCallback cb)
{
    Json::Value root;
    Json::FastWriter writer;

    if (!text_input_param.ps_id.empty())
        root["playServiceId"] = text_input_param.ps_id;

    root["token"] = text_input_param.token;
    root["errorCode"] = FAIL_EVENT_ERROR_CODE;

    cur_dialog_id = sendEvent(event_name, getContextInfo(), writer.write(root), std::move(cb));
}

void TextAgent::parsingTextSource(const char* message)
{
    Json::Value root;
    Json::Reader reader;
    TextInputParam text_input_param;

    try {
        if (!reader.parse(message, root))
            throw "parsing error";

        text_input_param.text = root["text"].asString();
        text_input_param.token = root["token"].asString();
        text_input_param.ps_id = root["playServiceId"].asString();

        if (!handle_interaction_control && interaction_control_manager->isMultiTurnActive())
            startInteractionControl(InteractionMode::MULTI_TURN);

        if (!handleTextCommonProcess(text_input_param))
            throw "The processing TextSource is incomplete.";
    } catch (const char* message) {
        sendEventTextSourceFailed(text_input_param);
        nugu_error(message);
    }
}

void TextAgent::parsingTextRedirect(const char* message)
{
    Json::Value root;
    Json::Reader reader;
    std::string target_ps_id;
    TextInputParam text_input_param;

    try {
        if (!reader.parse(message, root))
            throw "parsing error";

        text_input_param.text = root["text"].asString();
        text_input_param.token = root["token"].asString();
        target_ps_id = root["targetPlayServiceId"].asString();
        text_input_param.ps_id = !target_ps_id.empty() ? target_ps_id : root["playServiceId"].asString();

        startInteractionControl(getInteractionMode(root["interactionControl"]));

        if (!handleTextCommonProcess(text_input_param))
            throw "The processing TextRedirect is incomplete.";
    } catch (const char* message) {
        finishInteractionControl();
        sendEventTextRedirectFailed(text_input_param);
        nugu_error(message);
    }
}

bool TextAgent::handleTextCommonProcess(const TextInputParam& text_input_param)
{
    if (text_input_param.token.empty()) {
        nugu_error("There is no mandatory data in directive message");
        return false;
    }

    nugu_dbg("receive text interface : %s from mobile app", text_input_param.text.c_str());

    if (cur_state == TextState::BUSY) {
        nugu_warn("already request nugu service to the server");
        return false;
    }

    // if application consume text command, it's not process anymore.
    if (text_listener && text_listener->handleTextCommand(text_input_param.text, text_input_param.token))
        return false;

    if (timer)
        timer->start();

    cur_state = TextState::BUSY;

    sendEventTextInput(text_input_param);
    setReferrerDialogRequestId(nugu_directive_peek_name(getNuguDirective()), "");

    if (text_listener)
        text_listener->onState(cur_state, cur_dialog_id);

    capa_helper->sendCommand("Text", "ASR", "cancel", "");

    return true;
}

void TextAgent::notifyEventResponse(const std::string& msg_id, const std::string& data, bool success)
{
    finishInteractionControl();
}

void TextAgent::startInteractionControl(InteractionMode&& mode)
{
    interaction_mode = mode;
    interaction_control_manager->start(interaction_mode, getName());
    handle_interaction_control = true;
}

void TextAgent::finishInteractionControl()
{
    if (handle_interaction_control) {
        interaction_control_manager->finish(interaction_mode, getName());
        interaction_mode = InteractionMode::NONE;
        handle_interaction_control = false;
    }
}

} // NuguCapability
