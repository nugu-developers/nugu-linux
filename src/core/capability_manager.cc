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

#include <cmath>
#include <cstring>

#include "base/nugu_log.h"
#include "capability_manager.hh"

namespace NuguCore {

// define default property values
static const char* WAKEUP_WORD = "아리아";

CapabilityManager* CapabilityManager::instance = NULL;

CapabilityManager::CapabilityManager()
{
    nugu_dirseq_set_callback(dirseqCallback, this);

    wword = WAKEUP_WORD;
    playsync_manager = std::unique_ptr<PlaySyncManager>(new PlaySyncManager());
    focus_manager = std::unique_ptr<FocusManager>(new FocusManager());
    session_manager = std::unique_ptr<SessionManager>(new SessionManager());
    directive_sequencer = std::unique_ptr<DirectiveSequencer>(new DirectiveSequencer());
}

CapabilityManager::~CapabilityManager()
{
    caps.clear();
    events.clear();
    events_cname_map.clear();

    nugu_dirseq_unset_callback();
}

CapabilityManager* CapabilityManager::getInstance()
{
    if (!instance) {
        instance = new CapabilityManager();
    }
    return instance;
}

void CapabilityManager::destroyInstance()
{
    if (instance) {
        delete instance;
        instance = NULL;
    }
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

    return false;
}

bool CapabilityManager::onHandleDirective(NuguDirective* ndir)
{
    ICapabilityInterface* cap = findCapability(nugu_directive_peek_namespace(ndir));
    if (cap == nullptr) {
        nugu_warn("capability(%s) is not support", nugu_directive_peek_namespace(ndir));
        return false;
    }

    nugu_info("preprocessDirective");
    preprocessDirective(ndir);

    nugu_info("processDirective");
    cap->processDirective(ndir);

    return true;
}

NuguDirseqReturn CapabilityManager::dirseqCallback(NuguDirective* ndir, void* userdata)
{
    CapabilityManager* cap_manager = static_cast<CapabilityManager*>(userdata);
    const char* name_space = nugu_directive_peek_namespace(ndir);
    ICapabilityInterface* cap = cap_manager->findCapability(std::string(name_space));
    if (!cap) {
        nugu_warn("capability(%s) is not support", name_space);
        return NUGU_DIRSEQ_FAILURE;
    }

    cap_manager->preprocessDirective(ndir);

    const char* version = nugu_directive_peek_version(ndir);
    if (!cap_manager->isSupportDirectiveVersion(version, cap)) {
        nugu_error("directives[%s] cannot work on %s[%s] agent",
            version, cap->getName().c_str(), cap->getVersion().c_str());
        return NUGU_DIRSEQ_FAILURE;
    }

    cap->processDirective(ndir);
    return NUGU_DIRSEQ_SUCCESS;
}

void CapabilityManager::addCapability(const std::string& cname, ICapabilityInterface* cap)
{
    caps[cname] = cap;
}

void CapabilityManager::removeCapability(const std::string& cname)
{
    caps.erase(cname);
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

    events[msg_id] = event_desc;
    events_cname_map.emplace(msg_id, name_space);

    nugu_dbg("request event[%s] result - %s", msg_id, event_desc.c_str());
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

void CapabilityManager::onEventResponse(const char* msg_id, const char* json, bool success)
{
    if (!success)
        nugu_error("can't receive event response: msg_id=%s", msg_id);

    try {
        ICapabilityInterface* cap = findCapability(events_cname_map.at(msg_id));

        if (cap)
            cap->notifyEventResponse(msg_id, json, success);

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
    if (caps.find(cname) == caps.end())
        return nullptr;

    return caps[cname];
}

std::string CapabilityManager::makeContextInfo(const std::string& cname, Json::Value& ctx)
{
    Json::StyledWriter writer;
    Json::Value root;
    Json::Value client;

    for (auto cap : caps) {
        ICapabilityInterface* icap = cap.second;
        if (icap->getName() == cname)
            continue;

        // The delegation interface is supported by v1.1 or higher.
        if (icap->getName() == "Delegation" && icap->getVersion() == "1.0")
            continue;

        ctx[icap->getName()]["version"] = icap->getVersion();
    }

    client["wakeupWord"] = wword;
    client["os"] = "Linux";

    root["supportedInterfaces"] = ctx;
    root["client"] = client;

    return writer.write(root);
}

std::string CapabilityManager::makeAllContextInfo()
{
    Json::StyledWriter writer;
    Json::Value root;
    Json::Value ctx;
    Json::Value client;

    for (auto iter : caps)
        iter.second->updateInfoForContext(ctx);

    client["wakeupWord"] = wword;
    client["os"] = "Linux";

    root["supportedInterfaces"] = ctx;
    root["client"] = client;

    return writer.write(root);
}

std::string CapabilityManager::makeAllContextInfoStack()
{
    Json::StyledWriter writer;
    Json::Value root;
    Json::Value ctx;
    Json::Value client;

    for (auto iter : caps)
        iter.second->updateInfoForContext(ctx);

    client["wakeupWord"] = wword;
    client["os"] = "Linux";

    auto play_stack = playsync_manager->getAllPlayStackItems();

    for (const auto& element : play_stack) {
        client["playStack"].append(element);
    }

    root["supportedInterfaces"] = ctx;
    root["client"] = client;

    return writer.write(root);
}

void CapabilityManager::preprocessDirective(NuguDirective* ndir)
{
    std::string name_space = nugu_directive_peek_namespace(ndir);
    std::string dname = nugu_directive_peek_name(ndir);
    std::string groups = nugu_directive_peek_groups(ndir);

    sendCommandAll("receive_directive_group", groups);
    sendCommandAll("directive_dialog_id", nugu_directive_peek_dialog_id(ndir));
    playsync_manager->setDirectiveGroups(groups);
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

    for (auto iter : caps) {
        iter.second->receiveCommandAll(command, param);
    }
}

void CapabilityManager::sendCommand(const std::string& from, const std::string& to,
    const std::string& command, const std::string& param)
{
    for (auto iter : caps) {
        if (iter.second->getName() == to) {
            iter.second->receiveCommand(from, command, param);
            break;
        }
    }
}

void CapabilityManager::getCapabilityProperty(const std::string& cap, const std::string& property, std::string& value)
{
    for (auto iter : caps) {
        if (iter.second->getName() == cap) {
            iter.second->getProperty(property, value);
            break;
        }
    }
}

void CapabilityManager::getCapabilityProperties(const std::string& cap, const std::string& property, std::list<std::string>& values)
{
    for (auto iter : caps) {
        if (iter.second->getName() == cap) {
            iter.second->getProperties(property, values);
            break;
        }
    }
}

void CapabilityManager::suspendAll()
{
    for (const auto& iter : caps) {
        iter.second->suspend();
    }
}

void CapabilityManager::restoreAll()
{
    for (const auto& iter : caps) {
        iter.second->restore();
    }
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

DirectiveSequencer* CapabilityManager::getDirectiveSequencer()
{
    return directive_sequencer.get();
}

} // NuguCore
