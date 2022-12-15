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

#ifndef __NUGU_ROUTINE_MANAGER_H__
#define __NUGU_ROUTINE_MANAGER_H__

#include <json/json.h>
#include <memory>
#include <queue>
#include <set>
#include <vector>

#include "clientkit/routine_manager_interface.hh"
#include "nugu_timer.hh"

namespace NuguCore {

using namespace NuguClientKit;

class RoutineManager : public IRoutineManager {
public:
    using RoutineAction = struct _RoutineAction {
        std::string type;
        std::string play_service_id;
        std::string text;
        std::string token;
        Json::Value data;
        long long post_delay_msec;
        long long mute_delay_msec;
        long long action_timeout_msec;
    };
    using RoutineActions = std::vector<RoutineAction>;
    using RoutineActionDialogs = std::map<std::string, unsigned int>;

public:
    RoutineManager();
    virtual ~RoutineManager();

    void reset();
    void addListener(IRoutineManagerListener* listener) override;
    void removeListener(IRoutineManagerListener* listener) override;
    int getListenerCount();
    void setTimer(INuguTimer* timer);

    void setTextRequester(TextRequester requester) override;
    void setDataRequester(DataRequester requester) override;
    TextRequester getTextRequester();
    DataRequester getDataRequester();

    bool start(const std::string& token, const Json::Value& actions) override;
    void stop() override;
    void interrupt() override;
    void resume() override;
    bool move(unsigned int index) override;
    void finish() override;
    std::string getCurrentActionToken() override;
    unsigned int getCurrentActionIndex() override;
    unsigned int getCountableActionSize() override;
    unsigned int getCountableActionIndex() override;
    bool isRoutineProgress() override;
    bool isRoutineAlive() override;
    bool isActionProgress(const std::string& dialog_id) override;
    bool hasRoutineDirective(const NuguDirective* ndir) override;
    bool isConditionToStop(const NuguDirective* ndir) override;
    bool isConditionToFinishAction(const NuguDirective* ndir) override;
    bool isConditionToCancel(const NuguDirective* ndir) override;
    bool isMuteDelayed() override;
    void presetActionTimeout() override;
    void setPendingStop(const NuguDirective* ndir) override;
    bool hasToSkipMedia(const std::string& dialog_id) override;

    const RoutineActions& getActionContainer() const;
    const RoutineActionDialogs& getActionDialogs() const;
    RoutineActivity getActivity();
    std::string getToken();
    bool isInterrupted();
    bool isActionTimeout();
    bool isLastActionProcessed();
    bool hasPostDelayed();
    bool hasPendingStop();

private:
    using TimerCallback = std::function<void()>;

    bool hasNext();
    void next();
    void handleActionTimeout(const long long action_time_msec);
    void postHandle(TimerCallback&& func);
    void setActions(const Json::Value& actions);
    void setActivity(RoutineActivity activity);
    void removePreviousActionDialog();
    void clearContainer();

    const std::string ACTION_TYPE_TEXT = "TEXT";
    const std::string ACTION_TYPE_DATA = "DATA";
    const std::string ACTION_TYPE_BREAK = "BREAK";
    const std::vector<std::string> STOP_DIRECTIVE_FILTER {
        "ASR.ExpectSpeech"
    };
    const std::set<std::string> SKIP_FINISH_FILTER {
        "Display"
    };
    const long long DEFAULT_ACTION_TIME_OUT_MSEC = 5000;

    std::vector<IRoutineManagerListener*> listeners;
    RoutineActivity activity = RoutineActivity::IDLE;
    std::unique_ptr<INuguTimer> timer = nullptr;
    TextRequester text_requester = nullptr;
    DataRequester data_requester = nullptr;
    RoutineActionDialogs action_dialog_ids;
    RoutineActions action_queue;
    std::string token;
    std::string cur_action_token;
    unsigned int cur_action_index = 0;
    unsigned int cur_countable_action_index = 0;
    bool is_interrupted = false;
    bool is_mute_delayed = false;
    bool is_action_timeout = false;
    bool is_last_action_processed = false;
    bool has_post_delayed = false;
    bool has_pending_stop = false;
};

} // NuguCore

#endif /* __NUGU_ROUTINE_MANAGER_H__ */
