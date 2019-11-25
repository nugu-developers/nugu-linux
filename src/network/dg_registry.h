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

#ifndef __NETWORK_DEVICE_GATEWAY_REGISTRY_H__
#define __NETWORK_DEVICE_GATEWAY_REGISTRY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "dg_types.h"

typedef struct _dg_registry DGRegistry;

DGRegistry *dg_registry_new(void);
void dg_registry_free(DGRegistry *registry);

int dg_registry_request(DGRegistry *registry);

#ifdef __cplusplus
}
#endif

#endif
