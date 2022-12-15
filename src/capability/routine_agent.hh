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

#ifndef __NUGU_ROUTINE_AGENT_H__
#define __NUGU_ROUTINE_AGENT_H__

#include <map>

#include "capability/routine_interface.hh"
#include "clientkit/capability.hh"

namespace NuguCapability {

class RoutineAgent final : public Capability,
                           public IRoutineHandler,
                           public IRoutineManagerListener {
public:
    RoutineAgent();
    virtual ~RoutineAgent() = default;

    void initialize() override;
    void deInitialize() override;
    void setCapabilityListener(ICapabilityListener* clistener) override;
    void updateInfoForContext(Json::Value& ctx) override;
    bool getProperty(const std::string& property, std::string& value) override;
    void parsingDirective(const char* dname, const char* message) override;

    // implements IRoutineHandler
    bool startRoutine(const std::string& dialog_id, const std::string& data) override;
    bool next() override;
    bool prev() override;

private:
    using RoutineInfo = struct _RoutineInfo {
        std::string play_service_id;
        std::string token;
        std::string name;
        std::string routine_id;
        std::string routine_type;
        std::string routine_list_type;
        std::string source;
        Json::Value actions = Json::arrayValue;
    };

    // implements IRoutineManagerListener
    void onActivity(RoutineActivity activity) override;
    void onActionTimeout(bool last_action = false) override;

    void handleInterrupted();
    bool handleMoveControl(int offset);
    bool handlePendingActionTimeout();
    void clearRoutineInfo();

    void parsingStart(const char* message);
    void parsingStop(const char* message);
    void parsingContinue(const char* message);
    void parsingMove(const char* message);

    void sendEventStarted(EventResultCallback cb = nullptr);
    void sendEventFailed(EventResultCallback cb = nullptr);
    void sendEventFinished(EventResultCallback cb = nullptr);
    void sendEventStopped(EventResultCallback cb = nullptr);
    void sendEventMoveControl(const int offset, EventResultCallback cb = nullptr);
    void sendEventMoveSucceeded(EventResultCallback cb = nullptr);
    void sendEventMoveFailed(EventResultCallback cb = nullptr);
    void sendEventActionTimeoutTriggered(EventResultCallback cb = nullptr);

    void sendEventCommon(std::string&& event_name, Json::Value&& extra_value, EventResultCallback cb = nullptr, bool clear_routine_info = false);
    std::string sendEventActionTriggered(const std::string& ps_id, const Json::Value& data, EventResultCallback cb = nullptr);

    const unsigned int INTERRUPT_TIME_MSEC = 60 * 1000;
    const std::string FAIL_EVENT_ERROR_CODE = "NOT_SUPPORTED_STATE";
    const std::map<RoutineActivity, std::string> ROUTINE_ACTIVITY_TEXTS {
        { RoutineActivity::IDLE, "IDLE" },
        { RoutineActivity::PLAYING, "PLAYING" },
        { RoutineActivity::INTERRUPTED, "INTERRUPTED" },
        { RoutineActivity::FINISHED, "FINISHED" },
        { RoutineActivity::STOPPED, "STOPPED" },
        { RoutineActivity::SUSPENDED, "SUSPENDED" }
    };

    IRoutineListener* routine_listener = nullptr;
    RoutineActivity activity = RoutineActivity::IDLE;
    std::unique_ptr<INuguTimer> timer = nullptr;
    RoutineInfo routine_info {};
    bool action_timeout_in_last_action = false;
};

} // NuguCapability

#endif /* __NUGU_ROUTINE_AGENT_H__ */
