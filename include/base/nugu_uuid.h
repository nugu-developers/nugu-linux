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

#ifndef __NUGU_UUID_H__
#define __NUGU_UUID_H__

#include <time.h>
#include <nugu.h>
#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nugu_uuid.h
 * @defgroup uuid UUID
 * @ingroup SDKBase
 * @brief UUID generation functions
 *
 * @{
 */

/**
 * @brief Seconds for base timestamp: 2019/1/1 00:00:00 (GMT)
 */
#define NUGU_BASE_TIMESTAMP_SEC 1546300800

/**
 * @brief Milliseconds for base timestamp: 2019/1/1 00:00:00.000 (GMT)
 *
 * This value must be treated as 64 bits.
 */
#define NUGU_BASE_TIMESTAMP_MSEC 1546300800000

/**
 * @brief Maximum byte array UUID size
 */
#define NUGU_MAX_UUID_SIZE 16

/**
 * @brief Maximum base16 encoded UUID string size
 */
#define NUGU_MAX_UUID_STRING_SIZE (NUGU_MAX_UUID_SIZE * 2)

/**
 * @brief Generate time based UUID
 * @return memory allocated UUID string. Developer must free the data manually.
 */
NUGU_API char *nugu_uuid_generate_time(void);

/**
 * @brief Convert base16-encoded string to byte array
 * @param[in] base16 base16-encoded string
 * @param[in] base16_len length
 * @param[out] out memory allocated output buffer
 * @param[in] out_len size of output buffer
 * @return Result of conversion success or failure
 * @retval 0 Success
 * @retval -1 Failure
 */
NUGU_API int nugu_uuid_convert_bytes(const char *base16, size_t base16_len,
				     unsigned char *out, size_t out_len);

/**
 * @brief Convert byte array to base16-encoded string
 * @param[in] bytes byte array
 * @param[in] bytes_len length
 * @param[out] out memory allocated output buffer
 * @param[in] out_len size of output buffer
 * @return Result of conversion success or failure
 * @retval 0 Success
 * @retval -1 Failure
 */
NUGU_API int nugu_uuid_convert_base16(const unsigned char *bytes,
				      size_t bytes_len, char *out,
				      size_t out_len);

/**
 * @brief Convert byte array to base16-encoded string
 * @param[in] bytes byte array
 * @param[in] bytes_len length
 * @param[out] msec milliseconds
 * @return Result of conversion success or failure
 * @retval 0 Success
 * @retval -1 Failure
 */
NUGU_API int nugu_uuid_convert_msec(const unsigned char *bytes,
				    size_t bytes_len, gint64 *msec);

/**
 * @brief Generate random bytes and fill to destination buffer
 * @param[in] dest destination buffer
 * @param[in] dest_len length of buffer
 * @return Result
 * @retval 0 Success
 * @retval -1 Failure
 */
NUGU_API int nugu_uuid_fill_random(unsigned char *dest, size_t dest_len);

/**
 * @brief Fill to output buffer with NUGU UUID format using parameters
 * @param[in] msec milliseconds
 * @param[in] hash hash value(e.g. SHA1(token))
 * @param[in] hash_len length of hash value
 * @param[out] out memory allocated output buffer
 * @param[in] out_len size of output buffer
 * @return Result
 * @retval 0 Success
 * @retval -1 Failure
 */
NUGU_API int nugu_uuid_fill(gint64 msec, const unsigned char *hash,
			    size_t hash_len, unsigned char *out,
			    size_t out_len);
/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
