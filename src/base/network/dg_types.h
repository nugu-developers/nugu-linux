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

#ifndef __NETWORK_DEVICE_GATEWAY_TYPES_H__
#define __NETWORK_DEVICE_GATEWAY_TYPES_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct dg_health_check_policy {
	int ttl_ms;
	int ttl_max_ms;
	float beta;
	int retry_count_limit;
	int retry_delay_ms;
	int health_check_timeout_ms;
	int accumulation_time_ms;
};

struct equeue_data_attachment {
	void *data;
	size_t length;
	char *parent_msg_id;
	char *media_type;
	int is_end;
};

struct equeue_data_request_result {
	int success;
	char *msg_id;
	char *dialog_id;
	int code;
};

#ifdef __cplusplus
}
#endif

#endif
