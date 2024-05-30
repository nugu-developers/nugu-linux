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

#ifndef __NUGU_BATTERY_INTERFACE_H__
#define __NUGU_BATTERY_INTERFACE_H__

#include <clientkit/capability_interface.hh>
#include <nugu.h>

namespace NuguCapability {

using namespace NuguClientKit;

/**
 * @file battery_interface.hh
 * @defgroup BatteryInterface BatteryInterface
 * @ingroup SDKNuguCapability
 * @brief Battery capability interface
 *
 * It's used to handle battery information which is collected from device.
 *
 * @{
 */

/**
 * @brief Battery Information
 */
typedef struct _BatteryInfo {
    int level = -1; /**< battery level (0 ~ 100) */
    bool charging = false; /**< whether the battery is charged */
    bool approximate_level = false; /**< approximate battery level */
} BatteryInfo;

/**
 * @brief battery listener interface
 * @see IBatteryHandler
 */
class NUGU_API IBatteryListener : virtual public ICapabilityListener {
public:
    virtual ~IBatteryListener() = default;

    /**
     * @brief Request context information for battery context
     * @param[in] battery_info battery information
     */
    virtual void requestContext(BatteryInfo& battery_info) = 0;
};

/**
 * @brief battery handler interface
 * @see IBatteryListener
 */
class NUGU_API IBatteryHandler : virtual public ICapabilityInterface {
public:
    virtual ~IBatteryHandler() = default;
};

/**
 * @}
 */

} // NuguCapability

#endif /* __NUGU_BATTERY_INTERFACE_H__ */
