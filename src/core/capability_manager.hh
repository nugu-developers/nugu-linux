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

#ifndef __NUGU_CAPABILITY_AGENT_H__
#define __NUGU_CAPABILITY_AGENT_H__

#include <map>
#include <memory>

#include "base/nugu_event.h"
#include "clientkit/capability_interface.hh"

#include "directive_sequencer.hh"
#include "focus_manager.hh"
#include "interaction_control_manager.hh"
#include "playsync_manager.hh"
#include "routine_manager.hh"
#include "session_manager.hh"

namespace NuguCore {

class CapabilityManager : public INetworkManagerListener,
                          public IDirectiveSequencerListener {
private:
    CapabilityManager();
    virtual ~CapabilityManager();

public:
    static CapabilityManager* getInstance();
    static void destroyInstance();
    void resetInstance();

    PlaySyncManager* getPlaySyncManager();
    FocusManager* getFocusManager();
    SessionManager* getSessionManager();
    InteractionControlManager* getInteractionControlManager();
    DirectiveSequencer* getDirectiveSequencer();
    RoutineManager* getRoutineManager();

    void addCapability(const std::string& cname, ICapabilityInterface* cap);
    void removeCapability(const std::string& cname);

    void requestEventResult(NuguEvent* event);

    // overriding INetworkManagerListener
    void onEventSendResult(const char* msg_id, bool success, int code) override;
    void onEventResponse(const char* msg_id, const char* data, bool success) noexcept override;

    void setWakeupWord(const std::string& word);
    std::string getWakeupWord();

    std::string makeContextInfo(const std::string& cname, Json::Value& cap_ctx);
    std::string makeAllContextInfo();

    bool isSupportDirectiveVersion(const std::string& version, ICapabilityInterface* cap);

    bool sendCommand(const std::string& from, const std::string& to, const std::string& command, const std::string& param);
    void sendCommandAll(const std::string& command, const std::string& param);
    bool getCapabilityProperty(const std::string& cap, const std::string& property, std::string& value);
    bool getCapabilityProperties(const std::string& cap, const std::string& property, std::list<std::string>& values);
    void suspendAll();
    void restoreAll();

    // overriding IDirectiveSequencerListener
    bool onPreHandleDirective(NuguDirective* ndir) override;
    bool onHandleDirective(NuguDirective* ndir) override;
    void onCancelDirective(NuguDirective* ndir) override;

private:
    ICapabilityInterface* findCapability(const std::string& cname);
    Json::Value getBaseContextInfo(Json::Value& supported_interfaces, Json::Value&& playstack);

    static CapabilityManager* instance;
    std::map<std::string, ICapabilityInterface*> caps;
    std::map<std::string, std::string> events;
    std::map<std::string, std::string> events_cname_map;
    std::string wword;
    std::unique_ptr<PlaySyncManager> playsync_manager;
    std::unique_ptr<FocusManager> focus_manager;
    std::unique_ptr<SessionManager> session_manager;
    std::unique_ptr<DirectiveSequencer> directive_sequencer;
    std::unique_ptr<InteractionControlManager> interaction_control_manager;
    std::unique_ptr<RoutineManager> routine_manager;
};

} // NuguCore

#endif
