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

#include "speech_operator.hh"

namespace msg {
const std::string C_RED = "\033[1;91m";
const std::string C_YELLOW = "\033[1;93m";
const std::string C_BLUE = "\033[1;94m";
const std::string C_CYAN = "\033[1;96m";
const std::string C_WHITE = "\033[1;97m";
const std::string C_RESET = "\033[0m";

namespace asr {
    const std::string TAG = "[ASR] ";

    void state(const std::string& message)
    {
        std::cout << C_YELLOW
                  << TAG
                  << message
                  << C_RESET
                  << std::endl;
    }

    void callback(const std::string& func, const std::string& message = "")
    {
        std::cout << C_CYAN
                  << TAG
                  << func
                  << (!message.empty() ? " > " + message : "")
                  << C_RESET
                  << std::endl;
    }
}

namespace wakeup {
    const std::string TAG = "[Wakeup] ";

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

void SpeechOperator::onWakeupState(WakeupDetectState state)
{
    switch (state) {
    case WakeupDetectState::WAKEUP_DETECTING:
        msg::wakeup::state("wakeup detecting...");
        break;
    case WakeupDetectState::WAKEUP_DETECTED:
        msg::wakeup::state("wakeup detected");

        if (asr_handler)
            asr_handler->startRecognition();

        break;
    case WakeupDetectState::WAKEUP_FAIL:
        msg::wakeup::state("wakeup fail");
        break;
    }
}

IASRListener* SpeechOperator::getASRListener()
{
    return this;
}

void SpeechOperator::setWakeupHandler(IWakeupHandler* wakeup_handler)
{
    this->wakeup_handler = wakeup_handler;

    if (this->wakeup_handler)
        wakeup_handler->setListener(this);
}

void SpeechOperator::setASRHandler(IASRHandler* asr_handler)
{
    this->asr_handler = asr_handler;
}

void SpeechOperator::startWakeup()
{
    if (!wakeup_handler) {
        msg::error("It's fail to start wakeup detection.");
        return;
    }

    wakeup_handler->startWakeup();
}

void SpeechOperator::startListening()
{
    if (!asr_handler) {
        msg::error("It's fail to start speech recognition.");
        return;
    }

    asr_handler->startRecognition();
}

void SpeechOperator::stopListening()
{
    if (!asr_handler) {
        msg::error("It's fail to start speech recognition.");
        return;
    }

    asr_handler->stopRecognition();
}

void SpeechOperator::onState(ASRState state)
{
    switch (state) {
    case ASRState::IDLE: {
        msg::asr::state("IDLE");

        if (wakeup_handler)
            wakeup_handler->startWakeup();

        break;
    }
    case ASRState::EXPECTING_SPEECH: {
        msg::asr::state("EXPECTING_SPEECH");
        break;
    }
    case ASRState::LISTENING: {
        msg::asr::state("LISTENING");
        break;
    }
    case ASRState::RECOGNIZING: {
        msg::asr::state("RECOGNIZING");
        break;
    }
    case ASRState::BUSY: {
        msg::asr::state("BUSY");
        break;
    }
    }
}

void SpeechOperator::onNone()
{
    msg::asr::callback(__FUNCTION__);
}

void SpeechOperator::onPartial(const std::string& text)
{
    msg::asr::callback(__FUNCTION__, text);
}

void SpeechOperator::onComplete(const std::string& text)
{
    msg::asr::callback(__FUNCTION__, text);
}

void SpeechOperator::onError(ASRError error)
{
    switch (error) {
    case ASRError::RESPONSE_TIMEOUT: {
        msg::asr::callback(__FUNCTION__, "RESPONSE_TIMEOUT");
        break;
    }
    case ASRError::LISTEN_TIMEOUT: {
        msg::asr::callback(__FUNCTION__, "LISTEN_TIMEOUT");
        break;
    }
    case ASRError::LISTEN_FAILED: {
        msg::asr::callback(__FUNCTION__, "LISTEN_FAILED");
        break;
    }
    case ASRError::RECOGNIZE_ERROR: {
        msg::asr::callback(__FUNCTION__, "RECOGNIZE_ERROR");
        break;
    }
    case ASRError::UNKNOWN: {
        msg::asr::callback(__FUNCTION__, "UNKNOWN");
        break;
    }
    }

    std::cout << msg::C_RESET;
}

void SpeechOperator::onCancel()
{
    msg::asr::callback(__FUNCTION__);
}
