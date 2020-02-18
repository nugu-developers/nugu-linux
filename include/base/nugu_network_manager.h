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

#include <base/nugu_event.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nugu_network_manager.h
 * @defgroup nugu_network NetworkManager
 * @ingroup SDKBase
 * @brief Network management
 *
 * The network manager is responsible for managing connections and sending
 * and receiving events and directives with the server.
 *
 * @{
 */

/**
 * @brief network status
 *
 * Basic connection status flow
 *   - Normal connection: DISCONNECTED -> CONNECTING -> CONNECTED
 *   - Connection failed: DISCONNECTED -> CONNECTING -> DISCONNECTED
 *   - Token error: DISCONNECTED -> CONNECTING -> TOKEN_ERROR -> DISCONNECTED
 *
 * Connection recovery flow
 *   - Connection recovered: CONNECTED -> CONNECTING -> CONNECTED
 *   - Recovery failed: CONNECTED -> CONNECTING -> DISCONNECTED
 *   - Token error: CONNECTED -> CONNECTING -> TOKEN_ERROR -> DISCONNECTED
 * @see nugu_network_manager_set_status()
 * @see nugu_network_manager_get_status()
 * @see NetworkManagerStatusCallback
 */
typedef enum nugu_network_status {
	NUGU_NETWORK_DISCONNECTED, /**< Network disconnected */
	NUGU_NETWORK_CONNECTING, /**< Connection in progress */
	NUGU_NETWORK_CONNECTED, /**< Network connected */
	NUGU_NETWORK_TOKEN_ERROR /**< Token error */
} NuguNetworkStatus;

/**
 * @brief Callback prototype for receiving network status events
 * @see nugu_network_manager_set_status_callback()
 */
typedef void (*NuguNetworkManagerStatusCallback)(NuguNetworkStatus status,
						 void *userdata);

/**
 * @brief network handoff status
 * @see nugu_network_manager_set_handoff_status_callback()
 * @see NetworkManagerHandoffStatusCallback
 */
typedef enum nugu_network_handoff_status {
	NUGU_NETWORK_HANDOFF_FAILED,
	/**< Handoff failed */

	NUGU_NETWORK_HANDOFF_READY,
	/**< The handoff connection is ready but not yet switched. */

	NUGU_NETWORK_HANDOFF_COMPLETED
	/**< The transition to the handoff connection is complete. */
} NuguNetworkHandoffStatus;

/**
 * @brief Callback prototype for handoff status events
 */
typedef void (*NuguNetworkManagerHandoffStatusCallback)(
	NuguNetworkHandoffStatus status, void *userdata);

/**
 * @brief Callback prototype for result of event transfer request.
 * @see nugu_network_manager_send_event()
 */
typedef void (*NuguNetworkManagerEventResultCallback)(int success,
						      const char *msg_id,
						      const char *dialog_id,
						      int code, void *userdata);

/**
 * @brief network protocols
 */
enum nugu_network_protocol {
	NUGU_NETWORK_PROTOCOL_H2, /**< HTTP/2 with TLS */
	NUGU_NETWORK_PROTOCOL_H2C, /**< HTTP/2 over clean TCP */
	NUGU_NETWORK_PROTOCOL_UNKNOWN /**< Unknown protocol */
};

/**
 * @brief maximum size of address
 */
#define NUGU_NETWORK_MAX_ADDRESS 255

/**
 * @brief Server policy information used for network connections
 * @see nugu_network_manager_handoff
 */
typedef struct nugu_network_server_policy {
	enum nugu_network_protocol protocol; /**< protocol */
	char address[NUGU_NETWORK_MAX_ADDRESS + 1]; /**< IP address */
	char hostname[NUGU_NETWORK_MAX_ADDRESS + 1]; /**< dns name */
	int port; /**< port number */
	int retry_count_limit; /**< maximum number of connection retries */
	int connection_timeout_ms; /**< timeout value used when connecting */
	int is_charge; /**< 0: free, 1: normal */
} NuguNetworkServerPolicy;

/**
 * @brief Set network status callback
 * @param[in] callback callback function
 * @param[in] userdata data to pass to the user callback
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_network_manager_set_status_callback(
	NuguNetworkManagerStatusCallback callback, void *userdata);

/**
 * @brief Set handoff status callback
 * @param[in] callback callback function
 * @param[in] userdata data to pass to the user callback
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_network_manager_set_handoff_status_callback(
	NuguNetworkManagerHandoffStatusCallback callback, void *userdata);

/**
 * @brief Set event result callback
 * @param[in] callback callback function
 * @param[in] userdata data to pass to the user callback
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_network_manager_set_event_result_callback(
	NuguNetworkManagerEventResultCallback callback, void *userdata);

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
 * @param[in] is_sync synchronized blocking event(is_sync=1) or not(is_sync=0)
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_event_new()
 * @see nugu_network_manager_send_event_data()
 * @see nugu_network_manager_force_close_event()
 */
int nugu_network_manager_send_event(NuguEvent *nev, int is_sync);

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
 * @brief Force close the NUGU_EVENT_TYPE_WITH_ATTACHMENT type event.
 * @param[in] nev event object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_event_set_type()
 */
int nugu_network_manager_force_close_event(NuguEvent *nev);

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
 * @brief Handoff the current connection to new server
 * @return result
 * @retval 0 success
 * @retval -1 failure
 *
 * When a handoff request is received, the client tries to connect to another
 * server while maintaining the current connection.
 *
 *   - If the handoff connection is successful, change the current connection
 *     to the new server.
 *   - If the handoff connection fails, disconnect all connections and start
 *     over from the Registry step.
 *   - If the handoff connection is lost, start again from the Registry step.
 *
 */
int nugu_network_manager_handoff(const NuguNetworkServerPolicy *policy);

/**
 * @brief Set the access token value.
 * @param[in] token access token
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_network_manager_set_token(const char *token);

/**
 * @brief Get the access token value.
 * @return access token value. Please do not modify the data manually.
 */
const char *nugu_network_manager_peek_token(void);

/**
 * @brief Set the device gateway registry url.
 * @param[in] urlname gateway registry url
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_network_manager_set_registry_url(const char *urlname);

/**
 * @brief Get the device gateway registry url.
 * @return gateway registry url. Please do not modify the data manually.
 */
const char *nugu_network_manager_peek_registry_url(void);

/**
 * @brief Set the HTTP header UserAgent information.
 * @param[in] uagent UserAgent information
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_network_manager_set_useragent(const char *uagent);

/**
 * @brief Get the UserAgent information.
 * @return UserAgent information. Please do not modify the data manually.
 */
const char *nugu_network_manager_peek_useragent(void);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
