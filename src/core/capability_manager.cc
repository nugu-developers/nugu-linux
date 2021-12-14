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
#include <cmath>
#include <cstring>

#include "base/nugu_log.h"
#include "capability_manager.hh"

namespace NuguCore {

// define default property values
static const char* WAKEUP_WORD = "아리아";
static const char* CONTEXT_OS = "Linux";

CapabilityManager* CapabilityManager::instance = nullptr;

CapabilityManager::CapabilityManager()
    : wword(WAKEUP_WORD)
    , playsync_manager(std::unique_ptr<PlaySyncManager>(new PlaySyncManager()))
    , focus_manager(std::unique_ptr<FocusManager>(new FocusManager()))
    , session_manager(std::unique_ptr<SessionManager>(new SessionManager()))
    , directive_sequencer(std::unique_ptr<DirectiveSequencer>(new DirectiveSequencer()))
    , interaction_control_manager(std::unique_ptr<InteractionControlManager>(new InteractionControlManager()))
    , routine_manager(std::unique_ptr<RoutineManager>(new RoutineManager()))
{
    playsync_manager->setInteractionControlManager(interaction_control_manager.get());
}

CapabilityManager::~CapabilityManager()
{
    caps.clear();
    events.clear();
    events_cname_map.clear();
}

CapabilityManager* CapabilityManager::getInstance()
{
    if (!instance)
        instance = new CapabilityManager();

    return instance;
}

void CapabilityManager::destroyInstance()
{
    if (instance) {
        delete instance;
        instance = nullptr;
    }
}

void CapabilityManager::resetInstance()
{
    events.clear();
    events_cname_map.clear();

    playsync_manager->reset();
    focus_manager->reset();
    session_manager->reset();
    directive_sequencer->reset();
    interaction_control_manager->reset();
    routine_manager->reset();
}

bool CapabilityManager::onPreHandleDirective(NuguDirective* ndir)
{
    ICapabilityInterface* cap = findCapability(nugu_directive_peek_namespace(ndir));
    if (cap == nullptr) {
        nugu_warn("capability(%s) is not support", nugu_directive_peek_namespace(ndir));
        return false;
    }

    const char* version = nugu_directive_peek_version(ndir);

    if (!isSupportDirectiveVersion(version, cap)) {
        nugu_error("directives[%s] cannot work on %s[%s] agent",
            version, cap->getName().c_str(), cap->getVersion().c_str());

        /* Destroy the unsupported version of the directive */
        nugu_directive_unref(ndir);
        return true;
    }

    nugu_info("preprocessDirective - [%s.%s]", cap->getName().c_str(), nugu_directive_peek_name(ndir));
    cap->preprocessDirective(ndir);

    return false;
}

bool CapabilityManager::onHandleDirective(NuguDirective* ndir)
{
    ICapabilityInterface* cap = findCapability(nugu_directive_peek_namespace(ndir));
    if (cap == nullptr) {
        nugu_warn("capability(%s) is not support", nugu_directive_peek_namespace(ndir));
        return false;
    }

    if (isConditionToSendCommand(ndir)) {
        sendCommandAll("receive_directive_group", nugu_directive_peek_groups(ndir));
        sendCommandAll("directive_dialog_id", nugu_directive_peek_dialog_id(ndir));
    }

    nugu_info("processDirective - [%s.%s]", cap->getName().c_str(), nugu_directive_peek_name(ndir));
    cap->processDirective(ndir);

    return true;
}

void CapabilityManager::onCancelDirective(NuguDirective* ndir)
{
    ICapabilityInterface* cap = findCapability(nugu_directive_peek_namespace(ndir));
    if (cap == nullptr) {
        nugu_warn("capability(%s) is not support", nugu_directive_peek_namespace(ndir));
        return;
    }

    nugu_info("cancelDirective - [%s.%s]", cap->getName().c_str(), nugu_directive_peek_name(ndir));
    cap->cancelDirective(ndir);
}

void CapabilityManager::addCapability(const std::string& cname, ICapabilityInterface* cap)
{
    caps.emplace(cname, cap);
    directive_sequencer->addListener(cname, this);
}

void CapabilityManager::removeCapability(const std::string& cname)
{
    caps.erase(cname);
    directive_sequencer->removeListener(cname, this);
}

void CapabilityManager::requestEventResult(NuguEvent* event)
{
    if (!event) {
        nugu_error("event is null pointer");
        return;
    }

    const char* name_space = nugu_event_peek_namespace(event);
    const char* name = nugu_event_peek_name(event);
    const char* dialog_id = nugu_event_peek_dialog_id(event);
    const char* msg_id = nugu_event_peek_msg_id(event);

    if (!name_space || !name || !dialog_id || !msg_id) {
        nugu_error("event is not valid information");
        return;
    }

    std::string event_desc = std::string(name_space)
                                 .append(".")
                                 .append(name)
                                 .append(".")
                                 .append(msg_id)
                                 .append(".")
                                 .append(dialog_id);

    events.emplace(msg_id, event_desc);
    events_cname_map.emplace(msg_id, name_space);

    nugu_dbg("request event[%s] result - %s", msg_id, event_desc.c_str());
}

void CapabilityManager::onStatusChanged(NetworkStatus status)
{
    if (status == NetworkStatus::DISCONNECTED)
        sendCommandAll("network_disconnected", "");
}

void CapabilityManager::onEventSendResult(const char* msg_id, bool success, int code)
{
    if (events.find(msg_id) == events.end()) {
        nugu_warn("event[%s] is not monitoring target", msg_id);
        return;
    }

    std::string temp = events[msg_id];
    std::string cname = std::strtok((char*)temp.c_str(), ".");
    std::string desc = events[msg_id]
                           .append(".")
                           .append(std::to_string(success))
                           .append(".")
                           .append(std::to_string(code));

    nugu_dbg("report event result - %s", cname.c_str(), desc.c_str());

    ICapabilityInterface* cap = findCapability(cname);
    if (cap != nullptr)
        cap->notifyEventResult(desc);

    events.erase(msg_id);
}

void CapabilityManager::onEventResponse(const char* msg_id, const char* data, bool success) noexcept
{
    if (!success)
        nugu_error("can't receive event response: msg_id=%s", msg_id);

    try {
        ICapabilityInterface* cap = findCapability(events_cname_map.at(msg_id));

        if (cap) {
            std::string message_id = msg_id ? msg_id : "";
            std::string message_data = data ? data : "";

            cap->notifyEventResponse(message_id, message_data, success);
        }

        events_cname_map.erase(msg_id);
    } catch (std::out_of_range& exception) {
        nugu_warn("There is no agent which is mapping with the msg_id.");
    }
}

void CapabilityManager::setWakeupWord(const std::string& word)
{
    if (word.size())
        wword = word;
}

std::string CapabilityManager::getWakeupWord()
{
    return wword;
}

ICapabilityInterface* CapabilityManager::findCapability(const std::string& cname)
{
    try {
        return caps.at(cname);
    } catch (std::out_of_range& exception) {
        return nullptr;
    }
}

std::string CapabilityManager::makeContextInfo(const std::string& cname, Json::Value& cap_ctx)
{
    Json::FastWriter writer;

    for (const auto& cap : caps) {
        if (cap.second->getName() == cname)
            continue;

        cap.second->updateCompactContext(cap_ctx);
    }

    return writer.write(getBaseContextInfo(cap_ctx, Json::arrayValue));
}

std::string CapabilityManager::makeAllContextInfo()
{
    Json::FastWriter writer;
    Json::Value cap_ctx;
    Json::Value playstack_ctx = Json::arrayValue;
    const auto& playstacks = playsync_manager->getAllPlayStackItems();

    for (const auto& cap : caps)
        cap.second->updateInfoForContext(cap_ctx);

    for (const auto& playstack : playstacks)
        playstack_ctx.append(playstack);

    return writer.write(getBaseContextInfo(cap_ctx, std::move(playstack_ctx)));
}

Json::Value CapabilityManager::getBaseContextInfo(const Json::Value& supported_interfaces, Json::Value&& playstack)
{
    Json::Value root;
    Json::Value client;

    client["wakeupWord"] = wword;
    client["os"] = CONTEXT_OS;
    client["playStack"] = playstack;

    root["supportedInterfaces"] = supported_interfaces;
    root["client"] = client;

    return root;
}

bool CapabilityManager::isSupportDirectiveVersion(const std::string& version, ICapabilityInterface* cap)
{
    if (!version.size() || cap == nullptr)
        return false;

    int cur_ver = std::floor(std::stof(cap->getVersion()));
    int dir_ver = std::floor(std::stof(version));

    if (cur_ver != dir_ver)
        return false;

    return true;
}

void CapabilityManager::sendCommandAll(const std::string& command, const std::string& param)
{
    nugu_dbg("send %s with %s", command.c_str(), param.c_str());

    for (const auto& iter : caps)
        iter.second->receiveCommandAll(command, param);
}

bool CapabilityManager::sendCommand(const std::string& from, const std::string& to,
    const std::string& command, const std::string& param)
{
    for (const auto& iter : caps) {
        if (iter.second->getName() == to) {
            return iter.second->receiveCommand(from, command, param);
        }
    }

    return false;
}

bool CapabilityManager::getCapabilityProperty(const std::string& cap, const std::string& property, std::string& value)
{
    for (const auto& iter : caps) {
        if (iter.second->getName() == cap) {
            return iter.second->getProperty(property, value);
        }
    }

    return false;
}

bool CapabilityManager::getCapabilityProperties(const std::string& cap, const std::string& property, std::list<std::string>& values)
{
    for (const auto& iter : caps) {
        if (iter.second->getName() == cap) {
            return iter.second->getProperties(property, values);
        }
    }

    return false;
}

void CapabilityManager::suspendAll()
{
    for (const auto& iter : caps)
        iter.second->suspend();
}

void CapabilityManager::restoreAll()
{
    for (const auto& iter : caps)
        iter.second->restore();
}

PlaySyncManager* CapabilityManager::getPlaySyncManager()
{
    return playsync_manager.get();
}

FocusManager* CapabilityManager::getFocusManager()
{
    return focus_manager.get();
}

SessionManager* CapabilityManager::getSessionManager()
{
    return session_manager.get();
}

InteractionControlManager* CapabilityManager::getInteractionControlManager()
{
    return interaction_control_manager.get();
}

DirectiveSequencer* CapabilityManager::getDirectiveSequencer()
{
    return directive_sequencer.get();
}

RoutineManager* CapabilityManager::getRoutineManager()
{
    return routine_manager.get();
}

bool CapabilityManager::isConditionToSendCommand(const NuguDirective* ndir)
{
    std::string dnamespace = nugu_directive_peek_namespace(ndir);
    std::string dname = nugu_directive_peek_name(ndir);
    std::string dialog_id = nugu_directive_peek_dialog_id(ndir);
    std::string directive { dnamespace + "." + dname };

    if (NO_COMMAND_DIRECTIVE_FILTER.find(directive) != NO_COMMAND_DIRECTIVE_FILTER.cend())
        return false;

    if (std::find(progress_dialogs.cbegin(), progress_dialogs.cend(), dialog_id) != progress_dialogs.cend())
        return false;

    // pop oldest elements from progress_dialogs container if it exceed max size
    if (progress_dialogs.size() > PROGRESS_DIALOGS_MAX)
        progress_dialogs.pop_front();

    progress_dialogs.emplace_back(dialog_id);

    return true;
}

} // NuguCore
