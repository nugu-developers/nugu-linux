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

#ifndef __NUGU_SESSION_AGENT_H__
#define __NUGU_SESSION_AGENT_H__

#include "capability/session_interface.hh"
#include "clientkit/capability.hh"

namespace NuguCapability {

class SessionAgent final : public Capability,
                           public ISessionHandler,
                           public ISessionManagerListener {
public:
    SessionAgent();
    virtual ~SessionAgent() = default;

    void initialize() override;
    void deInitialize() override;
    void setCapabilityListener(ICapabilityListener* clistener) override;
    void updateInfoForContext(Json::Value& ctx) override;
    void parsingDirective(const char* dname, const char* message) override;

private:
    void parsingSet(const char* message);

    // implements ISessionManagerListener
    void activated(const std::string& dialog_id, Session session) override;
    void deactivated(const std::string& dialog_id) override;

    ISessionListener* session_listener = nullptr;
    std::string play_service_id;
    std::string session_id;
};

} // NuguCapability

#endif /* __NUGU_SESSION_AGENT_H__ */
