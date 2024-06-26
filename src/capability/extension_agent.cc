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
#include "extension_agent.hh"

namespace NuguCapability {

static const char* CAPABILITY_NAME = "Extension";
static const char* CAPABILITY_VERSION = "1.1";

ExtensionAgent::ExtensionAgent()
    : Capability(CAPABILITY_NAME, CAPABILITY_VERSION)
{
}

void ExtensionAgent::initialize()
{
    if (initialized) {
        nugu_warn("It's already initialized.");
        return;
    }

    Capability::initialize();

    ps_id.clear();

    addReferrerEvents("ActionSucceeded", "Action");
    addReferrerEvents("ActionFailed", "Action");

    initialized = true;
}

void ExtensionAgent::deInitialize()
{
    initialized = false;
}

void ExtensionAgent::setCapabilityListener(ICapabilityListener* clistener)
{
    if (clistener)
        extension_listener = dynamic_cast<IExtensionListener*>(clistener);
}

void ExtensionAgent::updateInfoForContext(NJson::Value& ctx)
{
    NJson::Value root;
    NJson::Value data;
    NJson::Reader reader;
    std::string context_info;

    if (extension_listener)
        extension_listener->requestContext(context_info);

    if (!context_info.size())
        return;

    if (!reader.parse(context_info, data)) {
        nugu_error("parsing error");
        return;
    }

    root["version"] = getVersion();
    root["data"] = data;

    ctx[getName()] = root;
}

void ExtensionAgent::parsingDirective(const char* dname, const char* message)
{
    nugu_dbg("message: %s", message);

    if (!strcmp(dname, "Action"))
        parsingAction(message);
    else
        nugu_warn("%s[%s] is not support %s directive", getName().c_str(), getVersion().c_str(), dname);
}

void ExtensionAgent::actionSucceeded()
{
    postProcessDirective();
    sendEventCommon("ActionSucceeded");
}

void ExtensionAgent::actionFailed()
{
    postProcessDirective();
    sendEventCommon("ActionFailed");
}

void ExtensionAgent::commandIssued(const std::string& ps_id, const std::string& data)
{
    sendEventCommandIssued(ps_id, data);
}

void ExtensionAgent::postProcessDirective()
{
    if (action_dir) {
        destroyDirective(action_dir);
        action_dir = nullptr;
    }
}

void ExtensionAgent::sendEventCommandIssued(const std::string& ps_id, const std::string& data, EventResultCallback cb)
{
    NJson::FastWriter writer;
    NJson::Value root;

    root["playServiceId"] = ps_id;
    root["data"] = data;

    sendEvent("CommandIssued", getContextInfo(), writer.write(root), std::move(cb));
}

void ExtensionAgent::sendEventCommon(std::string&& ename, EventResultCallback cb)
{
    NJson::FastWriter writer;
    NJson::Value root;

    root["playServiceId"] = ps_id;

    sendEvent(ename, getContextInfo(), writer.write(root), std::move(cb));
}

void ExtensionAgent::parsingAction(const char* message)
{
    NJson::Value root;
    NJson::Reader reader;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    ps_id = root["playServiceId"].asString();
    NJson::Value data = root["data"];

    if (data.isNull()) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    if (extension_listener) {
        NJson::FastWriter writer;
        std::string action = writer.write(data);

        destroy_directive_by_agent = true;
        action_dir = getNuguDirective();

        extension_listener->receiveAction(action, ps_id, nugu_directive_peek_dialog_id(action_dir));
    } else {
        actionFailed();
    }
}

} // NuguCapability
