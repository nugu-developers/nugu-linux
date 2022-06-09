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

#include "base/nugu_log.h"

#include "speech_recognizer_aggregator.hh"

namespace NuguClientKit {

struct SpeechRecognizerAggregator::Impl {
    std::shared_ptr<IWakeupHandler> wakeup_handler = nullptr;
    IASRHandler* asr_handler = nullptr;
    std::set<ISpeechRecognizerAggregatorListener*> listeners;

    void notifyResult(const RecognitionResult& result, const std::string& dialog_id);
    void notifyWakeupState(WakeupDetectState state, float power_noise, float power_speech);
    void notifyASRState(ASRState state, const std::string& dialog_id, ASRInitiator initiator);
};

/*******************************************************************************
 * define SpeechRecognizerAggregator
 ******************************************************************************/

SpeechRecognizerAggregator::SpeechRecognizerAggregator()
    : pimpl(std::unique_ptr<Impl>(new Impl()))
{
}

SpeechRecognizerAggregator::~SpeechRecognizerAggregator()
{
}

void SpeechRecognizerAggregator::setASRHandler(IASRHandler* asr_handler)
{
    if (asr_handler) {
        pimpl->asr_handler = asr_handler;
        pimpl->asr_handler->addListener(this);
    }
}

const std::shared_ptr<IWakeupHandler>& SpeechRecognizerAggregator::getWakeupHandler() const
{
    return pimpl->wakeup_handler;
}

void SpeechRecognizerAggregator::reset()
{
    if (pimpl->wakeup_handler)
        pimpl->wakeup_handler->stopWakeup();
}

void SpeechRecognizerAggregator::addListener(ISpeechRecognizerAggregatorListener* listener)
{
    if (listener)
        pimpl->listeners.emplace(listener);
}

void SpeechRecognizerAggregator::removeListener(ISpeechRecognizerAggregatorListener* listener)
{
    pimpl->listeners.erase(listener);
}

void SpeechRecognizerAggregator::setWakeupHandler(const std::shared_ptr<IWakeupHandler>& wakeup_handler)
{
    if (wakeup_handler) {
        pimpl->wakeup_handler = wakeup_handler;
        pimpl->wakeup_handler->setListener(this);
    }
}

bool SpeechRecognizerAggregator::setWakeupModel(const WakeupModelFile& model_file)
{
    if (model_file.net.empty() || model_file.search.empty() || !pimpl->wakeup_handler || !pimpl->asr_handler) {
        nugu_error("It's failed to change wakeup model.");
        return false;
    }

    pimpl->asr_handler->stopRecognition();
    pimpl->wakeup_handler->changeModel(model_file);

    return true;
}

void SpeechRecognizerAggregator::startListeningWithTrigger()
{
    if (!pimpl->wakeup_handler || !pimpl->asr_handler) {
        nugu_error("The wakeup or asr handler is not prepared.");
        return;
    }

    stopListening();
    pimpl->wakeup_handler->startWakeup();
}

void SpeechRecognizerAggregator::startListening(float power_noise, float power_speech, ASRInitiator initiator)
{
    if (!pimpl->asr_handler) {
        nugu_error("The asr handler is not prepared.");
        return;
    }

    stopListening();

    if (power_noise && power_speech)
        pimpl->asr_handler->startRecognition(power_noise, power_speech, initiator);
    else
        pimpl->asr_handler->startRecognition(initiator);
}

void SpeechRecognizerAggregator::stopListening(bool cancel)
{
    if (!pimpl->asr_handler) {
        nugu_error("The asr handler is not prepared.");
        return;
    }

    pimpl->asr_handler->stopRecognition(cancel);

    if (pimpl->wakeup_handler)
        pimpl->wakeup_handler->stopWakeup();
}

void SpeechRecognizerAggregator::onWakeupState(WakeupDetectState state, float power_noise, float power_speech)
{
    pimpl->notifyWakeupState(state, power_noise, power_speech);

    if (state == WakeupDetectState::WAKEUP_DETECTED)
        startListening(power_noise, power_speech, ASRInitiator::WAKE_UP_WORD);
}

void SpeechRecognizerAggregator::onState(ASRState state, const std::string& dialog_id, ASRInitiator initiator)
{
    pimpl->notifyASRState(state, dialog_id, initiator);
}

void SpeechRecognizerAggregator::onNone(const std::string& dialog_id)
{
    pimpl->notifyResult({ RecognitionResult::Status::None }, dialog_id);
}

void SpeechRecognizerAggregator::onPartial(const std::string& text, const std::string& dialog_id)
{
    RecognitionResult result { RecognitionResult::Status::Partial };
    result.recognized_text = text;

    pimpl->notifyResult(result, dialog_id);
}

void SpeechRecognizerAggregator::onComplete(const std::string& text, const std::string& dialog_id)
{
    RecognitionResult result { RecognitionResult::Status::Complete };
    result.recognized_text = text;

    pimpl->notifyResult(result, dialog_id);
}

void SpeechRecognizerAggregator::onError(ASRError error, const std::string& dialog_id, bool listen_timeout_fail_beep)
{
    RecognitionResult result { RecognitionResult::Status::Error };
    result.error = error;
    result.listen_timeout_fail_beep = listen_timeout_fail_beep;

    pimpl->notifyResult(result, dialog_id);
}

void SpeechRecognizerAggregator::onCancel(const std::string& dialog_id)
{
    pimpl->notifyResult({ RecognitionResult::Status::Cancel }, dialog_id);
}

/*******************************************************************************
 * define Impl
 ******************************************************************************/

void SpeechRecognizerAggregator::Impl::notifyResult(const RecognitionResult& result, const std::string& dialog_id)
{
    for (const auto& listener : listeners)
        listener->onResult(result, dialog_id);
}

void SpeechRecognizerAggregator::Impl::notifyWakeupState(WakeupDetectState state, float power_noise, float power_speech)
{
    for (const auto& listener : listeners)
        listener->onWakeupState(state, power_noise, power_speech);
}

void SpeechRecognizerAggregator::Impl::notifyASRState(ASRState state, const std::string& dialog_id, ASRInitiator initiator)
{
    for (const auto& listener : listeners)
        listener->onASRState(state, dialog_id, initiator);
}

} // NuguClientKit
