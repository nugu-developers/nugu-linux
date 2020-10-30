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

#include "focus_manager.hh"
#include "base/nugu_log.h"

namespace NuguCore {

FocusResource::FocusResource(const std::string& type, const std::string& name, int request_priority, int release_priority, IFocusResourceListener* listener, IFocusResourceObserver* observer)
    : type(type)
    , name(name)
    , request_priority(request_priority)
    , release_priority(release_priority)
    , state(FocusState::NONE)
    , listener(listener)
    , observer(observer)
{
}

void FocusResource::setState(FocusState state)
{
    if (this->state == state)
        return;

    this->state = state;
    listener->onFocusChanged(state);
    observer->onFocusChanged(type, name, state);
}

bool FocusResource::operator<(const FocusResource& resource)
{
    return (this->release_priority < resource.release_priority);
}

bool FocusResource::operator>(const FocusResource& resource)
{
    return (this->release_priority > resource.release_priority);
}

FocusManager::FocusManager()
    : focus_hold_priority(-1)
{
    observers.clear();

    request_configuration_map.clear();
    request_configuration_map[DIALOG_FOCUS_TYPE] = DIALOG_FOCUS_PRIORITY;
    request_configuration_map[COMMUNICATIONS_FOCUS_TYPE] = COMMUNICATIONS_FOCUS_PRIORITY;
    request_configuration_map[ALERTS_FOCUS_TYPE] = ALERTS_FOCUS_PRIORITY;
    request_configuration_map[CONTENT_FOCUS_TYPE] = CONTENT_FOCUS_PRIORITY;
    release_configuration_map = request_configuration_map;
    printConfigurations();
}

void FocusManager::reset()
{
    focus_resource_ordered_map.clear();
}

bool FocusManager::requestFocus(const std::string& type, const std::string& name, IFocusResourceListener* listener)
{
    if (listener == nullptr) {
        nugu_error("The parameter(listener) is nullptr");
        return false;
    }

    if (request_configuration_map.find(type) == request_configuration_map.end()) {
        nugu_error("The focus[%s] is not exist in focus configuration", type.c_str());
        return false;
    }

    if (focus_hold_priority == request_configuration_map[type]) {
        nugu_info("[%s] - Reset HOLD (priority - rel:%d)", type.c_str(), focus_hold_priority);
        focus_hold_priority = -1;
    }

    FocusResource* activate_focus = nullptr;
    // get current activated focus
    if (focus_resource_ordered_map.begin() != focus_resource_ordered_map.end()) {
        activate_focus = focus_resource_ordered_map.begin()->second.get();
        if (activate_focus->state == FocusState::BACKGROUND) {
            for (const auto& resource : focus_resource_ordered_map) {
                if (resource.second->state == FocusState::FOREGROUND) {
                    activate_focus = resource.second.get();
                    nugu_info("[%s] is activate focus", activate_focus->type.c_str());
                    break;
                }
            }
        }
    }

    int activate_priority = activate_focus ? activate_focus->release_priority : 0;
    int request_priority = request_configuration_map[type];
    int release_priority = release_configuration_map[type];

    if (focus_resource_ordered_map.find(release_priority) == focus_resource_ordered_map.end()) {
        // add new focus
        focus_resource_ordered_map[release_priority] = std::make_shared<FocusResource>(type, name, request_priority, release_priority, listener, this);
    } else {
        // update exist focus
        FocusResource* exist_focus = focus_resource_ordered_map[release_priority].get();

        if (exist_focus->name != name) {
            // set previous focus's state to FocusState::NONE
            exist_focus->setState(FocusState::NONE);

            // update new focus information
            exist_focus->name = name;
            exist_focus->listener = listener;
        }
    }

    bool higher_hold = false;
    if (request_priority > focus_hold_priority && focus_hold_priority != -1) {
        nugu_info("[%s] - HOLD requestFocus (priority - rel:%d)", type.c_str(), release_priority);
        higher_hold = true;
    }

    // get request focus
    FocusResource* request_focus = focus_resource_ordered_map[release_priority].get();
    FocusState request_focus_state = higher_hold ? FocusState::BACKGROUND : FocusState::FOREGROUND;

    if (activate_focus == nullptr || activate_focus == request_focus) {
        nugu_info("[%s - %s] - %s (priority - req:%d, rel:%d)", request_focus->type.c_str(), request_focus->name.c_str(), getStateString(request_focus_state).c_str(), request_focus->request_priority, request_focus->release_priority);
        request_focus->setState(request_focus_state);
    } else if (request_priority <= activate_priority) {
        nugu_info("[%s - %s] - BACKGROUND (priority - req:%d, rel:%d)", activate_focus->type.c_str(), activate_focus->name.c_str(), activate_focus->request_priority, activate_focus->release_priority);
        activate_focus->setState(FocusState::BACKGROUND);
        nugu_info("[%s - %s] - %s (priority - req:%d, rel:%d)", request_focus->type.c_str(), request_focus->name.c_str(), getStateString(request_focus_state).c_str(), request_focus->request_priority, request_focus->release_priority);
        request_focus->setState(request_focus_state);
    } else {
        nugu_info("[%s - %s] - BACKGROUND (priority - req:%d, rel:%d)", request_focus->type.c_str(), request_focus->name.c_str(), request_focus->request_priority, request_focus->release_priority);
        request_focus->setState(FocusState::BACKGROUND);
    }
    return true;
}

bool FocusManager::releaseFocus(const std::string& type, const std::string& name)
{
    if (release_configuration_map.find(type) == release_configuration_map.end()) {
        nugu_error("The focus[%s] is not exist in focus configuration", type.c_str());
        return false;
    }

    int priority = release_configuration_map[type];
    if (focus_resource_ordered_map.find(priority) == focus_resource_ordered_map.end())
        return true;

    FocusResource* release_focus = focus_resource_ordered_map[priority].get();

    if (name.size() && release_focus->name != name) {
        nugu_dbg("already released focus [%s - %s]", release_focus->type.c_str(), name.c_str());
        return true;
    }

    const std::string release_focus_type = release_focus->type;

    nugu_info("[%s - %s] - NONE (priority - req:%d, rel:%d)", release_focus->type.c_str(), release_focus->name.c_str(), release_focus->request_priority, release_focus->release_priority);
    release_focus->setState(FocusState::NONE);
    focus_resource_ordered_map.erase(priority);

    // focus resource is not exist anymore
    if (focus_resource_ordered_map.size() == 0)
        return true;

    // activate highest focus
    FocusResource* activate_focus = focus_resource_ordered_map.begin()->second.get();
    if (activate_focus->release_priority > focus_hold_priority && focus_hold_priority != -1) {
        nugu_info("[%s] - HOLD releaseFocus (priority - rel:%d)", release_focus_type.c_str(), activate_focus->release_priority);
        return true;
    }

    nugu_info("[%s - %s] - FOREGROUND (priority - req:%d, rel:%d)", activate_focus->type.c_str(), activate_focus->name.c_str(), activate_focus->request_priority, activate_focus->release_priority);
    activate_focus->setState(FocusState::FOREGROUND);
    return true;
}

bool FocusManager::holdFocus(const std::string& type)
{
    if (release_configuration_map.find(type) == release_configuration_map.end()) {
        nugu_error("The focus[%s] is not exist in focus configuration", type.c_str());
        return false;
    }

    focus_hold_priority = release_configuration_map[type];
    nugu_info("[%s] - HOLD (priority - rel:%d)", type.c_str(), focus_hold_priority);
    return true;
}

bool FocusManager::unholdFocus(const std::string& type)
{
    nugu_info("[%s] - UNHOLD (priority - rel:%d)", type.c_str(), focus_hold_priority);

    FocusResource* activate_focus = nullptr;
    // get current activated focus
    if (focus_resource_ordered_map.begin() == focus_resource_ordered_map.end())
        return true;

    activate_focus = focus_resource_ordered_map.begin()->second.get();
    if (activate_focus->state == FocusState::BACKGROUND) {
        nugu_info("[%s - %s] - FOREGROUND (priority - req:%d, rel:%d)", activate_focus->type.c_str(), activate_focus->name.c_str(), activate_focus->request_priority, activate_focus->release_priority);
        activate_focus->setState(FocusState::FOREGROUND);
    }

    focus_hold_priority = -1;
    return true;
}

void FocusManager::setConfigurations(std::vector<FocusConfiguration>& request, std::vector<FocusConfiguration>& release)
{
    request_configuration_map.clear();
    release_configuration_map.clear();

    for (auto configuration : request)
        request_configuration_map[configuration.type] = configuration.priority;

    for (auto configuration : release)
        release_configuration_map[configuration.type] = configuration.priority;

    printConfigurations();
}

void FocusManager::stopAllFocus()
{
    for (auto& container : focus_resource_ordered_map) {
        FocusResource* focus = container.second.get();
        nugu_info("[%s - %s] - NONE (priority - req:%d, rel:%d)", focus->type.c_str(), focus->name.c_str(), focus->request_priority, focus->release_priority);
        focus->setState(FocusState::NONE);
    }
    focus_resource_ordered_map.clear();
}

void FocusManager::stopForegroundFocus()
{
    if (focus_resource_ordered_map.size() == 0)
        return;

    FocusResource* activate_focus = focus_resource_ordered_map.begin()->second.get();
    nugu_info("[%s - %s] - NONE (priority - req:%d, rel:%d)", activate_focus->type.c_str(), activate_focus->name.c_str(), activate_focus->request_priority, activate_focus->release_priority);
    activate_focus->setState(FocusState::NONE);
    focus_resource_ordered_map.erase(activate_focus->release_priority);

    if (focus_resource_ordered_map.size() == 0)
        return;

    activate_focus = focus_resource_ordered_map.begin()->second.get();
    nugu_info("[%s - %s] - FOREGROUND (priority - req:%d, rel:%d)", activate_focus->type.c_str(), activate_focus->name.c_str(), activate_focus->request_priority, activate_focus->release_priority);
    activate_focus->setState(FocusState::FOREGROUND);
}

void FocusManager::addObserver(IFocusManagerObserver* observer)
{
    if (observer && std::find(observers.begin(), observers.end(), observer) == observers.end())
        observers.emplace_back(observer);
}

void FocusManager::removeObserver(IFocusManagerObserver* observer)
{
    auto iterator = std::find(observers.begin(), observers.end(), observer);

    if (iterator != observers.end())
        observers.erase(iterator);
}

int FocusManager::getFocusResourcePriority(const std::string& type)
{
    int priority = -1;

    if (release_configuration_map.find(type) != release_configuration_map.end())
        priority = release_configuration_map[type];

    return priority;
}

std::string FocusManager::getStateString(FocusState state)
{
    std::string name;

    switch (state) {
    case FocusState::NONE:
        name = "NONE";
        break;
    case FocusState::FOREGROUND:
        name = "FOREGROUND";
        break;
    case FocusState::BACKGROUND:
        name = "BACKGROUND";
        break;
    }

    return name;
}

void FocusManager::onFocusChanged(const std::string& type, const std::string& name, FocusState state)
{
    if (release_configuration_map.find(type) == release_configuration_map.end())
        return;

    if (observers.size() == 0)
        return;

    FocusConfiguration configuration;
    configuration.type = type;
    configuration.priority = release_configuration_map[type];

    for (auto& observer : observers)
        observer->onFocusChanged(configuration, state, name);
}

void FocusManager::printConfigurations()
{
    nugu_info("Focus Resource Configurtaion  =================");
    for (auto configuration : request_configuration_map)
        nugu_info("Request - priority:%4d, type: %s", configuration.second, configuration.first.c_str());
    nugu_info("-----------------------------------------------");
    for (auto configuration : release_configuration_map)
        nugu_info("Release - priority:%4d, type: %s", configuration.second, configuration.first.c_str());
    nugu_info("===============================================");
}

} // NuguCore
