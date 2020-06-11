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

#ifndef __NUGU_SOUND_AGENT_H__
#define __NUGU_SOUND_AGENT_H__

#include <map>

#include "capability/sound_interface.hh"
#include "clientkit/capability.hh"

namespace NuguCapability {

class SoundAgent final : public Capability,
                         public ISoundHandler {
public:
    SoundAgent();
    virtual ~SoundAgent() = default;

    void initialize() override;
    void deInitialize() override;

    void setCapabilityListener(ICapabilityListener* clistener) override;
    void updateInfoForContext(Json::Value& ctx) override;
    void parsingDirective(const char* dname, const char* message) override;
    void sendBeepResult(bool is_succeeded) override;

private:
    void parsingBeep(const char* message);
    void sendEventBeepSucceeded(EventResultCallback cb = nullptr);
    void sendEventBeepFailed(EventResultCallback cb = nullptr);
    void sendEventCommon(std::string&& event_name, EventResultCallback cb = nullptr);

    const std::map<std::string, BeepType> BEEP_TYPE_MAP {
        { "RESPONSE_FAIL", BeepType::RESPONSE_FAIL }
    };

    ISoundListener* sound_listener = nullptr;
    std::string play_service_id = "";
};

} // NuguCapability

#endif /* __NUGU_SOUND_AGENT_H__ */
