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

#include "capability_manager.hh"
#include "interface/nugu_configuration.hh"
#include "movement_agent.hh"
#include "nugu_log.h"

namespace NuguCore {

static const std::string capability_version = "1.0";

static MovementLightColor _color_string_to_value(const std::string& color)
{
    if (color.compare("WHITE") == 0)
        return MovementLightColor::WHITE;
    else if (color.compare("RED") == 0)
        return MovementLightColor::RED;
    else if (color.compare("BLUE") == 0)
        return MovementLightColor::BLUE;
    else if (color.compare("YELLOW") == 0)
        return MovementLightColor::YELLOW;
    else if (color.compare("GREEN") == 0)
        return MovementLightColor::GREEN;
    else if (color.compare("PURPLE") == 0)
        return MovementLightColor::PURPLE;
    else if (color.compare("PINK") == 0)
        return MovementLightColor::PINK;
    else if (color.compare("ORANGE") == 0)
        return MovementLightColor::ORANGE;
    else if (color.compare("VIOLET") == 0)
        return MovementLightColor::VIOLET;
    else if (color.compare("SKYBLUE") == 0)
        return MovementLightColor::SKYBLUE;

    nugu_error("invalid color value: %s", color.c_str());

    return MovementLightColor::WHITE;
}

static MovementDirection _direction_string_to_value(const std::string& direction)
{
    if (direction.compare("FRONT") == 0)
        return MovementDirection::FRONT;
    else if (direction.compare("BACK") == 0)
        return MovementDirection::BACK;
    else if (direction.compare("LEFT") == 0)
        return MovementDirection::LEFT;
    else if (direction.compare("RIGHT") == 0)
        return MovementDirection::RIGHT;
    else if (direction.compare("NONE") == 0)
        return MovementDirection::NONE;

    nugu_error("invalid direction value: %s", direction.c_str());

    return MovementDirection::NONE;
}

MovementAgent::MovementAgent()
    : Capability(CapabilityType::Movement, capability_version)
    , ps_id("")
    , cur_state(MovementState::STOP)
    , cur_direction(MovementDirection::NONE)
    , cur_light(false)
    , cur_lightcolor(MovementLightColor::WHITE)
    , movement_listener(nullptr)
{
}

MovementAgent::~MovementAgent()
{
}

void MovementAgent::initialize()
{
}

void MovementAgent::setCapabilityListener(ICapabilityListener* clistener)
{
    if (clistener)
        movement_listener = dynamic_cast<IMovementListener*>(clistener);
}

void MovementAgent::processDirective(NuguDirective* ndir)
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
    if (!strcmp(dname, "Move"))
        parsingMove(message);
    else if (!strcmp(dname, "Rotate"))
        parsingRotate(message);
    else if (!strcmp(dname, "ChangeLightColor"))
        parsingChangeLightColor(message);
    else if (!strcmp(dname, "TurnOnLight"))
        parsingTurnOnLight(message);
    else if (!strcmp(dname, "TurnOffLight"))
        parsingTurnOffLight(message);
    else if (!strcmp(dname, "Dance"))
        parsingDance(message);
    else if (!strcmp(dname, "Stop"))
        parsingStop(message);
    else {
        nugu_warn("%s[%s] is not support %s directive", getName().c_str(), getVersion().c_str(), dname);
    }

    destoryDirective(ndir);
}

void MovementAgent::updateInfoForContext(Json::Value& ctx)
{
    Json::Value movement;
    const char* has_light;

    movement["version"] = getVersion();

    switch (cur_state) {
    case MovementState::MOVING:
        movement["state"] = "MOVING";
        break;
    case MovementState::ROTATING:
        movement["state"] = "ROTATING";
        break;
    case MovementState::DANCING:
        movement["state"] = "DANCING";
        break;
    case MovementState::STOP:
        movement["state"] = "STOP";
        break;
    default:
        nugu_error("invalid state: %d", cur_state);
        break;
    }

    switch (cur_direction) {
    case MovementDirection::NONE:
        movement["direction"] = "NONE";
        break;
    case MovementDirection::FRONT:
        movement["direction"] = "FRONT";
        break;
    case MovementDirection::BACK:
        movement["direction"] = "BACK";
        break;
    case MovementDirection::LEFT:
        movement["direction"] = "LEFT";
        break;
    case MovementDirection::RIGHT:
        movement["direction"] = "RIGHT";
        break;
    default:
        nugu_error("invalid direction: %d", cur_direction);
        break;
    }

    has_light = nugu_config_get(NuguConfig::Key::MOVEMENT_WITH_LIGHT.c_str());
    if (has_light && g_strcmp0(has_light, "true") == 0) {
        if (cur_light)
            movement["light"] = "ON";
        else
            movement["light"] = "OFF";

        switch (cur_lightcolor) {
        case MovementLightColor::WHITE:
            movement["lightColor"] = "WHITE";
            break;
        case MovementLightColor::RED:
            movement["lightColor"] = "RED";
            break;
        case MovementLightColor::BLUE:
            movement["lightColor"] = "BLUE";
            break;
        case MovementLightColor::YELLOW:
            movement["lightColor"] = "YELLOW";
            break;
        case MovementLightColor::GREEN:
            movement["lightColor"] = "GREEN";
            break;
        case MovementLightColor::PURPLE:
            movement["lightColor"] = "PURPLE";
            break;
        case MovementLightColor::PINK:
            movement["lightColor"] = "PINK";
            break;
        case MovementLightColor::ORANGE:
            movement["lightColor"] = "ORANGE";
            break;
        case MovementLightColor::VIOLET:
            movement["lightColor"] = "VIOLET";
            break;
        case MovementLightColor::SKYBLUE:
            movement["lightColor"] = "SKYBLUE";
            break;
        default:
            nugu_error("invalid color: %d", cur_lightcolor);
            break;
        }
    }

    ctx[getName()] = movement;
}

std::string MovementAgent::getContextInfo()
{
    Json::Value ctx;
    CapabilityManager* cmanager = CapabilityManager::getInstance();

    updateInfoForContext(ctx);
    return cmanager->makeContextInfo(ctx);
}

void MovementAgent::receiveCommand(CapabilityType from, std::string command, const std::string& param)
{
    std::transform(command.begin(), command.end(), command.begin(), ::tolower);

    if (!command.compare("wakeup_detected")) {
        nugu_dbg("Stop by wakeup");

        if (movement_listener)
            movement_listener->onStop();

        cur_state = MovementState::STOP;
        cur_direction = MovementDirection::NONE;
    }
}

void MovementAgent::moveStarted(MovementDirection direction)
{
    sendEventMoveStarted();

    cur_state = MovementState::MOVING;
    cur_direction = direction;
}

void MovementAgent::moveFinished()
{
    sendEventMoveFinished();

    cur_state = MovementState::STOP;
    cur_direction = MovementDirection::NONE;
}

void MovementAgent::rotateStarted(MovementDirection direction)
{
    sendEventRotateStarted();

    cur_state = MovementState::ROTATING;
    cur_direction = direction;
}

void MovementAgent::rotateFinished()
{
    sendEventRotateFinished();

    cur_state = MovementState::STOP;
    cur_direction = MovementDirection::NONE;
}

void MovementAgent::changeColorSucceeded(MovementLightColor color)
{
    sendEventChangeColorSucceeded();

    cur_lightcolor = color;
}

void MovementAgent::turnOnLightSucceeded()
{
    sendEventTurnOnLightSucceeded();

    cur_light = true;
}

void MovementAgent::turnOffLightSucceeded()
{
    sendEventTurnOffLightSucceeded();

    cur_light = false;
}

void MovementAgent::danceStarted()
{
    sendEventDanceStarted();

    cur_state = MovementState::DANCING;
    cur_direction = MovementDirection::NONE;
}

void MovementAgent::danceFinished()
{
    sendEventDanceFinished();

    cur_state = MovementState::STOP;
    cur_direction = MovementDirection::NONE;
}

void MovementAgent::exceptionEncountered(const std::string& directive, const std::string& message)
{
    sendEventExceptionEncountered(directive, message);
}

void MovementAgent::sendEventMoveStarted()
{
    sendEventCommon("MoveStarted");
}

void MovementAgent::sendEventMoveFinished()
{
    sendEventCommon("MoveFinished");
}

void MovementAgent::sendEventRotateStarted()
{
    sendEventCommon("RotateStarted");
}

void MovementAgent::sendEventRotateFinished()
{
    sendEventCommon("RotateFinished");
}

void MovementAgent::sendEventChangeColorSucceeded()
{
    sendEventCommon("ChangeColorSucceeded");
}

void MovementAgent::sendEventTurnOnLightSucceeded()
{
    sendEventCommon("TurnOnLightSucceeded");
}

void MovementAgent::sendEventTurnOffLightSucceeded()
{
    sendEventCommon("TurnOffLightSucceeded");
}

void MovementAgent::sendEventDanceStarted()
{
    sendEventCommon("DanceStarted");
}

void MovementAgent::sendEventDanceFinished()
{
    sendEventCommon("DanceFinished");
}

void MovementAgent::sendEventExceptionEncountered(const std::string& directive, const std::string& message)
{
    Json::StyledWriter writer;
    Json::Value root;
    Json::Value error_obj;
    NuguEvent* event;
    std::string event_json;

    event = nugu_event_new(getName().c_str(), "EventExceptionEncountered",
        getVersion().c_str());

    nugu_event_set_context(event, getContextInfo().c_str());

    error_obj["directiveIssued"] = directive;
    error_obj["message"] = message;

    root["playServiceId"] = ps_id;
    root["error"] = error_obj;
    event_json = writer.write(root);

    nugu_event_set_json(event, event_json.c_str());

    sendEvent(event);

    nugu_event_free(event);
}

void MovementAgent::sendEventCommon(std::string ename)
{
    Json::StyledWriter writer;
    Json::Value root;
    NuguEvent* event;
    std::string event_json;

    event = nugu_event_new(getName().c_str(), ename.c_str(),
        getVersion().c_str());

    nugu_event_set_context(event, getContextInfo().c_str());

    root["playServiceId"] = ps_id;
    event_json = writer.write(root);

    nugu_event_set_json(event, event_json.c_str());

    sendEvent(event);

    nugu_event_free(event);
}

void MovementAgent::parsingMove(const char* message)
{
    Json::Value root;
    Json::Value prop;
    Json::Reader reader;
    std::string direction;
    long time_secs;
    long speed;
    long distance_cm;
    long count;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    ps_id = root["playServiceId"].asString();
    if (ps_id.size() == 0) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    prop = root["properties"];
    if (prop.empty()) {
        nugu_error("directive message syntex error");
        return;
    }

    direction = prop["direction"].asString();
    if (direction.size() == 0) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    time_secs = prop["timeInSec"].asLargestInt();
    speed = prop["speed"].asLargestInt();
    distance_cm = prop["distanceInCm"].asLargestInt();
    count = prop["count"].asLargestInt();

    if (movement_listener)
        movement_listener->onMove(_direction_string_to_value(direction), time_secs, speed, distance_cm, count);
}

void MovementAgent::parsingRotate(const char* message)
{
    Json::Value root;
    Json::Value prop;
    Json::Reader reader;
    std::string direction;
    long time_secs;
    long speed;
    long angle;
    long count;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    ps_id = root["playServiceId"].asString();
    if (ps_id.size() == 0) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    prop = root["properties"];
    if (prop.empty()) {
        nugu_error("directive message syntex error");
        return;
    }

    direction = prop["direction"].asString();
    if (direction.size() == 0) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    time_secs = prop["timeInSec"].asLargestInt();
    speed = prop["speed"].asLargestInt();
    angle = prop["angleInDegree"].asLargestInt();
    count = prop["count"].asLargestInt();

    if (movement_listener)
        movement_listener->onRotate(_direction_string_to_value(direction), time_secs, speed, angle, count);
}

void MovementAgent::parsingChangeLightColor(const char* message)
{
    Json::Value root;
    Json::Value prop;
    Json::Reader reader;
    std::string color;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    ps_id = root["playServiceId"].asString();
    if (ps_id.size() == 0) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    prop = root["properties"];
    if (prop.empty()) {
        nugu_error("directive message syntex error");
        return;
    }

    color = prop["color"].asString();
    if (color.size() == 0) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    if (movement_listener)
        movement_listener->onChangeLightColor(_color_string_to_value(color));
}

void MovementAgent::parsingTurnOnLight(const char* message)
{
    Json::Value root;
    Json::Value prop;
    Json::Reader reader;
    std::string color;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    ps_id = root["playServiceId"].asString();
    if (ps_id.size() == 0) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    prop = root["properties"];
    if (prop.empty()) {
        nugu_error("directive message syntex error");
        return;
    }

    color = prop["color"].asString();
    if (color.size() == 0) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    if (movement_listener)
        movement_listener->onTurnOnLight(_color_string_to_value(color));
}

void MovementAgent::parsingTurnOffLight(const char* message)
{
    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    ps_id = root["playServiceId"].asString();
    if (ps_id.size() == 0) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    if (movement_listener)
        movement_listener->onTurnOffLight();
}

void MovementAgent::parsingDance(const char* message)
{
    Json::Value root;
    Json::Value prop;
    Json::Reader reader;
    long count;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    ps_id = root["playServiceId"].asString();
    if (ps_id.size() == 0) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    prop = root["properties"];
    if (prop.empty()) {
        nugu_error("directive message syntex error");
        return;
    }

    count = prop["count"].asLargestInt();
    if (count <= 0) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    if (movement_listener)
        movement_listener->onDance(count);
}

void MovementAgent::parsingStop(const char* message)
{
    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    ps_id = root["playServiceId"].asString();
    if (ps_id.size() == 0) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    if (movement_listener)
        movement_listener->onStop();

    cur_state = MovementState::STOP;
    cur_direction = MovementDirection::NONE;
}

} // NuguCore
