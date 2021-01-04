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

#include "capability/extension_interface.hh"
#include "clientkit/capability.hh"

namespace NuguCapability {

class ExtensionAgent final : public Capability,
                             public IExtensionHandler {
public:
    ExtensionAgent();
    virtual ~ExtensionAgent() = default;

    void initialize() override;
    void deInitialize() override;
    void setCapabilityListener(ICapabilityListener* clistener) override;

    void setContextInformation(const std::string& ctx) override;
    void updateInfoForContext(Json::Value& ctx) override;
    void parsingDirective(const char* dname, const char* message) override;
    void actionSucceeded() override;
    void actionFailed() override;
    void commandIssued(const std::string& ps_id, const std::string& data) override;

private:
    void postProcessDirective();
    void sendEventCommandIssued(const std::string& ps_id, const std::string& data, EventResultCallback cb = nullptr);
    void sendEventCommon(std::string&& ename, EventResultCallback cb = nullptr);
    void parsingAction(const char* message);

    IExtensionListener* extension_listener = nullptr;
    NuguDirective* action_dir = nullptr;
    std::string context_info;
    std::string ps_id;
};

} // NuguCapability

#endif /* __NUGU_EXTENSION_AGENT_H__ */
