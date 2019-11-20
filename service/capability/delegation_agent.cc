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

#include "capability_manager.hh"
#include "delegation_agent.hh"
#include "nugu_log.h"

namespace NuguCore {

static const std::string capability_version = "1.0";

DelegationAgent::DelegationAgent()
    : Capability(CapabilityType::Delegation, capability_version)
{
}

DelegationAgent::~DelegationAgent()
{
}

void DelegationAgent::setCapabilityListener(ICapabilityListener* clistener)
{
    if (clistener)
        delegation_listener = dynamic_cast<IDelegationListener*>(clistener);
}

void DelegationAgent::updateInfoForContext(Json::Value& ctx)
{
    if (!delegation_listener) {
        nugu_warn("DelegationListener is not set");
        return;
    }

    std::string ps_id;
    std::string data;

    if (delegation_listener->requestContext(ps_id, data)) {
        Json::Value root;
        Json::Reader reader;

        if (ps_id.empty() || !reader.parse(data, root)) {
            nugu_error("The required parameters are not set");
            return;
        }

        Json::Value delegation;
        delegation["version"] = getVersion();
        delegation["playServiceId"] = ps_id;
        delegation["data"] = root;

        ctx[getName()] = delegation;
    }
}

std::string DelegationAgent::getContextInfo()
{
    Json::Value ctx;
    updateInfoForContext(ctx);

    return !ctx.empty() ? CapabilityManager::getInstance()->makeContextInfo(ctx) : "";
}

void DelegationAgent::parsingDirective(const char* dname, const char* message)
{
    nugu_dbg("message: %s", message);

    if (!strcmp(dname, "Delegate"))
        parsingDelegate(message);
    else {
        nugu_warn("%s[%s] is not support %s directive", getName().c_str(), getVersion().c_str(), dname);
    }
}

void DelegationAgent::parsingDelegate(const char* message)
{
    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    std::string app_id = root["appId"].asString();
    std::string ps_id = root["playServiceId"].asString();
    Json::Value data = root["data"];

    if (app_id.size() == 0 || ps_id.size() == 0 || data.isNull()) {
        nugu_error("The Manatory datas are insufficient to process");
        return;
    }

    if (delegation_listener) {
        Json::StyledWriter writer;
        delegation_listener->delegate(app_id, ps_id, writer.write(data));
    }
}

bool DelegationAgent::request()
{
    return sendEventRequest();
}

bool DelegationAgent::sendEventRequest()
{
    std::string context_info = getContextInfo();

    if (context_info.empty()) {
        nugu_error("Context Info is not exist");
        return false;
    }

    std::string ename = "Request";
    std::string payload = "";

    sendEvent(ename, context_info, payload);
    return true;
}

} // NuguCore
