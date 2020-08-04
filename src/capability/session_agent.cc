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
#include "session_agent.hh"

namespace NuguCapability {

static const char* CAPABILITY_NAME = "Session";
static const char* CAPABILITY_VERSION = "1.0";

SessionAgent::SessionAgent()
    : Capability(CAPABILITY_NAME, CAPABILITY_VERSION)
{
}

void SessionAgent::initialize()
{
    if (initialized) {
        nugu_info("It's already initialized.");
        return;
    }

    session_manager->addListener(this);

    initialized = true;
}

void SessionAgent::deInitialize()
{
    session_manager->removeListener(this);

    initialized = false;
}

void SessionAgent::setCapabilityListener(ICapabilityListener* clistener)
{
    if (clistener)
        session_listener = dynamic_cast<ISessionListener*>(clistener);
}

void SessionAgent::updateInfoForContext(Json::Value& ctx)
{
    Json::Value session;
    Json::Value session_list = session_manager->getActiveSessionInfo();

    session["version"] = getVersion();

    if (!session_list.empty())
        session["list"] = session_list;

    ctx[getName()] = session;
}

void SessionAgent::parsingDirective(const char* dname, const char* message)
{
    nugu_dbg("message: %s", message);

    if (!strcmp(dname, "Set"))
        parsingSet(message);
    else {
        nugu_warn("%s[%s] is not support %s directive",
            getName().c_str(), getVersion().c_str(), dname);
    }
}

void SessionAgent::parsingSet(const char* message)
{
    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    play_service_id = root["playServiceId"].asString();
    session_id = root["sessionId"].asString();

    if (play_service_id.empty() || session_id.empty()) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    session_manager->set(nugu_directive_peek_dialog_id(getNuguDirective()),
        Session { session_id, play_service_id });
}

void SessionAgent::activated(const std::string& dialog_id, Session session)
{
    // Even if it's possible to receive session info from SessionManager,
    // in currently, there are no reason to send session info to application.
    // So, it just leave that session info for handling later.

    nugu_dbg("session activated: %s", dialog_id.c_str());

    if (session_listener)
        session_listener->onState(SessionState::ACTIVE, dialog_id);
}

void SessionAgent::deactivated(const std::string& dialog_id)
{
    nugu_dbg("session deactivated: %s", dialog_id.c_str());

    focus_manager->unholdFocus(DIALOG_FOCUS_TYPE);

    if (session_listener)
        session_listener->onState(SessionState::INACTIVE, dialog_id);
}

} // NuguCapability
