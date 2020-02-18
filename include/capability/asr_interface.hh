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

#ifndef __NUGU_ASR_INTERFACE_H__
#define __NUGU_ASR_INTERFACE_H__

#include <clientkit/capability_interface.hh>

namespace NuguCapability {

using namespace NuguClientKit;

/**
 * @file asr_interface.hh
 * @defgroup ASRInterface ASRInterface
 * @ingroup SDKNuguCapability
 * @brief ASR capability interface
 *
 * ASR (AutomaticSpeechRecognition) is responsible for recording the audio
 * and delivering it to the server and receiving the result of speech recognition.
 *
 * @{
 */

#define NUGU_ASR_EPD_TYPE "CLIENT"
#define NUGU_ASR_ENCODING "COMPLETE"
#define NUGU_SERVER_RESPONSE_TIMEOUT_SEC 10

/**
 * @brief ASR state list
 * @see IASRListener::onState
 */
enum class ASRState {
    IDLE, /**< No interaction with the user */
    EXPECTING_SPEECH, /**< Received ExpectSpeech Directive */
    LISTENING, /**< The microphone is opened to listen to the user's speech */
    RECOGNIZING, /**< Initiated by sending a Recognize Event and continued streaming User speech is being input */
    BUSY /**< Waiting for response after streaming */
};

/**
 * @brief ASR error list
 * @see IASRListener::onError
 */
enum class ASRError {
    RESPONSE_TIMEOUT, /**< This event occurs when audio streaming is sent but there is no response from the server for a certain time. */
    LISTEN_TIMEOUT, /**< This event occurs when the microphone is opened but there is no speech from the user for a certain time. */
    LISTEN_FAILED, /**< This event occurs when an error occurs inside the audio recognizer module. */
    RECOGNIZE_ERROR, /**< This event occurs when a recognition error occurs from the server. */
    UNKNOWN /**< Unknown  */
};

/**
 * @brief Attributes for setting ASR options.
 * @see IASRHandler::setAttribute
 */
typedef struct {
    std::string model_path; /**< Epd model file path */
    std::string epd_type; /**< Epd type : CLIENT, SERVER */
    std::string asr_encoding; /**< Asr encoding type : PARTIAL, COMPLETE */
    int response_timeout; /**< Server response timeout about speech */
} ASRAttribute;

/**
 * @brief ASR listener interface
 * @see IASRHandler
 */
class IASRListener : public ICapabilityListener {
public:
    virtual ~IASRListener() = default;

    /**
     * @brief Report to the user asr state changed.
     * @param[in] state asr state
     * @see IASRHandler::startRecognition()
     * @see IASRHandler::stopRecognition()
     */
    virtual void onState(ASRState state) = 0;

    /**
     * @brief No speech recognition results.
     */
    virtual void onNone() = 0;

    /**
     * @brief The result of recognizing the user's speech in real time.
     * @param[in] text Speech recognition result
     */
    virtual void onPartial(const std::string& text) = 0;

    /**
     * @brief Speech recognition results which are reported naturally in situations based on the entire speech.
     * @param[in] text Speech recognition result
     */
    virtual void onComplete(const std::string& text) = 0;

    /**
     * @brief Report an error occurred during speech recognition to the user.
     * @param[in] error ASR error
     */
    virtual void onError(ASRError error) = 0;

    /**
     * @brief Speech recognition is canceled.
     */
    virtual void onCancel() = 0;
};

/**
 * @brief ASR handler interface
 * @see IASRListener
 */
class IASRHandler : virtual public ICapabilityInterface {
public:
    virtual ~IASRHandler() = default;

    /**
     * @brief Turn on the microphone and start speech recognition
     */
    virtual void startRecognition(void) = 0;

    /**
     * @brief Turn off the microphone and stop speech recognition
     */
    virtual void stopRecognition(void) = 0;

    /**
     * @brief Add the Listener object
     * @param[in] listener listener object
     */
    virtual void addListener(IASRListener* listener) = 0;

    /**
     * @brief Remove the Listener object
     * @param[in] listener listener object
     */
    virtual void removeListener(IASRListener* listener) = 0;

    /**
     * @brief Set attribute about speech recognition
     * @param[in] attribute attribute object
     */
    virtual void setAttribute(ASRAttribute&& attribute) = 0;
};

/**
 * @}
 */

} // NuguCapability

#endif /* __NUGU_ASR_INTERFACE_H__ */
