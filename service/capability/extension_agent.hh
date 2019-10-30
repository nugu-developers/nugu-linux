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

#ifndef __NUGU_EXTENSION_AGENT_H__
#define __NUGU_EXTENSION_AGENT_H__

#include <interface/capability/extension_interface.hh>

#include "capability.hh"

namespace NuguCore {

using namespace NuguInterface;

class ExtensionAgent : public Capability, public IExtensionHandler {
public:
    ExtensionAgent();
    virtual ~ExtensionAgent();

    void processDirective(NuguDirective* ndir) override;
    void updateInfoForContext(Json::Value& ctx) override;
    void setCapabilityListener(ICapabilityListener* clistener) override;
    void setContextData(std::string& data) override;

private:
    void sendEventCommon(std::string ename);
    void sendEventActionSucceeded();
    void sendEventActionFailed();

    void parsingAction(const char* message);

    IExtensionListener* extension_listener;
    std::string ext_data;
    std::string ps_id;
};

} // NuguCore

#endif /* __NUGU_EXTENSION_AGENT_H__ */
