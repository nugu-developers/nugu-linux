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

#ifndef __SPEAKER_STATUS_H__
#define __SPEAKER_STATUS_H__

#include <tuple>

class SpeakerStatus {
public:
    // 1:mic mute, 2:speaker mute, 3:volume
    using DefaultValues = std::tuple<bool, bool, int>;

public:
    static SpeakerStatus* getInstance();
    static void destroyInstance();

    SpeakerStatus(const SpeakerStatus&) = delete;
    SpeakerStatus(SpeakerStatus&&) = delete;

    SpeakerStatus& operator=(const SpeakerStatus&) = delete;
    SpeakerStatus& operator=(SpeakerStatus&&) = delete;

    void setDefaultValues(DefaultValues&& values);
    bool isMicMute();
    bool isSpeakerMute();
    void setMicMute(bool mute);
    void setSpeakerMute(bool mute);
    void setNUGUVolume(int volume);
    int getNUGUVolume();

private:
    SpeakerStatus() = default;
    virtual ~SpeakerStatus() = default;

    static SpeakerStatus* instance;

    bool mic_mute = false;
    bool speaker_mute = false;
    int nugu_volume = 0;
};

#endif /* __SPEAKER_STATUS_H__ */
