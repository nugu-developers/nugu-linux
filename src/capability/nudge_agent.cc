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

#include <cstring>

#include "base/nugu_log.h"
#include "nudge_agent.hh"

namespace NuguCapability {

static const char* CAPABILITY_NAME = "Nudge";
static const char* CAPABILITY_VERSION = "1.0";

NudgeAgent::NudgeAgent()
    : Capability(CAPABILITY_NAME, CAPABILITY_VERSION)
{
}

void NudgeAgent::initialize()
{
    if (initialized) {
        nugu_info("It's already initialized.");
        return;
    }

    Capability::initialize();

    state_checkers.clear();
    dialog_id.clear();
    nudge_info = NJson::nullValue;

    initialized = true;
}

void NudgeAgent::setCapabilityListener(ICapabilityListener* clistener)
{
    if (clistener)
        nudge_listener = dynamic_cast<INudgeListener*>(clistener);
}

void NudgeAgent::updateInfoForContext(NJson::Value& ctx)
{
    NJson::Value nudge;

    nudge["version"] = getVersion();

    if (!nudge_info.empty())
        nudge["nudgeInfo"] = nudge_info;

    ctx[getName()] = nudge;
}

void NudgeAgent::preprocessDirective(NuguDirective* ndir)
{
    const char* dname;
    const char* message;

    if (!ndir
        || !(dname = nugu_directive_peek_name(ndir))
        || !(message = nugu_directive_peek_json(ndir))) {
        nugu_error("The directive info is not exist.");
        return;
    }

    nugu_dbg("message: %s", message);

    if (!strcmp(dname, "Append")) {
        prepareNudgeStateCheck(ndir);
        parsingAppend(message);
    } else
        nugu_warn("%s[%s] is not support %s directive", getName().c_str(), getVersion().c_str(), dname);
}

void NudgeAgent::prepareNudgeStateCheck(NuguDirective* ndir)
{
    dialog_id = nugu_directive_peek_dialog_id(ndir);

    std::string dir_groups = nugu_directive_peek_groups(ndir);
    state_checkers.clear();

    for (const auto& filter : DIRECTIVE_FILTERS)
        if (dir_groups.find(filter.first + "." + filter.second) != std::string::npos)
            state_checkers.emplace(filter.first);
}

void NudgeAgent::parsingAppend(const char* message)
{
    NJson::Value root;
    NJson::Reader reader;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    if (root["nudgeInfo"].empty()) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    nudge_info = root["nudgeInfo"];
}

bool NudgeAgent::receiveCommand(const std::string& from, const std::string& command, const std::string& param)
{
    nugu_dbg("receive command(%s) from %s", command.c_str(), from.c_str());

    if (command == "clearNudge") {
        updateNudgeState(from, param);
    } else {
        nugu_error("invalid command: %s", command.c_str());
        return false;
    }

    return true;
}

void NudgeAgent::updateNudgeState(const std::string& filter, const std::string& dialog_id)
{
    if (this->dialog_id.empty() || this->dialog_id != dialog_id) {
        nugu_warn("The dialog ID is empty or not matched.");
        return;
    }

    state_checkers.erase(filter);

    if (state_checkers.empty()) {
        this->dialog_id.clear();
        nudge_info = NJson::nullValue;
    }
}

} // NuguCapability
