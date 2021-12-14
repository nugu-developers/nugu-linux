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
#include "routine_manager.hh"

namespace NuguCore {

RoutineManager::RoutineManager()
    : timer(std::unique_ptr<NUGUTimer>(new NUGUTimer(true)))
{
}

RoutineManager::~RoutineManager()
{
    clearContainer();
    listeners.clear();
}

void RoutineManager::reset()
{
    timer->stop();

    clearContainer();

    text_requester = nullptr;
    data_requester = nullptr;
    activity = RoutineActivity::IDLE;
    token = "";
    is_interrupted = false;
    has_post_delayed = false;
}

void RoutineManager::addListener(IRoutineManagerListener* listener)
{
    if (!listener) {
        nugu_warn("The listener is not exist.");
        return;
    }

    if (std::find(listeners.cbegin(), listeners.cend(), listener) == listeners.cend())
        listeners.emplace_back(listener);
}

void RoutineManager::removeListener(IRoutineManagerListener* listener)
{
    if (!listener) {
        nugu_warn("The listener is not exist.");
        return;
    }

    listeners.erase(
        std::remove_if(listeners.begin(), listeners.end(),
            [&](const IRoutineManagerListener* element) {
                return element == listener;
            }),
        listeners.end());
}

int RoutineManager::getListenerCount()
{
    return listeners.size();
}

void RoutineManager::setTimer(INuguTimer* timer)
{
    if (!timer) {
        nugu_warn("The timer is not exist.");
        return;
    }

    this->timer.reset(timer);
}

void RoutineManager::setTextRequester(TextRequester requester)
{
    if (requester)
        text_requester = requester;
}

void RoutineManager::setDataRequester(DataRequester requester)
{
    if (requester)
        data_requester = requester;
}

RoutineManager::TextRequester RoutineManager::getTextRequester()
{
    return text_requester;
}

RoutineManager::DataRequester RoutineManager::getDataRequester()
{
    return data_requester;
}

bool RoutineManager::start(const std::string& token, const Json::Value& actions)
{
    this->token = token;
    setActions(actions);

    return hasNext() ? next(), true : false;
}

void RoutineManager::stop()
{
    clearContainer();

    if (activity == RoutineActivity::PLAYING || activity == RoutineActivity::INTERRUPTED)
        setActivity(RoutineActivity::STOPPED);

    has_post_delayed = false;
    is_interrupted = false;
}

void RoutineManager::interrupt()
{
    is_interrupted = true;

    if (has_post_delayed)
        timer->stop();

    setActivity(RoutineActivity::INTERRUPTED);
}

void RoutineManager::resume()
{
    is_interrupted = false;

    has_post_delayed ? timer->restart() : finish();
}

void RoutineManager::finish()
{
    if (is_interrupted) {
        nugu_warn("The routine is interrupted.");
        return;
    }

    // remove previous progress action's dialog id
    action_dialog_ids.erase(
        std::find_if(action_dialog_ids.begin(), action_dialog_ids.end(),
            [&](const std::pair<std::string, int>& element) {
                return element.second == cur_action_index;
            }));

    if (hasNext()) {
        has_post_delayed ? postDelayed([&] { next(); }) : next();
    } else {
        cur_action_index = 0;

        if (activity == RoutineActivity::PLAYING)
            setActivity(RoutineActivity::FINISHED);
    }
}

int RoutineManager::getCurrentActionIndex()
{
    return cur_action_index;
}

bool RoutineManager::isRoutineProgress()
{
    return !is_interrupted && activity == RoutineActivity::PLAYING;
}

bool RoutineManager::isRoutineAlive()
{
    return isRoutineProgress() || !action_queue.empty();
}

bool RoutineManager::isActionProgress(const std::string& dialog_id)
{
    return action_dialog_ids.find(dialog_id) != action_dialog_ids.cend();
}

bool RoutineManager::hasRoutineDirective(const NuguDirective* ndir)
{
    if (!ndir) {
        nugu_warn("NuguDirective is not exist.");
        return false;
    }

    std::string dir_groups = nugu_directive_peek_groups(ndir);
    return dir_groups.find("Routine") != std::string::npos;
}

bool RoutineManager::isConditionToStop(const NuguDirective* ndir)
{
    if (!ndir) {
        nugu_warn("NuguDirective is not exist.");
        return false;
    }

    if (!isRoutineAlive() || hasRoutineDirective(ndir))
        return false;

    if (isActionProgress(nugu_directive_peek_dialog_id(ndir))) {
        if (hasNext()) {
            std::string dir_groups = nugu_directive_peek_groups(ndir);

            for (const auto& stop_directive : STOP_DIRECTIVE_FILTER)
                if (dir_groups.find(stop_directive) != std::string::npos)
                    return true;
        }

        return false;
    }

    return true;
}

bool RoutineManager::isConditionToFinishAction(const NuguDirective* ndir)
{
    if (!ndir)
        return false;

    return isActionProgress(nugu_directive_peek_dialog_id(ndir))
        && SKIP_FINISH_FILTER.find(nugu_directive_peek_namespace(ndir)) == SKIP_FINISH_FILTER.end();
}

const RoutineManager::RoutineActions& RoutineManager::getActionContainer()
{
    return action_queue;
}

const RoutineManager::RoutineActionDialogs& RoutineManager::getActionDialogs()
{
    return action_dialog_ids;
}

RoutineActivity RoutineManager::getActivity()
{
    return activity;
}

std::string RoutineManager::getToken()
{
    return token;
}

bool RoutineManager::isInterrupted()
{
    return is_interrupted;
}

bool RoutineManager::hasPostDelayed()
{
    return has_post_delayed;
}

bool RoutineManager::hasNext()
{
    return !action_queue.empty();
}

void RoutineManager::next()
{
    if (action_queue.empty())
        return;

    const auto& action = action_queue.front();
    std::string dialog_id;
    cur_action_index++;

    setActivity(RoutineActivity::PLAYING);

    if (action.type == ACTION_TYPE_TEXT)
        dialog_id = text_requester(token, action.text, action.play_service_id);
    else if (action.type == ACTION_TYPE_DATA)
        dialog_id = data_requester(action.play_service_id, action.data);

    if (!dialog_id.empty())
        action_dialog_ids.emplace(dialog_id, cur_action_index);

    if ((has_post_delayed = (action.post_delay_msec > 0)))
        timer->setInterval(action.post_delay_msec);

    action_queue.pop();
}

void RoutineManager::postDelayed(TimerCallback&& func)
{
    timer->setCallback(func);
    timer->start();
}

void RoutineManager::setActions(const Json::Value& actions)
{
    clearContainer();

    for (const auto& action : actions) {
        std::string type = action["type"].asString();
        std::string play_service_id = action["playServiceId"].asString();

        if (type.empty() || (type == "DATA" && (play_service_id.empty() || action["data"].empty())))
            continue;

        action_queue.push(RoutineAction {
            type,
            play_service_id,
            action["text"].asString(),
            action["token"].asString(),
            action["data"],
            action["postDelayInMilliseconds"].asLargestInt() });
    }
}

void RoutineManager::setActivity(RoutineActivity activity)
{
    if (this->activity == activity)
        return;

    this->activity = activity;

    for (const auto& listener : listeners)
        listener->onActivity(activity);
}

void RoutineManager::clearContainer()
{
    while (!action_queue.empty())
        action_queue.pop();

    action_dialog_ids.clear();
    cur_action_index = 0;
}

} // NuguCore
