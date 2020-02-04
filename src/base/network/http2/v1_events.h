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

#ifndef __HTTP2_V1_EVENTS_H__
#define __HTTP2_V1_EVENTS_H__

#include "http2/http2_network.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _v1_events V1Events;

V1Events *v1_events_new(const char *host, HTTP2Network *net);
void v1_events_free(V1Events *event);

int v1_events_send_json(V1Events *event, const char *data, size_t length);
int v1_events_send_binary(V1Events *event, int seq, int is_end, size_t length,
			  unsigned char *data);
int v1_events_send_done(V1Events *event);

#ifdef __cplusplus
}
#endif

#endif
