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

#ifndef __NUGU_DELEGATION_AGENT_H__
#define __NUGU_DELEGATION_AGENT_H__

#include <tuple>

#include <interface/capability/delegation_interface.hh>

#include "capability.hh"

namespace NuguCore {

using namespace NuguInterface;

class DelegationAgent : public Capability, public IDelegationHandler {
public:
    DelegationAgent();
    virtual ~DelegationAgent();

    void setCapabilityListener(ICapabilityListener* clistener) override;
    void updateInfoForContext(Json::Value& ctx) override;
    std::string getContextInfo() override;
    void processDirective(NuguDirective* ndir) override;
    bool request() override;

private:
    void parsingDelegate(const char* message);
    bool sendEventRequest();

    IDelegationListener* delegation_listener = nullptr;
};

} // NuguCore

#endif /* __NUGU_DELEGATION_AGENT_H__ */
