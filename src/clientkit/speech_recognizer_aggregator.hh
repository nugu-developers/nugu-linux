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

#ifndef __SPEECH_RECOGNIZER_AGGREGATOR_H__
#define __SPEECH_RECOGNIZER_AGGREGATOR_H__

#include "nugu.h"
#include "clientkit/speech_recognizer_aggregator_interface.hh"

namespace NuguClientKit {

class NUGU_API SpeechRecognizerAggregator : public ISpeechRecognizerAggregator,
                                   public IWakeupListener,
                                   public IASRListener {
public:
    SpeechRecognizerAggregator();
    virtual ~SpeechRecognizerAggregator();

    void setASRHandler(IASRHandler* asr_handler);
    const std::shared_ptr<IWakeupHandler>& getWakeupHandler() const;
    void reset();

    // implements ISpeechRecognizerAggregator
    void addListener(ISpeechRecognizerAggregatorListener* listener) override;
    void removeListener(ISpeechRecognizerAggregatorListener* listener) override;
    void setWakeupHandler(const std::shared_ptr<IWakeupHandler>& wakeup_handler) override;
    bool setWakeupModel(const WakeupModelFile& model_file) override;
    void startListeningWithTrigger() override;
    void startListening(float power_noise = 0, float power_speech = 0, ASRInitiator initiator = ASRInitiator::TAP) override;
    void stopListening(bool cancel = false) override;
    void finishListening() override;

    // implements IWakeupListener, IASRListener
    void onWakeupState(WakeupDetectState state, float power_noise, float power_speech) override;
    void onState(ASRState state, const std::string& dialog_id, ASRInitiator initiator) override;
    void onNone(const std::string& dialog_id) override;
    void onPartial(const std::string& text, const std::string& dialog_id) override;
    void onComplete(const std::string& text, const std::string& dialog_id) override;
    void onError(ASRError error, const std::string& dialog_id, bool listen_timeout_fail_beep = true) override;
    void onCancel(const std::string& dialog_id) override;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl;
};

} // NuguClientKit

#endif /* __SPEECH_RECOGNIZER_AGGREGATOR_H__ */
