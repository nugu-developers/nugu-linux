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

#ifndef __NUGU_WAKEUP_DETECTOR_H__
#define __NUGU_WAKEUP_DETECTOR_H__

#include "audio_input_processor.hh"

#define WAKEUP_NET_MODEL_FILE "nugu_model_wakeup_net.raw"
#define WAKEUP_SEARCH_MODEL_FILE "nugu_model_wakeup_search.raw"

#define POWER_SPEECH_PERIOD 7 // (140ms * 10 = 1400 ms) / 200ms
#define POWER_NOISE_PERIOD 35 // (140ms * 50 = 70000 ms) / 200ms

namespace NuguCore {

enum class WakeupState {
    FAIL,
    DETECTING,
    DETECTED,
    DONE
};

class IWakeupDetectorListener {
public:
    virtual ~IWakeupDetectorListener() = default;

    /* The callback is invoked in the main context. */
    virtual void onWakeupState(WakeupState state, const std::string& id, unsigned int noise = 0, unsigned int speech = 0) = 0;
};

class WakeupDetector : public AudioInputProcessor {
public:
    using Attribute = struct {
        std::string sample;
        std::string format;
        std::string channel;
        std::string model_path;
    };

public:
    WakeupDetector();
    WakeupDetector(Attribute&& attribute);
    virtual ~WakeupDetector();

    void setListener(IWakeupDetectorListener* listener);
    bool startWakeup(const std::string& id);
    void stopWakeup();

private:
    void initialize(Attribute&& attribute);
    void loop() override;
    void sendWakeupEvent(WakeupState state, const std::string& id, unsigned int noise = 0, unsigned int speech = 0);
    void setPower(unsigned int power);
    void getPower(unsigned int& noise, unsigned int& speech);

    IWakeupDetectorListener* listener = nullptr;

    // attribute
    std::string model_path = "";
    unsigned int power_speech[POWER_SPEECH_PERIOD];
    unsigned int power_noise[POWER_NOISE_PERIOD];
    int power_index = 0;
};

} // NuguCore

#endif /* __NUGU_WAKEUP_DETECTOR_H__ */
