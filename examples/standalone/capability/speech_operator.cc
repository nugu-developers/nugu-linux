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

#include <iostream>

#include <base/nugu_prof.h>

#include "speech_operator.hh"

namespace msg {
const char* C_RED = "\033[1;91m";
const char* C_YELLOW = "\033[1;93m";
const char* C_BLUE = "\033[1;94m";
const char* C_CYAN = "\033[1;96m";
const char* C_WHITE = "\033[1;97m";
const char* C_RESET = "\033[0m";

namespace asr {
    const char* TAG = "[ASR]";

    void state(const std::string& dialog_id, const std::string& message)
    {
        std::cout << C_YELLOW
                  << TAG
                  << "[id:" << dialog_id << "] "
                  << message
                  << C_RESET
                  << std::endl;
    }

    void callback(const std::string& func, const std::string& dialog_id, const std::string& message = "")
    {
        std::cout << C_CYAN
                  << TAG
                  << "[id:" << dialog_id << "] "
                  << func
                  << (!message.empty() ? " > " + message : "")
                  << C_RESET
                  << std::endl;
    }
}

namespace wakeup {
    const char* TAG = "[Wakeup] ";

    void state(const std::string& message)
    {
        std::cout << C_BLUE
                  << TAG
                  << message
                  << C_RESET
                  << std::endl;
    }
}

void error(const std::string& message)
{
    std::cout << C_RED
              << message
              << C_RESET
              << std::endl;
}
}

SpeechOperator::SpeechOperator()
{
}

ASRListener* SpeechOperator::getASRListener()
{
    return this;
}

void SpeechOperator::setWakeupHandler(IWakeupHandler* wakeup_handler, const std::string& wakeup_word)
{
    this->wakeup_handler = wakeup_handler;

    if (this->wakeup_handler)
        wakeup_handler->setListener(this);

    if (!wakeup_word.empty())
        this->wakeup_word = wakeup_word;
}

void SpeechOperator::setCapabilityHandler(ICapabilityInterface* handler)
{
    if (handler)
        this->asr_handler = dynamic_cast<IASRHandler*>(handler);
}

void SpeechOperator::startListeningWithWakeup()
{
    stopListeningAndWakeup();
    startWakeup();
}

void SpeechOperator::startListening(float noise, float speech, ASRInitiator initiator)
{
    stopListening();

    if (!asr_handler) {
        msg::error("It's fail to start speech recognition.");
        return;
    }

    if (noise && speech)
        asr_handler->startRecognition(noise, speech, initiator);
    else
        asr_handler->startRecognition(initiator);
}

void SpeechOperator::stopListeningAndWakeup()
{
    stopListening();
    stopWakeup();
}

bool SpeechOperator::changeWakeupWord(const WakeupModelFile& model_file, const std::string& wakeup_word)
{
    if (!wakeup_handler || model_file.net.empty()
        || model_file.search.empty() || wakeup_word.empty())
        return false;

    stopListening();

    wakeup_handler->changeModel(model_file);
    this->wakeup_word = wakeup_word;

    msg::wakeup::state("Wakeup word is changed to " + wakeup_word);

    return true;
}

void SpeechOperator::startWakeup()
{
    if (!wakeup_handler) {
        msg::error("It's fail to start wakeup detection.");
        return;
    }

    wakeup_handler->startWakeup();
}

void SpeechOperator::stopWakeup()
{
    if (wakeup_handler)
        wakeup_handler->stopWakeup();
}

void SpeechOperator::stopListening()
{
    if (!asr_handler) {
        msg::error("It's fail to stop speech recognition.");
        return;
    }

    asr_handler->stopRecognition();
}

void SpeechOperator::cancelListening()
{
    if (!asr_handler) {
        msg::error("It's fail to cancel speech recognition.");
        return;
    }

    asr_handler->stopRecognition(true);
}

void SpeechOperator::onWakeupState(WakeupDetectState state, float power_noise, float power_speech)
{
    switch (state) {
    case WakeupDetectState::WAKEUP_DETECTING:
        msg::wakeup::state("wakeup detecting (" + wakeup_word + ")...");
        break;
    case WakeupDetectState::WAKEUP_DETECTED:
        msg::wakeup::state("wakeup detected (power: " + std::to_string(power_noise) + ", " + std::to_string(power_speech) + ")");

        stopWakeup();
        startListening(power_noise, power_speech, ASRInitiator::WAKE_UP_WORD);

        break;
    case WakeupDetectState::WAKEUP_FAIL:
        msg::wakeup::state("wakeup fail");
        break;
    }
}

void SpeechOperator::onState(ASRState state, const std::string& dialog_id, ASRInitiator initiator)
{
    switch (state) {
    case ASRState::IDLE: {
        msg::asr::state(dialog_id, "IDLE");
        nugu_prof_dump(NUGU_PROF_TYPE_ASR_LISTENING_STARTED, NUGU_PROF_TYPE_ASR_RESULT);
        break;
    }
    case ASRState::EXPECTING_SPEECH: {
        msg::wakeup::state("stop wakeup");
        stopWakeup();

        msg::asr::state(dialog_id, "EXPECTING_SPEECH");
        break;
    }
    case ASRState::LISTENING: {
        msg::asr::state(dialog_id, "LISTENING (" + ASR_INITIATOR_TEXTS.at(initiator) + ")");
        // need to stop wakeup when asr state is changed from idle to listening
        // without wakeup detected
        stopWakeup();
        break;
    }
    case ASRState::RECOGNIZING: {
        msg::asr::state(dialog_id, "RECOGNIZING");
        break;
    }
    case ASRState::BUSY: {
        msg::asr::state(dialog_id, "BUSY");
        break;
    }
    }
}

void SpeechOperator::onNone(const std::string& dialog_id)
{
    msg::asr::callback(__FUNCTION__, dialog_id);
}

void SpeechOperator::onPartial(const std::string& text, const std::string& dialog_id)
{
    msg::asr::callback(__FUNCTION__, dialog_id, text);
}

void SpeechOperator::onComplete(const std::string& text, const std::string& dialog_id)
{
    msg::asr::callback(__FUNCTION__, dialog_id, text);
}

void SpeechOperator::onError(ASRError error, const std::string& dialog_id, bool listen_timeout_fail_beep)
{
    switch (error) {
    case ASRError::RESPONSE_TIMEOUT: {
        msg::asr::callback(__FUNCTION__, dialog_id, "RESPONSE_TIMEOUT");
        break;
    }
    case ASRError::LISTEN_TIMEOUT: {
        msg::asr::callback(__FUNCTION__, dialog_id, "LISTEN_TIMEOUT");
        break;
    }
    case ASRError::LISTEN_FAILED: {
        msg::asr::callback(__FUNCTION__, dialog_id, "LISTEN_FAILED");
        break;
    }
    case ASRError::RECOGNIZE_ERROR: {
        msg::asr::callback(__FUNCTION__, dialog_id, "RECOGNIZE_ERROR");
        break;
    }
    case ASRError::UNKNOWN: {
        msg::asr::callback(__FUNCTION__, dialog_id, "UNKNOWN");
        break;
    }
    }

    std::cout << msg::C_RESET;
}

void SpeechOperator::onCancel(const std::string& dialog_id)
{
    msg::asr::callback(__FUNCTION__, dialog_id);
}
