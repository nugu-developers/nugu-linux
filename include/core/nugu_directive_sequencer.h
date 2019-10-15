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

#ifndef __NUGU_DIRECTIVE_SEQUENCER_H__
#define __NUGU_DIRECTIVE_SEQUENCER_H__

#include <core/nugu_directive.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nugu_directive_sequencer.h
 * @defgroup nugu_dirseq Directive Sequencer
 * @ingroup SDKCore
 * @brief Directive Sequencer
 *
 * The Directive Sequencer manages all messages (directives) received from
 * the NUGU server.
 *
 * @{
 */

/**
 * @brief Callback return type
 */
enum dirseq_return {
	DIRSEQ_STOP_PROPAGATE, /**< Stop event propagation */
	DIRSEQ_CONTINUE, /**< Pass to the next callback */
	DIRSEQ_REMOVE /**< Remove the directive */
};

/**
 * @brief Callback return type
 * @see enum dirseq_return
 */
typedef enum dirseq_return DirseqReturn;

/**
 * @brief Callback prototype
 * @see DirseqReturn
 */
typedef DirseqReturn (*DirseqCallback)(NuguDirective *ndir, void *userdata);

/**
 * @brief Add event callback to Directive sequencer
 * @param[in] callback callback function
 * @param[in] userdata data to pass to the user callback
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_dirseq_add_callback(DirseqCallback callback, void *userdata);

/**
 * @brief Remove event callback
 * @param[in] callback callback function
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_dirseq_remove_callback(DirseqCallback callback);

/**
 * @brief Add new directive object to directive sequencer
 * @param[in] ndir directive object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_dirseq_push(NuguDirective *ndir);

/**
 * @brief Find the directive by message-id
 * @param[in] msgid message-id
 * @return directive object
 */
NuguDirective *nugu_dirseq_find_by_msgid(const char *msgid);

/**
 * @brief Completes(remove) a specific directive in the directive sequencer.
 * @param[in] ndir directive object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_dirseq_complete(NuguDirective *ndir);

/**
 * @brief Remove all the directives in the sequencer
 */
void nugu_dirseq_deinitialize(void);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
