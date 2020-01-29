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

#ifndef __NUGU_MIC_AGENT_H__
#define __NUGU_MIC_AGENT_H__

#include "interface/capability/mic_interface.hh"

#include "capability.hh"

namespace NuguCore {

using namespace NuguInterface;

class MicAgent : public Capability, public IMicHandler {
public:
    MicAgent();
    virtual ~MicAgent();

    void parsingDirective(const char* dname, const char* message) override;
    void updateInfoForContext(Json::Value& ctx) override;
    void setCapabilityListener(ICapabilityListener* clistener) override;
    void enable() override;
    void disable() override;

private:
    void sendEventCommon(const std::string& ename);
    void sendEventSetMicSucceeded();
    void sendEventSetMicFailed();

    void control(bool enable, bool send_event = false);

    void parsingSetMic(const char* message);

    IMicListener* mic_listener;
    MicStatus cur_status;
    MicStatus pre_status;
    std::string ps_id;
};

} // NuguCore

#endif /* __NUGU_MIC_AGENT_H__ */
