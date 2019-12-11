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

#ifndef __NUGU_TIMER_H__
#define __NUGU_TIMER_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nugu_timer.h
 * @defgroup NuguTimer Timer
 * @ingroup SDKBase
 * @brief Timer manipulation functions
 *
 * Timer manipulation functions
 *
 * @{
 */

/**
 * @brief Timer object
 */
typedef struct _nugu_timer NuguTimer;

/**
 * @brief Callback prototype for timeout
 */
typedef void (*timeoutCallback)(void *userdata);

/**
 * @brief Create new timer object
 * @param[in] interval interval seconds
 * @param[in] repeat repeat count
 * @return timer object
 */
NuguTimer *nugu_timer_new(long interval, int repeat);

/**
 * @brief Destroy the timer object
 * @param[in] timer timer object
 */
void nugu_timer_delete(NuguTimer *timer);

/**
 * @brief Set interval
 * @param[in] timer timer object
 * @param[in] interval interval seconds
 * @see nugu_timer_get_interval()
 */
void nugu_timer_set_interval(NuguTimer *timer, long interval);

/**
 * @brief Get interval
 * @param[in] timer timer object
 * @return interval
 * @see nugu_timer_set_interval()
 */
long nugu_timer_get_interval(NuguTimer *timer);

/**
 * @brief Set repeat count
 * @param[in] timer timer object
 * @param[in] repeat number of times to repeat
 * @see nugu_timer_get_repeat()
 */
void nugu_timer_set_repeat(NuguTimer *timer, int repeat);

/**
 * @brief Get repeat count
 * @param[in] timer timer object
 * @return repeat count
 * @see nugu_timer_set_repeat()
 */
int nugu_timer_get_repeat(NuguTimer *timer);

/**
 * @brief Start the timer
 * @param[in] timer timer object
 * @see nugu_timer_stop()
 */
void nugu_timer_start(NuguTimer *timer);

/**
 * @brief Stop the timer
 * @param[in] timer timer object
 * @see nugu_timer_start()
 */
void nugu_timer_stop(NuguTimer *timer);

/**
 * @brief Set timeout callback
 * @param[in] timer timer object
 * @param[in] callback callback function
 * @param[in] userdata data to pass to the user callback
 */
void nugu_timer_set_callback(NuguTimer *timer, timeoutCallback callback,
			     void *userdata);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
