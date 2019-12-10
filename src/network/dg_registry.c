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

#include <stdlib.h>
#include <string.h>

#include "nugu_log.h"
#include "nugu_network_manager.h"

#include "dg_registry.h"
#include "http2/v1_policies.h"

struct _dg_registry {
	HTTP2Network *net;
};

DGRegistry *dg_registry_new(void)
{
	struct _dg_registry *registry;

	registry = calloc(1, sizeof(struct _dg_registry));
	if (!registry) {
		error_nomem();
		return NULL;
	}

	registry->net = http2_network_new();
	if (!registry->net) {
		free(registry);
		return NULL;
	}

	http2_network_set_token(registry->net,
				nugu_network_manager_peek_token());
	http2_network_enable_curl_log(registry->net);
	http2_network_start(registry->net);

	return registry;
}

void dg_registry_free(DGRegistry *registry)
{
	g_return_if_fail(registry != NULL);

	http2_network_free(registry->net);
	registry->net = NULL;

	memset(registry, 0, sizeof(struct _dg_registry));
	free(registry);
}

int dg_registry_request(DGRegistry *registry)
{
	const char *host;

	g_return_val_if_fail(registry != NULL, -1);
	g_return_val_if_fail(registry->net != NULL, -1);

	host = nugu_network_manager_peek_registry_url();
	if (!host) {
		nugu_error("Gateway registry host is not set.");
		return -1;
	}

	return v1_policies_get(registry->net, host);
}
