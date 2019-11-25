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

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_ADDRESS 255

enum dg_protocol {
	DG_PROTOCOL_H2, /**< HTTP/2 with TLS */
	DG_PROTOCOL_H2C, /**< HTTP/2 over clean TCP */
	DG_PROTOCOL_UNKNOWN
};

struct dg_server_policy {
	enum dg_protocol protocol;
	char address[MAX_ADDRESS + 1];
	char hostname[MAX_ADDRESS + 1];
	int port;
	int retry_count_limit;
	int connection_timeout_ms;
	int is_charge;
};

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

#ifdef __cplusplus
}
#endif

#endif
