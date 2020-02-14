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
static const char* CAPABILITY_VERSION = "1.0";

SpeakerAgent::SpeakerAgent()
    : Capability(CAPABILITY_NAME, CAPABILITY_VERSION)
    , speaker_listener(nullptr)
    , ps_id("")
{
}

SpeakerAgent::~SpeakerAgent()
{
    for (auto container : speakers)
        delete container.second;

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
    for (auto container : speakers) {
        SpeakerInfo* sinfo = container.second;
        if (!sinfo->can_control)
            continue;

        if (volumes.empty())
            speaker["volumes"] = Json::Value(Json::arrayValue);

        Json::Value volume;
        volume["name"] = getSpeakerName(sinfo->type);
        volume["volume"] = sinfo->volume;
        volume["minVolume"] = sinfo->min;
        volume["maxVolume"] = sinfo->max;
        volume["muted"] = sinfo->mute;

        speaker["volumes"].append(volume);
    }
    ctx[getName()] = speaker;
}

void SpeakerAgent::setCapabilityListener(ICapabilityListener* clistener)
{
    if (clistener)
        speaker_listener = dynamic_cast<ISpeakerListener*>(clistener);
}

void SpeakerAgent::setSpeakerInfo(std::map<SpeakerType, SpeakerInfo*> info)
{
    if (info.size() == 0)
        return;

    for (auto container : speakers)
        delete container.second;

    for (auto container : info) {
        SpeakerInfo* sinfo = new SpeakerInfo();

        sinfo->type = container.second->type;
        sinfo->min = container.second->min;
        sinfo->max = container.second->max;
        sinfo->volume = container.second->volume;
        sinfo->mute = container.second->mute;
        sinfo->can_control = container.second->can_control;

        speakers[sinfo->type] = sinfo;

        nugu_dbg("speaker - %s %d[%d - %d], can_control: %d", getSpeakerName(sinfo->type).c_str(), sinfo->volume, sinfo->min, sinfo->max, sinfo->can_control);
    }
}

void SpeakerAgent::informVolumeSucceeded(SpeakerType type, int volume)
{
    if (cur_speaker.type == type && cur_speaker.volume == volume)
        sendEventSetVolumeSucceeded();

    updateSpeakerInfo(type, volume, cur_speaker.mute);
}

void SpeakerAgent::informVolumeFailed(SpeakerType type, int volume)
{
    if (cur_speaker.type == type && cur_speaker.volume == volume)
        sendEventSetVolumeFailed();

    updateSpeakerInfo(type, volume, cur_speaker.mute);
}

void SpeakerAgent::informMuteSucceeded(SpeakerType type, bool mute)
{
    if (cur_speaker.type == type && cur_speaker.mute == mute)
        sendEventSetMuteSucceeded();

    updateSpeakerInfo(type, cur_speaker.volume, mute);
}

void SpeakerAgent::informMuteFailed(SpeakerType type, bool mute)
{
    if (cur_speaker.type == type && cur_speaker.mute == mute)
        sendEventSetMuteFailed();

    updateSpeakerInfo(type, cur_speaker.volume, mute);
}

void SpeakerAgent::updateSpeakerInfo(SpeakerType type, int volume, bool mute)
{
    for (auto container : speakers) {
        SpeakerInfo* sinfo = container.second;
        if (sinfo->type == type) {
            sinfo->volume = volume;
            sinfo->mute = mute;
            break;
        }
    }
}

bool SpeakerAgent::getSpeakerType(const std::string& name, SpeakerType& type)
{
    if (name == "NUGU") {
        type = SpeakerType::NUGU;
        return true;
    } else if (name == "CALL") {
        type = SpeakerType::CALL;
        return true;
    } else if (name == "ALARM") {
        type = SpeakerType::ALARM;
        return true;
    } else if (name == "EXTERNAL") {
        type = SpeakerType::EXTERNAL;
        return true;
    }
    return false;
}

std::string SpeakerAgent::getSpeakerName(SpeakerType& type)
{
    if (type == SpeakerType::CALL)
        return "CALL";
    else if (type == SpeakerType::ALARM)
        return "ALARM";
    else if (type == SpeakerType::EXTERNAL)
        return "EXTERNAL";
    else
        return "NUGU";
}

void SpeakerAgent::sendEventSetVolumeSucceeded()
{
    sendEventCommon("SetVolumeSucceeded");
}

void SpeakerAgent::sendEventSetVolumeFailed()
{
    sendEventCommon("SetVolumeFailed");
}

void SpeakerAgent::sendEventSetMuteSucceeded()
{
    sendEventCommon("SetMuteSucceeded");
}

void SpeakerAgent::sendEventSetMuteFailed()
{
    sendEventCommon("SetMuteFailed");
}

void SpeakerAgent::sendEventCommon(const std::string& ename)
{
    std::string payload = "";
    Json::Value root;
    Json::StyledWriter writer;

    root["playServiceId"] = ps_id;
    payload = writer.write(root);

    sendEvent(ename, getContextInfo(), payload);
}

void SpeakerAgent::parsingSetVolume(const char* message)
{
    Json::Value root;
    Json::Reader reader;
    Json::Value volumes;
    std::string rate;

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

        cur_speaker.type = type;
        cur_speaker.volume = volume;

        if (speaker_listener)
            speaker_listener->requestSetVolume(type, volume, rate == "SLOW");
    }
}

void SpeakerAgent::parsingSetMute(const char* message)
{
    Json::Value root;
    Json::Reader reader;
    Json::Value volumes;

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

        cur_speaker.type = type;
        cur_speaker.mute = mute;

        if (speaker_listener)
            speaker_listener->requestSetMute(type, mute);
    }
}

} // NuguCapability
