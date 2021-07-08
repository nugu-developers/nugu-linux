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

#ifndef __SPEECH_OPERATOR_H__
#define __SPEECH_OPERATOR_H__

#include <capability/asr_interface.hh>
#include <clientkit/wakeup_interface.hh>

using namespace NuguClientKit;
using namespace NuguCapability;

class SpeechOperator : public IWakeupListener,
                       public IASRListener {
public:
    virtual ~SpeechOperator() = default;

    IASRListener* getASRListener();
    void setWakeupHandler(IWakeupHandler* wakeup_handler, const std::string& wakeup_word);
    void setASRHandler(IASRHandler* asr_handler);
    void startListeningWithWakeup();
    void startListening(float noise = 0, float speech = 0, ASRInitiator initiator = ASRInitiator::TAP);
    void stopListeningAndWakeup();
    void cancelListening();
    bool changeWakeupWord(const WakeupModelFile& model_file, const std::string& wakeup_word);

    void onWakeupState(WakeupDetectState state, float power_noise, float power_speech) override;
    void onState(ASRState state, const std::string& dialog_id) override;
    void onNone(const std::string& dialog_id) override;
    void onPartial(const std::string& text, const std::string& dialog_id) override;
    void onComplete(const std::string& text, const std::string& dialog_id) override;
    void onError(ASRError error, const std::string& dialog_id, bool listen_timeout_fail_beep) override;
    void onCancel(const std::string& dialog_id) override;

private:
    void startWakeup();
    void stopWakeup();
    void stopListening();

    IWakeupHandler* wakeup_handler = nullptr;
    IASRHandler* asr_handler = nullptr;
    std::string wakeup_word;
};

#endif /* __SPEECH_OPERATOR_H__ */
