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
#include <vector>

#include "clientkit/routine_manager_interface.hh"
#include "nugu_timer.hh"

namespace NuguCore {

using namespace NuguClientKit;

class RoutineManager : public IRoutineManager {
public:
    using RoutineAction = struct {
        std::string type;
        std::string play_service_id;
        std::string text;
        std::string token;
        Json::Value data;
        long long post_delay_msec;
    };
    using RoutineActions = std::queue<RoutineAction>;
    using RoutineActionDialogs = std::map<std::string, int>;

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
    void finish() override;
    int getCurrentActionIndex() override;
    bool isRoutineProgress() override;
    bool isRoutineAlive() override;
    bool isActionProgress(const std::string& dialog_id) override;
    bool hasRoutineDirective(const NuguDirective* ndir) override;
    bool isConditionToStop(const NuguDirective* ndir) override;

    const RoutineActions& getActionContainer();
    const RoutineActionDialogs& getActionDialogs();
    RoutineActivity getActivity();
    std::string getToken();
    bool isInterrupted();
    bool hasPostDelayed();

private:
    using TimerCallback = std::function<void()>;

    bool hasNext();
    void next();
    void postDelayed(TimerCallback&& func);
    void setActions(const Json::Value& actions);
    void setActivity(RoutineActivity activity);
    void clearContainer();

    const std::string ACTION_TYPE_TEXT = "TEXT";
    const std::string ACTION_TYPE_DATA = "DATA";

    std::vector<IRoutineManagerListener*> listeners;
    std::vector<std::string> stop_directive_filter;
    RoutineActivity activity = RoutineActivity::IDLE;
    std::unique_ptr<INuguTimer> timer = nullptr;
    TextRequester text_requester = nullptr;
    DataRequester data_requester = nullptr;
    RoutineActionDialogs action_dialog_ids;
    RoutineActions action_queue;
    std::string token;
    int cur_action_index = 0;
    bool is_interrupted = false;
    bool has_post_delayed = false;
};

} // NuguCore

#endif /* __NUGU_ROUTINE_MANAGER_H__ */
