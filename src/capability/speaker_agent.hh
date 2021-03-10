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

#ifndef __NUGU_SPEAKER_AGENT_H__
#define __NUGU_SPEAKER_AGENT_H__

#include "capability/speaker_interface.hh"
#include "clientkit/capability.hh"

namespace NuguCapability {

class SpeakerAgent final : public Capability,
                           public ISpeakerHandler {
public:
    SpeakerAgent();
    virtual ~SpeakerAgent() = default;

    void initialize() override;
    void deInitialize() override;

    void parsingDirective(const char* dname, const char* message) override;
    void updateInfoForContext(Json::Value& ctx) override;
    void setCapabilityListener(ICapabilityListener* clistener) override;
    bool getProperty(const std::string& property, std::string& value) override;

    void setSpeakerInfo(const std::map<SpeakerType, SpeakerInfo>& info) override;

    void informVolumeChanged(SpeakerType type, int volume) override;
    void informMuteChanged(SpeakerType type, bool mute) override;

    void sendEventVolumeChanged(const std::string& ps_id, bool result) override;
    void sendEventMuteChanged(const std::string& ps_id, bool result) override;

private:
    void sendEventCommon(const std::string& ps_id, const std::string& ename, EventResultCallback cb = nullptr);
    void sendEventSetVolumeSucceeded(const std::string& ps_id, EventResultCallback cb = nullptr);
    void sendEventSetVolumeFailed(const std::string& ps_id, EventResultCallback cb = nullptr);
    void sendEventSetMuteSucceeded(const std::string& ps_id, EventResultCallback cb = nullptr);
    void sendEventSetMuteFailed(const std::string& ps_id, EventResultCallback cb = nullptr);

    void parsingSetVolume(const char* message);
    void parsingSetMute(const char* message);

    void updateSpeakerVolume(SpeakerType type, int volume);
    void updateSpeakerMute(SpeakerType type, bool mute);
    bool getSpeakerType(const std::string& name, SpeakerType& type) noexcept;
    std::string getSpeakerName(const SpeakerType& type) noexcept;

    std::map<SpeakerType, std::unique_ptr<SpeakerInfo>> speakers;
    std::map<SpeakerType, std::string> speaker_names_for_types;
    std::map<std::string, SpeakerType> speaker_types_for_names;
    ISpeakerListener* speaker_listener;
};

} // NuguCore

#endif /* __NUGU_SPEAKER_AGENT_H__ */
