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

#ifndef __NUGU_TTS_INTERFACE_H__
#define __NUGU_TTS_INTERFACE_H__

#include <capability/capability_interface.hh>

namespace NuguCapability {

/**
 * @file tts_interface.hh
 * @defgroup TTSInterface TTSInterface
 * @ingroup SDKNuguCapability
 * @brief TTS capability interface
 *
 * TTS(Text-To-Speech) plays a role in text to speech.
 *
 * @{
 */

/**
 * @brief TTSState
 */
enum class TTSState {
    TTS_SPEECH_START, /**< Status starting speech in TTS */
    TTS_SPEECH_FINISH /**< Status finishing speech in TTS */
};

/**
 * @brief tts listener interface
 * @see ITTSHandler
 */
class ITTSListener : public ICapabilityListener {
public:
    virtual ~ITTSListener() = default;

    /**
     * @brief Report changes in the speech state to the user.
     * @param[in] state tts state
     * @param[in] dialog_id dialog request id
     * @see ITTSHandler::requestTTS()
     */
    virtual void onTTSState(TTSState state, const std::string& dialog_id) = 0;

    /**
     * @brief Report the speech sentence to the User.
     * @param[in] text sentense
     * @param[in] dialog_id dialog request id
     */
    virtual void onTTSText(const std::string& text, const std::string& dialog_id) = 0;

    /**
     * @brief Report canceled the text speech to the User.
     * @param[in] dialog_id dialog request id
     */
    virtual void onTTSCancel(const std::string& dialog_id) = 0;
};

/**
 * @brief tts handler interface
 * @see ITTSListener
 */
class ITTSHandler : virtual public ICapabilityInterface {
public:
    virtual ~ITTSHandler() = default;

    /**
     * @brief Stop currently speech.
     */
    virtual void stopTTS() = 0;

    /**
     * @brief request the sentence to speech.
     * @param[in] text sentense
     * @param[in] play_service_id received from server
     */
    virtual void requestTTS(const std::string& text, const std::string& play_service_id) = 0;
};

/**
 * @}
 */

} // NuguCapability

#endif /* __NUGU_TTS_INTERFACE_H__ */
