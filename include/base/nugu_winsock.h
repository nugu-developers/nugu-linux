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

#ifndef __NUGU_WINSOCKET_H__
#define __NUGU_WINSOCKET_H__

#ifdef __MSYS__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nugu_winsock.h
 * @defgroup NuguWinSock WindowSocket
 * @ingroup SDKBase
 * @brief Window tcp socket pair
 *
 * @{
 */

/**
 * @brief window socket type
 */
typedef enum _nugu_winsock_type {
	NUGU_WINSOCKET_SERVER, /**< Server socket type */
	NUGU_WINSOCKET_CLIENT, /**< Client socket type */
} NuguWinSocketType;

/**
 * @brief NuguWinSocket
 */
typedef struct _nugu_winsock_t NuguWinSocket;

/**
 * @brief Initialize window socket
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_winsock_init(void);

/**
 * @brief Deinitialize window socket
 */
void nugu_winsock_deinit(void);

/**
 * @brief Create window socket
 * @return window socket
 * @see nugu_winsock_remove()
 */
NuguWinSocket *nugu_winsock_create(void);

/**
 * @brief Remove window socket
 * @param[in] wsock window socket
 * @see nugu_winsock_create()
 */
void nugu_winsock_remove(NuguWinSocket *wsock);

/**
 * @brief Get window socket handle for read/write
 * @param[in] wsock window socket
 * @param[in] type window socket type
 * @return window socket handle
 * @retval -1 failure
 */
int nugu_winsock_get_handle(NuguWinSocket *wsock, NuguWinSocketType type);

/**
 * @brief Check for data in window socket
 * @param[in] handle window socket handle
 * @return result
 * @retval -1 failure
 * @retval 0 data is exist
 */
int nugu_winsock_check_for_data(int handle);

/**
 * @brief Read data in window socket
 * @param[in] handle window socket handle
 * @param[out] buf data buffer
 * @param[in] len data length
 * @return read data size
 * @retval -1 failure
 */
int nugu_winsock_read(int handle, char *buf, int len);

/**
 * @brief Write data to window socket
 * @param[in] handle window socket handle
 * @param[in] buf data buffer
 * @param[in] len data length
 * @return written data size
 * @retval -1 failure
 */
int nugu_winsock_write(int handle, const char *buf, int len);

#ifdef __cplusplus
}
#endif

#endif // __MSYS__

#endif // __NUGU_WINSOCKET_H__
