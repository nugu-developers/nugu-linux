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

#ifndef __NUGU_EXTENSION_INTERFACE_H__
#define __NUGU_EXTENSION_INTERFACE_H__

#include <clientkit/capability_interface.hh>
#include <nugu.h>

namespace NuguCapability {

using namespace NuguClientKit;

/**
 * @file extension_interface.hh
 * @defgroup ExtensionInterface ExtensionInterface
 * @ingroup SDKNuguCapability
 * @brief Extension capability interface
 *
 * It handle directives which are not defined in other capability agents.
 *
 * @{
 */

/**
 * @brief extension listener interface
 * @see IExtensionHandler
 */
class NUGU_API IExtensionListener : virtual public ICapabilityListener {
public:
    virtual ~IExtensionListener() = default;

    /**
     * @brief Notified when receiving Action directive from server.
     * @param[in] data received raw data
     * @param[in] ps_id received playServiceId
     * @param[in] dialog_id dialogId of received action
     */
    virtual void receiveAction(const std::string& data, const std::string& ps_id, const std::string& dialog_id) = 0;

    /**
     * @brief Request context information for extension context
     * @param[in] context_info context information
     */
    virtual void requestContext(std::string& context_info) = 0;
};

/**
 * @brief extension handler interface
 * @see IExtensionListener
 */
class NUGU_API IExtensionHandler : virtual public ICapabilityInterface {
public:
    virtual ~IExtensionHandler() = default;

    /**
     * @brief Call if handling action succeed
     */
    virtual void actionSucceeded() = 0;

    /**
     * @brief Call if handling action fail
     */
    virtual void actionFailed() = 0;

    /**
     * @brief Request the specific command to Play
     * @param[in] ps_id playServiceId for handling request
     * @param[in] data raw data to request
     */
    virtual void commandIssued(const std::string& ps_id, const std::string& data) = 0;
};

/**
 * @}
 */

} // NuguCapability

#endif /* __NUGU_EXTENSION_INTERFACE_H__ */
