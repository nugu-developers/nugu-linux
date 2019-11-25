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

#ifndef __NUGU_EVENT_H__
#define __NUGU_EVENT_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nugu_event.h
 * @defgroup NuguEvent NUGU Event
 * @ingroup SDKCore
 * @brief The message object sent to the server
 *
 * An event is a message sent to the server and consists of the following
 * elements:
 *   - namespace
 *   - name
 *   - version
 *   - message-id
 *   - dialog-request-id
 *   - context
 *   - payload
 *
 * @{
 */

/**
 * @brief Event object
 */
typedef struct _nugu_event NuguEvent;

/**
 * @brief Create new event object
 * @param[in] name_space capability name space (e.g. "ASR")
 * @param[in] name capability name (e.g. "Recognize")
 * @param[in] version version string (e.g. "1.0")
 * @return event object
 * @see nugu_event_free()
 */
NuguEvent *nugu_event_new(const char *name_space, const char *name,
			  const char *version);

/**
 * @brief Destroy the event object
 * @param[in] nev event object
 * @see nugu_event_new()
 */
void nugu_event_free(NuguEvent *nev);

/**
 * @brief Get the namespace of event
 * @param[in] nev event object
 * @return namespace. Please don't free the data manually.
 */
const char *nugu_event_peek_namespace(NuguEvent *nev);

/**
 * @brief Get the name of event
 * @param[in] nev event object
 * @return name. Please don't free the data manually.
 */
const char *nugu_event_peek_name(NuguEvent *nev);

/**
 * @brief Get the version of event
 * @param[in] nev event object
 * @return version. Please don't free the data manually.
 */
const char *nugu_event_peek_version(NuguEvent *nev);

/**
 * @brief Get the message-id of event
 * @param[in] nev event object
 * @return message-id. Please don't free the data manually.
 */
const char *nugu_event_peek_msg_id(NuguEvent *nev);

/**
 * @brief Set text context of event
 * @param[in] nev event object
 * @param[in] context json type context information
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_event_set_context(NuguEvent *nev, const char *context);

/**
 * @brief Get the context of event
 * @param[in] nev event object
 * @return context. Please don't free the data manually.
 */
const char *nugu_event_peek_context(NuguEvent *nev);

/**
 * @brief Set the payload of event
 * @param[in] nev event object
 * @param[in] json json type payload
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_event_set_json(NuguEvent *nev, const char *json);

/**
 * @brief Get the payload of event
 * @param[in] nev event object
 * @return json type payload. Please don't free the data manually.
 */
const char *nugu_event_peek_json(NuguEvent *nev);

/**
 * @brief  Set the dialog-request-id of event
 * @param[in] nev event object
 * @param[in] dialog_id dialog-request-id
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_event_set_dialog_id(NuguEvent *nev, const char *dialog_id);

/**
 * @brief Get the dialog-request-id of event
 * @param[in] nev event object
 * @return dialog-request-id. Please don't free the data manually.
 */
const char *nugu_event_peek_dialog_id(NuguEvent *nev);

/**
 * @brief  Set the referer-dialog-request-id of event
 * @param[in] nev event object
 * @param[in] referrer_id referrer-dialog-request-id
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_event_set_referrer_id(NuguEvent *nev, const char *referrer_id);

/**
 * @brief Get the referer-dialog-request-id of event
 * @param[in] nev event object
 * @return referrer-dialog-request-id. Please don't free the data manually.
 */
const char *nugu_event_peek_referrer_id(NuguEvent *nev);

/**
 * @brief Get the current sequence number of attachment data
 * @param[in] nev event object
 * @return sequence number
 */
int nugu_event_get_seq(NuguEvent *nev);

/**
 * @brief Increase the sequence number
 * @param[in] nev event object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_event_increase_seq(NuguEvent *nev);

/**
 * @brief Generate JSON payload using nugu_event attributes
 * @param[in] nev event object
 * @return memory allocated JSON string. Developer must free the data manually.
 * @retval NULL failure
 */
char *nugu_event_generate_payload(NuguEvent *nev);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
