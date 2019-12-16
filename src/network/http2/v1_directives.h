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

#ifndef __HTTP2_V1_DIRECTIVES_H__
#define __HTTP2_V1_DIRECTIVES_H__

#include "http2/http2_network.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _v1_directives V1Directives;

V1Directives *v1_directives_new(const char *host, int connection_timeout_secs);
void v1_directives_free(V1Directives *dir);

int v1_directives_establish(V1Directives *dir, HTTP2Network *net);
int v1_directives_establish_sync(V1Directives *dir, HTTP2Network *net);

#ifdef __cplusplus
}
#endif

#endif
