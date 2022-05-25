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

#ifndef __SPEECH_RECOGNIZER_AGGREGATOR_LISTENER_H__
#define __SPEECH_RECOGNIZER_AGGREGATOR_LISTENER_H__

#include <clientkit/speech_recognizer_aggregator_interface.hh>

using namespace NuguClientKit;

class SpeechRecognizerAggregatorListener : public ISpeechRecognizerAggregatorListener {
public:
    explicit SpeechRecognizerAggregatorListener(const std::string& wakeup_word);

    void onWakeupState(WakeupDetectState state, float power_noise, float power_speech) override;
    void onASRState(ASRState state, const std::string& dialog_id, ASRInitiator initiator) override;
    void onResult(const RecognitionResult& result, const std::string& dialog_id) override;

    void setWakeupWord(const std::string& wakeup_word);

private:
    void handleASRError(ASRError error, const std::string& dialog_id);

    const std::map<ASRInitiator, std::string> ASR_INITIATOR_TEXTS {
        { ASRInitiator::WAKE_UP_WORD, "WAKE_UP_WORD" },
        { ASRInitiator::PRESS_AND_HOLD, "PRESS_AND_HOLD" },
        { ASRInitiator::TAP, "TAP" },
        { ASRInitiator::EXPECT_SPEECH, "EXPECT_SPEECH" },
        { ASRInitiator::EARSET, "EARSET" }
    };

    std::string wakeup_word;
};

#endif /* __SPEECH_RECOGNIZER_AGGREGATOR_LISTENER_H__ */
