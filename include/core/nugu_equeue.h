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

#ifndef __NUGU_EQUEUE_H__
#define __NUGU_EQUEUE_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nugu_equeue.h
 * @defgroup NuguEventQueue Event Queue
 * @ingroup SDKCore
 * @brief Event queue for passing events between threads.
 *
 * Queue for raising event callbacks with passing data from another
 * thread to the thread context in which GMainloop runs.
 *
 * Manage the queue thread-safely using GAsyncQueue and wakeup the GMainloop
 * using eventfd.
 *
 * @{
 */

/**
 * @brief event types
 */
enum nugu_equeue_type {
	NUGU_EQUEUE_TYPE_NEW_DIRECTIVE = 0, /**< received new directive */
	NUGU_EQUEUE_TYPE_NEW_ATTACHMENT, /**< received new attachment */

	NUGU_EQUEUE_TYPE_INVALID_TOKEN, /**< received invalid token response */

	NUGU_EQUEUE_TYPE_SEND_PING_FAILED, /**< failed to send ping request */
	NUGU_EQUEUE_TYPE_SEND_EVENT_FAILED, /**< failed to send event */
	NUGU_EQUEUE_TYPE_SEND_ATTACHMENT_FAILED,
	/**< failed to send attachment */

	NUGU_EQUEUE_TYPE_REGISTRY_HEALTH, /**< received health check policy */
	NUGU_EQUEUE_TYPE_REGISTRY_SERVERS, /**< received server list */
	NUGU_EQUEUE_TYPE_REGISTRY_FAILED,
	/**< failed to connect device-gateway-registry */

	NUGU_EQUEUE_TYPE_SERVER_CONNECTED, /**< connected to server */
	NUGU_EQUEUE_TYPE_SERVER_DISCONNECTED, /**< disconnected from server */

	NUGU_EQUEUE_TYPE_MAX = 255 /**< maximum value for type id */
};

/**
 * @brief Callback prototype for receiving an event
 */
typedef void (*NuguEqueueCallback)(enum nugu_equeue_type type, void *data,
				   void *userdata);

/**
 * @brief Callback prototype for releasing data after event delivery is done.
 */
typedef void (*NuguEqueueDestroyCallback)(void *data);

/**
 * @brief Initialize the event queue
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_equeue_initialize(void);

/**
 * @brief De-initialize the event queue
 */
void nugu_equeue_deinitialize(void);

/**
 * @brief Set handler for event type
 * @param[in] type type-id
 * @param[in] callback callback function
 * @param[in] destroy_callback destroy notifier function
 * @param[in] userdata data to pass to the user callback
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_equeue_set_handler(enum nugu_equeue_type type,
			    NuguEqueueCallback callback,
			    NuguEqueueDestroyCallback destroy_callback,
			    void *userdata);

/**
 * @brief Unset handler for event type
 * @param[in] type type-id
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_equeue_unset_handler(enum nugu_equeue_type type);

/**
 * @brief Push new event with data to queue and trigger event callback
 * in GMainloop thread context
 *
 * @param[in] type type-id
 * @param[in] data event data
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_equeue_push(enum nugu_equeue_type type, void *data);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
