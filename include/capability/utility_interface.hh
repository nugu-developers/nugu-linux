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

#ifndef __NUGU_UTILITY_INTERFACE_H__
#define __NUGU_UTILITY_INTERFACE_H__

#include <clientkit/capability_interface.hh>
#include <nugu.h>

namespace NuguCapability {

using namespace NuguClientKit;

/**
 * @file utility_interface.hh
 * @defgroup UtilityInterface UtilityInterface
 * @ingroup SDKNuguCapability
 * @brief Utility capability interface
 *
 * It's for blocking directive set which is located in the rear.
 *
 * @{
 */

/**
 * @brief utility listener interface
 * @see IUtilityHandler
 */
class NUGU_API IUtilityListener : virtual public ICapabilityListener {
public:
    virtual ~IUtilityListener() = default;
};

/**
 * @brief utility handler interface
 * @see IUtilityListener
 */
class NUGU_API IUtilityHandler : virtual public ICapabilityInterface {
public:
    virtual ~IUtilityHandler() = default;
};

/**
 * @}
 */

} // NuguCapability

#endif /* __NUGU_UTILITY_INTERFACE_H__ */
