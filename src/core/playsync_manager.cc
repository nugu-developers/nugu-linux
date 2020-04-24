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
#include "playsync_manager.hh"

#define HOLD_TIME_SHORT 7 // 7 sec
#define HOLD_TIME_MID 15 // 15 sec
#define HOLD_TIME_LONG 30 // 30 sec
#define HOLD_TIME_LONGEST (10 * 60) // 10 min
#define DEFAULT_HOLD_TIME HOLD_TIME_SHORT

namespace NuguCore {

/*******************************************************************************
 * >>> Temp
 ******************************************************************************/
namespace Test {
    void showAllContents(const PlaySyncManager::ContextMap& context_map)
    {
        std::string context_log("\n>>>>> (ContextStack) >>>>>\n");

        for (auto context : context_map) {
            context_log.append("[")
                .append(context.first)
                .append("] ");

            for (const auto& element : context.second) {
                context_log.append(element)
                    .append(", ");
            }

            context_log.append("\n");
        }

        context_log.append("<<<<< (ContextStack) <<<<<\n");

        nugu_dbg(context_log.c_str());
    }
}
/*******************************************************************************
 * <<< Temp
 ******************************************************************************/

PlaySyncManager::LongTimer::LongTimer(int interval, int repeat)
    : NUGUTimer(interval, repeat)
{
}

bool PlaySyncManager::LongTimer::hasPending()
{
    return has_pending;
}

void PlaySyncManager::LongTimer::start(unsigned int sec)
{
    NUGUTimer::start(sec);
    has_pending = false;
}

void PlaySyncManager::LongTimer::stop(bool pending)
{
    NUGUTimer::stop();
    has_pending = pending;
}

PlaySyncManager::PlaySyncManager()
    : DURATION_MAP({ { "SHORT", HOLD_TIME_SHORT },
        { "MID", HOLD_TIME_MID },
        { "LONG", HOLD_TIME_LONG },
        { "LONGEST", HOLD_TIME_LONGEST } })
    , timer(std::unique_ptr<NUGUTimer>(new NUGUTimer(DEFAULT_HOLD_TIME * NUGU_TIMER_UNIT_SEC, 1)))
    , long_timer(std::unique_ptr<LongTimer>(new LongTimer(HOLD_TIME_LONGEST * NUGU_TIMER_UNIT_SEC, 1)))
{
}

PlaySyncManager::~PlaySyncManager()
{
    renderer_map.clear();
    context_map.clear();
    context_stack.clear();
    layer_map.clear();
}

void PlaySyncManager::addContext(const std::string& ps_id, const std::string& cap_name)
{
    addContext(ps_id, cap_name, {});
}

void PlaySyncManager::addContext(const std::string& ps_id, const std::string& cap_name, DisplayRenderer&& renderer)
{
    if (ps_id.empty()) {
        nugu_warn("Invalid PlayServiceId.");
        return;
    }

    // add renderer if exist
    if (renderer.listener) {
        if (renderer_map.find(ps_id) == renderer_map.end() || renderer_map[ps_id].cap_name == cap_name)
            addRenderer(ps_id, renderer);

        if (renderer.only_rendering)
            return;
    }

    // stop previous timer if exist
    if (timer)
        timer->stop();

    // remove previous context
    auto play_stack = getAllPlayStackItems();

    for (const auto& play_item : play_stack) {
        if (ps_id != play_item)
            removeContextInnerly(play_item, cap_name);
    }

    if (context_map.find(ps_id) != context_map.end()) {
        addStackElement(ps_id, cap_name);
    } else {
        nugu_dbg("[context] add context");

        std::vector<std::string> stack_elements;
        stack_elements.emplace_back(cap_name);
        context_map[ps_id] = stack_elements;
        context_stack.emplace_back(ps_id);
        layer_map.emplace(ps_id, layer);
    }

    // temp: Just debugging & test. Please, maintain in some period.
    Test::showAllContents(context_map);
}

void PlaySyncManager::removeContextAction(const std::string& ps_id, bool immediately)
{
    if (!removeRenderer(ps_id, immediately))
        return;

    context_map.erase(ps_id);
    layer_map.erase(ps_id);
    context_stack.erase(remove(context_stack.begin(), context_stack.end(), ps_id), context_stack.end());

    // temp: Just debugging & test. Please, maintain in some period.
    Test::showAllContents(context_map);
}

void PlaySyncManager::removeContext(const std::string& ps_id, const std::string& cap_name, bool immediately)
{
    if (ps_id.empty()) {
        nugu_warn("Invalid PlayServiceId.");
        return;
    }

    // reset or stop pending long timer if exist
    if (long_timer && long_timer->hasPending() && !is_expect_speech) {
        try {
            if (layer_map.at(ps_id) == PlaysyncLayer::Media)
                long_timer->stop();
            else
                long_timer->start();
        } catch (std::out_of_range& exception) {
            nugu_warn(exception.what());
        }
    }

    removeContextInnerly(ps_id, cap_name, immediately);
}

void PlaySyncManager::removeContextInnerly(const std::string& ps_id, const std::string& cap_name, bool immediately)
{
    if (ps_id.empty()) {
        nugu_warn("Invalid PlayServiceId.");
        return;
    }

    if (context_map.find(ps_id) == context_map.end()) {
        nugu_warn("There are not exist matched context.");
        return;
    }

    if (removeStackElement(ps_id, cap_name) && !is_expect_speech) {
        auto timerCallback = [=](int count, int repeat) {
            nugu_dbg("[context] remove context");
            removeContextAction(ps_id, immediately);
        };

        if (immediately || (renderer_map.find(ps_id) == renderer_map.end() && context_stack.size() >= 2)) {
            nugu_dbg("[context] try to remove context immediately.");
            removeContextAction(ps_id, immediately);
        } else {
            nugu_dbg("[context] try to remove context by timer.");
            if (timer) {
                timer->setCallback(timerCallback);
                timer->setInterval(getRendererInterval(ps_id) * NUGU_TIMER_UNIT_SEC);
                timer->start();
            }
        }
    }
    is_expect_speech = false;
}

void PlaySyncManager::removeContextLater(const std::string& ps_id, const std::string& cap_name, unsigned int sec)
{
    long_timer->stop();
    long_timer->setInterval((sec > 0 ? sec : HOLD_TIME_LONGEST) * NUGU_TIMER_UNIT_SEC);
    long_timer->setCallback([=](int count, int repeat) {
        removeContextInnerly(ps_id, cap_name, true);
    });
    long_timer->start();
}

void PlaySyncManager::clearPendingContext(const std::string& ps_id)
{
    renderer_map.erase(ps_id);
    removeContextInnerly(ps_id, "Display");
}

std::vector<std::string> PlaySyncManager::getAllPlayStackItems()
{
    std::vector<std::string> play_stack(context_stack);
    std::reverse(play_stack.begin(), play_stack.end());
    return play_stack;
}

std::string PlaySyncManager::getPlayStackItem(const std::string& cap_name)
{
    for (auto context : context_map) {
        for (const auto& element : context.second) {
            if (element == cap_name)
                return context.first;
        }
    }

    return "";
}

PlaysyncLayer PlaySyncManager::getLayerType(const std::string& ps_id)
{
    try {
        return layer_map.at(ps_id);
    } catch (std::out_of_range& exception) {
        return PlaysyncLayer::Info; // It return Info layer defaults
    }
}

void PlaySyncManager::addStackElement(const std::string& ps_id, const std::string& cap_name)
{
    auto& stack_elements = context_map[ps_id];

    if (std::find(stack_elements.begin(), stack_elements.end(), cap_name) == stack_elements.end()) {
        stack_elements.emplace_back(cap_name);
        layer_map[ps_id] = layer;
    }
}

bool PlaySyncManager::removeStackElement(const std::string& ps_id, const std::string& cap_name)
{
    auto& stack_elements = context_map[ps_id];
    stack_elements.erase(remove(stack_elements.begin(), stack_elements.end(), cap_name), stack_elements.end());

    return stack_elements.empty();
}

void PlaySyncManager::addRenderer(const std::string& ps_id, DisplayRenderer& renderer)
{
    // remove previous renderers firstly
    auto renderer_keys = getKeyOfMap(renderer_map);

    for (const auto& renderer_key : renderer_keys) {
        removeRenderer(renderer_key);
    }

    renderer_map.emplace(ps_id, renderer);

    renderer.listener->onSyncDisplayContext(renderer.display_id);
}

bool PlaySyncManager::removeRenderer(const std::string& ps_id, bool unconditionally)
{
    if (renderer_map.find(ps_id) == renderer_map.end())
        return true;

    auto renderer = renderer_map[ps_id];
    bool result = renderer.listener->onReleaseDisplayContext(renderer.display_id, unconditionally)
        || unconditionally;

    if (result)
        renderer_map.erase(ps_id);

    return result;
}

int PlaySyncManager::getRendererInterval(const std::string& ps_id)
{
    int duration_time = DEFAULT_HOLD_TIME;

    if (renderer_map.find(ps_id) != renderer_map.end()) {
        try {
            duration_time = DURATION_MAP.at(renderer_map.at(ps_id).duration);
        } catch (std::out_of_range& exception) {
            nugu_warn(exception.what());
        }
    }

    return duration_time;
}

template <typename T, typename V>
std::vector<std::string> PlaySyncManager::getKeyOfMap(const std::map<T, V>& map)
{
    std::vector<std::string> keys;
    keys.reserve(map.size());

    std::transform(map.begin(), map.end(), std::back_inserter(keys),
        [](std::pair<T, V> const& pair) {
            return pair.first;
        });

    return keys;
}

void PlaySyncManager::setExpectSpeech(bool expect_speech)
{
    is_expect_speech = expect_speech;
}

void PlaySyncManager::holdContext()
{
    if (timer)
        timer->stop();

    if (long_timer) {
        long_timer->stop(true);
    }
}

void PlaySyncManager::clearContextHold()
{
    if (timer)
        timer->start();
}

void PlaySyncManager::onASRError(bool expect_speech)
{
    is_expect_speech = false;

    if (!context_stack.empty())
        removeContextInnerly(context_stack.back(), "TTS", expect_speech);

    // reset pending long timer if exists
    if (long_timer && long_timer->hasPending())
        long_timer->start();
}

void PlaySyncManager::setDirectiveGroups(const std::string& groups)
{
    if (groups.find("AudioPlayer") != std::string::npos)
        layer = PlaysyncLayer::Media;
    else if (groups.find("NuguCall") != std::string::npos)
        layer = PlaysyncLayer::Call;
    else if (groups.find("Alerts") != std::string::npos)
        layer = PlaysyncLayer::Alert;
    else
        layer = PlaysyncLayer::Info;
}
} // NuguCore
