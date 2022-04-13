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

#ifndef __NUGU_BUFFER_H__
#define __NUGU_BUFFER_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nugu_buffer.h
 * @defgroup Buffer Buffer
 * @ingroup SDKBase
 * @brief Buffer manipulation functions
 *
 * The Buffer module makes it easy to add, delete, and move data in byte units.
 *
 * The Buffer object allocates and uses a single flat buffer inside, and
 * adjusts its size flexibly as needed.
 *
 * The Buffer object is not thread safe.
 *
 * @{
 */

/**
 * @brief Not found return type of nugu_buffer_find_byte()
 * @see nugu_buffer_find_byte()
 */
#define NUGU_BUFFER_NOT_FOUND ((size_t)-1)

/**
 * @brief Buffer object
 */
typedef struct _nugu_buffer NuguBuffer;

/**
 * @brief Create new buffer object
 * @param[in] default_size default allocation size of buffer
 *            (0 is default size)
 * @return Buffer object
 * @see nugu_buffer_free()
 */
NuguBuffer *nugu_buffer_new(size_t default_size);

/**
 * @brief Destroy the buffer object
 * @param[in] buf buffer object
 * @param[in] data_free If 1, the internal buffer is also freed. If 0,
 * only the buffer object is freed, not the internal buffer.
 * @return If data_free is false, developer must free the internal buffer
 * manually. If false, always return NULL.
 * @see nugu_buffer_new()
 */
void *nugu_buffer_free(NuguBuffer *buf, int is_data_free);

/**
 * @brief Append the data to buffer object
 * @param[in] buf buffer object
 * @param[in] data The data to add to the buffer.
 * @param[in] data_len Length of the data
 * @return added length (0 = failure)
 */
size_t nugu_buffer_add(NuguBuffer *buf, const void *data, size_t data_len);

/**
 * @brief Append the data to buffer object
 * @param[in] buf buffer object
 * @param[in] byte The data to add to the buffer.
 * @return added length (0 = failure)
 */
size_t nugu_buffer_add_byte(NuguBuffer *buf, unsigned char byte);

/**
 * @brief Append the data to buffer object
 * @param[in] buf buffer object
 * @param[in] pos position
 * @param[in] byte The data to add to the buffer.
 * @return Result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_buffer_set_byte(NuguBuffer *buf, size_t pos, unsigned char byte);

/**
 * @brief Get the internal buffer
 * @param[in] buf buffer object
 * @return Internal buffer address. Please do not modify the data manually.
 * @see nugu_buffer_peek_byte()
 */
const void *nugu_buffer_peek(NuguBuffer *buf);

/**
 * @brief Gets the size of the entire data added to the buffer.
 * @param[in] buf buffer object
 * @return size of data
 * @see nugu_buffer_get_alloc_size()
 */
size_t nugu_buffer_get_size(NuguBuffer *buf);

/**
 * @brief Gets the size of the entire data allocated for the buffer.
 * @param[in] buf buffer object
 * @return size of allocated internal buffer.
 * @see nugu_buffer_get_size()
 */
size_t nugu_buffer_get_alloc_size(NuguBuffer *buf);

/**
 * @brief Get the position of the data you want to find.
 * @param[in] buf buffer object
 * @param[in] want byte data you want to find
 * @return position. if fail, return NUGU_BUFFER_NOT_FOUND
 * @see NUGU_BUFFER_NOT_FOUND
 */
size_t nugu_buffer_find_byte(NuguBuffer *buf, unsigned char want);

/**
 * @brief Get data at a specific position.
 * @param[in] buf buffer object
 * @param[in] pos position
 * @return byte data
 */
unsigned char nugu_buffer_peek_byte(NuguBuffer *buf, size_t pos);

/**
 * @brief Clear the buffer
 * @param[in] buf buffer object
 * @return Result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_buffer_clear_from()
 */
int nugu_buffer_clear(NuguBuffer *buf);

/**
 * @brief Clear data from a specific position to the end.
 * @param[in] buf buffer object
 * @param[in] pos position
 * @return Result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_buffer_clear()
 */
int nugu_buffer_clear_from(NuguBuffer *buf, size_t pos);

/**
 * @brief Delete a certain amount of data and move the remaining data forward.
 * @param[in] buf buffer object
 * @param[in] size size to delete.
 * @return Result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_buffer_shift_left(NuguBuffer *buf, size_t size);

/**
 * @brief Extract data by a certain size.
 * @param[in] buf buffer object
 * @param[in] size size to extract
 * @return Extracted data. Developer must free the data manually.
 */
void *nugu_buffer_pop(NuguBuffer *buf, size_t size);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
