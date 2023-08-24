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

#include "permission_agent.hh"

static const char* CAPABILITY_NAME = "Permission";
static const char* CAPABILITY_VERSION = "1.0";

PermissionAgent::PermissionAgent()
    : Capability(CAPABILITY_NAME, CAPABILITY_VERSION)
{
}

void PermissionAgent::setCapabilityListener(ICapabilityListener* clistener)
{
    if (clistener)
        permission_listener = dynamic_cast<IPermissionListener*>(clistener);
}

void PermissionAgent::updateInfoForContext(NJson::Value& ctx)
{
    NJson::Value permission;
    std::vector<PermissionInfo> permission_infos;

    permission["version"] = getVersion();

    if (permission_listener)
        permission_listener->requestContext(permission_infos);

    for (const auto& permission_info : permission_infos) {
        if (permission_info.name.empty() || permission_info.state.empty())
            continue;

        NJson::Value permission_element;
        permission_element["name"] = permission_info.name;
        permission_element["state"] = permission_info.state;
        permission["permissions"].append(permission_element);
    }

    ctx[getName()] = permission;
}
