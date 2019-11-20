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
#include <string.h>

#include "capability_manager.hh"
#include "nugu_log.h"
#include "permission_agent.hh"

namespace NuguCore {

static const std::string capability_version = "1.0";

PermissionAgent::PermissionAgent()
    : Capability(CapabilityType::Permission, capability_version)
{
    // set permission type map which is composed by string key and permission::type value
    std::transform(Permission::TypeMap.cbegin(), Permission::TypeMap.cend(), std::inserter(permission_type_map, permission_type_map.end()),
        [](const std::pair<Permission::Type, std::string>& element) {
            return std::make_pair(element.second, element.first);
        });
}

PermissionAgent::~PermissionAgent()
{
}

void PermissionAgent::setCapabilityListener(ICapabilityListener* clistener)
{
    if (clistener)
        permission_listener = dynamic_cast<IPermissionListener*>(clistener);
}

void PermissionAgent::updateInfoForContext(Json::Value& ctx)
{
    Json::Value permission;
    Json::Value permissions = Json::Value(Json::arrayValue);

    permission["version"] = getVersion();

    if (permission_listener) {
        std::string type;
        std::string state;
        std::list<PermissionState> permission_list;

        permission_listener->requestContext(permission_list);

        for (const auto& permission_element : permission_list) {
            try {
                type = Permission::TypeMap.at(permission_element.type);
                state = Permission::StateMap.at(permission_element.state);
            } catch (std::out_of_range& exception) {
                nugu_warn("Such permission type or state is not exist.");
            }

            if (!type.empty() && !state.empty()) {
                Json::Value item;
                item[type] = state;
                permissions.append(item);
            }
        }
    }

    if (!permissions.empty())
        permission["permissions"] = permissions;

    ctx[getName()] = permission;
}

void PermissionAgent::parsingDirective(const char* dname, const char* message)
{
    nugu_dbg("message: %s", message);

    if (!strcmp(dname, "RequestAccess"))
        parsingRequestAccess(message);
    else {
        nugu_warn("%s[%s] is not support %s directive", getName().c_str(), getVersion().c_str(), dname);
    }
}

void PermissionAgent::parsingRequestAccess(const char* message)
{
    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    Json::Value permissions = root["permissions"];
    ps_id = root["playServiceId"].asString();

    if (!permissions.isArray() || permissions.empty() || ps_id.size() == 0) {
        nugu_error("The Manatory datas are insufficient to process");
        return;
    }

    std::set<Permission::Type> permission_set;

    for (unsigned int i = 0; i < permissions.size(); i++) {
        try {
            permission_set.insert(permission_type_map.at(permissions[i].asString()));
        } catch (std::out_of_range& exception) {
            nugu_warn("Such permission type is not exist.");
        }
    }

    if (permission_set.empty()) {
        nugu_error("The Required permission_set is empty or not composed correctly.");
        return;
    }

    // It has to handle synchronously
    if (permission_listener) {
        permission_listener->requestPermission(permission_set);
        sendEventRequestAccessCompleted();
    }
}

void PermissionAgent::sendEventRequestAccessCompleted()
{
    std::string payload = "";
    Json::Value root = ""; // It defined empty in NDI currently
    Json::StyledWriter writer;

    payload = writer.write(root);

    sendEvent("RequestAccessCompleted", CapabilityManager::getInstance()->makeAllContextInfoStack(), payload);
}

} // NuguCore
