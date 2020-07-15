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

private:
    // implements IRoutineManagerListener
    void onActivity(RoutineActivity activity) override;

    void handleInterrupted();
    void clearRoutineInfo();

    void parsingStart(const char* message);
    void parsingStop(const char* message);
    void parsingContinue(const char* message);

    void sendEventStarted(EventResultCallback cb = nullptr);
    void sendEventFailed(EventResultCallback cb = nullptr);
    void sendEventFinished(EventResultCallback cb = nullptr);
    void sendEventStopped(EventResultCallback cb = nullptr);
    void sendEventCommon(std::string&& event_name, Json::Value&& extra_value, EventResultCallback cb = nullptr, bool clear_routine_info = true);
    std::string sendEventActionTriggered(const std::string& ps_id, const Json::Value& data, EventResultCallback cb = nullptr);

    const unsigned int INTERRUPT_TIME_MSEC = 60 * 1000;
    const std::string FAIL_EVENT_ERROR_CODE = "NOT_SUPPORTED_STATE";

    std::map<RoutineActivity, std::string> routine_activity_texts;
    IRoutineListener* routine_listener = nullptr;
    RoutineActivity activity = RoutineActivity::IDLE;
    std::unique_ptr<INuguTimer> timer = nullptr;
    Json::Value actions = Json::arrayValue;
    std::string play_service_id;
    std::string token;
};

} // NuguCapability

#endif /* __NUGU_ROUTINE_AGENT_H__ */
