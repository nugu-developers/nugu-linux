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
#include "speaker_agent.hh"

namespace NuguCapability {

static const char* CAPABILITY_NAME = "Speaker";
static const char* CAPABILITY_VERSION = "1.1";

SpeakerAgent::SpeakerAgent()
    : Capability(CAPABILITY_NAME, CAPABILITY_VERSION)
    , speaker_listener(nullptr)
{
    // compose maps for SpeakerType and name string
    speaker_names_for_types = {
        { SpeakerType::NUGU, "NUGU" },
        { SpeakerType::MUSIC, "MUSIC" },
        { SpeakerType::RINGTON, "RINGTON" },
        { SpeakerType::CALL, "CALL" },
        { SpeakerType::NOTIFICATION, "NOTIFICATION" },
        { SpeakerType::ALARM, "ALARM" },
        { SpeakerType::VOICE_COMMAND, "VOICE_COMMAND" },
        { SpeakerType::NAVIGATION, "NAVIGATION" },
        { SpeakerType::SYSTEM_SOUND, "SYSTEM_SOUND" },
    };

    std::for_each(speaker_names_for_types.cbegin(), speaker_names_for_types.cend(),
        [&](const std::pair<SpeakerType, std::string>& element) {
            speaker_types_for_names.emplace(element.second, element.first);
        });
}

void SpeakerAgent::initialize()
{
    if (initialized) {
        nugu_info("It's already initialized.");
        return;
    }

    Capability::initialize();

    addReferrerEvents("SetVolumeSucceeded", "SetVolume");
    addReferrerEvents("SetVolumeFailed", "SetVolume");
    addReferrerEvents("SetMuteSucceeded", "SetMute");
    addReferrerEvents("SetMuteFailed", "SetMute");

    addBlockingPolicy("SetVolume", { BlockingMedium::AUDIO, true });
    addBlockingPolicy("SetMute", { BlockingMedium::AUDIO, true });

    initialized = true;
}

void SpeakerAgent::deInitialize()
{
    speakers.clear();
}

void SpeakerAgent::parsingDirective(const char* dname, const char* message)
{
    nugu_dbg("message: %s", message);

    // directive name check
    if (!strcmp(dname, "SetVolume"))
        parsingSetVolume(message);
    else if (!strcmp(dname, "SetMute"))
        parsingSetMute(message);
    else {
        nugu_warn("%s[%s] is not support %s directive", getName().c_str(), getVersion().c_str(), dname);
    }
}

void SpeakerAgent::updateInfoForContext(Json::Value& ctx)
{
    Json::Value speaker;
    Json::Value volumes;

    speaker["version"] = getVersion();
    for (const auto& container : speakers) {
        SpeakerInfo* sinfo = container.second.get();
        if (!sinfo->can_control)
            continue;

        Json::Value volume;
        volume["name"] = getSpeakerName(sinfo->type);
        if (sinfo->volume != NUGU_SPEAKER_UNABLE_CONTROL)
            volume["volume"] = sinfo->volume;
        if (sinfo->min != NUGU_SPEAKER_UNABLE_CONTROL)
            volume["minVolume"] = sinfo->min;
        if (sinfo->max != NUGU_SPEAKER_UNABLE_CONTROL)
            volume["maxVolume"] = sinfo->max;
        if (sinfo->step != NUGU_SPEAKER_UNABLE_CONTROL)
            volume["defaultVolumeStep"] = sinfo->step;
        if (sinfo->mute != NUGU_SPEAKER_UNABLE_CONTROL)
            volume["muted"] = sinfo->mute ? true : false;

        speaker["volumes"].append(volume);
    }
    ctx[getName()] = speaker;
}

void SpeakerAgent::setCapabilityListener(ICapabilityListener* clistener)
{
    if (clistener)
        speaker_listener = dynamic_cast<ISpeakerListener*>(clistener);
}

bool SpeakerAgent::getProperty(const std::string& property, std::string& value)
{
    std::string convert_property;
    convert_property.resize(property.size());
    std::transform(property.cbegin(), property.cend(), convert_property.begin(), ::toupper);

    value = std::to_string(NUGU_SPEAKER_MAX_VOLUME);

    SpeakerType type;
    if (!getSpeakerType(convert_property, type)) {
        nugu_warn("property(%s) is not exist, so use the property(nugu)", property.c_str());
        type = SpeakerType::NUGU;
    }

    if (speakers.find(type) != speakers.end() && speakers[type]->can_control)
        value = std::to_string(speakers[type]->volume);
    else if (speakers.find(SpeakerType::NUGU) != speakers.end() && speakers[SpeakerType::NUGU]->can_control)
        value = std::to_string(speakers[SpeakerType::NUGU]->volume);

    nugu_dbg("request to get property(%s) and return value(%s)", property.c_str(), value.c_str());

    return true;
}

void SpeakerAgent::setSpeakerInfo(const std::map<SpeakerType, SpeakerInfo>& info)
{
    if (info.size() == 0)
        return;

    for (const auto& container : info) {
        speakers.emplace(container.second.type, std::unique_ptr<SpeakerInfo>(new SpeakerInfo(container.second)));

        nugu_dbg("speaker - %s %d[%d - %d], mute: %d, can_control: %d",
            getSpeakerName(container.second.type).c_str(), container.second.volume, container.second.min, container.second.max, container.second.mute, container.second.can_control);
    }
}

void SpeakerAgent::informVolumeChanged(SpeakerType type, int volume)
{
    nugu_dbg("application change the volume[%d] => %d", type, volume);
    updateSpeakerVolume(type, volume);
}

void SpeakerAgent::informMuteChanged(SpeakerType type, bool mute)
{
    nugu_dbg("application change the mute[%d] => %d", type, mute);
    updateSpeakerMute(type, mute);
}

void SpeakerAgent::sendEventVolumeChanged(const std::string& ps_id, bool result)
{
    nugu_dbg("application send the volume result(%d) event to the Play(%s)", result, ps_id.c_str());
    if (result)
        sendEventSetVolumeSucceeded(ps_id);
    else
        sendEventSetVolumeFailed(ps_id);
}

void SpeakerAgent::sendEventMuteChanged(const std::string& ps_id, bool result)
{
    nugu_dbg("application send the mute result(%d) event to the Play(%s)", result, ps_id.c_str());
    if (result)
        sendEventSetMuteSucceeded(ps_id);
    else
        sendEventSetMuteFailed(ps_id);
}

void SpeakerAgent::updateSpeakerVolume(SpeakerType type, int volume)
{
    for (const auto& container : speakers) {
        SpeakerInfo* sinfo = container.second.get();
        if (sinfo->type == type) {
            sinfo->volume = volume;
            break;
        }
    }
}

void SpeakerAgent::updateSpeakerMute(SpeakerType type, bool mute)
{
    for (const auto& container : speakers) {
        SpeakerInfo* sinfo = container.second.get();
        if (sinfo->type == type) {
            sinfo->mute = mute;
            break;
        }
    }
}

bool SpeakerAgent::getSpeakerType(const std::string& name, SpeakerType& type)
{
    try {
        type = speaker_types_for_names.at(name);
        return true;
    } catch (const std::out_of_range& oor) {
        return false;
    }
}

std::string SpeakerAgent::getSpeakerName(const SpeakerType& type)
{
    try {
        return speaker_names_for_types.at(type);
    } catch (const std::out_of_range& oor) {
        return "NUGU";
    }
}

void SpeakerAgent::sendEventSetVolumeSucceeded(const std::string& ps_id, EventResultCallback cb)
{
    sendEventCommon(ps_id, "SetVolumeSucceeded", std::move(cb));
}

void SpeakerAgent::sendEventSetVolumeFailed(const std::string& ps_id, EventResultCallback cb)
{
    sendEventCommon(ps_id, "SetVolumeFailed", std::move(cb));
}

void SpeakerAgent::sendEventSetMuteSucceeded(const std::string& ps_id, EventResultCallback cb)
{
    sendEventCommon(ps_id, "SetMuteSucceeded", std::move(cb));
}

void SpeakerAgent::sendEventSetMuteFailed(const std::string& ps_id, EventResultCallback cb)
{
    sendEventCommon(ps_id, "SetMuteFailed", std::move(cb));
}

void SpeakerAgent::sendEventCommon(const std::string& ps_id, const std::string& ename, EventResultCallback cb)
{
    std::string payload = "";
    Json::Value root;
    Json::StyledWriter writer;

    if (ps_id.size() == 0) {
        nugu_error("there is something wrong [%s]", ename.c_str());
        return;
    }

    root["playServiceId"] = ps_id;
    payload = writer.write(root);

    sendEvent(ename, getContextInfo(), payload, std::move(cb));
}

void SpeakerAgent::parsingSetVolume(const char* message)
{
    Json::Value root;
    Json::Reader reader;
    Json::Value volumes;
    std::string rate;
    std::string ps_id;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    ps_id = root["playServiceId"].asString();
    rate = root["rate"].asString();
    volumes = root["volumes"];

    if (ps_id.size() == 0 || volumes.empty()) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    for (int i = 0; i < (int)volumes.size(); i++) {
        if (volumes[i]["name"].empty() || volumes[i]["volume"].empty()) {
            nugu_error("There is no mandatory data in directive message");
            return;
        }

        std::string name = volumes[i]["name"].asString();
        int volume = volumes[i]["volume"].asInt();

        SpeakerType type;
        if (!getSpeakerType(name, type)) {
            nugu_error("The name(%s) is outside the scope of the specification", name.c_str());
            continue;
        }

        if (speaker_listener)
            speaker_listener->requestSetVolume(ps_id, type, volume, rate == "SLOW");
    }
}

void SpeakerAgent::parsingSetMute(const char* message)
{
    Json::Value root;
    Json::Reader reader;
    Json::Value volumes;
    std::string ps_id;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    ps_id = root["playServiceId"].asString();
    volumes = root["volumes"];

    if (ps_id.size() == 0 || volumes.empty()) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    for (int i = 0; i < (int)volumes.size(); i++) {
        if (volumes[i]["name"].empty() || volumes[i]["mute"].empty()) {
            nugu_error("There is no mandatory data in directive message");
            return;
        }

        std::string name = volumes[i]["name"].asString();
        bool mute = volumes[i]["mute"].asBool();

        SpeakerType type;
        if (!getSpeakerType(name, type)) {
            nugu_error("The name(%s) is outside the scope of the specification", name.c_str());
            continue;
        }

        if (speaker_listener)
            speaker_listener->requestSetMute(ps_id, type, mute);
    }
}

} // NuguCapability
