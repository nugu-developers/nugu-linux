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

#include <interface/capability/capability_interface.hh>

namespace NuguInterface {

/**
 * @file location_interface.hh
 * @defgroup LocationInterface LocationInterface
 * @ingroup SDKNuguInterface
 * @brief Location capability interface
 *
 * It is used to handle location information which is composed by latitude and longitude.
 * Also, it should check state whether location information available.
 *
 * @{
 */

typedef struct {
    std::string latitude;
    std::string longitude;
} LocationInfo;

/**
 * @brief location listener interface
 */
class ILocationListener : public ICapabilityListener {
public:
    virtual ~ILocationListener() = default;

    /**
     * @brief Request context information about location state and current from user application.
     * @param[in] location_info context information about location state and current
     */
    virtual void requestContext(LocationInfo& location_info) = 0;
};

/**
 * @}
 */

} // NuguInterface

#endif /* __NUGU_LOCATION_INTERFACE_H__ */
