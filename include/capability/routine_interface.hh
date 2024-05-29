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

#ifndef __NUGU_ROUTINE_INTERFACE_H__
#define __NUGU_ROUTINE_INTERFACE_H__

#include <nugu.h>
#include <clientkit/capability_interface.hh>

namespace NuguCapability {

using namespace NuguClientKit;

/**
 * @file routine_interface.hh
 * @defgroup RoutineInterface RoutineInterface
 * @ingroup SDKNuguCapability
 * @brief Routine capability interface
 *
 * It's for controlling the execution of actions which are belong to routine.
 *
 * @{
 */

/**
 * @brief routine listener interface
 * @see IRoutineHandler
 */
class NUGU_API IRoutineListener : virtual public ICapabilityListener {
public:
    virtual ~IRoutineListener() = default;
};

/**
 * @brief routine handler interface
 * @see IRoutineListener
 */
class NUGU_API IRoutineHandler : virtual public ICapabilityInterface {
public:
    virtual ~IRoutineHandler() = default;

    /**
     * @brief start routine using by data which is composed of Routine.Start directive.
     * @param[in] dialog_id dialog id
     * @param[in] data Routine.Start directive payload
     * @return whether starting routine is success or not
     */
    virtual bool startRoutine(const std::string& dialog_id, const std::string& data) = 0;

    /**
     * @brief move to the next action and process.
     * @return Result of operation
     * @retval true succeed to move and process
     * @retval false fail to move and process
     */
    virtual bool next() = 0;

    /**
     * @brief move to the previous action and process.
     * @return Result of operation
     * @retval true succeed to move and process
     * @retval false fail to move and process
     */
    virtual bool prev() = 0;
};

/**
 * @}
 */

} // NuguCapability

#endif /* __NUGU_ROUTINE_INTERFACE_H__ */
