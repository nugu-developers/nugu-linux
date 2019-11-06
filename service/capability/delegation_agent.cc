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

bool DelegationAgent::setContextInfo(const std::string& ps_id, const std::string& data)
{
    if (ps_id.empty() || data.empty()) {
        nugu_error("The required datas are empty");
        return false;
    }

    std::get<ContextInfo::IS_USE>(context_info) = true;
    std::get<ContextInfo::PLAY_SERVICE_ID>(context_info) = ps_id;
    std::get<ContextInfo::DATA>(context_info) = data;

    return true;
}

void DelegationAgent::removeContextInfo()
{
    std::get<ContextInfo::IS_USE>(context_info) = false;
    std::get<ContextInfo::PLAY_SERVICE_ID>(context_info).clear();
    std::get<ContextInfo::DATA>(context_info).clear();
}

void DelegationAgent::updateInfoForContext(Json::Value& ctx)
{
    if (std::get<ContextInfo::IS_USE>(context_info)) {
        Json::Value delegation;
        delegation["version"] = getVersion();
        delegation["playServiceId"] = std::get<ContextInfo::PLAY_SERVICE_ID>(context_info);

        Json::Value root;
        Json::Reader reader;

        if (!reader.parse(std::get<ContextInfo::DATA>(context_info), root)) {
            nugu_error("parsing error");
            return;
        }

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

void DelegationAgent::processDirective(NuguDirective* ndir)
{
    const char* dname = nugu_directive_peek_name(ndir);
    const char* message = nugu_directive_peek_json(ndir);

    if (!message || !dname) {
        nugu_error("directive message is not correct");
        destoryDirective(ndir);
        return;
    }

    nugu_dbg("message: %s", message);

    if (!strcmp(dname, "Delegate"))
        parsingDelegate(message);
    else {
        nugu_warn("%s[%s] is not support %s directive", getName().c_str(), getVersion().c_str(), dname);
    }

    destoryDirective(ndir);
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
