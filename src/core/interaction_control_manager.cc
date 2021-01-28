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

#include "base/nugu_log.h"
#include "interaction_control_manager.hh"

namespace NuguCore {

InteractionControlManager::~InteractionControlManager()
{
    clearContainer();
    listeners.clear();
}

void InteractionControlManager::reset()
{
    clearContainer();
}

void InteractionControlManager::addListener(IInteractionControlManagerListener* listener)
{
    if (!listener) {
        nugu_warn("The listener is not exist.");
        return;
    }

    if (std::find(listeners.cbegin(), listeners.cend(), listener) == listeners.cend())
        listeners.emplace_back(listener);
}

void InteractionControlManager::removeListener(IInteractionControlManagerListener* listener)
{
    if (!listener) {
        nugu_warn("The listener is not exist.");
        return;
    }

    listeners.erase(
        std::remove_if(listeners.begin(), listeners.end(),
            [&](const IInteractionControlManagerListener* element) {
                return element == listener;
            }),
        listeners.end());
}

int InteractionControlManager::getListenerCount()
{
    return listeners.size();
}

void InteractionControlManager::notifyHasMultiTurn()
{
    for (const auto& listener : listeners)
        listener->onHasMultiTurn();
}

bool InteractionControlManager::isMultiTurnActive()
{
    return !requester_set.empty();
}

void InteractionControlManager::start(InteractionMode mode, const std::string& requester)
{
    if (mode != InteractionMode::MULTI_TURN) {
        nugu_warn("The mode is not multi-turn.");
        return;
    }

    if (requester.empty() || requester_set.find(requester) != requester_set.end()) {
        nugu_warn("The requester is empty or already exist.");
        return;
    }

    requester_set.emplace(requester);

    // It notify one time when the first requester is added.
    if (requester_set.size() == 1)
        notifyModeChange(true);
}

void InteractionControlManager::finish(InteractionMode mode, const std::string& requester)
{
    if (mode != InteractionMode::MULTI_TURN) {
        nugu_warn("The mode is not multi-turn.");
        return;
    }

    if (requester.empty() || requester_set.find(requester) == requester_set.end()) {
        nugu_warn("The requester is empty or already removed.");
        return;
    }

    requester_set.erase(requester);

    // It notify one time when all requesters are removed.
    if (requester_set.empty())
        notifyModeChange(false);
}

void InteractionControlManager::clear()
{
    if (!requester_set.empty())
        notifyModeChange(false);

    clearContainer();
}

const InteractionControlManager::Requesters& InteractionControlManager::getAllRequesters()
{
    return requester_set;
}

void InteractionControlManager::notifyModeChange(bool is_multi_turn)
{
    for (const auto& listener : listeners)
        listener->onModeChanged(is_multi_turn);
}

void InteractionControlManager::clearContainer()
{
    requester_set.clear();
}

} // NuguCore
