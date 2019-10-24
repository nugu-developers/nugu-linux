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

#ifndef __NUGU_MOVEMENT_AGENT_H__
#define __NUGU_MOVEMENT_AGENT_H__

#include <interface/capability/movement_interface.hh>
#include <string>

#include "capability.hh"

namespace NuguCore {

using namespace NuguInterface;

class MovementAgent : public Capability, public IMovementHandler {
public:
    MovementAgent();
    virtual ~MovementAgent();
    void initialize() override;

    void processDirective(NuguDirective* ndir) override;
    void updateInfoForContext(Json::Value& ctx) override;
    std::string getContextInfo();
    void setCapabilityListener(ICapabilityListener* clistener) override;

    void receiveCommand(CapabilityType from, std::string command, const std::string& param) override;

    void moveStarted(MovementDirection direction) override;
    void moveFinished() override;
    void rotateStarted(MovementDirection direction) override;
    void rotateFinished() override;
    void changeColorSucceeded(MovementLightColor color) override;
    void turnOnLightSucceeded() override;
    void turnOffLightSucceeded() override;
    void danceStarted() override;
    void danceFinished() override;
    void exceptionEncountered(const std::string& directive, const std::string& message) override;

private:
    void sendEventMoveStarted();
    void sendEventMoveFinished();
    void sendEventRotateStarted();
    void sendEventRotateFinished();
    void sendEventChangeColorSucceeded();
    void sendEventTurnOnLightSucceeded();
    void sendEventTurnOffLightSucceeded();
    void sendEventDanceStarted();
    void sendEventDanceFinished();
    void sendEventExceptionEncountered(const std::string& directive, const std::string& message);
    void sendEventCommon(std::string ename);

    void parsingMove(const char* message);
    void parsingRotate(const char* message);
    void parsingChangeLightColor(const char* message);
    void parsingTurnOnLight(const char* message);
    void parsingTurnOffLight(const char* message);
    void parsingDance(const char* message);
    void parsingStop(const char* message);

    std::string ps_id;
    MovementState cur_state;
    MovementDirection cur_direction;
    bool cur_light;
    MovementLightColor cur_lightcolor;

    IMovementListener* movement_listener;
};

} // NuguCore

#endif
