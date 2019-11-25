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

#ifndef __NUGU_NETWORK_MANAGER_H__
#define __NUGU_NETWORK_MANAGER_H__

#include <stdio.h>
#include <core/nugu_event.h>
#include <core/nugu_directive.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nugu_network_manager.h
 * @defgroup nugu_network NetworkManager
 * @ingroup SDKCore
 * @brief Network management
 *
 * The network manager is responsible for managing connections and sending
 * and receiving events and directives with the server.
 *
 * @{
 */

/**
 * @brief network status
 */
enum nugu_network_status {
	NUGU_NETWORK_UNKNOWN, /**< Initial state */
	NUGU_NETWORK_CONNECTING, /**< Connection in progress */
	NUGU_NETWORK_CONNECTED, /**< Network connected */
	NUGU_NETWORK_DISCONNECTED, /**< Network disconnected */
	NUGU_NETWORK_TOKEN_ERROR /**< Token error */
};

/**
 * @see enum nugu_network_status
 */
typedef enum nugu_network_status NuguNetworkStatus;

/**
 * @brief Callback prototype for receiving network status events
 */
typedef void (*NetworkManagerCallback)(void *userdata);

/**
 * @brief Set network status callback
 * @param[in] callback callback function
 * @param[in] userdata data to pass to the user callback
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_network_manager_set_callback(NetworkManagerCallback callback,
				      void *userdata);

/**
 * @brief Set the current network status
 * @param[in] network_status network status
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_network_manager_get_status()
 */
int nugu_network_manager_set_status(NuguNetworkStatus network_status);

/**
 * @brief Get the current network status
 * @return NuguNetworkStatus network status
 * @see nugu_network_manager_set_status()
 */
NuguNetworkStatus nugu_network_manager_get_status(void);

/**
 * @brief Send the event to server
 * @param[in] nev event object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_event_new()
 * @see nugu_network_manager_send_event_data()
 */
int nugu_network_manager_send_event(NuguEvent *nev);

/**
 * @brief Send the attachment data of event to server
 * @param[in] nev event object
 * @param[in] is_end data is last(is_end=1) or not(is_end=0)
 * @param[in] length length of data
 * @param[in] data data to send
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_network_manager_send_event()
 */
int nugu_network_manager_send_event_data(NuguEvent *nev, int is_end,
					 size_t length, unsigned char *data);

/**
 * @brief Initialize the network manager
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_network_manager_initialize(void);

/**
 * @brief De-initialize the network manager
 */
void nugu_network_manager_deinitialize(void);

/**
 * @brief Connect to server
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_network_manager_disconnect()
 */
int nugu_network_manager_connect(void);

/**
 * @brief Disconnect from server
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_network_manager_connect()
 */
int nugu_network_manager_disconnect(void);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
