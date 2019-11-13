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

#include "nugu_log.h"
#include "playsync_manager.hh"

#define HOLD_TIME_SHORT (7 * 1000) // 7 sec
#define HOLD_TIME_MID (15 * 1000) // 15 sec
#define HOLD_TIME_LONG (30 * 1000) // 30 sec
#define HOLD_TIME_LONGEST (10 * 60 * 1000) // 10 min
#define DEFAULT_HOLD_TIME HOLD_TIME_SHORT

namespace NuguCore {

const std::map<std::string, long> PlaySyncManager::DURATION_MAP = {
    { "SHORT", HOLD_TIME_SHORT },
    { "MID", HOLD_TIME_MID },
    { "LONG", HOLD_TIME_LONG },
    { "LONGEST", HOLD_TIME_LONGEST }
};

/*******************************************************************************
 * >>> Temp
 ******************************************************************************/
namespace Test {
    void showAllContents(std::map<std::string, std::vector<CapabilityType>> context_map)
    {
        std::string context_log("\n>>>>> (ContextStack) >>>>>\n");

        for (auto context : context_map) {
            context_log.append("[")
                .append(context.first)
                .append("] ");

            for (auto element : context.second) {
                if (element == CapabilityType::TTS)
                    context_log.append("TTS, ");
                else if (element == CapabilityType::AudioPlayer)
                    context_log.append("AudioPlayer, ");
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

PlaySyncManager::PlaySyncManager()
{
    timer = nugu_timer_new(DEFAULT_HOLD_TIME, 1);
    timer_cb_param.instance = this;
}

PlaySyncManager::~PlaySyncManager()
{
    if (timer) {
        nugu_timer_delete(timer);
        timer = nullptr;
    }

    renderer_map.clear();
    context_map.clear();
    context_stack.clear();
}

void PlaySyncManager::addContext(const std::string& ps_id, CapabilityType cap_type)
{
    addContext(ps_id, cap_type, {});
}

void PlaySyncManager::addContext(const std::string& ps_id, CapabilityType cap_type, DisplayRenderer renderer)
{
    if (ps_id.empty()) {
        nugu_error("Invalid PlayServiceId.");
        return;
    }

    // add renderer if exist
    if (renderer.listener) {
        if (renderer_map.find(ps_id) == renderer_map.end() || renderer_map[ps_id].cap_type == cap_type)
            addRenderer(ps_id, renderer);

        if (renderer.only_rendering)
            return;
    }

    // remove previous context
    auto play_stack = getAllPlayStackItems();

    for (auto play_item : play_stack) {
        if (ps_id != play_item)
            removeContext(play_item, cap_type);
    }

    if (context_map.find(ps_id) != context_map.end()) {
        addStackElement(ps_id, cap_type);
    } else {
        nugu_dbg("[context] add context");

        std::vector<CapabilityType> stack_elements;
        stack_elements.push_back(cap_type);
        context_map[ps_id] = stack_elements;
        context_stack.push_back(ps_id);
    }

    // temp: Just debugging & test. Please, maintain in some period.
    Test::showAllContents(context_map);
}

void PlaySyncManager::removeContext(std::string ps_id, CapabilityType cap_type, bool immediately)
{
    if (ps_id.empty()) {
        nugu_error("Invalid PlayServiceId.");
        return;
    }

    if (removeStackElement(ps_id, cap_type) && !is_expect_speech) {
        auto timerCallback = [](void* userdata) {
            nugu_dbg("[context] remove context");

            TimerCallbackParam* param = static_cast<TimerCallbackParam*>(userdata);

            if (!param->instance->removeRenderer(param->ps_id, param->immediately))
                return;

            param->instance->context_map.erase(param->ps_id);
            param->instance->context_stack.erase(remove(param->instance->context_stack.begin(), param->instance->context_stack.end(), param->ps_id),
                param->instance->context_stack.end());
            param->callback();
        };

        timer_cb_param.ps_id = ps_id;
        timer_cb_param.immediately = immediately;
        timer_cb_param.callback = [&]() {
            // temp: Just debugging & test. Please, maintain in some period.
            Test::showAllContents(context_map);
        };

        if (immediately || (renderer_map.find(ps_id) == renderer_map.end() && context_stack.size() >= 2)) {
            nugu_dbg("[context] try to remove context immediately.");

            timerCallback(&timer_cb_param);
        } else {
            nugu_dbg("[context] try to remove context by timer.");

            nugu_timer_set_callback(timer, timerCallback, &timer_cb_param);
            setTimerInterval(ps_id);
            nugu_timer_start(timer);
        }
    }

    is_expect_speech = false;
}

void PlaySyncManager::clearPendingContext(std::string ps_id)
{
    renderer_map.erase(ps_id);
    removeContext(ps_id, CapabilityType::Display);
}

std::vector<std::string> PlaySyncManager::getAllPlayStackItems()
{
    std::vector<std::string> play_stack(context_stack);
    std::reverse(play_stack.begin(), play_stack.end());
    return play_stack;
}

std::string PlaySyncManager::getPlayStackItem(CapabilityType cap_type)
{
    for (auto context : context_map) {
        for (auto element : context.second) {
            if (element == cap_type)
                return context.first;
        }
    }

    return "";
}

void PlaySyncManager::addStackElement(std::string ps_id, CapabilityType cap_type)
{
    auto& stack_elements = context_map[ps_id];

    if (std::find(stack_elements.begin(), stack_elements.end(), cap_type) == stack_elements.end())
        stack_elements.push_back(cap_type);
}

bool PlaySyncManager::removeStackElement(std::string ps_id, CapabilityType cap_type)
{
    auto& stack_elements = context_map[ps_id];
    stack_elements.erase(remove(stack_elements.begin(), stack_elements.end(), cap_type), stack_elements.end());

    return stack_elements.empty();
}

void PlaySyncManager::addRenderer(std::string ps_id, DisplayRenderer& renderer)
{
    // remove previous renderers firstly
    auto renderer_keys = getKeyOfMap(renderer_map);

    for (auto renderer_key : renderer_keys) {
        removeRenderer(renderer_key);
    }

    renderer_map[ps_id] = renderer;
    renderer.listener->onSyncDisplayContext(renderer.display_id);
}

bool PlaySyncManager::removeRenderer(std::string ps_id, bool unconditionally)
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

void PlaySyncManager::setTimerInterval(const std::string& ps_id)
{
    long duration_time = DEFAULT_HOLD_TIME;

    if (renderer_map.find(ps_id) != renderer_map.end()) {
        try {
            duration_time = DURATION_MAP.at(renderer_map.at(ps_id).duration);
        } catch (std::out_of_range& exception) {
            nugu_error(exception.what());
        }
    }

    nugu_timer_set_interval(timer, duration_time);
}

template <typename T, typename V>
std::vector<std::string> PlaySyncManager::getKeyOfMap(std::map<T, V>& map)
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

void PlaySyncManager::onMicOn()
{
    nugu_timer_stop(timer);
}

void PlaySyncManager::onASRError()
{
    is_expect_speech = false;

    if (!context_stack.empty())
        removeContext(context_stack.back(), CapabilityType::TTS);
}
} // NuguCore
