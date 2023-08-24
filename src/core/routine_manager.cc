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
    is_mute_delayed = false;
    is_action_timeout = false;
    has_post_delayed = false;
    has_pending_stop = false;
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

bool RoutineManager::start(const std::string& token, const NJson::Value& actions)
{
    this->token = token;
    setActions(actions);

    return hasNext() ? next(), true : false;
}

void RoutineManager::stop()
{
    has_pending_stop = false;

    clearContainer();

    if (activity != RoutineActivity::FINISHED)
        setActivity(RoutineActivity::STOPPED);

    has_post_delayed = false;
    is_interrupted = false;

    if (is_action_timeout || is_mute_delayed) {
        timer->stop();
        is_action_timeout = false;
        is_mute_delayed = false;
    }
}

void RoutineManager::interrupt()
{
    if (activity == RoutineActivity::SUSPENDED) {
        nugu_warn("The routine is suspended.");
        return;
    }

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

bool RoutineManager::move(unsigned int index)
{
    if (activity == RoutineActivity::FINISHED || activity == RoutineActivity::STOPPED) {
        nugu_warn("The routine is not alive");
        return false;
    }

    if (index < 1 || index > action_queue.size()) {
        nugu_warn("It's out of index.");
        return false;
    }

    if (is_mute_delayed) {
        nugu_warn("The routine is mute delayed.");
        return false;
    }

    if (is_action_timeout)
        timer->stop();

    removePreviousActionDialog();

    cur_action_index = index - 1;

    // find executable action type (TEXT|DATA)
    for (; cur_action_index < action_queue.size(); cur_action_index++)
        if (action_queue[cur_action_index].type != ACTION_TYPE_BREAK)
            break;

    // skip processing next, if no executable action type exist until the end
    if (!(is_last_action_processed = (cur_action_index == action_queue.size())))
        next();

    return true;
}

void RoutineManager::finish()
{
    if (is_interrupted) {
        nugu_warn("The routine is interrupted.");
        return;
    }

    if (is_action_timeout || is_mute_delayed) {
        nugu_warn("The routine is action timeout or mute delayed.");
        return;
    }

    removePreviousActionDialog();

    if (hasNext()) {
        has_post_delayed ? postHandle([&] { next(); }) : next();
    } else {
        cur_action_index = 0;
        cur_countable_action_index = 0;
        cur_action_token.clear();
        has_pending_stop = false;

        if (activity == RoutineActivity::PLAYING || activity == RoutineActivity::SUSPENDED)
            setActivity(RoutineActivity::FINISHED);
    }
}

std::string RoutineManager::getCurrentActionToken()
{
    return cur_action_token;
}

unsigned int RoutineManager::getCurrentActionIndex()
{
    return cur_action_index;
}

unsigned int RoutineManager::getCountableActionSize()
{
    unsigned int size = 0;

    for (const auto& action : action_queue)
        if (action.type != ACTION_TYPE_BREAK)
            size++;

    return size;
}

unsigned int RoutineManager::getCountableActionIndex()
{
    return cur_countable_action_index;
}

bool RoutineManager::isActionValid(const NJson::Value& action)
{
    std::string type = action["type"].asString();
    std::string play_service_id = action["playServiceId"].asString();

    if (type.empty() || (type == ACTION_TYPE_DATA && (play_service_id.empty() || action["data"].empty())))
        return false;

    return true;
}

bool RoutineManager::isRoutineProgress()
{
    return !is_interrupted && activity == RoutineActivity::PLAYING;
}

bool RoutineManager::isRoutineAlive()
{
    return isRoutineProgress() || (!action_queue.empty() && !is_last_action_processed);
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

    if (!isRoutineAlive() || hasRoutineDirective(ndir) || activity == RoutineActivity::SUSPENDED)
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

bool RoutineManager::isConditionToCancel(const NuguDirective* ndir)
{
    if (!ndir)
        return false;

    std::string dir_groups = nugu_directive_peek_groups(ndir);

    if (dir_groups.find("Routine.Stop") != std::string::npos)
        return false;
    else if (dir_groups.find("Routine") != std::string::npos)
        return true;

    return false;
}

bool RoutineManager::isMuteDelayed()
{
    return is_mute_delayed;
}

void RoutineManager::presetActionTimeout()
{
    if (is_last_action_processed) {
        nugu_warn("The all actions are processed");
        return;
    }

    if (action_queue[cur_action_index - 1].action_timeout_msec == 0)
        handleActionTimeout(DEFAULT_ACTION_TIME_OUT_MSEC);
}

void RoutineManager::setPendingStop(const NuguDirective* ndir)
{
    std::string dir_groups = nugu_directive_peek_groups(ndir);

    has_pending_stop = dir_groups.find("Routine.Stop") != std::string::npos;
}

bool RoutineManager::hasToSkipMedia(const std::string& dialog_id)
{
    if (!isRoutineAlive() || has_pending_stop)
        return false;

    if (isActionProgress(dialog_id) || activity == RoutineActivity::SUSPENDED)
        return false;

    return true;
}

const RoutineManager::RoutineActions& RoutineManager::getActionContainer() const
{
    return action_queue;
}

const RoutineManager::RoutineActionDialogs& RoutineManager::getActionDialogs() const
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

bool RoutineManager::isActionTimeout()
{
    return is_action_timeout;
}

bool RoutineManager::isLastActionProcessed()
{
    return is_last_action_processed;
}

bool RoutineManager::hasPostDelayed()
{
    return has_post_delayed;
}

bool RoutineManager::hasPendingStop()
{
    return has_pending_stop;
}

bool RoutineManager::hasNext()
{
    return cur_action_index < action_queue.size();
}

void RoutineManager::next()
{
    if (cur_action_index >= action_queue.size()) {
        nugu_warn("The cur_action_index is invalid.");
        return;
    }

    const auto& action = action_queue[cur_action_index++];
    std::string dialog_id;
    cur_action_token = action.token;
    is_last_action_processed = (cur_action_index == action_queue.size());
    is_action_timeout = is_mute_delayed = has_post_delayed = false;
    has_pending_stop = false;

    setActivity((action.type == ACTION_TYPE_BREAK) ? RoutineActivity::SUSPENDED : RoutineActivity::PLAYING);

    if (action.type == ACTION_TYPE_BREAK) {
        if ((is_mute_delayed = (action.mute_delay_msec > 0)))
            timer->setInterval(action.mute_delay_msec);

        is_mute_delayed ? postHandle([&] { next(); }) : next();
    } else {
        if (action.type == ACTION_TYPE_TEXT)
            dialog_id = text_requester(token, action.text, action.play_service_id);
        else if (action.type == ACTION_TYPE_DATA)
            dialog_id = data_requester(action.play_service_id, action.data);

        if (!dialog_id.empty())
            action_dialog_ids.emplace(dialog_id, cur_action_index);

        if (action.action_timeout_msec > 0)
            handleActionTimeout(action.action_timeout_msec);
        else if ((has_post_delayed = (action.post_delay_msec > 0)))
            timer->setInterval(action.post_delay_msec);

        cur_countable_action_index++;
    }
}

void RoutineManager::handleActionTimeout(const long long action_time_msec)
{
    is_action_timeout = true;
    setActivity(RoutineActivity::SUSPENDED);

    timer->setInterval(action_time_msec);
    postHandle([&] {
        is_action_timeout = false;

        for (const auto& listener : listeners)
            listener->onActionTimeout(is_last_action_processed);
    });
}

void RoutineManager::postHandle(TimerCallback&& func)
{
    timer->setCallback(func);
    timer->start();
}

void RoutineManager::setActions(const NJson::Value& actions)
{
    clearContainer();

    for (const auto& action : actions)
        if (isActionValid(action))
            action_queue.emplace_back(RoutineAction {
                action["type"].asString(),
                action["playServiceId"].asString(),
                action["text"].asString(),
                action["token"].asString(),
                action["data"],
                action["postDelayInMilliseconds"].asLargestInt(),
                action["muteDelayInMilliseconds"].asLargestInt(),
                action["actionTimeoutInMilliseconds"].asLargestInt() });
}

void RoutineManager::setActivity(RoutineActivity activity)
{
    if (this->activity == activity)
        return;

    this->activity = activity;

    for (const auto& listener : listeners)
        listener->onActivity(activity);
}

void RoutineManager::removePreviousActionDialog()
{
    auto cur_action_dialog_id = std::find_if(action_dialog_ids.begin(), action_dialog_ids.end(),
        [&](const std::pair<std::string, unsigned int>& element) {
            return element.second == cur_action_index;
        });

    if (cur_action_dialog_id != action_dialog_ids.end())
        action_dialog_ids.erase(cur_action_dialog_id);
}

void RoutineManager::clearContainer()
{
    action_queue.clear();
    action_dialog_ids.clear();
    cur_action_index = 0;
    cur_countable_action_index = 0;
    cur_action_token.clear();
    is_last_action_processed = false;
}

} // NuguCore
