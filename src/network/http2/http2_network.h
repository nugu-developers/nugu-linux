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

#ifndef __HTTP2_NETWORK_H__
#define __HTTP2_NETWORK_H__

#include "http2_request.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _http2_network HTTP2Network;

HTTP2Network *http2_network_new();
void http2_network_free(HTTP2Network *net);

int http2_network_add_request(HTTP2Network *net, HTTP2Request *req);
int http2_network_remove_request_sync(HTTP2Network *net, HTTP2Request *req);

int http2_network_start(HTTP2Network *net);

void http2_network_enable_curl_log(HTTP2Network *net);
void http2_network_disable_curl_log(HTTP2Network *net);

#ifdef __cplusplus
}
#endif

#endif
