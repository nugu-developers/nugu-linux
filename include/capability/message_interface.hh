/*
 * Copyright (c) 2021 SK Telecom Co., Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or` implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __NUGU_MESSAGE_INTERFACE_H__
#define __NUGU_MESSAGE_INTERFACE_H__

#include <clientkit/capability.hh>

namespace NuguCapability {

using namespace NuguClientKit;

/**
 * @file message_interface.hh
 * @defgroup MessageInterface MessageInterface
 * @ingroup SDKNuguCapability
 * @brief Message capability interface
 *
 * Read and send message.
 *
 * @{
 */

/**
 * @brief MessageState
 */
enum class MessageState {
    MESSAGE_SPEECH_START, /**< Status starting speech in MESSAGE */
    MESSAGE_SPEECH_STOP, /**< Status stopping speech in MESSAGE */
    MESSAGE_SPEECH_FINISH /**< Status finishing speech in MESSAGE */
};

/**
 * @brief message listener interface
 * @see IMessageHandler
 */
class IMessageListener : public ICapabilityListener {
public:
    virtual ~IMessageListener() = default;

    /**
     * @brief Process send candidates
     * @param[in] payload directive's payload
     */
    virtual void processSendCandidates(const std::string& payload) = 0;

    /**
     * @brief Process send message
     * @param[in] payload directive's payload
     */
    virtual void processSendMessage(const std::string& payload) = 0;

    /**
     * @brief Process get message
     * @param[in] payload directive's payload
     */
    virtual void processGetMessage(const std::string& payload) = 0;

    /**
     * @brief Report changes in the speech state to the user.
     * @param[in] state message state
     * @param[in] received_time message received time
     * @param[in] token message received token
     * @param[in] dialog_id dialog request id
     */
    virtual void messageStateChanged(MessageState state, const std::string& received_time, const std::string& token, const std::string& dialog_id) = 0;

};

/**
 * @brief message handler interface
 * @see IMessageListener
 */
class IMessageHandler : virtual public ICapabilityInterface {
public:
    virtual ~IMessageHandler() = default;

    /**
     * @brief The result of send candidates process
     * @param[in] context_template template information in context
     * @param[in] payload event's payload
     */
    virtual void candidatesListed(const std::string& context_template, const std::string& payload) = 0;

    /**
     * @brief The succeed result of send message process
     * @param[in] payload event's payload
     */
    virtual void sendMessageSucceeded(const std::string& payload) = 0;

    /**
     * @brief The failed result of send message process
     * @param[in] payload event's payload
     */
    virtual void sendMessageFailed(const std::string& payload) = 0;

    /**
     * @brief The succeed result of get message process
     * @param[in] payload event's payload
     */
    virtual void getMessageSucceeded(const std::string& payload) = 0;

    /**
     * @brief The failed result of get message process
     * @param[in] payload event's payload
     */
    virtual void getMessageFailed(const std::string& payload) = 0;
};

/**
 * @}
 */

} // NuguCapability

#endif /* __NUGU_MESSAGE_INTERFACE_H__ */
