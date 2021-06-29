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
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __NUGU_PHONE_CALL_INTERFACE_H__
#define __NUGU_PHONE_CALL_INTERFACE_H__

#include <clientkit/capability_interface.hh>

namespace NuguCapability {

using namespace NuguClientKit;

/**
 * @file phone_call_interface.hh
 * @defgroup PhoneCallInterface PhoneCallInterface
 * @ingroup SDKNuguCapability
 * @brief PhoneCall capability interface
 *
 * Dial, hangup and answer the phone call
 *
 * @{
 */

/**
 * @brief phone call listener interface
 * @see IPhoneCallHandler
 */
class IPhoneCallListener : public ICapabilityListener {
public:
    virtual ~IPhoneCallListener() = default;

    /**
     * @brief Process send candidates
     * @param[in] payload directive's payload
     */
    virtual void processSendCandidataes(const std::string& payload) = 0;
    /**
     * @brief Process make call
     * @param[in] payload directive's payload
     */
    virtual void processMakeCall(const std::string& payload) = 0;
    /**
     * @brief Process end call
     * @param[in] payload directive's payload
     */
    virtual void processEndCall(const std::string& payload) = 0;
    /**
     * @brief Process accept call
     * @param[in] payload directive's payload
     */
    virtual void processAcceptCall(const std::string& payload) = 0;
    /**
     * @brief Process block incoming call
     * @param[in] payload directive's payload
     */
    virtual void processBlockIncomingCall(const std::string& payload) = 0;
};

/**
 * @brief phone call handler interface
 * @see IPhoneCallListener
 */
class IPhoneCallHandler : virtual public ICapabilityInterface {
public:
    virtual ~IPhoneCallHandler() = default;

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
    virtual void callArrived(const std::string& payload) = 0;
    /**
     * @brief The failed result of send message process
     * @param[in] payload event's payload
     */
    virtual void callEnded(const std::string& payload) = 0;
    /**
     * @brief The succeed result of get message process
     * @param[in] payload event's payload
     */
    virtual void callEstablished(const std::string& payload) = 0;
    /**
     * @brief The succeeded result of make call process
     * @param[in] payload event's payload
     */
    virtual void makeCallSucceeded(const std::string& payload) = 0;
    /**
     * @brief The failed result of make call process
     * @param[in] payload event's payload
     */
    virtual void makeCallFailed(const std::string& payload) = 0;
};

/**
 * @}
 */

} // NuguCapability

#endif /* __NUGU_PHONE_CALL_INTERFACE_H__ */
