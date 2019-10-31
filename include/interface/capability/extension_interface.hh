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

#include <string>

#include <interface/capability/capability_interface.hh>

namespace NuguInterface {

/**
 * @file extension_interface.hh
 * @defgroup ExtensionInterface ExtensionInterface
 * @ingroup SDKNuguInterface
 * @brief Extension capability interface
 *
 * It serves as a gateway for communicating commands and information with other applications.
 *
 * @{
 */

/**
 * @brief ExtensionResult
 */
enum class ExtensionResult {
    SUCCEEDED, /**< Returns when the execution of another application operation is successful. */
    FAILED /**< Returns when the execution of another application operation is failed. */
};

/**
 * @brief extension listener interface
 * @see IExtensionHandler
 */
class IExtensionListener : public ICapabilityListener {
public:
    virtual ~IExtensionListener() = default;

    /**
     * @brief Pass it to the user to control the external application.
     * @param[in] data extension action message
     */
    virtual ExtensionResult receiveAction(std::string& data) = 0;

    /**
     * @brief Request context information from the external application.
     * @param[in] data context information
     */
    virtual void requestContext(std::string& data) = 0;
};

/**
 * @brief extension handler interface
 * @see IExtensionListener
 */
class IExtensionHandler : public ICapabilityHandler {
public:
    virtual ~IExtensionHandler() = default;
};

/**
 * @}
 */

} // NuguInterface

#endif /* __NUGU_EXTENSION_INTERFACE_H__ */
