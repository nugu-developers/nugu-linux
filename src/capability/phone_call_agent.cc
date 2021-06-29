/*
 * Copyright (c) 2021 SK Telecom Co., Ltd. All rights reserved.
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
#include "phone_call_agent.hh"

namespace NuguCapability {

static const char* CAPABILITY_NAME = "PhoneCall";
static const char* CAPABILITY_VERSION = "1.2";

PhoneCallAgent::PhoneCallAgent()
    : Capability(CAPABILITY_NAME, CAPABILITY_VERSION)
    , phone_call_listener(nullptr)
    , cur_state(PhoneCallState::IDLE)
    , focus_state(FocusState::NONE)
{
}

void PhoneCallAgent::initialize()
{
    if (initialized) {
        nugu_info("It's already initialized.");
        return;
    }

    Capability::initialize();

    cur_state = PhoneCallState::IDLE;
    focus_state = FocusState::NONE;
    context_template = "";
    context_recipient = "";

    playsync_manager->addListener(getName(), this);

    initialized = true;
}

void PhoneCallAgent::deInitialize()
{
    if (!initialized)
        return;

    playsync_manager->removeListener(getName());

    initialized = false;
}

void PhoneCallAgent::parsingDirective(const char* dname, const char* message)
{
    nugu_dbg("message: %s", message);

    // directive name check
    if (!strcmp(dname, "SendCandidates"))
        parsingSendCandidates(message);
    else if (!strcmp(dname, "MakeCall"))
        parsingMakeCall(message);
    else if (!strcmp(dname, "EndCall"))
        parsingEndCall(message);
    else if (!strcmp(dname, "AcceptCall"))
        parsingAcceptCall(message);
    else if (!strcmp(dname, "BlockIncomingCall"))
        parsingBlockIncomingCall(message);
    else {
        nugu_warn("%s[%s] is not support %s directive", getName().c_str(), getVersion().c_str(), dname);
    }
}

void PhoneCallAgent::updateInfoForContext(Json::Value& ctx)
{
    Json::Value phone_call;
    Json::Value temp;
    Json::Reader reader;

    phone_call["version"] = getVersion();
    phone_call["state"] = getStateStr(cur_state);

    if (context_template.size() && reader.parse(context_template, temp))
        phone_call["template"] = temp;

    if (context_recipient.size() && reader.parse(context_recipient, temp))
        phone_call["recipient"] = temp;

    ctx[getName()] = phone_call;
}

void PhoneCallAgent::setCapabilityListener(ICapabilityListener* clistener)
{
    if (clistener)
        phone_call_listener = dynamic_cast<IPhoneCallListener*>(clistener);
}

void PhoneCallAgent::candidatesListed(const std::string& context_template, const std::string& payload)
{
    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(context_template, root) || !reader.parse(payload, root)) {
        nugu_error("context template or payload is not json format!!");
        return;
    }

    this->context_template = context_template;
    sendEvent("CandidatesListed", capa_helper->makeAllContextInfo(), payload,
        [&](const std::string& ename, const std::string& msg_id, const std::string& dialog_id, int success, int code) {
            nugu_info("CandidatesListed result callback - success: %d", success);
            interaction_control_manager->finish(InteractionMode::MULTI_TURN, getName());
        });
}

void PhoneCallAgent::callArrived(const std::string& payload)
{
    Json::Value root;
    Json::Value caller;
    Json::Reader reader;
    Json::StyledWriter writer;

    if (!reader.parse(payload, root)) {
        nugu_error("payload is not json format!!");
        return;
    }

    if (cur_state != PhoneCallState::IDLE) {
        nugu_warn("The current state is not PhoneCallState::IDLE");
        return;
    }

    caller = root["caller"];
    if (!caller.empty()) {
        context_recipient = writer.write(caller);
    }

    setState(PhoneCallState::INCOMING);
    sendEvent("CallArrived", capa_helper->makeAllContextInfo(), payload);

    context_recipient = "";
}

void PhoneCallAgent::callEnded(const std::string& payload)
{
    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(payload, root)) {
        nugu_error("payload is not json format!!");
        return;
    }

    if (cur_state == PhoneCallState::IDLE) {
        nugu_warn("The current state is already PhoneCallState::IDLE");
        return;
    }

    setState(PhoneCallState::IDLE);
    sendEvent("CallEnded", getContextInfo(), payload);
}

void PhoneCallAgent::callEstablished(const std::string& payload)
{
    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(payload, root)) {
        nugu_error("payload is not json format!!");
        return;
    }

    if (cur_state == PhoneCallState::IDLE) {
        nugu_warn("The current state is PhoneCallState::IDLE");
        return;
    }

    setState(PhoneCallState::ESTABLISHED);
    sendEvent("CallEstablished", getContextInfo(), payload);
}

void PhoneCallAgent::makeCallSucceeded(const std::string& payload)
{
    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(payload, root)) {
        nugu_error("payload is not json format!!");
        return;
    }

    if (cur_state != PhoneCallState::OUTGOING) {
        nugu_warn("The current state is not PhoneCallState::OUTGOING");
        return;
    }

    sendEvent("MakeCallSucceeded", getContextInfo(), payload);
}

void PhoneCallAgent::makeCallFailed(const std::string& payload)
{
    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(payload, root)) {
        nugu_error("payload is not json format!!");
        return;
    }

    if (cur_state != PhoneCallState::OUTGOING) {
        nugu_warn("The current state is not PhoneCallState::OUTGOING");
        return;
    }

    setState(PhoneCallState::IDLE);
    sendEvent("MakeCallFailed", getContextInfo(), payload);
}

void PhoneCallAgent::onFocusChanged(FocusState state)
{
    nugu_info("Focus Changed(%s -> %s)", focus_manager->getStateString(focus_state).c_str(), focus_manager->getStateString(state).c_str());
    focus_state = state;
}

void PhoneCallAgent::onSyncState(const std::string& ps_id, PlaySyncState state, void* extra_data)
{
}

void PhoneCallAgent::onDataChanged(const std::string& ps_id, std::pair<void*, void*> extra_datas)
{
}

void PhoneCallAgent::onStackChanged(const std::pair<std::string, std::string>& ps_ids)
{
    if (!ps_ids.first.empty())
        nugu_info("play stack added => %s", ps_ids.first.c_str());

    if (!ps_ids.second.empty()) {
        nugu_info("play stack removed => %s", ps_ids.second.c_str());
        if (ps_ids.second == playstackctl_ps_id) {
            nugu_info("release sync(%s) action => clear context_template", ps_ids.second.c_str());
            context_template = "";
        }
    }
}

void PhoneCallAgent::parsingSendCandidates(const char* message)
{
    Json::Value root;
    Json::Reader reader;
    InteractionMode interaction_mode;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    if (root["playServiceId"].empty() || root["intent"].empty()) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    playstackctl_ps_id = getPlayServiceIdInStackControl(root["playStackControl"]);
    interaction_mode = getInteractionMode(root["interactionControl"]);
    interaction_control_manager->start(interaction_mode, getName());

    context_template = "";

    if (phone_call_listener)
        phone_call_listener->processSendCandidates(message);
}

void PhoneCallAgent::parsingMakeCall(const char* message)
{
    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    if (root["playServiceId"].empty() || root["recipient"].empty() || root["callType"].empty()) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    setState(PhoneCallState::OUTGOING);

    if (phone_call_listener)
        phone_call_listener->processMakeCall(message);
}

void PhoneCallAgent::parsingEndCall(const char* message)
{
    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    if (root["playServiceId"].empty()) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    if (phone_call_listener)
        phone_call_listener->processEndCall(message);
}

void PhoneCallAgent::parsingAcceptCall(const char* message)
{
    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    if (root["playServiceId"].empty() || root["speakerPhone"].empty()) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    if (phone_call_listener)
        phone_call_listener->processAcceptCall(message);
}

void PhoneCallAgent::parsingBlockIncomingCall(const char* message)
{
    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    if (root["playServiceId"].empty()) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    if (phone_call_listener)
        phone_call_listener->processBlockIncomingCall(message);
}

void PhoneCallAgent::setState(PhoneCallState state)
{
    if (cur_state == state)
        return;

    nugu_info("PhoneCallState is changed (%s -> %s)", getStateStr(cur_state).c_str(), getStateStr(state).c_str());
    cur_state = state;

    if (cur_state == PhoneCallState::IDLE)
        focus_manager->releaseFocus(CALL_FOCUS_TYPE, CAPABILITY_NAME);
    else
        focus_manager->requestFocus(CALL_FOCUS_TYPE, CAPABILITY_NAME, this);

    if (phone_call_listener)
        phone_call_listener->callStateChanged(state);
}

std::string PhoneCallAgent::getStateStr(PhoneCallState state)
{
    std::string state_str;

    switch (state) {
    case PhoneCallState::IDLE:
        state_str = "IDLE";
        break;
    case PhoneCallState::OUTGOING:
        state_str = "OUTGOING";
        break;
    case PhoneCallState::INCOMING:
        state_str = "INCOMING";
        break;
    case PhoneCallState::ESTABLISHED:
        state_str = "ESTABLISHED";
        break;
    }

    return state_str;
}

} // NuguCapability
