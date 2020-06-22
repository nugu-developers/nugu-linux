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

#ifndef __NUGU_DIRECTIVE_H__
#define __NUGU_DIRECTIVE_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nugu_directive.h
 * @defgroup NuguDirective NUGU Directive
 * @ingroup SDKBase
 * @brief The message object received from the server
 *
 * The directive refers to a message received from the server and consists
 * of the following elements.
 *   - namespace
 *   - name
 *   - version
 *   - message-id
 *   - dialog-request-id
 *   - payload (json)
 *
 * @{
 */

/**
 * @brief Directive object
 */
typedef struct _nugu_directive NuguDirective;

/**
 * @brief Callback prototype for receiving an attachment
 */
typedef void (*NuguDirectiveDataCallback)(NuguDirective *ndir, int seq,
					  void *userdata);

/**
 * @brief Create new directive object
 * @param[in] name_space capability name space (e.g. "TTS")
 * @param[in] name capability name (e.g. "Speak")
 * @param[in] version version string (e.g. "1.0")
 * @param[in] msg_id unique message-id
 * @param[in] dialog_id unique dialog-request-id
 * @param[in] referrer_id referrer-dialog-request-id
 * @param[in] json payload
 * @param[in] groups groups
 * @return directive object
 * @see nugu_directive_free()
 */
NuguDirective *nugu_directive_new(const char *name_space, const char *name,
				  const char *version, const char *msg_id,
				  const char *dialog_id,
				  const char *referrer_id, const char *json,
				  const char *groups);

/**
 * @brief Increment the reference count of the directive object.
 * @param[in] ndir directive object
 * @see nugu_directive_new()
 */
void nugu_directive_ref(NuguDirective *ndir);

/**
 * @brief Decrement the reference count of the directive object.
 * @param[in] ndir directive object
 * @see nugu_directive_new()
 */
void nugu_directive_unref(NuguDirective *ndir);

/**
 * @brief Get the namespace of directive
 * @param[in] ndir directive object
 * @return namespace. Please don't free the data manually.
 */
const char *nugu_directive_peek_namespace(NuguDirective *ndir);

/**
 * @brief Get the name of directive
 * @param[in] ndir directive object
 * @return name. Please don't free the data manually.
 */
const char *nugu_directive_peek_name(NuguDirective *ndir);

/**
 * @brief Get the group of directive
 * @param[in] ndir directive object
 * @return groups. Please don't free the data manually.
 */
const char *nugu_directive_peek_groups(NuguDirective *ndir);

/**
 * @brief Get the version of directive
 * @param[in] ndir directive object
 * @return version. Please don't free the data manually.
 */
const char *nugu_directive_peek_version(NuguDirective *ndir);

/**
 * @brief Get the message-id of directive
 * @param[in] ndir directive object
 * @return message-id. Please don't free the data manually.
 */
const char *nugu_directive_peek_msg_id(NuguDirective *ndir);

/**
 * @brief Get the dialog-request-id of directive
 * @param[in] ndir directive object
 * @return dialog-request-id. Please don't free the data manually.
 */
const char *nugu_directive_peek_dialog_id(NuguDirective *ndir);

/**
 * @brief Get the referer-dialog-request-id of directive
 * @param[in] ndir directive object
 * @return referrer-dialog-request-id. Please don't free the data manually.
 */
const char *nugu_directive_peek_referrer_id(NuguDirective *ndir);

/**
 * @brief Get the payload of directive
 * @param[in] ndir directive object
 * @return json type payload. Please don't free the data manually.
 */
const char *nugu_directive_peek_json(NuguDirective *ndir);

/**
 * @brief Get the active status of directive.
 * "active" means the directive is added to the directive sequencer.
 * @param[in] ndir directive object
 * @return status
 * @retval 1 active
 * @retval 0 not active
 * @retval -1 failure
 * @see nugu_directive_set_active()
 */
int nugu_directive_is_active(NuguDirective *ndir);

/**
 * @brief Set the active status of directive.
 * @param[in] ndir directive object
 * @param[in] flag active status(1 = active, 0 = not active)
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_directive_is_active()
 */
int nugu_directive_set_active(NuguDirective *ndir, int flag);

/**
 * @brief Add attachment data to directive. (e.g. TTS payload)
 * @param[in] ndir directive object
 * @param[in] length length of data
 * @param[in] data data
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_directive_set_data_callback()
 * @see nugu_directive_remove_data_callback()
 * @see nugu_directive_get_data()
 * @see nugu_directive_get_data_size()
 */
int nugu_directive_add_data(NuguDirective *ndir, size_t length,
			    const unsigned char *data);

/**
 * @brief Set the attachment data status to "Received all data"
 * @param[in] ndir directive object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_directive_is_data_end()
 */
int nugu_directive_close_data(NuguDirective *ndir);

/**
 * @brief Get the attachment data status
 * @param[in] ndir directive object
 * @return status
 * @retval 0 Data receiving is not yet completed.
 * @retval 1 Received all data
 * @retval -1 failure
 * @see nugu_directive_close_data()
 */
int nugu_directive_is_data_end(NuguDirective *ndir);

/**
 * @brief Set the attachment mime type
 * @param[in] ndir directive object
 * @param[in] type mime type
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_directive_peek_media_type()
 */
int nugu_directive_set_media_type(NuguDirective *ndir, const char *type);

/**
 * @brief Get the attachment mime type
 * @param[in] ndir directive object
 * @return mime type
 * @see nugu_directive_set_media_type()
 */
const char *nugu_directive_peek_media_type(NuguDirective *ndir);

/**
 * @brief Get the attachment data received so far.
 * When this function is called, the internal receive buffer is cleared.
 * @param[in] ndir directive object
 * @param[out] length attachment length
 * @return received attachment data. Developer must free the data manually.
 * @see nugu_directive_get_data_size()
 */
unsigned char *nugu_directive_get_data(NuguDirective *ndir, size_t *length);

/**
 * @brief Get the size of attachment data received so far.
 * @param[in] ndir directive object
 * @return size of attachment data
 * @see nugu_directive_get_data()
 */
size_t nugu_directive_get_data_size(NuguDirective *ndir);

/**
 * @brief Set attachment received event callback
 * @param[in] ndir directive object
 * @param[in] callback callback function
 * @param[in] userdata data to pass to the user callback
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_directive_remove_data_callback()
 */
int nugu_directive_set_data_callback(NuguDirective *ndir,
				     NuguDirectiveDataCallback callback,
				     void *userdata);

/**
 * @brief Remove attachment received event callback
 * @param[in] ndir directive object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_directive_set_data_callback()
 */
int nugu_directive_remove_data_callback(NuguDirective *ndir);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
