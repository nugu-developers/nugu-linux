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
#include "sound_agent.hh"

namespace NuguCapability {

static const char* CAPABILITY_NAME = "Sound";
static const char* CAPABILITY_VERSION = "1.0";

SoundAgent::SoundAgent()
    : Capability(CAPABILITY_NAME, CAPABILITY_VERSION)
{
}

void SoundAgent::initialize()
{
    if (initialized) {
        nugu_info("It's already initialized.");
        return;
    }

    Capability::initialize();

    play_service_id = "";

    addBlockingPolicy("Beep", { BlockingMedium::AUDIO, true });

    initialized = true;
}

void SoundAgent::deInitialize()
{
    initialized = false;
}

void SoundAgent::setCapabilityListener(ICapabilityListener* clistener)
{
    if (clistener)
        sound_listener = dynamic_cast<ISoundListener*>(clistener);
}

void SoundAgent::updateInfoForContext(Json::Value& ctx)
{
    Json::Value sound;

    sound["version"] = getVersion();

    ctx[getName()] = sound;
}

void SoundAgent::sendBeepResult(bool is_succeeded)
{
    if (is_succeeded)
        sendEventBeepSucceeded();
    else
        sendEventBeepFailed();
}
/*******************************************************************************
 * parse directive
 ******************************************************************************/

void SoundAgent::parsingDirective(const char* dname, const char* message)
{
    nugu_dbg("message: %s", message);

    if (!strcmp(dname, "Beep"))
        parsingBeep(message);
    else {
        nugu_warn("%s[%s] is not support %s directive",
            getName().c_str(), getVersion().c_str(), dname);
    }
}

void SoundAgent::parsingBeep(const char* message)
{
    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    play_service_id = root["playServiceId"].asString();
    std::string beep_name = root["beepName"].asString();

    if (play_service_id.empty() || beep_name.empty()) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    if (sound_listener) {
        try {
            sound_listener->handleBeep(BEEP_TYPE_MAP.at(beep_name), nugu_directive_peek_dialog_id(getNuguDirective()));
        } catch (const std::out_of_range& oor) {
            nugu_warn("There are not exist matched beep type.");

            sendBeepResult(false);
        }
    }
}

/*******************************************************************************
 * send event
 ******************************************************************************/

void SoundAgent::sendEventBeepSucceeded(EventResultCallback cb)
{
    sendEventCommon("BeepSucceeded", std::move(cb));
}

void SoundAgent::sendEventBeepFailed(EventResultCallback cb)
{
    sendEventCommon("BeepFailed", std::move(cb));
}

void SoundAgent::sendEventCommon(std::string&& event_name, EventResultCallback cb)
{
    if (play_service_id.empty()) {
        nugu_error("The mandatory data is not exist.");
        return;
    }

    Json::StyledWriter writer;
    Json::Value root;

    root["playServiceId"] = play_service_id;

    sendEvent(event_name, getContextInfo(), writer.write(root), std::move(cb));
}

} // NuguCapability
