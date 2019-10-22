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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include "nugu_config.h"

static GHashTable *_default_hash;

EXPORT_API void nugu_config_initialize(void)
{
	if (!_default_hash)
		_default_hash = g_hash_table_new_full(g_str_hash, g_str_equal,
						      g_free, g_free);
}

EXPORT_API void nugu_config_deinitialize(void)
{
	if (_default_hash) {
		g_hash_table_remove_all(_default_hash);
		_default_hash = NULL;
	}
}

EXPORT_API const char *nugu_config_get(const char *key)
{
	g_return_val_if_fail(key != NULL, NULL);
	g_return_val_if_fail(_default_hash != NULL, NULL);

	return (const char *)g_hash_table_lookup(_default_hash, key);
}

EXPORT_API int nugu_config_set(const char *key, const char *value)
{
	g_return_val_if_fail(key != NULL, -1);
	g_return_val_if_fail(_default_hash != NULL, -1);

	if (g_hash_table_lookup(_default_hash, key)) {
		g_hash_table_replace(_default_hash, g_strdup(key),
				     g_strdup(value));
	} else {
		g_hash_table_insert(_default_hash, g_strdup(key),
				    g_strdup(value));
	}

	return 0;
}
