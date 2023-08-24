/*
 * Copyright (c) 2021 SK Telecom Co., Ltd. All rights reserved.
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

#ifndef __NUGU_PHONE_CALL_AGENT_H__
#define __NUGU_PHONE_CALL_AGENT_H__

#include "capability/phone_call_interface.hh"
#include "clientkit/capability.hh"

namespace NuguCapability {

class PhoneCallAgent final : public Capability,
                             public IPhoneCallHandler,
                             public IFocusResourceListener,
                             public IPlaySyncManagerListener {
public:
    PhoneCallAgent();
    virtual ~PhoneCallAgent() = default;

    void initialize() override;
    void deInitialize() override;
    void parsingDirective(const char* dname, const char* message) override;
    void updateInfoForContext(NJson::Value& ctx) override;
    void setCapabilityListener(ICapabilityListener* clistener) override;

    // implements IPhoneCallHandler
    void candidatesListed(const std::string& context_template, const std::string& payload) override;
    void callArrived(const std::string& payload) override;
    void callEnded(const std::string& payload) override;
    void callEstablished(const std::string& payload) override;
    void makeCallSucceeded(const std::string& payload) override;
    void makeCallFailed(const std::string& payload) override;
    void setNumberBlockable(bool flag) override;

    // implements IFocusResourceListener
    void onFocusChanged(FocusState state) override;

    // implements IPlaySyncManagerListener
    void onSyncState(const std::string& ps_id, PlaySyncState state, void* extra_data) override;
    void onDataChanged(const std::string& ps_id, std::pair<void*, void*> extra_datas) override;
    void onStackChanged(const std::pair<std::string, std::string>& ps_ids) override;

private:
    void parsingSendCandidates(const char* message);
    void parsingMakeCall(const char* message);
    void parsingEndCall(const char* message);
    void parsingAcceptCall(const char* message);
    void parsingBlockIncomingCall(const char* message);
    void parsingBlockNumber(const char* message);

    void setState(PhoneCallState state);
    std::string getStateStr(PhoneCallState state);

    IPhoneCallListener* phone_call_listener;
    PhoneCallState cur_state;
    FocusState focus_state;
    std::string context_template;
    std::string context_recipient;
    std::string playstackctl_ps_id;
    NJson::Value interaction_control_payload;
    bool blockable;
};

} // NuguCapability

#endif /* __NUGU_PHONE_CALL_AGENT_H__ */
