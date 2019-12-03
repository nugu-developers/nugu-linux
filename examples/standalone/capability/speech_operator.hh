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

#include <interface/capability/asr_interface.hh>
#include <interface/wakeup_interface.hh>

using namespace NuguInterface;

class SpeechOperator : public IWakeupListener,
                       public IASRListener {

public:
    void onWakeupState(WakeupDetectState state) override;
    void onState(ASRState state) override;
    void onNone() override;
    void onPartial(const std::string& text) override;
    void onComplete(const std::string& text) override;
    void onError(ASRError error) override;
    void onCancel() override;

    IASRListener* getASRListener();
    void setWakeupHandler(IWakeupHandler* wakeup_handler);
    void setASRHandler(IASRHandler* asr_handler);
    void startWakeup();
    void startListening();
    void stopListening();

private:
    IWakeupHandler* wakeup_handler = nullptr;
    IASRHandler* asr_handler = nullptr;
};

#endif /* __SPEECH_OPERATOR_H__ */
