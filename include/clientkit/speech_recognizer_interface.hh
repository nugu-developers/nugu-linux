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

#ifndef __NUGU_SPEECH_RECOGNIZER_INTERFACE_H__
#define __NUGU_SPEECH_RECOGNIZER_INTERFACE_H__

#include <string>

namespace NuguClientKit {

/**
 * @file speech_recognizer_interface.hh
 * @defgroup SpeechRecognizerInterface SpeechRecognizerInterface
 * @ingroup SDKNuguClientKit
 * @brief Speech Recognizer Interface
 *
 * Start/Stop Speech Recognition and receive current listening state
 *
 * @{
 */

/**
 * @brief ListeningState
 */
enum class ListeningState {
    READY, /**< Ready to listen speech */
    LISTENING, /**< Listening speech */
    SPEECH_START, /**< Detect speech start point */
    SPEECH_END, /**< Detect speech end point */
    TIMEOUT, /**< Listening timeout */
    FAILED, /**< Listening failed */
    DONE /**< Listening complete */
};

/**
 * @brief Attributes about epd.
 */
typedef struct {
    int epd_timeout; /**< epd timeout sec */
    int epd_max_duration; /**< epd max duration sec */
    long epd_pause_length; /**< epd pause length msec */
} EpdAttribute;

/**
 * @brief SpeechRecognizer listener interface
 * @see ISpeechRecognizer
 */
class ISpeechRecognizerListener {
public:
    virtual ~ISpeechRecognizerListener() = default;

    /**
     * @brief Report to the user listening state changed.
     * @param[in] state listening state
     * @param[in] id listening id
     */
    virtual void onListeningState(ListeningState state, const std::string& id) = 0;

    /**
     * @brief Get current audio input stream
     * @param[in] buf audio input data
     * @param[in] length audio input length
     * @param[in] is_end whether final data
     */
    virtual void onRecordData(unsigned char* buf, int length, bool is_end) = 0;
};

/**
 * @brief SpeechRecognizer interface
 * @see ISpeechRecognizerListener
 */
class ISpeechRecognizer {
public:
    virtual ~ISpeechRecognizer() = default;

    /**
     * @brief Set SpeechRecognizer listener object
     * @param[in] listener ISpeechRecognizerListener object
     */
    virtual void setListener(ISpeechRecognizerListener* listener) = 0;

    /**
     * @brief Start listening speech
     * @param[in] id listening id
     * @return result of start listening
     */
    virtual bool startListening(const std::string& id) = 0;

    /**
     * @brief Stop listening speech
     */
    virtual void stopListening() = 0;

    /**
     * @brief Set EPD attribute
     * @param[in] attribute EPD attribute
     */
    virtual void setEpdAttribute(const EpdAttribute& attribute) = 0;

    /**
     * @brief Get EPD attribute
     * @return EPD attribute
     */
    virtual EpdAttribute getEpdAttribute() = 0;

    /**
     * @brief Get audio recorder's mute
     * @return mute
     */
    virtual bool isMute() = 0;

    /**
     * @brief Get mime type information of recorder
     * @return mime type
     */
    virtual std::string getMimeType() = 0;
};

/**
 * @}
 */

} //NuguClientKit

#endif /* __NUGU_SPEECH_RECOGNIZER_INTERFACE_H__*/
