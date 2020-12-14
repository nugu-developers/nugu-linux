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

#include "base/nugu_log.h"
#include "chips_agent.hh"

namespace NuguCapability {

static const char* CAPABILITY_NAME = "Chips";
static const char* CAPABILITY_VERSION = "1.1";

ChipsAgent::ChipsAgent()
    : Capability(CAPABILITY_NAME, CAPABILITY_VERSION)
{
}

void ChipsAgent::setCapabilityListener(ICapabilityListener* clistener)
{
    if (clistener)
        chips_listener = dynamic_cast<IChipsListener*>(clistener);
}

void ChipsAgent::updateInfoForContext(Json::Value& ctx)
{
    Json::Value chips;

    chips["version"] = getVersion();
    ctx[getName()] = chips;
}

void ChipsAgent::parsingDirective(const char* dname, const char* message)
{
    nugu_dbg("message: %s", message);

    if (!strcmp(dname, "Render"))
        parsingRender(message);
    else
        nugu_warn("%s[%s] is not support %s directive", getName().c_str(), getVersion().c_str(), dname);
}

void ChipsAgent::parsingRender(const char* message)
{
    Json::Value root;
    Json::Reader reader;
    Json::Value chips;

    std::vector<ChipContent> contents;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    if (root["playServiceId"].empty() || root["target"].empty() || root["chips"].empty()) {
        nugu_error("The required parameters are not set");
        return;
    }

    nugu_dbg("chips target: ", root["target"].asString().c_str());

    chips = root["chips"]; // type(Action, General), text

    for (int i = 0; i < (int)chips.size(); i++) {
        if (chips[i]["type"].empty() || chips[i]["text"].empty()) {
            nugu_error("There is no mandatory data in directive message");
            return;
        }

        ChipContent content;
        content.type = chips[i]["type"].asString();
        content.text = chips[i]["text"].asString();
        content.token = chips[i]["token"].asString();
        contents.emplace_back(content);

        nugu_dbg("[%d] type: %s, text: %s, token: %s", i, content.type.c_str(), content.text.c_str(), content.token.c_str());
    }

    if (chips_listener)
        chips_listener->onReceiveRender(contents);
}

} // NuguCapability
