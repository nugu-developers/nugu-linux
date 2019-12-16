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

#ifndef __NUGU_RING_BUFFER_H__
#define __NUGU_RING_BUFFER_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nugu_ringbuffer.h
 * @defgroup NuguRingBuffer RingBuffer
 * @ingroup SDKBase
 * @brief RingBuffer manipulation functions
 *
 * The ring buffer manages items by setting the size and
 * number of items as a basic unit.
 *
 * @{
 */

/**
 * @brief RingBuffer object
 */
typedef struct _nugu_ring_buffer NuguRingBuffer;

/**
 * @brief Create new ringbuffer object
 * @param[in] item_size default item size
 * @param[in] max_items count of items
 * @return ringbuffer object
 */
NuguRingBuffer *nugu_ring_buffer_new(int item_size, int max_items);

/**
 * @brief Destroy the ringbuffer object
 * @param[in] buf ringbuffer object
 */
void nugu_ring_buffer_free(NuguRingBuffer *buf);

/**
 * @brief Resize the ringbuffer
 * @param[in] buf ringbuffer object
 * @param[in] item_size default item size
 * @param[in] max_items count of items
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_ring_buffer_resize(NuguRingBuffer *buf, int item_size, int max_items);

/**
 * @brief Push data to ringbuffer
 * @param[in] buf ringbuffer object
 * @param[in] data data
 * @param[in] size size of data
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_ring_buffer_push_data(NuguRingBuffer *buf, const char *data, int size);

/**
 * @brief Read item from ringbuffer
 * @param[in] buf ringbuffer object
 * @param[out] item item
 * @param[out] size size of item
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_ring_buffer_read_item(NuguRingBuffer *buf, char *item, int *size);

/**
 * @brief Get count
 * @param[in] buf ringbuffer object
 * @return result
 * @retval >0 success (count)
 * @retval -1 failure
 */
int nugu_ring_buffer_get_count(NuguRingBuffer *buf);

/**
 * @brief Get itemsize
 * @param[in] buf ringbuffer object
 * @return result
 * @retval >0 success (itemsize)
 * @retval -1 failure
 */
int nugu_ring_buffer_get_item_size(NuguRingBuffer *buf);

/**
 * @brief Get maxcount
 * @param[in] buf ringbuffer object
 * @return result
 * @retval >0 success (max count)
 * @retval -1 failure
 */
int nugu_ring_buffer_get_maxcount(NuguRingBuffer *buf);

/**
 * @brief Clear the ringbuffer
 * @param[in] buf ringbuffer object
 */
void nugu_ring_buffer_clear_items(NuguRingBuffer *buf);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
