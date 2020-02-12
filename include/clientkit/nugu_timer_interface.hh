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

/**
 * @brief nugu timer interface
 */
class INuguTimer {
public:
    /**
     * @brief Timer Callback
     * @param[in] count timeout count
     * @param[in] repeat timeout repeat
     */
    typedef std::function<void(int, int)> timer_callback;

public:
    virtual ~INuguTimer() = default;

    /**
     * @brief Set timer's timeout interval
     * @param[in] sec timeout (unit: sec)
     */
    virtual void setInterval(unsigned int sec) = 0;
    /**
     * @brief Get timer's timeout interval
     * @return timeout interval (unit: sec)
     */
    virtual unsigned int getInterval() = 0;
    /**
     * @brief Set timer's timeout repeat
     * @param[in] count repeat (at least 1 value)
     */
    virtual void setRepeat(unsigned int count) = 0;
    /**
     * @brief Get timer's timeout repeat
     * @return timeout repeat
     */
    virtual unsigned int getRepeat() = 0;
    /**
     * @brief Get timer's timeout count
     * @return timeout count
     */
    virtual unsigned int getCount() = 0;
    /**
     * @brief Request stop timer
     */
    virtual void stop() = 0;
    /**
     * @brief Request start timer with new interval
     * @param[in] sec timeout interval. The timer works with internal interval if sec is 0.
     */
    virtual void start(unsigned int sec = 0) = 0;
    /**
     * @brief Request re-start timer with new interval
     * @param[in] sec timeout interval. The timer works with internal interval if sec is 0.
     */
    virtual void restart(unsigned int sec = 0) = 0;
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
