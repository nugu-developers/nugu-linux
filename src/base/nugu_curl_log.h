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

#ifndef __NUGU_CURL_LOG_H__
#define __NUGU_CURL_LOG_H__

#ifdef __cplusplus
extern "C" {
#endif

enum curl_log_type {
	CURL_LOG_TYPE_TEXT,
	CURL_LOG_TYPE_HEADER,
	CURL_LOG_TYPE_DATA
};

enum curl_log_dir {
	CURL_LOG_DIR_NONE, /* no direction */
	CURL_LOG_DIR_RECV,
	CURL_LOG_DIR_SEND
};

int nugu_curl_debug_callback(void *handle, curl_infotype type, char *data,
			     size_t size, void *userptr);

#ifdef __cplusplus
}
#endif

#endif
