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

#ifndef __NUGU_MIC_INTERFACE_H__
#define __NUGU_MIC_INTERFACE_H__

#include <clientkit/capability_interface.hh>
#include <nugu.h>

namespace NuguCapability {

using namespace NuguClientKit;

/**
 * @file mic_interface.hh
 * @defgroup MicInterface MicInterface
 * @ingroup SDKNuguCapability
 * @brief MIC capability interface
 *
 * Control the microphone on or off via button or mobile nugu application.
 *
 * @{
 */

/**
 * @brief MicStatus
 */
enum class MicStatus {
    ON, /**< MIC ON status */
    OFF /**< MIC OFF status */
};

/**
 * @brief mic listener interface
 * @see IMicHandler
 */
class NUGU_API IMicListener : virtual public ICapabilityListener {
public:
    virtual ~IMicListener() = default;

    /**
     * @brief Report to the user mic status changed.
     * @param[in] status mic status
     */
    virtual void micStatusChanged(MicStatus& status) = 0;
};

/**
 * @brief mic handler interface
 * @see IMicListener
 */
class NUGU_API IMicHandler : virtual public ICapabilityInterface {
public:
    virtual ~IMicHandler() = default;

    /**
     * @brief application enables mic.
     */
    virtual void enable() = 0;

    /**
     * @brief if application disables mic, EPD and KWD will not work.
     */
    virtual void disable() = 0;
};

/**
 * @}
 */

} // NuguCapability

#endif /* __NUGU_MIC_INTERFACE_H__ */
