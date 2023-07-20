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
#include "routine_agent.hh"

namespace NuguCapability {

static const char* CAPABILITY_NAME = "Routine";
static const char* CAPABILITY_VERSION = "1.3";

RoutineAgent::RoutineAgent()
    : Capability(CAPABILITY_NAME, CAPABILITY_VERSION)
{
}

void RoutineAgent::initialize()
{
    if (initialized) {
        nugu_info("It's already initialized.");
        return;
    }

    Capability::initialize();

    routine_info = {};
    action_timeout_in_last_action = false;

    routine_manager->addListener(this);
    routine_manager->setDataRequester([&](const std::string& ps_id, const Json::Value& data) {
        return sendEventActionTriggered(ps_id, data);
    });

    timer = std::unique_ptr<INuguTimer>(core_container->createNuguTimer(true));

    addReferrerEvents("Started", "Start");
    addReferrerEvents("Failed", "Start");
    addReferrerEvents("Stopped", "Start");
    addReferrerEvents("Finished", "Start");
    addReferrerEvents("ActionTriggered", "Start");

    addBlockingPolicy("Continue", { BlockingMedium::AUDIO, false });

    initialized = true;
}

void RoutineAgent::deInitialize()
{
    if (timer) {
        timer->stop();
        timer.reset();
    }

    routine_manager->removeListener(this);

    initialized = false;
}

void RoutineAgent::setCapabilityListener(ICapabilityListener* clistener)
{
    if (clistener)
        routine_listener = dynamic_cast<IRoutineListener*>(clistener);
}

void RoutineAgent::updateInfoForContext(Json::Value& ctx)
{
    Json::Value routine;

    routine["version"] = getVersion();
    routine["routineActivity"] = ROUTINE_ACTIVITY_TEXTS.at(activity);

    if (!routine_info.token.empty())
        routine["token"] = routine_info.token;

    if (!routine_info.name.empty())
        routine["name"] = routine_info.name;

    if (!routine_info.routine_id.empty())
        routine["routineId"] = routine_info.routine_id;

    if (!routine_info.routine_type.empty())
        routine["routineType"] = routine_info.routine_type;

    if (!routine_info.routine_list_type.empty())
        routine["routineListType"] = routine_info.routine_list_type;

    if (!routine_info.source.empty())
        routine["source"] = routine_info.source;

    int current_action_index = routine_manager->getCurrentActionIndex();

    if (current_action_index > 0)
        routine["currentAction"] = current_action_index;

    if (!routine_info.actions.empty())
        routine["actions"] = routine_info.actions;

    ctx[getName()] = routine;
}

bool RoutineAgent::getProperty(const std::string& property, std::string& value)
{
    if (property == "routineActivity") {
        value = ROUTINE_ACTIVITY_TEXTS.at(activity);
    } else {
        nugu_error("invalid property: %s", property.c_str());
        return false;
    }

    return true;
}

void RoutineAgent::onActivity(RoutineActivity activity)
{
    this->activity = activity;

    switch (this->activity) {
    case RoutineActivity::PLAYING:
        if (routine_manager->getCurrentActionIndex() == 1)
            sendEventStarted();
        break;
    case RoutineActivity::INTERRUPTED:
        handleInterrupted();
        break;
    case RoutineActivity::STOPPED:
        if (timer)
            timer->stop();

        sendEventStopped();
        break;
    case RoutineActivity::FINISHED:
        sendEventFinished();
        break;
    case RoutineActivity::IDLE:
        break;
    case RoutineActivity::SUSPENDED:
        break;
    }
}

void RoutineAgent::onActionTimeout(bool last_action)
{
    action_timeout_in_last_action = last_action;

    sendEventActionTimeoutTriggered();
}

void RoutineAgent::handleInterrupted()
{
    if (!timer) {
        nugu_error("Timer is not created.");
        return;
    }

    timer->setCallback([&]() {
        routine_manager->stop();
    });
    timer->setInterval(INTERRUPT_TIME_MSEC);
    timer->start();
}

bool RoutineAgent::handleMoveControl(int offset)
{
    if (!routine_manager->isRoutineAlive()) {
        nugu_warn("Routine is not alive.");
        return false;
    }

    sendEventMoveControl(offset);
    return true;
}

bool RoutineAgent::handlePendingActionTimeout()
{
    if (!action_timeout_in_last_action)
        return false;

    action_timeout_in_last_action = false;
    routine_manager->finish();

    return true;
}

void RoutineAgent::clearRoutineInfo()
{
    routine_info = {};
}

bool RoutineAgent::startRoutine(const std::string& dialog_id, const std::string& data)
{
    Json::Value root;
    Json::Reader reader;

    try {
        setReferrerDialogRequestId("Start", dialog_id);

        if (!reader.parse(data, root))
            throw "parsing error";

        if (root["playServiceId"].empty() || root["token"].empty() || root["actions"].empty())
            throw "There is no mandatory data in directive message";

        routine_info.play_service_id = root["playServiceId"].asString();
        routine_info.token = root["token"].asString();
        routine_info.name = root["name"].asString();
        routine_info.routine_id = root["routineId"].asString();
        routine_info.routine_type = root["routineType"].asString();
        routine_info.routine_list_type = root["routineListType"].asString();
        routine_info.source = root["source"].asString();

        for (const auto& action : root["actions"])
            if (routine_manager->isActionValid(action))
                routine_info.actions.append(action);

        if (!routine_manager->start(routine_info.token, routine_info.actions))
            throw "Routine start is failed";
    } catch (const char* message) {
        nugu_error(message);
        return false;
    }

    return true;
}

bool RoutineAgent::next()
{
    return handleMoveControl(1);
}

bool RoutineAgent::prev()
{
    return handleMoveControl(-1);
}

/*******************************************************************************
 * parse directive
 ******************************************************************************/

void RoutineAgent::parsingDirective(const char* dname, const char* message)
{
    nugu_dbg("message: %s", message);

    if (!strcmp(dname, "Start")) {
        parsingStart(message);
    } else if (!strcmp(dname, "Stop")) {
        parsingStop(message);
    } else if (!strcmp(dname, "Continue")) {
        parsingContinue(message);
    } else if (!strcmp(dname, "Move")) {
        if (!handlePendingActionTimeout())
            parsingMove(message);
    } else
        nugu_warn("%s[%s] is not support %s directive", getName().c_str(), getVersion().c_str(), dname);
}

void RoutineAgent::parsingStart(const char* message)
{
    if (routine_manager->isRoutineAlive()) {
        routine_manager->stop();

        if (timer)
            timer->stop();
    }

    if (!startRoutine(nugu_directive_peek_dialog_id(getNuguDirective()), message))
        sendEventFailed();
}

void RoutineAgent::parsingStop(const char* message)
{
    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    if (root["playServiceId"].empty() || root["token"].empty()) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    if (routine_info.token != root["token"].asString()) {
        nugu_error("Token is not matched with currents.");
        return;
    }

    routine_info.play_service_id = root["playServiceId"].asString();
    routine_manager->stop();
}

void RoutineAgent::parsingContinue(const char* message)
{
    Json::Value root;
    Json::Reader reader;

    try {
        if (!reader.parse(message, root))
            throw "parsing error";

        if (root["playServiceId"].empty() || root["token"].empty())
            throw "There is no mandatory data in directive message";

        if (routine_info.token != root["token"].asString())
            throw "Token is not matched with currents.";

        if (timer)
            timer->stop();

        routine_info.play_service_id = root["playServiceId"].asString();
        routine_manager->resume();
    } catch (const char* exception_message) {
        sendEventFailed();
        nugu_error(exception_message);
    }
}

void RoutineAgent::parsingMove(const char* message)
{
    Json::Value root;
    Json::Reader reader;
    unsigned int position;
    unsigned int action_count = routine_info.actions.size();

    try {
        if (!reader.parse(message, root))
            throw "parsing error";

        if (root["playServiceId"].empty() || root["position"].empty())
            throw "There is no mandatory data in directive message";

        routine_info.play_service_id = root["playServiceId"].asString();
        position = root["position"].asUInt();

        if (position < 1)
            position = 1;
        else if (position > action_count)
            position = action_count;

        routine_manager->move(position) ? sendEventMoveSucceeded() : throw "Move failed";
    } catch (const char* exception_message) {
        sendEventMoveFailed();
        nugu_error(exception_message);
    }
}

/*******************************************************************************
 * send event
 ******************************************************************************/

void RoutineAgent::sendEventStarted(EventResultCallback cb)
{
    sendEventCommon("Started", Json::Value(), std::move(cb));
}

void RoutineAgent::sendEventFailed(EventResultCallback cb)
{
    Json::Value root;
    root["errorMessage"] = FAIL_EVENT_ERROR_CODE;

    sendEventCommon("Failed", std::move(root), std::move(cb), true);
}

void RoutineAgent::sendEventFinished(EventResultCallback cb)
{
    sendEventCommon("Finished", Json::Value(), std::move(cb), true);
}

void RoutineAgent::sendEventStopped(EventResultCallback cb)
{
    sendEventCommon("Stopped", Json::Value(), std::move(cb), true);
}

void RoutineAgent::sendEventMoveControl(const int offset, EventResultCallback cb)
{
    Json::Value root;
    root["offset"] = offset;

    sendEventCommon("MoveControl", std::move(root), std::move(cb));
}

void RoutineAgent::sendEventMoveSucceeded(EventResultCallback cb)
{
    sendEventCommon("MoveSucceeded", Json::Value(), std::move(cb));
}

void RoutineAgent::sendEventMoveFailed(EventResultCallback cb)
{
    Json::Value root;
    root["errorMessage"] = FAIL_EVENT_ERROR_CODE;

    sendEventCommon("MoveFailed", std::move(root), std::move(cb));
}

void RoutineAgent::sendEventActionTimeoutTriggered(EventResultCallback cb)
{
    std::string cur_action_token = routine_manager->getCurrentActionToken();

    if (cur_action_token.empty()) {
        nugu_error("The mandatory data for sending event is missed.");
        return;
    }

    Json::Value root;
    root["token"] = cur_action_token;

    sendEventCommon("ActionTimeoutTriggered", std::move(root), std::move(cb));
}

std::string RoutineAgent::sendEventActionTriggered(const std::string& ps_id, const Json::Value& data, EventResultCallback cb)
{
    if (ps_id.empty() || data.empty()) {
        nugu_error("The mandatory data for sending event is missed.");
        return "";
    }

    Json::FastWriter writer;
    Json::Value root;

    root["data"] = data;
    root["playServiceId"] = ps_id;

    return sendEvent("ActionTriggered", getContextInfo(), writer.write(root), std::move(cb));
}

void RoutineAgent::sendEventCommon(std::string&& event_name, Json::Value&& extra_value, EventResultCallback cb, bool clear_routine_info)
{
    if (routine_info.play_service_id.empty()) {
        nugu_error("The mandatory data for sending event is missed.");
        return;
    }

    Json::FastWriter writer;
    Json::Value root { extra_value };

    root["playServiceId"] = routine_info.play_service_id;

    sendEvent(event_name, getContextInfo(), writer.write(root), std::move(cb));

    if (clear_routine_info)
        clearRoutineInfo();
}

} // NuguCapability
