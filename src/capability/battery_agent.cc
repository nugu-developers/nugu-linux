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

#include "battery_agent.hh"

namespace NuguCapability {

static const char* CAPABILITY_NAME = "Battery";
static const char* CAPABILITY_VERSION = "1.1";

BatteryAgent::BatteryAgent()
    : Capability(CAPABILITY_NAME, CAPABILITY_VERSION)
{
}

void BatteryAgent::setCapabilityListener(ICapabilityListener* clistener)
{
    if (clistener)
        battery_listener = dynamic_cast<IBatteryListener*>(clistener);
}

void BatteryAgent::updateInfoForContext(NJson::Value& ctx)
{
    NJson::Value battery;
    BatteryInfo battery_info;

    battery["version"] = getVersion();

    if (battery_listener)
        battery_listener->requestContext(battery_info);

    if (battery_info.level >= 0 && battery_info.level <= 100)
        battery["level"] = battery_info.level;

    battery["charging"] = battery_info.charging;
    battery["approximateLevel"] = battery_info.approximate_level;

    ctx[getName()] = battery;
}

} // NuguCapability
