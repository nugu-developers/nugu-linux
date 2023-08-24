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
#include "utility_agent.hh"

namespace NuguCapability {

static const char* CAPABILITY_NAME = "Utility";
static const char* CAPABILITY_VERSION = "1.0";

UtilityAgent::UtilityAgent()
    : Capability(CAPABILITY_NAME, CAPABILITY_VERSION)
{
}

void UtilityAgent::initialize()
{
    if (initialized) {
        nugu_info("It's already initialized.");
        return;
    }

    Capability::initialize();

    timer = std::unique_ptr<INuguTimer>(core_container->createNuguTimer(true));

    addBlockingPolicy("Block", { BlockingMedium::ANY, true });

    initialized = true;
}

void UtilityAgent::deInitialize()
{
    if (timer) {
        timer->stop();
        timer.reset();
    }

    ps_id.clear();

    initialized = false;
}

void UtilityAgent::setCapabilityListener(ICapabilityListener* clistener)
{
    if (clistener)
        utility_listener = dynamic_cast<IUtilityListener*>(clistener);
}

void UtilityAgent::updateInfoForContext(NJson::Value& ctx)
{
    NJson::Value utility;

    utility["version"] = getVersion();
    ctx[getName()] = utility;
}

void UtilityAgent::parsingDirective(const char* dname, const char* message)
{
    nugu_dbg("message: %s", message);

    if (!strcmp(dname, "Block"))
        parsingBlock(message);
    else
        nugu_warn("%s[%s] is not support %s directive", getName().c_str(), getVersion().c_str(), dname);
}

void UtilityAgent::parsingBlock(const char* message)
{
    NJson::Value root;
    NJson::Reader reader;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    if (root["playServiceId"].empty()) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    ps_id = root["playServiceId"].asString();

    if (!root["sleepInMillisecond"].empty())
        delayReleasingBlocking(root["sleepInMillisecond"].asLargestInt());
}

void UtilityAgent::delayReleasingBlocking(long sleep_time_msec)
{
    if (sleep_time_msec <= 0 || !timer)
        return;

    destroy_directive_by_agent = true;

    timer->setCallback([&]() {
        destroyDirective(getNuguDirective());
    });
    timer->setInterval(sleep_time_msec);
    timer->start();
}

} // NuguCapability
