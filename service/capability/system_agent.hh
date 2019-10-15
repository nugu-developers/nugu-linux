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

#ifndef __NUGU_SYSTEM_AGENT_H__
#define __NUGU_SYSTEM_AGENT_H__

#include <functional>

#include <interface/capability/system_interface.hh>

#include "capability.hh"

namespace NuguCore {

using namespace NuguInterface;

class SystemAgent : public Capability, public ISystemHandler {
public:
    SystemAgent();
    virtual ~SystemAgent();
    void initialize() override;

    void processDirective(NuguDirective* ndir) override;
    void updateInfoForContext(Json::Value& ctx) override;
    std::string getContextInfo();
    void receiveCommand(CapabilityType from, std::string command, std::string param) override;
    void receiveCommandAll(std::string command, std::string param) override;
    void setCapabilityListener(ICapabilityListener* clistener) override;

    void sendEventSynchronizeState(void) override;
    void sendEventUserInactivityReport(int seconds);
    void sendEventDisconnect(void);
    void sendEventEcho(void);

private:
    // parsing directive
    void parsingResetUserInactivity(const char* message);
    void parsingHandoffConnection(const char* message);
    void parsingTurnOff(const char* message);
    void parsingUpdateState(const char* message);
    void parsingException(const char* message);
    void parsingNoDirectives(const char* message, const char* dialog_id);
    void parsingRevoke(const char* message);

    ISystemListener* system_listener;
    int battery;
};

} // NuguCore

#endif
