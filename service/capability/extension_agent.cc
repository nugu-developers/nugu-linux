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
#include "extension_agent.hh"
#include "nugu_log.h"

namespace NuguCore {

static const std::string capability_version = "1.0";

ExtensionAgent::ExtensionAgent()
    : Capability(CapabilityType::Extension, capability_version)
{
}

ExtensionAgent::~ExtensionAgent()
{
}

void ExtensionAgent::initialize()
{
}

void ExtensionAgent::processDirective(NuguDirective* ndir)
{
    const char* dname = nugu_directive_peek_name(ndir);
    const char* message = nugu_directive_peek_json(ndir);

    if (!message || !dname) {
        nugu_error("directive message is not correct");
        destoryDirective(ndir);
        return;
    }

    nugu_dbg("message: %s", message);

    // directive name check
    if (!strcmp(dname, "Action"))
        parsingAction(message);
    else {
        nugu_warn("%s[%s] is not support %s directive", getName().c_str(), getVersion().c_str(), dname);
    }

    destoryDirective(ndir);
}

void ExtensionAgent::updateInfoForContext(Json::Value& ctx)
{
    Json::Value extension;

    extension["version"] = getVersion();

    ctx[getName()] = extension;
}

std::string ExtensionAgent::getContextInfo()
{
    Json::Value ctx;
    CapabilityManager* cmanager = CapabilityManager::getInstance();

    updateInfoForContext(ctx);
    return cmanager->makeContextInfo(ctx);
}

void ExtensionAgent::setCapabilityListener(ICapabilityListener* clistener)
{
    if (clistener)
        extension_listener = dynamic_cast<IExtensionListener*>(clistener);
}

void ExtensionAgent::sendEventActionSucceeded()
{
    sendEventCommon("ActionSucceeded");
}

void ExtensionAgent::sendEventActionFailed()
{
    sendEventCommon("ActionFailed");
}

void ExtensionAgent::sendEventCommon(std::string ename)
{
    NuguEvent* event = nugu_event_new(getName().c_str(), ename.c_str(), getVersion().c_str());

    nugu_event_set_context(event, getContextInfo().c_str());

    Json::Value root;
    Json::StyledWriter writer;
    root["playServiceId"] = ps_id;

    nugu_event_set_json(event, writer.write(root).c_str());

    sendEvent(event);

    nugu_event_free(event);
}

void ExtensionAgent::parsingAction(const char* message)
{
    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    ps_id = root["playServiceId"].asString();
    Json::Value data = root["data"];

    if (data.isNull()) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    if (extension_listener) {
        Json::StyledWriter writer;

        if (extension_listener->action(writer.write(data)) == ExtensionResult::SUCCEEDED)
            sendEventActionSucceeded();
        else
            sendEventActionFailed();
    }
}

} // NuguCore
