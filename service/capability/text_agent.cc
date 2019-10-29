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

#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <interface/nugu_configuration.hh>

#include "capability_manager.hh"
#include "nugu_log.h"
#include "text_agent.hh"

namespace NuguCore {
static const std::string capability_version = "1.0";

TextAgent::TextAgent()
    : Capability(CapabilityType::Text, capability_version)
    , text_listener(nullptr)
    , timer(nullptr)
    , cur_state(TextState::IDLE)
    , cur_dialog_id("")
{
}

TextAgent::~TextAgent()
{
    if (timer) {
        nugu_timer_delete(timer);
        timer = nullptr;
    }
}

void TextAgent::initialize()
{
    if (initialized) {
        nugu_info("It's already initialized.");
        return;
    }

    std::string timeout = nugu_config_get(NuguConfig::Key::SERVER_RESPONSE_TIMEOUT.c_str());
    timer = nugu_timer_new(std::stoi(timeout), 1);
    nugu_timer_set_callback(
        timer, [](void* userdata) {
            TextAgent* text = static_cast<TextAgent*>(userdata);
            text->notifyResponseTimeout();
        },
        this);

    initialized = true;
}

void TextAgent::processDirective(NuguDirective* ndir)
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

    nugu_dbg("message: %s", message);

    // directive name check
    if (!strcmp(dname, "TextSource"))
        parsingTextSource(message);
    else {
        nugu_warn("%s[%s] is not support %s directive",
            getName().c_str(), getVersion().c_str(), dname);
    }

    destoryDirective(ndir);
}

void TextAgent::updateInfoForContext(Json::Value& ctx)
{
    Json::Value text;

    text["version"] = getVersion();

    ctx[getName()] = text;
}

std::string TextAgent::getContextInfo()
{
    Json::Value ctx;
    CapabilityManager* cmanager = CapabilityManager::getInstance();

    updateInfoForContext(ctx);
    return cmanager->makeContextInfo(ctx);
}

void TextAgent::receiveCommandAll(std::string command, const std::string& param)
{
    std::transform(command.begin(), command.end(), command.begin(), ::tolower);

    if (command == "directive_dialog_id" && param == cur_dialog_id) {
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
    Json::StyledWriter writer;
    Json::Value root;
    NuguEvent* event;
    std::string event_json;
    std::string ps_id = "";
    std::string property = "";
    std::string session_id = "";
    std::list<std::string> domainTypes;

    CapabilityManager* cmanager = CapabilityManager::getInstance();
    cmanager->getCapabilityProperty(CapabilityType::ASR, "playServiceId", ps_id);
    cmanager->getCapabilityProperty(CapabilityType::ASR, "property", property);
    cmanager->getCapabilityProperty(CapabilityType::ASR, "sessionId", session_id);
    cmanager->getCapabilityProperties(CapabilityType::ASR, "domainTypes", domainTypes);

    event = nugu_event_new(getName().c_str(), "TextInput",
        getVersion().c_str());

    cur_dialog_id = nugu_event_peek_dialog_id(event);

    nugu_event_set_context(event, getContextInfo().c_str());

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
    event_json = writer.write(root);

    nugu_event_set_json(event, event_json.c_str());

    sendEvent(event);

    nugu_event_free(event);
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
        return;
    }

    nugu_timer_start(timer);
    cur_state = TextState::BUSY;
    if (text_listener)
        text_listener->onState(cur_state);

    sendEventTextInput(text, token);
}

} // NuguCore
