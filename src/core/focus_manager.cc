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

FocusResource::FocusResource(const std::string& type, const std::string& name, int priority, IFocusResourceListener* listener, IFocusResourceObserver* observer)
    : type(type)
    , name(name)
    , priority(priority)
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
    return (this->priority < resource.priority);
}

bool FocusResource::operator>(const FocusResource& resource)
{
    return (this->priority > resource.priority);
}

FocusManager::FocusManager()
{
    observers.clear();

    configuration_map.clear();
    configuration_map[DIALOG_FOCUS_TYPE] = DIALOG_FOCUS_PRIORITY;
    configuration_map[COMMUNICATIONS_FOCUS_TYPE] = COMMUNICATIONS_FOCUS_PRIORITY;
    configuration_map[ALERTS_FOCUS_TYPE] = ALERTS_FOCUS_PRIORITY;
    configuration_map[CONTENT_FOCUS_TYPE] = CONTENT_FOCUS_PRIORITY;
    printConfigurations();
}

void FocusManager::reset()
{
    focus_resource_ordered_map.clear();
    focus_hold_map.clear();
}

bool FocusManager::requestFocus(const std::string& type, const std::string& name, IFocusResourceListener* listener)
{
    if (listener == nullptr) {
        nugu_error("The parameter(listener) is nullptr");
        return false;
    }

    if (configuration_map.find(type) == configuration_map.end()) {
        nugu_error("The focus[%s] is not exist in focus configuration", type.c_str());
        return false;
    }

    if (focus_hold_map[type]) {
        focus_hold_map[type] = false;
        nugu_info("[%s] - Reset HOLD", type.c_str());
    }

    FocusResource* activate_focus = nullptr;
    // get current activated focus
    if (focus_resource_ordered_map.begin() != focus_resource_ordered_map.end())
        activate_focus = focus_resource_ordered_map.begin()->second.get();

    int priority = configuration_map[type];
    if (focus_resource_ordered_map.find(priority) == focus_resource_ordered_map.end()) {
        // add new focus
        focus_resource_ordered_map[priority] = std::make_shared<FocusResource>(type, name, priority, listener, this);
    } else {
        // update exist focus
        FocusResource* exist_focus = focus_resource_ordered_map[priority].get();

        if (exist_focus->name != name) {
            // set previous focus's state to FocusState::NONE
            exist_focus->setState(FocusState::NONE);

            // update new focus information
            exist_focus->name = name;
            exist_focus->listener = listener;
        }
    }

    bool higher_hold = false;
    for (const auto& hold : focus_hold_map) {
        if (!hold.second)
            continue;

        if (priority > configuration_map[hold.first]) {
            nugu_info("[%s] - HOLD requestFocus", type.c_str());
            higher_hold = true;
            break;
        }
    }

    // get request focus
    FocusResource* request_focus = focus_resource_ordered_map[priority].get();
    FocusState request_focus_state = higher_hold ? FocusState::BACKGROUND : FocusState::FOREGROUND;

    if (activate_focus == nullptr || activate_focus == request_focus) {
        nugu_info("[%s - %s] - %s (priority: %d)", request_focus->type.c_str(), request_focus->name.c_str(), getStateString(request_focus_state).c_str(), request_focus->priority);
        request_focus->setState(request_focus_state);
    } else if (*request_focus < *activate_focus) {
        nugu_info("[%s - %s] - BACKGROUND (priority: %d)", activate_focus->type.c_str(), activate_focus->name.c_str(), activate_focus->priority);
        activate_focus->setState(FocusState::BACKGROUND);
        nugu_info("[%s - %s] - %s (priority: %d)", request_focus->type.c_str(), request_focus->name.c_str(), getStateString(request_focus_state).c_str(), request_focus->priority);
        request_focus->setState(request_focus_state);
    } else {
        nugu_info("[%s - %s] - BACKGROUND (priority: %d)", request_focus->type.c_str(), request_focus->name.c_str(), request_focus->priority);
        request_focus->setState(FocusState::BACKGROUND);
    }
    return true;
}

bool FocusManager::releaseFocus(const std::string& type, const std::string& name)
{
    if (configuration_map.find(type) == configuration_map.end()) {
        nugu_error("The focus[%s] is not exist in focus configuration", type.c_str());
        return false;
    }

    int priority = configuration_map[type];
    if (focus_resource_ordered_map.find(priority) == focus_resource_ordered_map.end())
        return true;

    FocusResource* release_focus = focus_resource_ordered_map[priority].get();

    if (name.size() && release_focus->name != name) {
        nugu_dbg("already released focus [%s - %s]", release_focus->type.c_str(), name.c_str());
        return true;
    }

    const std::string release_focus_type = release_focus->type;

    nugu_info("[%s - %s] - NONE (priority: %d)", release_focus->type.c_str(), release_focus->name.c_str(), release_focus->priority);
    release_focus->setState(FocusState::NONE);
    focus_resource_ordered_map.erase(priority);

    // focus resource is not exist anymore
    if (focus_resource_ordered_map.size() == 0)
        return true;

    if (focus_hold_map[release_focus_type]) {
        nugu_info("[%s] - HOLD releaseFocus", release_focus_type.c_str());
        return true;
    }

    // activate highest focus
    FocusResource* activate_focus = focus_resource_ordered_map.begin()->second.get();
    nugu_info("[%s - %s] - FOREGROUND (priority: %d)", activate_focus->type.c_str(), activate_focus->name.c_str(), activate_focus->priority);
    activate_focus->setState(FocusState::FOREGROUND);
    return true;
}

bool FocusManager::holdFocus(const std::string& type)
{
    nugu_info("[%s] - HOLD", type.c_str());
    focus_hold_map[type] = true;
    return true;
}

bool FocusManager::unholdFocus(const std::string& type)
{
    focus_hold_map[type] = false;

    nugu_info("[%s] - UNHOLD", type.c_str());

    FocusResource* activate_focus = nullptr;
    // get current activated focus
    if (focus_resource_ordered_map.begin() == focus_resource_ordered_map.end())
        return true;

    activate_focus = focus_resource_ordered_map.begin()->second.get();
    if (activate_focus->state == FocusState::BACKGROUND) {
        nugu_info("[%s - %s] - FOREGROUND (priority: %d)", activate_focus->type.c_str(), activate_focus->name.c_str(), activate_focus->priority);
        activate_focus->setState(FocusState::FOREGROUND);
    }

    return true;
}

void FocusManager::setConfigurations(std::vector<FocusConfiguration>& configurations)
{
    configuration_map.clear();

    for (auto configuration : configurations)
        configuration_map[configuration.type] = configuration.priority;

    printConfigurations();
}

void FocusManager::stopAllFocus()
{
    for (auto& container : focus_resource_ordered_map) {
        FocusResource* focus = container.second.get();
        nugu_info("[%s - %s] - NONE (priority: %d)", focus->type.c_str(), focus->name.c_str(), focus->priority);
        focus->setState(FocusState::NONE);
    }
    focus_resource_ordered_map.clear();
}

void FocusManager::stopForegroundFocus()
{
    if (focus_resource_ordered_map.size() == 0)
        return;

    FocusResource* activate_focus = focus_resource_ordered_map.begin()->second.get();
    nugu_info("[%s - %s] - NONE (priority: %d)", activate_focus->type.c_str(), activate_focus->name.c_str(), activate_focus->priority);
    activate_focus->setState(FocusState::NONE);
    focus_resource_ordered_map.erase(activate_focus->priority);

    if (focus_resource_ordered_map.size() == 0)
        return;

    activate_focus = focus_resource_ordered_map.begin()->second.get();
    nugu_info("[%s - %s] - FOREGROUND (priority: %d)", activate_focus->type.c_str(), activate_focus->name.c_str(), activate_focus->priority);
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

    if (configuration_map.find(type) != configuration_map.end())
        priority = configuration_map[type];

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
    if (configuration_map.find(type) == configuration_map.end())
        return;

    if (observers.size() == 0)
        return;

    FocusConfiguration configuration;
    configuration.type = type;
    configuration.priority = configuration_map[type];

    for (auto& observer : observers)
        observer->onFocusChanged(configuration, state, name);
}

void FocusManager::printConfigurations()
{
    nugu_info("Focus Resource Configurtaion ==================");
    for (auto configuration : configuration_map)
        nugu_info("Resource - priority:%4d, type: %s", configuration.second, configuration.first.c_str());
    nugu_info("===============================================");
}

} // NuguCore
