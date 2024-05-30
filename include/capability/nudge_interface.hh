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

#ifndef __NUGU_NUDGE_INTERFACE_H__
#define __NUGU_NUDGE_INTERFACE_H__

#include <clientkit/capability_interface.hh>
#include <nugu.h>

namespace NuguCapability {

using namespace NuguClientKit;

/**
 * @file nudge_interface.hh
 * @defgroup NudgeInterface NudgeInterface
 * @ingroup SDKNuguCapability
 * @brief Nudge capability interface
 *
 * It's for managing the state and context of nudge dux
 *
 * @{
 */

/**
 * @brief nudge listener interface
 * @see INudgeHandler
 */
class NUGU_API INudgeListener : virtual public ICapabilityListener {
public:
    virtual ~INudgeListener() = default;
};

/**
 * @brief nudge handler interface
 * @see INudgeListener
 */
class NUGU_API INudgeHandler : virtual public ICapabilityInterface {
public:
    virtual ~INudgeHandler() = default;
};

/**
 * @}
 */

} // NuguCapability

#endif /* __NUGU_NUDGE_INTERFACE_H__ */
