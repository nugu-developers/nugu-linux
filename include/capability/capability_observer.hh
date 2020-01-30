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

#ifndef __NUGU_CAPABILITY_OBSERVER_H__
#define __NUGU_CAPABILITY_OBSERVER_H__

#include <string>

namespace NuguCapability {

/**
 * @file capability_observer.hh
 * @defgroup CapabilityInterface CapabilityObserver
 * @ingroup SDKNuguCapability
 * @brief Capability observer interface
 *
 *
 * @{
 */

/**
 * @brief CapabilitySignal
 */
enum class CapabilitySignal {
    DIALOG_REQUEST_ID /**< Dialog request id generated when event is forwarded to server */
};

/**
 * @brief capability observer interface
 * @see ICapabilityObservable
 */
class ICapabilityObserver {
public:
    virtual ~ICapabilityObserver() = default;

    /**
     * @brief report a signal which is invoked from some Capability to user as including data if exist.
     * @param[in] c_name capability name to report
     * @param[in] signal capability signal
     * @param[in] data capability signal's metadata
     */
    virtual void notify(std::string c_name, CapabilitySignal signal, void* data) = 0;
};

/**
 * @}
 */

} // NuguCapability

#endif /* __NUGU_CAPABILITY_OBSERVER_H__ */
