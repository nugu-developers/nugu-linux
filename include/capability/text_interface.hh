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

#ifndef __NUGU_TEXT_INTERFACE_H__
#define __NUGU_TEXT_INTERFACE_H__

#include <clientkit/capability_interface.hh>

namespace NuguCapability {

using namespace NuguClientKit;

/**
 * @file text_interface.hh
 * @defgroup TextInterface TextInterface
 * @ingroup SDKNuguCapability
 * @brief Text capability interface
 *
 * It is possible to request NUGU service based on text rather than speech recognition.
 *
 * @{
 */

#define NUGU_SERVER_RESPONSE_TIMEOUT_SEC 10 /** @def Set server response timeout about 10s */

/**
 * @brief TextState
 */
enum class TextState {
    IDLE, /**< Status that can receive Text input */
    BUSY /**< Waiting for response after text input */
};

/**
 * @brief TextError
 */
enum class TextError {
    RESPONSE_TIMEOUT /** Server response timeout for text input request */
};

/**
 * @brief Attributes for setting Text options.
 */
typedef struct _TextAttribute {
    int response_timeout; /**< Server response timeout about sent text */
} TextAttribute;

/**
 * @brief text listener interface
 * @see ITextHandler
 */
class ITextListener : virtual public ICapabilityListener {
public:
    virtual ~ITextListener() = default;

    /**
     * @brief TextState changed
     * @param[in] state text state
     * @param[in] dialog_id dialog request id
     * @see ITextHandler::requestTextInput()
     */
    virtual void onState(TextState state, const std::string& dialog_id) = 0;

    /**
     * @brief When server processing for text input requests is completed
     * @param[in] dialog_id dialog request id
     */
    virtual void onComplete(const std::string& dialog_id) = 0;

    /**
     * @brief An error occurred during requesting text input.
     * @param[in] error text error
     * @param[in] dialog_id dialog request id
     */
    virtual void onError(TextError error, const std::string& dialog_id) = 0;

    /**
     * @brief Handle text command and return whether consumed.
     * @param[in] text text command
     * @param[in] token received token
     * @return whether text command is consumed or not.
     * If it return true, the event about failure of sending text input is sent to server.
     */
    virtual bool handleTextCommand(const std::string& text, const std::string& token) = 0;
};

/**
 * @brief text handler interface
 * @see ITextListener
 */
class ITextHandler : virtual public ICapabilityInterface {
public:
    virtual ~ITextHandler() = default;

    /**
     * @brief Request NUGU services based on text input.
     * @param[in] text text command
     * @param[in] token received token
     * @param[in] source source that triggered the text input
     * @param[in] include_dialog_attribute whether including dialog attribute
     * @return dialog request id if a NUGU service request succeeds with user text, otherwise empty string
     */
    virtual std::string requestTextInput(const std::string& text, const std::string& token = "", const std::string& source = "", bool include_dialog_attribute = true) = 0;

    /**
     * @brief Set attribute about response
     * @param[in] attribute attribute object
     */
    virtual void setAttribute(TextAttribute&& attribute) = 0;
};

/**
 * @}
 */

} // NuguCapability

#endif /* __NUGU_TEXT_INTERFACE_H__ */
