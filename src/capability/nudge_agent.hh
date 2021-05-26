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

#ifndef __NUGU_NUDGE_AGENT_H__
#define __NUGU_NUDGE_AGENT_H__

#include "capability/nudge_interface.hh"
#include "clientkit/capability.hh"

namespace NuguCapability {

class NudgeAgent final : public Capability,
                         public INudgeHandler {
public:
    NudgeAgent();
    virtual ~NudgeAgent() = default;

    void initialize() override;
    void setCapabilityListener(ICapabilityListener* clistener) override;
    void updateInfoForContext(Json::Value& ctx) override;
    void preprocessDirective(NuguDirective* ndir) override;
    bool receiveCommand(const std::string& from, const std::string& command, const std::string& param) override;

private:
    void parsingAppend(const char* message);
    void prepareNudgeStateCheck(NuguDirective* ndir);
    void updateNudgeState(const std::string& filter, const std::string& dialog_id);

    std::vector<std::pair<std::string, std::string>> directive_filters;
    INudgeListener* nudge_listener = nullptr;

    std::set<std::string> state_checkers;
    std::string dialog_id;
    Json::Value nudge_info;
};

} // NuguCapability

#endif /* __NUGU_NUDGE_AGENT_H__ */
