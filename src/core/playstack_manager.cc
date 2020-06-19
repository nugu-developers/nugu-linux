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
#include "playstack_manager.hh"

namespace NuguCore {

/*******************************************************************************
 * Define PlayStackManager::StackTimer
 *******************************************************************************/

bool PlayStackManager::StackTimer::isStarted()
{
    return is_started;
}

void PlayStackManager::StackTimer::start(unsigned int sec)
{
    NUGUTimer::start(sec);
    is_started = true;
}

void PlayStackManager::StackTimer::stop()
{
    NUGUTimer::stop();
    is_started = false;
}

/*******************************************************************************
 * Define PlayStackManager
 *******************************************************************************/

PlayStackManager::PlayStackManager()
    : timer(std::unique_ptr<StackTimer>(new StackTimer()))
{
}

PlayStackManager::~PlayStackManager()
{
    clearContainer();
    listeners.clear();
}

void PlayStackManager::addListener(IPlayStackManagerListener* listener)
{
    if (!listener) {
        nugu_warn("The listener is not exist.");
        return;
    }

    if (std::find(listeners.cbegin(), listeners.cend(), listener) == listeners.cend())
        listeners.emplace_back(listener);
}

void PlayStackManager::removeListener(IPlayStackManagerListener* listener)
{
    if (!listener) {
        nugu_warn("The listener is not exist.");
        return;
    }

    removeItemInList(listeners, listener);
}

int PlayStackManager::getListenerCount()
{
    return listeners.size();
}

void PlayStackManager::add(const std::string& ps_id, NuguDirective* ndir)
{
    if (ps_id.empty() || !ndir) {
        nugu_warn("The PlayServiceId or directive is empty.");
        return;
    }

    if (playstack_container.first.find(ps_id) != playstack_container.first.end()) {
        nugu_warn("%s is already added.", ps_id.c_str());
        return;
    }

    PlayStackLayer layer = extractPlayStackLayer(ndir);

    if (!isStackedCondition(layer))
        clearContainer();

    addToContainer(ps_id, layer);
}

void PlayStackManager::remove(const std::string& ps_id, PlayStackRemoveMode mode)
{
    if (ps_id.empty()) {
        nugu_warn("The PlayServiceId is empty.");
        return;
    }

    if (mode == PlayStackRemoveMode::Immediately)
        removeFromContainer(ps_id);
    else {
        timer->setCallback([&, ps_id](int count, int repeat) {
            removeFromContainer(ps_id);
        });

        timer->setInterval(mode == PlayStackRemoveMode::Later ? LONG_HOLD_TIME : DEFAULT_HOLD_TIME);
        timer->start();
    }
}

void PlayStackManager::stopHolding()
{
    if (timer->isStarted()) {
        timer->stop();
        has_holding = true;
    }
}

void PlayStackManager::resetHolding()
{
    if (has_holding) {
        timer->start();
        has_holding = false;
    }
}

bool PlayStackManager::isActiveHolding()
{
    return timer->isStarted();
}

PlayStackLayer PlayStackManager::getPlayStackLayer(const std::string& ps_id)
{
    try {
        return playstack_container.first.at(ps_id);
    } catch (std::out_of_range& exception) {
        nugu_warn("The related PlayStackLayer is not exist.");
        return PlayStackLayer::None;
    }
}

std::vector<std::string> PlayStackManager::getAllPlayStackItems()
{
    return playstack_container.second;
}

PlayStackManager::PlayStack PlayStackManager::getPlayStackContainer()
{
    return playstack_container;
}

PlayStackLayer PlayStackManager::extractPlayStackLayer(NuguDirective* ndir)
{
    std::string groups = nugu_directive_peek_groups(ndir);

    if (groups.find("AudioPlayer") != std::string::npos)
        return PlayStackLayer::Media;
    else if (groups.find("NuguCall") != std::string::npos)
        return PlayStackLayer::Call;
    else if (groups.find("Alerts") != std::string::npos)
        return PlayStackLayer::Alert;
    else
        return PlayStackLayer::Info;
}

void PlayStackManager::addToContainer(const std::string& ps_id, PlayStackLayer layer)
{
    playstack_container.first.emplace(ps_id, layer);
    playstack_container.second.emplace_back(ps_id);

    for (const auto& listener : listeners)
        listener->onStackAdded(ps_id);
}

void PlayStackManager::removeFromContainer(const std::string& ps_id)
{
    playstack_container.first.erase(ps_id);
    removeItemInList(playstack_container.second, ps_id);

    for (const auto& listener : listeners)
        listener->onStackRemoved(ps_id);
}

void PlayStackManager::clearContainer()
{
    playstack_container.first.clear();
    playstack_container.second.clear();
}

bool PlayStackManager::isStackedCondition(PlayStackLayer layer)
{
    auto result = std::find_if(playstack_container.first.cbegin(), playstack_container.first.cend(),
        [](const std::pair<std::string, PlayStackLayer>& item) {
            return item.second == PlayStackLayer::Media;
        });

    return result != playstack_container.first.cend() && layer != PlayStackLayer::Media;
}

template <typename T>
void PlayStackManager::removeItemInList(std::vector<T>& list, const T& item)
{
    list.erase(
        std::remove_if(list.begin(), list.end(),
            [&](const T& element) {
                return element == item;
            }),
        list.end());
}

} // NuguCore
