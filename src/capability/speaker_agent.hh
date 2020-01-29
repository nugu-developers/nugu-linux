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

#include "capability.hh"
#include "capability/speaker_interface.hh"

namespace NuguCapability {

class SpeakerAgent : public Capability, public ISpeakerHandler {
public:
    SpeakerAgent();
    virtual ~SpeakerAgent();

    void parsingDirective(const char* dname, const char* message) override;
    void updateInfoForContext(Json::Value& ctx) override;
    void setCapabilityListener(ICapabilityListener* clistener) override;

    void setSpeakerInfo(std::map<SpeakerType, SpeakerInfo*> info) override;

    void informVolumeSucceeded(SpeakerType type, int volume) override;
    void informVolumeFailed(SpeakerType type, int volume) override;

    void informMuteSucceeded(SpeakerType type, bool mute) override;
    void informMuteFailed(SpeakerType type, bool mute) override;

private:
    void sendEventCommon(const std::string& ename);
    void sendEventSetVolumeSucceeded();
    void sendEventSetVolumeFailed();
    void sendEventSetMuteSucceeded();
    void sendEventSetMuteFailed();

    void parsingSetVolume(const char* message);
    void parsingSetMute(const char* message);

    void updateSpeakerInfo(SpeakerType type, int volume, bool mute);
    bool getSpeakerType(const std::string& name, SpeakerType& type);
    std::string getSpeakerName(SpeakerType& type);

    std::map<SpeakerType, SpeakerInfo*> speakers;
    SpeakerInfo cur_speaker;
    ISpeakerListener* speaker_listener;
    std::string ps_id;
};

} // NuguCore

#endif /* __NUGU_SPEAKER_AGENT_H__ */
