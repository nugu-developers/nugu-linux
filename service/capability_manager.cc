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

#include <glib.h>
#include <stdlib.h>
#include <string.h>

#include "capability_manager.hh"
#include "nugu_configuration.hh"
#include "nugu_log.h"

namespace NuguCore {

using namespace NuguInterface;

CapabilityManager* CapabilityManager::instance = NULL;

static NuguFocusResult on_focus(NuguFocus* focus, NuguFocusResource rsrc,
    void* event, void* userdata)
{
    IFocusListener* listener = static_cast<IFocusListener*>(userdata);

    return listener->onFocus(rsrc, event);
}

static NuguFocusResult on_unfocus(NuguFocus* focus, NuguFocusResource rsrc,
    void* event, void* userdata)
{
    IFocusListener* listener = static_cast<IFocusListener*>(userdata);

    return listener->onUnfocus(rsrc, event);
}

static NuguFocusStealResult on_steal_request(NuguFocus* focus, NuguFocusResource rsrc,
    void* event, NuguFocus* target, void* userdata)
{
    IFocusListener* listener = static_cast<IFocusListener*>(userdata);

    return listener->onStealRequest(rsrc, event, nugu_focus_get_type(target));
}

CapabilityManager::CapabilityManager()
{
    nugu_dirseq_add_callback(dirseqCallback, this);

    wword = nugu_config_get(NuguConfig::Key::WAKEUP_WORD.c_str());
    playsync_manager = std::unique_ptr<PlaySyncManager>(new PlaySyncManager());
}

CapabilityManager::~CapabilityManager()
{
    caps.clear();
    focusmap.clear();

    nugu_dirseq_remove_callback(dirseqCallback);
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

DirseqReturn CapabilityManager::dirseqCallback(NuguDirective* ndir, void* userdata)
{
    CapabilityManager* agent = static_cast<CapabilityManager*>(userdata);
    const char* name_space = nugu_directive_peek_namespace(ndir);
    ICapabilityInterface* cap = agent->findCapability(std::string(name_space));
    if (!cap) {
        nugu_dbg("capability(%s) is not support", name_space);
        return DIRSEQ_REMOVE;
    }

    agent->preprocessDirective(ndir);
    cap->processDirective(ndir);
    return DIRSEQ_CONTINUE;
}

void CapabilityManager::addCapability(std::string cname, ICapabilityInterface* cap)
{
    caps[cname] = cap;
}

void CapabilityManager::removeCapability(std::string cname)
{
    caps.erase(cname);
}

void CapabilityManager::setWakeupWord(std::string word)
{
    if (word.size())
        wword = word;
}

ICapabilityInterface* CapabilityManager::findCapability(std::string cname)
{
    if (caps.find(cname) == caps.end())
        return NULL;

    return caps[cname];
}

std::string CapabilityManager::makeContextInfo(Json::Value& ctx)
{
    Json::StyledWriter writer;
    Json::Value root;
    Json::Value client;

    root["supportedInterfaces"] = ctx;
    client["wakeupWord"] = wword;
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

    root["supportedInterfaces"] = ctx;
    client["wakeupWord"] = wword;
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

    root["supportedInterfaces"] = ctx;
    client["wakeupWord"] = wword;

    auto play_stack = playsync_manager->getAllPlayStackItems();

    for (auto element : play_stack) {
        client["playStack"].append(element);
    }

    root["client"] = client;

    return writer.write(root);
}

void CapabilityManager::preprocessDirective(NuguDirective* ndir)
{
    sendCommandAll("directive_dialog_id", nugu_directive_peek_dialog_id(ndir));
}

void CapabilityManager::sendCommandAll(std::string command, std::string param)
{
    nugu_dbg("send %s with %s", command.c_str(), param.c_str());

    for (auto iter : caps) {
        iter.second->receiveCommandAll(command, param);
    }
}

void CapabilityManager::sendCommand(CapabilityType from, CapabilityType to,
    std::string command, std::string param)
{
    for (auto iter : caps) {
        if (iter.second->getType() == to) {
            iter.second->receiveCommand(from, command, param);
            break;
        }
    }
}

void CapabilityManager::getCapabilityProperty(CapabilityType cap, std::string property, std::string& value)
{
    for (auto iter : caps) {
        if (iter.second->getType() == cap) {
            iter.second->getProperty(property, value);
            break;
        }
    }
}

void CapabilityManager::getCapabilityProperties(CapabilityType cap, std::string property, std::list<std::string>& values)
{
    for (auto iter : caps) {
        if (iter.second->getType() == cap) {
            iter.second->getProperties(property, values);
            break;
        }
    }
}

bool CapabilityManager::isFocusOn(NuguFocusResource rsrc)
{
    NuguFocus* focus = nugu_focus_peek_top(rsrc);

    if (focus)
        return true;
    else
        return false;
}

int CapabilityManager::addFocus(std::string fname, NuguFocusType type, IFocusListener* listener)
{
    NuguFocus* focus;

    focus = nugu_focus_new(fname.c_str(), type, on_focus, on_unfocus, on_steal_request, listener);
    if (!focus)
        return -1;

    focusmap[fname] = focus;

    return 0;
}

int CapabilityManager::removeFocus(std::string fname)
{
    NuguFocus* focus;

    focus = focusmap[fname];
    if (!focus)
        return -1;

    nugu_focus_free(focus);
    focusmap.erase(fname);

    return 0;
}

int CapabilityManager::requestFocus(std::string fname, NuguFocusResource rsrc, void* event)
{
    NuguFocus* focus;

    focus = focusmap[fname];
    if (!focus)
        return -1;

    return nugu_focus_request(focus, rsrc, event);
}

int CapabilityManager::releaseFocus(std::string fname, NuguFocusResource rsrc)
{
    NuguFocus* focus;

    focus = focusmap[fname];
    if (!focus)
        return -1;

    return nugu_focus_release(focus, rsrc);
}

PlaySyncManager* CapabilityManager::getPlaySyncManager()
{
    return playsync_manager.get();
}

} // NuguCore
