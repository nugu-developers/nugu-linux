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

#ifndef __HTTP2_GATEWAY_REGISTRY_H__
#define __HTTP2_GATEWAY_REGISTRY_H__

#include <glib.h>

#include "http2_network.h"

#ifdef __cplusplus
extern "C" {
#endif

struct _gateway_server {
	char *protocol;
	char *address;
	char *hostname;
	int port;
	int retry_count_limit;
	int connection_timeout;
	char *charge;
};

struct _gateway_health_policy {
	int ttl;
	int ttl_max;
	float beta;
	int retry_count_limit;
	int retry_delay;
	int health_check_timeout;
	int accumulation_time;
};

typedef struct _gateway_server GatewayServer;
typedef struct _gateway_health_policy GatewayHealthPolicy;
typedef struct _gateway_registry GatewayRegistry;
typedef void (*GatewayRegistryCallback)(GatewayRegistry *registry,
					int response_code, void *userdata);

GatewayRegistry *gateway_registry_new();
void gateway_registry_free(GatewayRegistry *registry);

int gateway_registry_set_header(GatewayRegistry *registry, const char *header);

int gateway_registry_set_callback(GatewayRegistry *registry,
				  GatewayRegistryCallback callback,
				  void *userdata);
int gateway_registry_policy(GatewayRegistry *registry, HTTP2Network *net);

GList *gateway_registry_get_servers(GatewayRegistry *registry);
void gateway_registry_server_list_free(GList *servers);

int gateway_registry_get_health_policy(GatewayRegistry *registry,
				       GatewayHealthPolicy *policy);

#ifdef __cplusplus
}
#endif

#endif
