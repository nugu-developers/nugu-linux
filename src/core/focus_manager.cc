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

static bool _compare_priority(const std::shared_ptr<FocusResource>& first, const std::shared_ptr<FocusResource>& second)
{
    return (first->release_priority < second->release_priority);
}

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
    observer->onFocusChanged(type, name, state);
    listener->onFocusChanged(state);
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
    : processing(false)
{
    observers.clear();

    request_configuration_map.clear();
    request_configuration_map[CALL_FOCUS_TYPE] = CALL_FOCUS_REQUEST_PRIORITY;
    request_configuration_map[ASR_USER_FOCUS_TYPE] = ASR_USER_FOCUS_REQUEST_PRIORITY;
    request_configuration_map[INFO_FOCUS_TYPE] = INFO_FOCUS_REQUEST_PRIORITY;
    request_configuration_map[ASR_DM_FOCUS_TYPE] = ASR_DM_FOCUS_REQUEST_PRIORITY;
    request_configuration_map[ALERTS_FOCUS_TYPE] = ALERTS_FOCUS_REQUEST_PRIORITY;
    request_configuration_map[ASR_BEEP_FOCUS_TYPE] = ASR_BEEP_FOCUS_REQUEST_PRIORITY;
    request_configuration_map[MEDIA_FOCUS_TYPE] = MEDIA_FOCUS_REQUEST_PRIORITY;
    request_configuration_map[SOUND_FOCUS_TYPE] = SOUND_FOCUS_REQUEST_PRIORITY;
    request_configuration_map[DUMMY_FOCUS_TYPE] = DUMMY_FOCUS_REQUEST_PRIORITY;

    release_configuration_map.clear();
    release_configuration_map[CALL_FOCUS_TYPE] = CALL_FOCUS_RELEASE_PRIORITY;
    release_configuration_map[ASR_USER_FOCUS_TYPE] = ASR_USER_FOCUS_RELEASE_PRIORITY;
    release_configuration_map[ASR_DM_FOCUS_TYPE] = ASR_DM_FOCUS_RELEASE_PRIORITY;
    release_configuration_map[INFO_FOCUS_TYPE] = INFO_FOCUS_RELEASE_PRIORITY;
    release_configuration_map[ALERTS_FOCUS_TYPE] = ALERTS_FOCUS_RELEASE_PRIORITY;
    release_configuration_map[ASR_BEEP_FOCUS_TYPE] = ASR_BEEP_FOCUS_RELEASE_PRIORITY;
    release_configuration_map[MEDIA_FOCUS_TYPE] = MEDIA_FOCUS_RELEASE_PRIORITY;
    release_configuration_map[SOUND_FOCUS_TYPE] = SOUND_FOCUS_RELEASE_PRIORITY;
    release_configuration_map[DUMMY_FOCUS_TYPE] = DUMMY_FOCUS_RELEASE_PRIORITY;
    printConfigurations();
}

void FocusManager::reset()
{
    focus_resource_ordered_list.clear();
}

bool FocusManager::requestFocus(const std::string& type, const std::string& name, IFocusResourceListener* listener)
{
    if (listener == nullptr) {
        nugu_error("The parameter(listener) is nullptr");
        return false;
    }

    if (request_configuration_map.find(type) == request_configuration_map.end()
        || release_configuration_map.find(type) == release_configuration_map.end()) {
        nugu_error("The focus[%s] is not exist in focus configuration", type.c_str());
        return false;
    }

    if (focus_hold_type == type) {
        nugu_info("[%s] - Reset HOLD (priority - req:%d, rel:%d)", type.c_str(), request_configuration_map[focus_hold_type], release_configuration_map[focus_hold_type]);
        focus_hold_type = "";
    }

    std::shared_ptr<FocusResource> activate_focus;
    // get current activated focus
    if (focus_resource_ordered_list.size()) {
        activate_focus = focus_resource_ordered_list.front();
        if (activate_focus->state == FocusState::BACKGROUND) {
            for (const auto& resource : focus_resource_ordered_list) {
                if (resource->state == FocusState::FOREGROUND) {
                    activate_focus = resource;
                    nugu_info("[%s] is activate focus", activate_focus->type.c_str());
                    break;
                }
            }
        }
    }

    int activate_priority = activate_focus ? activate_focus->release_priority : 0;
    int request_priority = request_configuration_map[type];
    int release_priority = release_configuration_map[type];

    // get request focus
    std::shared_ptr<FocusResource> request_focus;
    std::shared_ptr<FocusResource> exist_focus;
    for (const auto& item : focus_resource_ordered_list) {
        if (item->type == type) {
            exist_focus = item;
            break;
        }
    }

    if (exist_focus.get()) {
        if (exist_focus->name != name) {
            // set previous focus's state to FocusState::NONE
            exist_focus->setState(FocusState::NONE);

            // update new focus information
            exist_focus->name = name;
            exist_focus->listener = listener;
        }
        request_focus = exist_focus;
    } else {
        // add new focus
        request_focus = std::make_shared<FocusResource>(type, name, request_priority, release_priority, listener, this);
        focus_resource_ordered_list.push_back(request_focus);
        focus_resource_ordered_list.sort(_compare_priority);
    }

    bool higher_hold = false;
    if (focus_hold_type.size() && request_priority > request_configuration_map[focus_hold_type]) {
        nugu_info("[%s] - HOLD requestFocus (priority - req:%d, rel:%d)", type.c_str(), request_configuration_map[focus_hold_type], release_configuration_map[focus_hold_type]);
        higher_hold = true;
    }

    FocusState request_focus_state = higher_hold ? FocusState::BACKGROUND : FocusState::FOREGROUND;

    if (activate_focus.get() == nullptr || activate_focus == request_focus) {
        nugu_info("[%s - %s] - %s (priority - req:%d, rel:%d)", request_focus->type.c_str(), request_focus->name.c_str(), getStateString(request_focus_state).c_str(), request_focus->request_priority, request_focus->release_priority);
        request_focus->setState(request_focus_state);
    } else if (request_priority <= activate_priority) {
        nugu_info("[%s - %s] - BACKGROUND (priority - req:%d, rel:%d)", activate_focus->type.c_str(), activate_focus->name.c_str(), activate_focus->request_priority, activate_focus->release_priority);
        processing = true;
        activate_focus->setState(FocusState::BACKGROUND);
        processing = false;
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

    std::shared_ptr<FocusResource> release_focus;
    bool find_release = false;

    for (const auto& resource : focus_resource_ordered_list) {
        if (resource->type == type && resource->name == name) {
            find_release = true;
            release_focus = resource;
            break;
        }
    }

    if (!find_release)
        return true;

    if (name.size() && release_focus->name != name) {
        nugu_dbg("already released focus [%s - %s]", release_focus->type.c_str(), name.c_str());
        return true;
    }

    const std::string release_focus_type = release_focus->type;

    nugu_info("[%s - %s] - NONE (priority - req:%d, rel:%d)", release_focus->type.c_str(), release_focus->name.c_str(), release_focus->request_priority, release_focus->release_priority);
    release_focus->setState(FocusState::NONE);
    focus_resource_ordered_list.remove(release_focus);

    // focus resource is not exist anymore
    if (focus_resource_ordered_list.size() == 0)
        return true;

    // prevent to callback duplicated
    if (processing)
        return true;

    // activate highest focus
    std::shared_ptr<FocusResource> activate_focus = focus_resource_ordered_list.front();
    if (focus_hold_type.size() && activate_focus->release_priority > release_configuration_map[focus_hold_type]) {
        nugu_info("[%s] - HOLD requestFocus (priority - req:%d, rel:%d)", type.c_str(), request_configuration_map[focus_hold_type], release_configuration_map[focus_hold_type]);
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

    focus_hold_type = type;
    nugu_info("[%s] - HOLD (priority - req:%d, rel:%d)", type.c_str(), request_configuration_map[type], release_configuration_map[type]);
    return true;
}

bool FocusManager::unholdFocus(const std::string& type)
{
    nugu_info("[%s] - UNHOLD (priority - req:%d, rel:%d)", type.c_str(), request_configuration_map[focus_hold_type], release_configuration_map[focus_hold_type]);

    if (focus_hold_type != type)
        return false;

    focus_hold_type = "";

    // get current activated focus
    if (focus_resource_ordered_list.size() == 0)
        return true;

    std::shared_ptr<FocusResource> activate_focus = focus_resource_ordered_list.front();
    if (activate_focus->state == FocusState::BACKGROUND) {
        nugu_info("[%s - %s] - FOREGROUND (priority - req:%d, rel:%d)", activate_focus->type.c_str(), activate_focus->name.c_str(), activate_focus->request_priority, activate_focus->release_priority);
        activate_focus->setState(FocusState::FOREGROUND);
    }

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
    for (auto& resource : focus_resource_ordered_list) {
        nugu_info("[%s - %s] - NONE (priority - req:%d, rel:%d)", resource->type.c_str(), resource->name.c_str(), resource->request_priority, resource->release_priority);
        resource->setState(FocusState::NONE);
    }
    focus_resource_ordered_list.clear();
}

void FocusManager::stopForegroundFocus()
{
    if (focus_resource_ordered_list.size() == 0)
        return;

    std::shared_ptr<FocusResource> activate_focus = focus_resource_ordered_list.front();
    nugu_info("[%s - %s] - NONE (priority - req:%d, rel:%d)", activate_focus->type.c_str(), activate_focus->name.c_str(), activate_focus->request_priority, activate_focus->release_priority);
    activate_focus->setState(FocusState::NONE);
    focus_resource_ordered_list.remove(activate_focus);

    if (focus_resource_ordered_list.size() == 0)
        return;

    activate_focus = focus_resource_ordered_list.front();
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
