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

#ifndef __NUGU_SYSTEM_INTERFACE_H__
#define __NUGU_SYSTEM_INTERFACE_H__

#include <clientkit/capability_interface.hh>
#include <nugu.h>

namespace NuguCapability {

using namespace NuguClientKit;

/**
 * @file system_interface.hh
 * @defgroup SystemInterface SystemInterface
 * @ingroup SDKNuguCapability
 * @brief System capability interface
 *
 * It is responsible for system related functions of device such as network handoff processing and context sync.
 *
 * @{
 */

/**
 * @brief SystemException
 */
enum class SystemException {
    UNAUTHORIZED_REQUEST_EXCEPTION, /**< Occurs when the oauth token is invalid  */
    PLAY_ROUTER_PROCESSING_EXCEPTION, /**< Occurs when play router cannot find play  */
    ASR_RECOGNIZING_EXCEPTION, /**< Occurs when error is recognized in ASR  */
    TTS_SPEAKING_EXCEPTION, /**< Occurs when error is speaking in TTS  */
    INTERNAL_SERVICE_EXCEPTION /**< Occurs on Internal Error on Server  */
};

/**
 * @brief RevokeReason
 */
enum class RevokeReason {
    REVOKED_DEVICE, /**< Token revoke due to device removal */
    WITHDRAWN_USER /**< Token revoke due to user withdrawal */
};

/**
 * @brief system listener interface
 * @see ISystemHandler
 */
class NUGU_API ISystemListener : virtual public ICapabilityListener {
public:
    virtual ~ISystemListener() = default;

    /**
     * @brief Deliver the exception received from the server to the user.
     * @param[in] exception server exception
     * @param[in] dialog_id dialog request id
     */
    virtual void onException(SystemException exception, const std::string& dialog_id) = 0;

    /**
     * @brief Received a device turn off request from the server.
     */
    virtual void onTurnOff() = 0;

    /**
     * @brief Received a revoke request from the server.
     * @param[in] reason reason for revoke
     */
    virtual void onRevoke(RevokeReason reason) = 0;

    /**
     * @brief Received a no directive from the server.
     * @param[in] dialog_id dialog request id
     */
    virtual void onNoDirective(const std::string& dialog_id) = 0;
};

/**
 * @brief system handler interface
 * @see ISystemListener
 */
class NUGU_API ISystemHandler : virtual public ICapabilityInterface {
public:
    virtual ~ISystemHandler() = default;

    /**
     * @brief When the device is connected to NUGU Platform, all state information is delivered through the context.
     */
    virtual void synchronizeState() = 0;

    /**
     * @brief Update a timer that measures the user's inactivity.
     */
    virtual void updateUserActivity() = 0;
};

/**
 * @}
 */

} // NuguCapability

#endif /* __NUGU_SYSTEM_INTERFACE_H__ */
