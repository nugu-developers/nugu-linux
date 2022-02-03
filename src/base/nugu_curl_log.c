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

#include "curl/curl.h"
#include "base/nugu_log.h"
#include "nugu_curl_log.h"

static void _network_curl_log(void *req, enum curl_log_type type,
			      enum curl_log_dir dir, const char *msg,
			      size_t size, char *data)
{
	const char *color = "";
	const char *dir_hint = "";

	/* Stream closed */
	if (size == 0) {
		nugu_log_print(NUGU_LOG_MODULE_NETWORK, NUGU_LOG_LEVEL_WARNING,
			       NULL, NULL, -1, "[CURL] (%p) %s: 0 bytes", req,
			       msg);
		return;
	}

	if (dir == CURL_LOG_DIR_RECV) {
		color = NUGU_ANSI_COLOR_RECV;
		dir_hint = NUGU_LOG_MARK_RECV;
	} else if (dir == CURL_LOG_DIR_SEND) {
		color = NUGU_ANSI_COLOR_SEND;
		dir_hint = NUGU_LOG_MARK_SEND;
	}

	if (type == CURL_LOG_TYPE_DATA) {
		nugu_log_print(
			NUGU_LOG_MODULE_NETWORK, NUGU_LOG_LEVEL_DEBUG, NULL,
			NULL, -1,
			"[CURL] (%p) %s: %s%zd bytes" NUGU_ANSI_COLOR_NORMAL,
			req, msg, color, size);
	} else {
		int flag = 0;

		/* temporary remove a last new-line code */
		if (data[size - 1] == '\n') {
			data[size - 1] = '\0';
			flag = 1;
		}

		if (type == CURL_LOG_TYPE_TEXT)
			/* Curl information text */
			nugu_log_print(NUGU_LOG_MODULE_NETWORK,
				       NUGU_LOG_LEVEL_DEBUG, NULL, NULL, -1,
				       "[CURL] (%p) %s", req, data);
		else
			/* header */
			nugu_log_print(
				NUGU_LOG_MODULE_NETWORK, NUGU_LOG_LEVEL_DEBUG,
				NULL, NULL, -1,
				"[CURL] (%p) %s: %s%s" NUGU_ANSI_COLOR_NORMAL,
				req, msg, color, data);

		if (flag)
			data[size - 1] = '\n';
	}

	/* hexdump for header and data */
	if (type != CURL_LOG_TYPE_TEXT)
		nugu_hexdump(NUGU_LOG_MODULE_NETWORK_TRACE, (uint8_t *)data,
			     size, color, NUGU_ANSI_COLOR_NORMAL, dir_hint);
}

int nugu_curl_debug_callback(void *handle, curl_infotype type, char *data,
			     size_t size, void *userptr)
{
	switch (type) {
	case CURLINFO_TEXT:
		_network_curl_log(userptr, CURL_LOG_TYPE_TEXT,
				  CURL_LOG_DIR_NONE, NULL, size, data);
		break;
	case CURLINFO_HEADER_OUT:
		_network_curl_log(userptr, CURL_LOG_TYPE_HEADER,
				  CURL_LOG_DIR_SEND, "Send header", size, data);
		break;
	case CURLINFO_DATA_OUT:
		_network_curl_log(userptr, CURL_LOG_TYPE_DATA,
				  CURL_LOG_DIR_SEND, "Send data", size, data);
		break;
	case CURLINFO_SSL_DATA_OUT:
		break;
	case CURLINFO_HEADER_IN:
		_network_curl_log(userptr, CURL_LOG_TYPE_HEADER,
				  CURL_LOG_DIR_RECV, "Recv header", size, data);
		break;
	case CURLINFO_DATA_IN:
		_network_curl_log(userptr, CURL_LOG_TYPE_DATA,
				  CURL_LOG_DIR_RECV, "Recv data", size, data);
		break;
	case CURLINFO_SSL_DATA_IN:
		break;
	default:
		break;
	}

	return 0;
}
