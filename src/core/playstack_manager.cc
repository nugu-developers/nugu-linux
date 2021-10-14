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
#include <stdexcept>

#include "base/nugu_log.h"
#include "playstack_manager.hh"

namespace NuguCore {

/*******************************************************************************
 * for debugging
 ******************************************************************************/

namespace Debug {
    // NOLINTNEXTLINE (cert-err58-cpp)
    const std::map<PlayStackActivity, std::string> PLAYSTACK_ACTIVITY_TEXT {
        { PlayStackActivity::Alert, "[Alert] " },
        { PlayStackActivity::Call, "[Call] " },
        { PlayStackActivity::TTS, "[TTS] " },
        { PlayStackActivity::Media, "[Media] " },
    };

    void showPlayStack(const PlayStackManager::PlayStack& playstack_container)
    {
        std::string playstack_log("\n>>>>> [PlayStack] >>>>>\n");

        for (const auto& item : playstack_container.second) {
            try {
                const std::string& activity = PLAYSTACK_ACTIVITY_TEXT.at(playstack_container.first.at(item));
                playstack_log.append(activity).append(item).append("\n");
            } catch (std::out_of_range& exception) {
                // pass silently
            }
        }

        playstack_log.append("<<<<< [PlayStack] <<<<<\n");

        nugu_dbg(playstack_log.c_str());
    }
}

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

void PlayStackManager::StackTimer::notifyCallback()
{
    NUGUTimer::notifyCallback();
    is_started = false;
}

/*******************************************************************************
 * Define PlayStackManager
 *******************************************************************************/

PlayStackManager::PlayStackManager()
    : timer(std::unique_ptr<StackTimer>(new StackTimer()))
{
    timer->setSingleShot(true);
}

PlayStackManager::~PlayStackManager()
{
    clearContainer();
    listeners.clear();
}

void PlayStackManager::reset()
{
    timer->stop();

    clearContainer();

    has_long_timer = false;
    has_holding = false;
    is_expect_speech = false;
    is_stacked = false;
    has_adding_playstack = false;
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

void PlayStackManager::setTimer(IStackTimer* timer)
{
    if (!timer) {
        nugu_warn("The timer is not exist.");
        return;
    }

    this->timer.reset(timer);
}

bool PlayStackManager::add(const std::string& ps_id, NuguDirective* ndir)
{
    has_holding = false;

    if (ps_id.empty() || !ndir) {
        nugu_warn("The PlayServiceId or directive is empty.");
        return false;
    }

    PlayStackActivity activity = extractPlayStackActivity(ndir);
    is_expect_speech = hasExpectSpeech(ndir);
    is_stacked = isStackedCondition(activity);

    if (playstack_container.first.find(ps_id) != playstack_container.first.end()) {
        PlayStackActivity exist_activity = getPlayStackActivity(ps_id);

        if (exist_activity == activity || exist_activity == PlayStackActivity::Media) {
            nugu_warn("%s is already added.", ps_id.c_str());
            return false;
        } else if (activity != PlayStackActivity::Media) {
            playstack_container.first.at(ps_id) = activity;
            Debug::showPlayStack(playstack_container);
            return false;
        }
    }

    has_adding_playstack = hasDisplayRenderingInfo(ndir);
    handlePreviousStack(is_stacked);

    return addToContainer(ps_id, activity);
}

bool PlayStackManager::replace(const std::string& prev_ps_id, const std::string& new_ps_id)
{
    if (prev_ps_id.empty() || new_ps_id.empty() || prev_ps_id == new_ps_id) {
        nugu_warn("The PlayServiceId is empty or same.");
        return false;
    }

    try {
        PlayStackActivity activity = playstack_container.first.at(prev_ps_id);

        playstack_container.first.erase(prev_ps_id);
        removeItemInList(playstack_container.second, prev_ps_id);

        playstack_container.first.emplace(new_ps_id, activity);
        playstack_container.second.emplace_back(new_ps_id);
    } catch (std::out_of_range& exception) {
        nugu_warn("The playstack is not exist.");
        return false;
    }

    return true;
}

void PlayStackManager::remove(const std::string& ps_id, PlayStackRemoveMode mode)
{
    has_holding = false;

    if (ps_id.empty() || playstack_container.first.find(ps_id) == playstack_container.first.end()) {
        nugu_warn("The PlayServiceId is empty or not exist in PlayStack.");
        return;
    }

    if (mode == PlayStackRemoveMode::Immediately) {
        removeFromContainer(ps_id);

        if (has_long_timer && getPlayStackActivity(ps_id) == PlayStackActivity::Media) {
            timer->stop();
            has_long_timer = false;
        }
    } else if (!is_expect_speech && isStackedCondition(ps_id)) {
        removeFromContainer(ps_id);
    } else {
        timer->setCallback([&, ps_id]() {
            removeFromContainer(ps_id);
            has_long_timer = false;
        });

        has_long_timer = (mode == PlayStackRemoveMode::Later);
        timer->setInterval((has_long_timer ? hold_times_sec.long_time : hold_times_sec.normal_time) * NUGU_TIMER_UNIT_SEC);
        timer->start();
    }
}

bool PlayStackManager::isStackedCondition(NuguDirective* ndir)
{
    return ndir && isStackedCondition(extractPlayStackActivity(ndir));
}

bool PlayStackManager::hasExpectSpeech(NuguDirective* ndir)
{
    return hasKeyword(ndir, { "ASR.ExpectSpeech" });
}

void PlayStackManager::stopHolding()
{
    if (timer->isStarted() && !has_long_timer) {
        timer->stop();
        has_holding = true;
    }
}

void PlayStackManager::resetHolding()
{
    if (has_holding) {
        (is_expect_speech && is_stacked) ? timer->notifyCallback() : timer->start();
        is_expect_speech = false;
        has_holding = false;
    }
}

void PlayStackManager::clearHolding()
{
    timer->stop();
    has_long_timer = false;
}

bool PlayStackManager::isActiveHolding()
{
    return timer->isStarted();
}

bool PlayStackManager::hasAddingPlayStack()
{
    return has_adding_playstack;
}

void PlayStackManager::setDefaultPlayStackHoldTime(PlayStakcHoldTimes&& hold_times_sec)
{
    if (hold_times_sec.normal_time > 0)
        default_hold_times_sec.normal_time = hold_times_sec.normal_time;

    if (hold_times_sec.long_time > 0)
        default_hold_times_sec.long_time = hold_times_sec.long_time;

    setPlayStackHoldTime(PlayStakcHoldTimes { default_hold_times_sec });
}

void PlayStackManager::setPlayStackHoldTime(PlayStakcHoldTimes&& hold_times_sec)
{
    this->hold_times_sec = hold_times_sec;

    if (this->hold_times_sec.long_time <= 0)
        this->hold_times_sec.long_time = DEFAULT_LONG_HOLD_TIME_SEC;
}

void PlayStackManager::resetPlayStackHoldTime()
{
    hold_times_sec = default_hold_times_sec;
}

const PlayStackManager::PlayStakcHoldTimes& PlayStackManager::getPlayStackHoldTime()
{
    return hold_times_sec;
}

PlayStackActivity PlayStackManager::getPlayStackActivity(const std::string& ps_id) noexcept
{
    try {
        return playstack_container.first.at(ps_id);
    } catch (std::out_of_range& exception) {
        nugu_warn("The related PlayStackActivity is not exist.");
        return PlayStackActivity::None;
    }
}

std::vector<std::string> PlayStackManager::getAllPlayStackItems()
{
    std::vector<std::string> reverse_play_stack(playstack_container.second);
    std::reverse(reverse_play_stack.begin(), reverse_play_stack.end());

    return reverse_play_stack;
}

const PlayStackManager::PlayStack& PlayStackManager::getPlayStackContainer()
{
    return playstack_container;
}

std::set<bool> PlayStackManager::getFlagSet()
{
    return { has_long_timer, has_holding, is_expect_speech, is_stacked };
}

PlayStackActivity PlayStackManager::extractPlayStackActivity(NuguDirective* ndir)
{
    std::string groups = nugu_directive_peek_groups(ndir);

    if (groups.find("AudioPlayer") != std::string::npos)
        return PlayStackActivity::Media;
    else if (groups.find("NuguCall") != std::string::npos)
        return PlayStackActivity::Call;
    else if (groups.find("Alerts") != std::string::npos)
        return PlayStackActivity::Alert;
    else
        return PlayStackActivity::TTS;
}

std::string PlayStackManager::getNoneMediaLayerStack()
{
    for (const auto& item : playstack_container.first)
        if (item.second != PlayStackActivity::Media)
            return item.first;

    return "";
}

void PlayStackManager::handlePreviousStack(bool is_stacked)
{
    if (is_stacked) {
        removeFromContainer(getNoneMediaLayerStack());
    } else {
        for (const auto& item : playstack_container.second)
            notifyStackRemoved(item);

        clearContainer();
    }
}

bool PlayStackManager::hasDisplayRenderingInfo(NuguDirective* ndir)
{
    return hasKeyword(ndir, { "Display", "AudioPlayer" });
}

bool PlayStackManager::hasKeyword(NuguDirective* ndir, std::vector<std::string>&& keywords)
{
    if (!ndir) {
        nugu_warn("The directive is empty.");
        return false;
    }

    const char* tmp_ndir_groups = nugu_directive_peek_groups(ndir);
    std::string ndir_groups = tmp_ndir_groups ? tmp_ndir_groups : "";

    for (const auto& keyword : keywords)
        if (ndir_groups.find(keyword) != std::string::npos)
            return true;

    return false;
}

bool PlayStackManager::addToContainer(const std::string& ps_id, PlayStackActivity activity)
{
    if (playstack_container.first.find(ps_id) != playstack_container.first.end()) {
        nugu_warn("%s is already added.", ps_id.c_str());
        return false;
    }

    playstack_container.first.emplace(ps_id, activity);
    playstack_container.second.emplace_back(ps_id);

    has_adding_playstack = false;

    for (const auto& listener : listeners)
        listener->onStackAdded(ps_id);

    Debug::showPlayStack(playstack_container);

    return true;
}

void PlayStackManager::removeFromContainer(const std::string& ps_id)
{
    if (ps_id.empty()) {
        nugu_warn("The PlayServiceId is empty.");
        return;
    }

    playstack_container.first.erase(ps_id);
    removeItemInList(playstack_container.second, ps_id);

    notifyStackRemoved(ps_id);

    Debug::showPlayStack(playstack_container);
}

void PlayStackManager::notifyStackRemoved(const std::string& ps_id)
{
    for (const auto& listener : listeners)
        listener->onStackRemoved(ps_id);
}

void PlayStackManager::clearContainer()
{
    playstack_container.first.clear();
    playstack_container.second.clear();
}

bool PlayStackManager::isStackedCondition(PlayStackActivity activity)
{
    auto result = std::find_if(playstack_container.first.cbegin(), playstack_container.first.cend(),
        [](const std::pair<std::string, PlayStackActivity>& item) {
            return item.second == PlayStackActivity::Media;
        });

    return result != playstack_container.first.cend() && activity != PlayStackActivity::Media;
}

bool PlayStackManager::isStackedCondition(const std::string& ps_id)
{
    return isStackedCondition(getPlayStackActivity(ps_id));
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
