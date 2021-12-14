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
static const char* CAPABILITY_VERSION = "1.2";

ChipsAgent::ChipsAgent()
    : Capability(CAPABILITY_NAME, CAPABILITY_VERSION)
{
}

void ChipsAgent::initialize()
{
    if (initialized) {
        nugu_warn("It's already initialized.");
        return;
    }

    Capability::initialize();

    addBlockingPolicy("Render", { BlockingMedium::AUDIO, true });

    initialized = true;
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

    try {
        if (!reader.parse(message, root))
            throw std::logic_error("parsing error");

        if (root["playServiceId"].empty() || root["target"].empty() || root["chips"].empty())
            throw std::logic_error("The required parameters are not set");

        ChipsInfo chips_info;
        chips_info.play_service_id = root["playServiceId"].asString();
        chips_info.target = TARGETS.at(root["target"].asString());

        for (const auto& chip : root["chips"]) {
            if (chip["type"].empty() || chip["text"].empty())
                continue;

            chips_info.contents.emplace_back(ChipsInfo::Content {
                CHIPS_TYPES.at(chip["type"].asString()),
                chip["text"].asString(),
                chip["token"].asString() });
        }

        if (chips_listener)
            chips_listener->onReceiveRender(std::move(chips_info));
    } catch (std::out_of_range& e) {
        nugu_error("The target or chip.type is invalid");
    } catch (std::logic_error& e) {
        nugu_error(e.what());
    }
}

} // NuguCapability
