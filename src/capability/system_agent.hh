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

#include "capability/system_interface.hh"
#include "clientkit/capability.hh"

namespace NuguCapability {

class SystemAgent final : public Capability,
                          public ISystemHandler {
public:
    SystemAgent();
    virtual ~SystemAgent();

    void initialize() override;
    void deInitialize() override;

    void parsingDirective(const char* dname, const char* message) override;
    void updateInfoForContext(Json::Value& ctx) override;
    void setCapabilityListener(ICapabilityListener* clistener) override;

    bool receiveCommand(const std::string& from, const std::string& command, const std::string& param) override;

    void synchronizeState() override;
    void updateUserActivity() override;

    void sendEventSynchronizeState(EventResultCallback cb = nullptr);
    void sendEventUserInactivityReport(int seconds, EventResultCallback cb = nullptr);
    void sendEventEcho(EventResultCallback cb = nullptr);
    void sendEventDisconnect(EventResultCallback cb = nullptr);

private:
    // parsing directive
    void parsingResetUserInactivity(const char* message);
    void parsingHandoffConnection(const char* message);
    void parsingTurnOff(const char* message);
    void parsingUpdateState(const char* message);
    void parsingException(const char* message);
    void parsingNoDirectives(const char* message);
    void parsingRevoke(const char* message);
    void parsingNoop(const char* message);
    void parsingResetConnection(const char* message);

    ISystemListener* system_listener;
    INuguTimer* timer;
};

} // NuguCapability

#endif
