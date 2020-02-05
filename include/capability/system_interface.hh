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
    REVOKED_DEVICE
};

/**
 * @brief system listener interface
 * @see ISystemHandler
 */
class ISystemListener : public ICapabilityListener {
public:
    virtual ~ISystemListener() = default;

    /**
     * @brief Deliver the exception received from the server to the user.
     * @param[in] exception server exception
     */
    virtual void onException(SystemException exception) = 0;

    /**
     * @brief Received a device turn off request from the server.
     */
    virtual void onTurnOff(void) = 0;

    /**
     * @brief Received a revoke request from the server.
     * @param[in] reason reason for revoke
     */
    virtual void onRevoke(RevokeReason reason) = 0;

    /**
     * @brief SDK request information about device's charging status
     * @param[out] charge device's charging status
     * @return return device charge status accessible
     */
    virtual bool requestDeviceCharging(bool& charge) = 0;

    /**
     * @brief SDK request information about device's remaining battery
     * @param[out] battery device's battery
     * @return return device battery measurability
     */
    virtual bool requestDeviceBattery(int& battery) = 0;
};

/**
 * @brief system handler interface
 * @see ISystemListener
 */
class ISystemHandler : virtual public ICapabilityInterface {
public:
    virtual ~ISystemHandler() = default;

    /**
     * @brief When the device is connected to NUGU Platform, all state information is delivered through the context.
     */
    virtual void synchronizeState(void) = 0;

    /**
     * @brief Send disconnect event to server.
     */
    virtual void disconnect(void) = 0;

    /**
     * @brief Update a timer that measures the user's inactivity.
     */
    virtual void updateUserActivity(void) = 0;
};

/**
 * @}
 */

} // NuguCapability

#endif /* __NUGU_SYSTEM_INTERFACE_H__ */
