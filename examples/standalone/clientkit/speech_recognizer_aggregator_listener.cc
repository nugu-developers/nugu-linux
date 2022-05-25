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

#include "speech_recognizer_aggregator_listener.hh"

namespace msg {
static const char* C_RED = "\033[1;91m";
static const char* C_YELLOW = "\033[1;93m";
static const char* C_BLUE = "\033[1;94m";
static const char* C_CYAN = "\033[1;96m";
static const char* C_RESET = "\033[0m";

namespace asr {
    const char* TAG = "[ASR]";

    void state(const std::string& dialog_id, const std::string& message)
    {
        std::cout << C_YELLOW << TAG
                  << "[id:" << dialog_id << "] " << message
                  << C_RESET << std::endl;
    }

    void callback(const std::string& func, const std::string& dialog_id, const std::string& message = "")
    {
        std::cout << C_CYAN << TAG
                  << "[id:" << dialog_id << "] " << func
                  << (!message.empty() ? " > " + message : "")
                  << C_RESET << std::endl;
    }
}

namespace wakeup {
    const char* TAG = "[Wakeup] ";

    void state(const std::string& message)
    {
        std::cout << C_BLUE << TAG
                  << message
                  << C_RESET << std::endl;
    }
}

void error(const std::string& message)
{
    std::cout << C_RED
              << message
              << C_RESET << std::endl;
}
}

SpeechRecognizerAggregatorListener::SpeechRecognizerAggregatorListener(const std::string& wakeup_word)
    : wakeup_word(wakeup_word)
{
}

void SpeechRecognizerAggregatorListener::onWakeupState(WakeupDetectState state, float power_noise, float power_speech)
{
    switch (state) {
    case WakeupDetectState::WAKEUP_DETECTING:
        msg::wakeup::state("wakeup detecting (" + wakeup_word + ")...");
        break;
    case WakeupDetectState::WAKEUP_DETECTED:
        msg::wakeup::state("wakeup detected (power: " + std::to_string(power_noise) + ", " + std::to_string(power_speech) + ")");
        break;
    case WakeupDetectState::WAKEUP_FAIL:
        msg::wakeup::state("wakeup fail");
        break;
    }
}

void SpeechRecognizerAggregatorListener::onASRState(ASRState state, const std::string& dialog_id, ASRInitiator initiator)
{
    switch (state) {
    case ASRState::IDLE: {
        msg::asr::state(dialog_id, "IDLE");
        nugu_prof_dump(NUGU_PROF_TYPE_ASR_LISTENING_STARTED, NUGU_PROF_TYPE_ASR_RESULT);
        break;
    }
    case ASRState::EXPECTING_SPEECH: {
        msg::asr::state(dialog_id, "EXPECTING_SPEECH");
        break;
    }
    case ASRState::LISTENING: {
        msg::asr::state(dialog_id, "LISTENING (" + ASR_INITIATOR_TEXTS.at(initiator) + ")");
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

void SpeechRecognizerAggregatorListener::onResult(const RecognitionResult& result, const std::string& dialog_id)
{
    switch (result.status) {
    case RecognitionResult::Status::None:
        msg::asr::callback("onNone", dialog_id);
        break;
    case RecognitionResult::Status::Partial:
        msg::asr::callback("onPartial", dialog_id, result.recognized_text);
        break;
    case RecognitionResult::Status::Complete:
        msg::asr::callback("onComplete", dialog_id, result.recognized_text);
        break;
    case RecognitionResult::Status::Cancel:
        msg::asr::callback("onCancel", dialog_id);
        break;
    case RecognitionResult::Status::Error:
        handleASRError(result.error, dialog_id);
        break;
    }
}

void SpeechRecognizerAggregatorListener::setWakeupWord(const std::string& wakeup_word)
{
    this->wakeup_word = wakeup_word;
}

void SpeechRecognizerAggregatorListener::handleASRError(ASRError error, const std::string& dialog_id)
{
    switch (error) {
    case ASRError::RESPONSE_TIMEOUT: {
        msg::asr::callback("onError", dialog_id, "RESPONSE_TIMEOUT");
        break;
    }
    case ASRError::LISTEN_TIMEOUT: {
        msg::asr::callback("onError", dialog_id, "LISTEN_TIMEOUT");
        break;
    }
    case ASRError::LISTEN_FAILED: {
        msg::asr::callback("onError", dialog_id, "LISTEN_FAILED");
        break;
    }
    case ASRError::RECOGNIZE_ERROR: {
        msg::asr::callback("onError", dialog_id, "RECOGNIZE_ERROR");
        break;
    }
    case ASRError::UNKNOWN: {
        msg::asr::callback("onError", dialog_id, "UNKNOWN");
        break;
    }
    }
    std::cout << msg::C_RESET;
}
