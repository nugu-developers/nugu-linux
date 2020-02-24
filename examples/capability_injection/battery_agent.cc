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

#include <algorithm>
#include <iostream>

#include "battery_agent.hh"

static const char* CAPABILITY_NAME = "Battery";
static const char* CAPABILITY_VERSION = "1.0";

void BatteryAgent::setBatteryLevel(const std::string& level)
{
    battery_level = level;
}

void BatteryAgent::setCharging(bool charging)
{
    battery_charging = charging;
}

void BatteryAgent::setNuguCoreContainer(INuguCoreContainer* core_container)
{
    this->core_container = core_container;
}

void BatteryAgent::initialize()
{
    // TODO : implements service logic
}

void BatteryAgent::deInitialize()
{
    // TODO : implements service logic
}

void BatteryAgent::suspend()
{
    // TODO : implements service logic
}

void BatteryAgent::restore()
{
    // TODO : implements service logic
}

std::string BatteryAgent::getName()
{
    return CAPABILITY_NAME;
}

std::string BatteryAgent::getVersion()
{
    return CAPABILITY_VERSION;
}

void BatteryAgent::processDirective(NuguDirective* ndir)
{
    // TODO : implements service logic
}

void BatteryAgent::updateInfoForContext(Json::Value& ctx)
{
    Json::Value battery;

    battery["version"] = getVersion();
    battery["level"] = battery_level;
    battery["charging"] = battery_charging;
    ctx[getName()] = battery;

    std::cout << ">> Collecting BatteryAgent Context Info : " << battery << std::endl;
}

void BatteryAgent::receiveCommand(const std::string& from, const std::string& command, const std::string& param)
{
    // TODO : implements service logic
}

void BatteryAgent::receiveCommandAll(const std::string& command, const std::string& param)
{
    // TODO : implements service logic
}

void BatteryAgent::getProperty(const std::string& property, std::string& value)
{
    // TODO : implements service logic
}

void BatteryAgent::getProperties(const std::string& property, std::list<std::string>& values)
{
    // TODO : implements service logic
}

void BatteryAgent::setCapabilityListener(ICapabilityListener* clistener)
{
    if (clistener)
        battery_listener = dynamic_cast<IBatteryListener*>(clistener);
}
