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

#ifndef __NETWORK_DEVICE_GATEWAY_SERVER_H__
#define __NETWORK_DEVICE_GATEWAY_SERVER_H__

#include "base/nugu_network_manager.h"

#include "dg_types.h"

#ifdef __cplusplus
extern "C" {
#endif

enum dg_server_type {
	DG_SERVER_TYPE_NORMAL,
	DG_SERVER_TYPE_HANDOFF,
};

typedef struct _dg_server DGServer;

DGServer *dg_server_new(const NuguNetworkServerPolicy *policy);
void dg_server_free(DGServer *server);

int dg_server_set_type(DGServer *server, enum dg_server_type type);
enum dg_server_type dg_server_get_type(DGServer *server);

int dg_server_connect_async(DGServer *server);

int dg_server_start_health_check(DGServer *server,
				 const struct dg_health_check_policy *policy);
int dg_server_stop_health_check(DGServer *server);

unsigned int dg_server_get_retry_count(DGServer *server);
unsigned int dg_server_get_retry_count_limit(DGServer *server);
int dg_server_is_retry_over(DGServer *server);
void dg_server_increase_retry_count(DGServer *server);
void dg_server_reset_retry_count(DGServer *server);

int dg_server_send_event(DGServer *server, NuguEvent *nev);
int dg_server_send_attachment(DGServer *server, NuguEvent *nev, int is_end,
			      size_t length, unsigned char *data);
int dg_server_force_close_event(DGServer *server, NuguEvent *nev);

int dg_server_update_last_asr_time(DGServer *server);

#ifdef __cplusplus
}
#endif

#endif
