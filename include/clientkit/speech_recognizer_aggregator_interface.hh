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

#ifndef __SPEECH_RECOGNIZER_AGGREGATOR_INTERFACE_H__
#define __SPEECH_RECOGNIZER_AGGREGATOR_INTERFACE_H__

#include <capability/asr_interface.hh>
#include <clientkit/wakeup_interface.hh>

namespace NuguClientKit {

using namespace NuguCapability;

/**
 * @file speech_recognizer_aggregator_interface.hh
 * @defgroup SpeechRecognizerAggregatorInterface SpeechRecognizerAggregatorInterface
 * @ingroup SDKNuguClientKit
 * @brief SpeechRecognizerAggregator Interface
 *
 * Interface of SpeechRecognizerAggregator for operating wakeup detection and
 * speech recognition process conveniently.
 *
 * @{
 */

/**
 * @brief Model for holding recognition result.
 * @see ISpeechRecognizerAggregatorListener::onResult
 */
typedef struct {
    /**
     * @brief Result status
     */
    enum class Status {
        None, /**< no result */
        Partial, /**< partial text received */
        Complete, /**< complete text received */
        Cancel, /**< recognition canceled  */
        Error /**< error occurred  */
    };

    Status status; /**< result status */
    std::string recognized_text; /**< partial or complete recognized text */
    ASRError error; /**< ASR error */
    bool listen_timeout_fail_beep; /**< flag whether to play fail beep or not when listen-timeout occurred */
} RecognitionResult;

/**
 * @brief SpeechRecognizerAggregator listener interface
 * @see ISpeechRecognizerAggregator
 */
class ISpeechRecognizerAggregatorListener {
public:
    virtual ~ISpeechRecognizerAggregatorListener() = default;

    /**
     * @brief Notify to user the wakeup detection state changed.
     * @param[in] state wakeup detection state
     * @param[in] power_noise min power value
     * @param[in] power_speech max power value
     */
    virtual void onWakeupState(WakeupDetectState state, float power_noise, float power_speech) = 0;

    /**
     * @brief Notify to user the asr state changed.
     * @param[in] state asr state
     * @param[in] dialog_id dialog request id
     * @param[in] initiator asr initiator
     */
    virtual void onASRState(ASRState state, const std::string& dialog_id, ASRInitiator initiator) = 0;

    /**
     * @brief Notify to user the recognition result.
     * @param[in] result recognition result
     * @param[in] dialog_id dialog request id
     */
    virtual void onResult(const RecognitionResult& result, const std::string& dialog_id) = 0;
};

/**
 * @brief SpeechRecognizerAggregator interface
 * @see ISpeechRecognizerAggregatorListener
 */
class ISpeechRecognizerAggregator {
public:
    virtual ~ISpeechRecognizerAggregator() = default;

    /**
     * @brief Add the ISpeechRecognizerAggregatorListener object.
     * @param[in] listener ISpeechRecognizerAggregatorListener object
     */
    virtual void addListener(ISpeechRecognizerAggregatorListener* listener) = 0;

    /**
     * @brief Remove the ISpeechRecognizerAggregatorListener object.
     * @param[in] listener ISpeechRecognizerAggregatorListener object
     */
    virtual void removeListener(ISpeechRecognizerAggregatorListener* listener) = 0;

    /**
     * @brief Set wakeup model file.
     * @param[in] model_file wakeup model file
     */
    virtual void setWakeupModel(const WakeupModelFile& model_file) = 0;

    /**
     * @brief Start detecting wakeup and progress recognizing speech after wakeup detected.
     */
    virtual void startListeningWithTrigger() = 0;

    /**
     * @brief Start recognizing speech.
     * @param[in] power_noise min wakeup power value
     * @param[in] power_speech max wakeup power value
     * @param[in] initiator asr initiator
     */
    virtual void startListening(float power_noise = 0, float power_speech = 0, ASRInitiator initiator = ASRInitiator::TAP) = 0;

    /**
     * @brief Stop both recognizing speech and detecting wakeup.
     * @param[in] cancel if true, cancel the directives to be received for current dialog
     */
    virtual void stopListening(bool cancel = false) = 0;
};

} // NuguClientKit

/**
 * @}
 */

#endif /* __SPEECH_RECOGNIZER_AGGREGATOR_INTERFACE_H__ */
