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

#ifndef __NUGU_LOCATION_INTERFACE_H__
#define __NUGU_LOCATION_INTERFACE_H__

#include <clientkit/capability_interface.hh>
#include <nugu.h>

namespace NuguCapability {

using namespace NuguClientKit;

/**
 * @file location_interface.hh
 * @defgroup LocationInterface LocationInterface
 * @ingroup SDKNuguCapability
 * @brief Location capability interface
 *
 * It's used to handle location information which is collected from device.
 *
 * @{
 */

/**
 * @brief Location Information
 */
typedef struct _LocationInfo {
    std::string latitude; /**< latitude coordinates */
    std::string longitude; /**< longitude coordinates */
} LocationInfo;

/**
 * @brief location listener interface
 * @see ILocationHandler
 */
class NUGU_API ILocationListener : virtual public ICapabilityListener {
public:
    virtual ~ILocationListener() = default;

    /**
     * @brief Request context information for location context
     * @param[in] location_info location information
     */
    virtual void requestContext(LocationInfo& location_info) = 0;
};

/**
 * @brief location handler interface
 * @see ILocationListener
 */
class NUGU_API ILocationHandler : virtual public ICapabilityInterface {
public:
    virtual ~ILocationHandler() = default;
};

/**
 * @}
 */

} // NuguCapability

#endif /* __NUGU_LOCATION_INTERFACE_H__ */
