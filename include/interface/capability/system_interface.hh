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

#include <interface/capability/capability_interface.hh>

namespace NuguInterface {

/**
 * @file system_interface.hh
 * @defgroup SystemInterface SystemInterface
 * @ingroup SDKNuguInterface
 * @brief System capability interface
 *
 * It is responsible for system related functions of device such as network handoff processing and context sync.
 *
 * @{
 */

/**
 * @brief ExtensionResult
 */
enum class SystemMessage {
    ROUTE_ERROR_NOT_FOUND_PLAY /**< This message is sent from the server router if play is not found.  */
};

/**
 * @brief system listener interface
 * @see ISystemHandler
 */
class ISystemListener : public ICapabilityListener {
public:
    virtual ~ISystemListener() = default;

    /**
     * @brief Deliver the message received from the server to the user.
     * @param[in] message server message
     */
    virtual void onSystemMessageReport(SystemMessage message) = 0;
};

/**
 * @brief system handler interface
 * @see ISystemListener
 */
class ISystemHandler : public ICapabilityHandler {
public:
    virtual ~ISystemHandler() = default;

    /**
     * @brief When the device is connected to NUGU Platform, all state information is delivered through the context.
     */
    virtual void sendEventSynchronizeState(void) = 0;
};

/**
 * @}
 */

} // NuguInterface

#endif /* __NUGU_SYSTEM_INTERFACE_H__ */
