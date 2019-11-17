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

#include "location_agent.hh"
#include "nugu_log.h"

namespace NuguCore {

static const std::string capability_version = "1.0";

LocationAgent::LocationAgent()
    : Capability(CapabilityType::Location, capability_version)
{
}

LocationAgent::~LocationAgent()
{
}

void LocationAgent::setCapabilityListener(ICapabilityListener* clistener)
{
    if (clistener)
        location_listener = dynamic_cast<ILocationListener*>(clistener);
}

void LocationAgent::updateInfoForContext(Json::Value& ctx)
{
    Json::Value location;
    LocationInfo location_info { Location::State::UNAVAILABLE, false, "", "" };
    std::string state = Location::StateMap.at(location_info.state);

    location["version"] = getVersion();

    if (location_listener) {
        location_listener->requestContext(location_info);

        try {
            state = Location::StateMap.at(location_info.state);
        } catch (std::out_of_range& exception) {
            nugu_warn("Such location state is not exist.");
        }
    }

    location["state"] = state;

    // set current if required conditions are satisfied
    if (location_info.state == Location::State::AVAILABLE && location_info.permission_granted
        && !location_info.latitude.empty() && !location_info.longitude.empty()) {
        Json::Value current;

        current["latitude"] = location_info.latitude;
        current["longitude"] = location_info.longitude;
        location["current"] = current;
    }

    ctx[getName()] = location;
}

} // NuguCore