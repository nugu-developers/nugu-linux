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

#ifndef __NUGU_PROF_H__
#define __NUGU_PROF_H__

#include <base/nugu_uuid.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nugu_prof.h
 * @defgroup Profiling Profiling
 * @ingroup SDKBase
 * @brief Profiling functions
 *
 * The profiling module provides notification and performance measurement
 * for each condition.
 *
 * @{
 */

/**
 * @brief Profiling type list
 * @see nugu_prof_mark()
 * @see nugu_prof_mark_data()
 * @see nugu_prof_get_last_data()
 */
enum nugu_prof_type {
	NUGU_PROF_TYPE_SDK_CREATED,
	/**< SDK created. timestamp baseline */

	NUGU_PROF_TYPE_SDK_PLUGIN_INIT_START,
	/**< Plugin loading and init start */

	NUGU_PROF_TYPE_SDK_PLUGIN_INIT_DONE,
	/**< All plugin initialized */

	NUGU_PROF_TYPE_SDK_INIT_DONE,
	/**< SDK initialized and ready */

	NUGU_PROF_TYPE_NETWORK_CONNECT_REQUEST,
	/**< Network manager connect */

	NUGU_PROF_TYPE_NETWORK_REGISTRY_REQUEST,
	/**< HTTP/2 Request for GET /v1/policies */

	NUGU_PROF_TYPE_NETWORK_REGISTRY_RESPONSE,
	/**< HTTP/2 Response for GET /v1/policies */

	NUGU_PROF_TYPE_NETWORK_REGISTRY_FAILED,
	/**< HTTP/2 Failed for GET /v1/policies */

	NUGU_PROF_TYPE_NETWORK_SERVER_ESTABLISH_REQUEST,
	/**< HTTP/2 Request for GET /v2/directives */

	NUGU_PROF_TYPE_NETWORK_SERVER_ESTABLISH_RESPONSE,
	/**< HTTP/2 long polling established for /v2/directives */

	NUGU_PROF_TYPE_NETWORK_SERVER_ESTABLISH_FAILED,
	/**< HTTP/2 Failed for GET /v2/directives */

	NUGU_PROF_TYPE_NETWORK_CONNECTED,
	/**< Network manager connected */

	NUGU_PROF_TYPE_NETWORK_DIRECTIVES_CLOSED,
	/**< HTTP/2 /v2/directives stream closed by server */

	NUGU_PROF_TYPE_NETWORK_DNS_FAILED,
	/**< DNS Resolving failed */

	NUGU_PROF_TYPE_NETWORK_SSL_FAILED,
	/**< SSL failed */

	NUGU_PROF_TYPE_NETWORK_INTERNAL_ERROR,
	/**< Network internal error */

	NUGU_PROF_TYPE_NETWORK_INVALID_TOKEN,
	/**< Invalid token */

	NUGU_PROF_TYPE_NETWORK_PING_REQUEST,
	/**< HTTP/2 Request for GET /v2/ping */

	NUGU_PROF_TYPE_NETWORK_PING_RESPONSE,
	/**< HTTP/2 Response for GET /v2/ping */

	NUGU_PROF_TYPE_NETWORK_PING_FAILED,
	/**< HTTP/2 Failed for GET /v2/ping */

	NUGU_PROF_TYPE_NETWORK_EVENT_REQUEST,
	/**< HTTP/2 Request for POST /v2/events */

	NUGU_PROF_TYPE_NETWORK_EVENT_RESPONSE,
	/**< HTTP/2 Response for POST /v2/events */

	NUGU_PROF_TYPE_NETWORK_EVENT_FAILED,
	/**< HTTP/2 Failed for POST /v2/events */

	NUGU_PROF_TYPE_NETWORK_EVENT_ATTACHMENT_REQUEST,
	/**< HTTP/2 Request for POST /v2/events attachment */

	NUGU_PROF_TYPE_NETWORK_EVENT_ATTACHMENT_RESPONSE,
	/**< HTTP/2 Response for POST /v2/events attachment */

	NUGU_PROF_TYPE_NETWORK_EVENT_ATTACHMENT_FAILED,
	/**< HTTP/2 Failed for POST /v2/events attachment */

	NUGU_PROF_TYPE_LAST_SERVER_INITIATIVE_DATA,
	/**< Last received data from /v2/directives */

	NUGU_PROF_TYPE_WAKEUP_KEYWORD_DETECTED,
	/**< Wakeup keyword detected */

	NUGU_PROF_TYPE_ASR_LISTENING_STARTED,
	/**< ASR listening started */

	NUGU_PROF_TYPE_ASR_RECOGNIZE,
	/**< ASR.Recognize event */

	NUGU_PROF_TYPE_ASR_RECOGNIZING_STARTED,
	/**< ASR recognizing started */

	NUGU_PROF_TYPE_ASR_END_POINT_DETECTED,
	/**< ASR end point detected */

	NUGU_PROF_TYPE_ASR_TIMEOUT,
	/**< ASR listening timeout */

	NUGU_PROF_TYPE_ASR_FIRST_ATTACHMENT,
	/**< ASR first attachment */

	NUGU_PROF_TYPE_ASR_LAST_ATTACHMENT,
	/**< ASR last attachment */

	NUGU_PROF_TYPE_ASR_RESULT,
	/**< ASR result received */

	NUGU_PROF_TYPE_TTS_SPEAK_DIRECTIVE,
	/**< TTS.Speak directive received */

	NUGU_PROF_TYPE_TTS_FAILED,
	/**< TTS play failed */

	NUGU_PROF_TYPE_TTS_STARTED,
	/**< TTS started */

	NUGU_PROF_TYPE_TTS_FIRST_ATTACHMENT,
	/**< TTS receive first attachment */

	NUGU_PROF_TYPE_TTS_LAST_ATTACHMENT,
	/**< TTS receive last attachment */

	NUGU_PROF_TYPE_TTS_STOPPED,
	/**< TTS stopped */

	NUGU_PROF_TYPE_TTS_FINISHED,
	/**< TTS finished */

	NUGU_PROF_TYPE_AUDIO_STARTED,
	/**< AudioPlayer started */

	NUGU_PROF_TYPE_AUDIO_FINISHED,
	/**< AudioPlayer finished */

	NUGU_PROF_TYPE_MAX
	/**< Just last value */
};

/**
 * @brief Profiling raw data
 * @see NuguProfCallback
 */
struct nugu_prof_data {
	enum nugu_prof_type type; /**< profiling type */
	char dialog_id[NUGU_MAX_UUID_STRING_SIZE + 1]; /* dialog request id */
	char msg_id[NUGU_MAX_UUID_STRING_SIZE + 1]; /* message id */
	struct timespec timestamp; /* timestamp */
	char *contents; /* additional contents */
};

/**
 * @brief Callback prototype for receiving an attachment
 * @see nugu_prof_set_callback()
 */
typedef void (*NuguProfCallback)(enum nugu_prof_type type,
				 const struct nugu_prof_data *data,
				 void *userdata);

/**
 * @brief clear all cached profiling data
 */
void nugu_prof_clear(void);

/**
 * @brief turn on the profiling trace log message
 */
void nugu_prof_enable_tracelog(void);

/**
 * @brief turn off the profiling trace log message
 */
void nugu_prof_disable_tracelog(void);

/**
 * @brief Set profiling callback
 * @param[in] callback callback function
 * @param[in] userdata data to pass to the user callback
 */
void nugu_prof_set_callback(NuguProfCallback callback, void *userdata);

/**
 * @brief Marking to profiling data and emit the callback
 * @param[in] type profiling type
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_prof_mark(enum nugu_prof_type type);

/**
 * @brief Marking to profiling data with additional id and emit the callback
 * @param[in] type profiling type
 * @param[in] dialog_id dialog request id
 * @param[in] msg_id message id
 * @param[in] contents additional contents
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_prof_mark_data(enum nugu_prof_type type, const char *dialog_id,
			const char *msg_id, const char *contents);

/**
 * @brief Get last cached data by profiling type
 * @param[in] type profiling type
 * @return memory allocated nugu_prof_data struct. developer must free the data.
 */
struct nugu_prof_data *nugu_prof_get_last_data(enum nugu_prof_type type);

/**
 * @brief Get the time difference(ts2 - ts1) value in milliseconds
 * @param[in] ts1 time value
 * @param[in] ts2 time value
 * @return milliseconds
 */
int nugu_prof_get_diff_msec_timespec(const struct timespec *ts1,
				     const struct timespec *ts2);

/**
 * @brief Get the time difference(prof2 - prof1) value in milliseconds
 * @param[in] prof1 time value
 * @param[in] prof2 time value
 * @return milliseconds
 */
int nugu_prof_get_diff_msec(const struct nugu_prof_data *prof1,
			    const struct nugu_prof_data *prof2);

/**
 * @brief Dump the profiling data between 'from' type to 'to' type
 * @param[in] from start type to dump
 * @param[in] to end type to dump
 */
void nugu_prof_dump(enum nugu_prof_type from, enum nugu_prof_type to);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
