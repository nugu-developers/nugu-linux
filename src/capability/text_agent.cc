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
#include "core/capability_manager.hh"

#include "text_agent.hh"

namespace NuguCapability {

static const char* CAPABILITY_NAME = "Text";
static const char* CAPABILITY_VERSION = "1.0";

// define default property values
static const int SERVER_RESPONSE_TIMEOUT_MSEC = 10000;

TextAgent::TextAgent()
    : Capability(CAPABILITY_NAME, CAPABILITY_VERSION)
    , text_listener(nullptr)
    , timer(nullptr)
    , cur_state(TextState::IDLE)
    , cur_dialog_id("")
    , response_timeout(SERVER_RESPONSE_TIMEOUT_MSEC)
{
}

TextAgent::~TextAgent()
{
    if (timer) {
        nugu_timer_delete(timer);
        timer = nullptr;
    }
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

    timer = nugu_timer_new(response_timeout, 1);
    nugu_timer_set_callback(
        timer, [](void* userdata) {
            TextAgent* text = static_cast<TextAgent*>(userdata);
            text->notifyResponseTimeout();
        },
        this);

    initialized = true;
}

void TextAgent::parsingDirective(const char* dname, const char* message)
{
    nugu_dbg("message: %s", message);

    // directive name check
    if (!strcmp(dname, "TextSource"))
        parsingTextSource(message);
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

    if (convert_command == "directive_dialog_id" && param == cur_dialog_id) {
        nugu_dbg("process receive command => directive_dialog_id(%s)", param.c_str());

        nugu_timer_stop(timer);
        cur_dialog_id = "";

        cur_state = TextState::IDLE;
        if (text_listener)
            text_listener->onState(cur_state);
    }
}

void TextAgent::setCapabilityListener(ICapabilityListener* clistener)
{
    if (clistener)
        text_listener = dynamic_cast<ITextListener*>(clistener);
}

bool TextAgent::requestTextInput(std::string text)
{
    nugu_dbg("receive text interface : %s from user app", text.c_str());
    if (cur_state == TextState::BUSY) {
        nugu_warn("already request nugu service to the server");
        return false;
    }

    nugu_timer_start(timer);
    cur_state = TextState::BUSY;
    if (text_listener)
        text_listener->onState(cur_state);

    sendEventTextInput(text, "");
    return true;
}

void TextAgent::notifyResponseTimeout()
{
    if (text_listener)
        text_listener->onError(TextError::RESPONSE_TIMEOUT);

    cur_state = TextState::IDLE;
    if (text_listener)
        text_listener->onState(cur_state);
}

void TextAgent::sendEventTextInput(const std::string& text, const std::string& token)
{
    CapabilityEvent event("TextInput", this);
    std::string payload = "";
    Json::StyledWriter writer;
    Json::Value root;
    std::string ps_id = "";
    std::string property = "";
    std::string session_id = "";
    std::list<std::string> domainTypes;

    CapabilityManager* cmanager = CapabilityManager::getInstance();
    cmanager->getCapabilityProperty("ASR", "es.playServiceId", ps_id);
    cmanager->getCapabilityProperty("ASR", "es.property", property);
    cmanager->getCapabilityProperty("ASR", "es.sessionId", session_id);
    cmanager->getCapabilityProperties("ASR", "es.domainTypes", domainTypes);

    root["text"] = text;
    if (token.size())
        root["token"] = token;
    if (ps_id.size())
        root["playServiceId"] = ps_id;
    if (property.size())
        root["property"] = property;
    if (session_id.size())
        root["sessionId"] = session_id;
    if (domainTypes.size()) {
        while (!domainTypes.empty()) {
            root["domainTypes"].append(domainTypes.front());
            domainTypes.pop_front();
        }
    }
    payload = writer.write(root);

    cur_dialog_id = event.getDialogMessageId();

    sendEvent(&event, CapabilityManager::getInstance()->makeAllContextInfoStack(), payload);
}

void TextAgent::sendEventTextSourceFailed(const std::string& text, const std::string& token)
{
    std::string ename = "TextSourceFailed";
    std::string payload = "";
    Json::StyledWriter writer;
    Json::Value root;

    root["text"] = text;
    root["token"] = token;
    payload = writer.write(root);

    sendEvent(ename, getContextInfo(), payload);
}

void TextAgent::parsingTextSource(const char* message)
{
    Json::Value root;
    Json::Reader reader;
    std::string text;
    std::string token;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    text = root["text"].asString();
    token = root["token"].asString();

    if (text.size() == 0 || token.size() == 0) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    nugu_dbg("receive text interface : %s from mobile app", text.c_str());

    if (cur_state == TextState::BUSY) {
        nugu_warn("already request nugu service to the server");
        sendEventTextSourceFailed(text, token);
        return;
    }

    nugu_timer_start(timer);
    cur_state = TextState::BUSY;
    if (text_listener)
        text_listener->onState(cur_state);

    sendEventTextInput(text, token);
}

} // NuguCapability
