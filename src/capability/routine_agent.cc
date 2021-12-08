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
static const char* CAPABILITY_VERSION = "1.2";

RoutineAgent::RoutineAgent()
    : Capability(CAPABILITY_NAME, CAPABILITY_VERSION)
{
    routine_activity_texts = {
        { RoutineActivity::IDLE, "IDLE" },
        { RoutineActivity::PLAYING, "PLAYING" },
        { RoutineActivity::INTERRUPTED, "INTERRUPTED" },
        { RoutineActivity::FINISHED, "FINISHED" },
        { RoutineActivity::STOPPED, "STOPPED" }
    };
}

void RoutineAgent::initialize()
{
    if (initialized) {
        nugu_info("It's already initialized.");
        return;
    }

    Capability::initialize();

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
    routine["routineActivity"] = routine_activity_texts.at(activity);

    if (!token.empty())
        routine["token"] = token;

    int current_action_index = routine_manager->getCurrentActionIndex();

    if (current_action_index > 0)
        routine["currentAction"] = current_action_index;

    if (!actions.empty())
        routine["actions"] = actions;

    ctx[getName()] = routine;
}

bool RoutineAgent::getProperty(const std::string& property, std::string& value)
{
    if (property == "routineActivity") {
        value = routine_activity_texts.at(activity);
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
        playsync_manager->releaseSyncUnconditionally();
        sendEventStopped();
        break;
    case RoutineActivity::FINISHED:
        sendEventFinished();
        break;
    case RoutineActivity::IDLE:
        break;
    }
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

void RoutineAgent::clearRoutineInfo()
{
    play_service_id.clear();
    token.clear();
    actions.clear();
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

        play_service_id = root["playServiceId"].asString();
        token = root["token"].asString();
        actions = root["actions"];

        if (!routine_manager->start(token, actions))
            throw "Routine start is failed";
    } catch (const char* message) {
        nugu_error(message);
        return false;
    }

    return true;
}

/*******************************************************************************
 * parse directive
 ******************************************************************************/

void RoutineAgent::parsingDirective(const char* dname, const char* message)
{
    nugu_dbg("message: %s", message);

    if (!strcmp(dname, "Start"))
        parsingStart(message);
    else if (!strcmp(dname, "Stop"))
        parsingStop(message);
    else if (!strcmp(dname, "Continue"))
        parsingContinue(message);
    else
        nugu_warn("%s[%s] is not support %s directive", getName().c_str(), getVersion().c_str(), dname);
}

void RoutineAgent::parsingStart(const char* message)
{
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

    if (token != root["token"].asString()) {
        nugu_error("Token is not matched with currents.");
        return;
    }

    play_service_id = root["playServiceId"].asString();
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

        if (token != root["token"].asString())
            throw "Token is not matched with currents.";

        if (timer)
            timer->stop();

        play_service_id = root["playServiceId"].asString();
        routine_manager->resume();
    } catch (const char* exception_message) {
        sendEventFailed();
        nugu_error(exception_message);
    }
}

/*******************************************************************************
 * send event
 ******************************************************************************/

void RoutineAgent::sendEventStarted(EventResultCallback cb)
{
    sendEventCommon("Started", Json::Value(), std::move(cb), false);
}

void RoutineAgent::sendEventFailed(EventResultCallback cb)
{
    Json::Value root;
    root["errorMessage"] = FAIL_EVENT_ERROR_CODE;

    sendEventCommon("Failed", std::move(root), std::move(cb));
}

void RoutineAgent::sendEventFinished(EventResultCallback cb)
{
    sendEventCommon("Finished", Json::Value(), std::move(cb));
}

void RoutineAgent::sendEventStopped(EventResultCallback cb)
{
    sendEventCommon("Stopped", Json::Value(), std::move(cb));
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
    if (play_service_id.empty()) {
        nugu_error("The mandatory data for sending event is missed.");
        return;
    }

    Json::FastWriter writer;
    Json::Value root { extra_value };

    root["playServiceId"] = play_service_id;

    sendEvent(event_name, getContextInfo(), writer.write(root), std::move(cb));

    if (clear_routine_info)
        clearRoutineInfo();
}

} // NuguCapability
