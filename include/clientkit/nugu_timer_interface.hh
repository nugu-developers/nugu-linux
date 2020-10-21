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

#ifndef __NUGU_TIMER_INTERFACE_H__
#define __NUGU_TIMER_INTERFACE_H__

#include <functional>
#include <string>

namespace NuguClientKit {

/**
 * @file nugu_timer_interface.hh
 * @defgroup NuguTimerInterface NuguTimerInterface
 * @ingroup SDKNuguClientKit
 * @brief NUGU Timer Interface
 *
 * Timer provide by NUGU SDK
 *
 * @{
 */

/** @brief time unit value(second) */
#define NUGU_TIMER_UNIT_SEC 1000

/**
 * @brief nugu timer interface
 */
class INuguTimer {
public:
    /**
     * @brief Timer Callback
     */
    typedef std::function<void()> timer_callback;

public:
    virtual ~INuguTimer() = default;

    /**
     * @brief Set timer's timeout interval
     * @param[in] msec timeout (unit: msec)
     */
    virtual void setInterval(unsigned int msec) = 0;
    /**
     * @brief Get timer's timeout interval
     * @return timeout interval (unit: msec)
     */
    virtual unsigned int getInterval() = 0;
    /**
     * @brief Set timer running single shot
     * @param[in] singleshot singleshot
     */
    virtual void setSingleShot(bool singleShot) = 0;
    /**
     * @brief Get timer single shot property
     * @return single shot value
     */
    virtual bool getSingleShot() = 0;
    /**
     * @brief Request stop timer
     */
    virtual void stop() = 0;
    /**
     * @brief Request start timer with new interval
     * @param[in] msec timeout interval. The timer works with internal interval if msec is 0.
     */
    virtual void start(unsigned int msec = 0) = 0;
    /**
     * @brief Request re-start timer with new interval
     * @param[in] msec timeout interval. The timer works with internal interval if msec is 0.
     */
    virtual void restart(unsigned int msec = 0) = 0;
    /**
     * @brief Request set timeout callback
     * @param[in] cb timeout callback
     */
    virtual void setCallback(timer_callback cb) = 0;
};

/**
 * @}
 */

} // NuguClientKit

#endif
