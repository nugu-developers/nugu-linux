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
#include <dlfcn.h>

#include <glib.h>

#include "base/nugu_log.h"
#include "base/nugu_plugin.h"

struct _plugin {
	const struct nugu_plugin_desc *desc;
	void *data;
	void *handle;
	gboolean active;
};

static GList *_plugin_list;

EXPORT_API NuguPlugin *nugu_plugin_new(struct nugu_plugin_desc *desc)
{
	NuguPlugin *p;

	g_return_val_if_fail(desc != NULL, NULL);
	g_return_val_if_fail(desc->load != NULL, NULL);
	g_return_val_if_fail(desc->init != NULL, NULL);
	g_return_val_if_fail(desc->name != NULL, NULL);

	if (desc->load() < 0)
		return NULL;

	p = malloc(sizeof(struct _plugin));
	if (!p) {
		nugu_error_nomem();
		return NULL;
	}

	p->desc = desc;
	p->data = NULL;
	p->handle = NULL;
	p->active = FALSE;

	return p;
}

EXPORT_API NuguPlugin *nugu_plugin_new_from_file(const char *filepath)
{
	void *handle;
	struct nugu_plugin_desc *desc;
	NuguPlugin *p;

	g_return_val_if_fail(filepath != NULL, NULL);

	handle = dlopen(filepath, RTLD_NOW | RTLD_LOCAL);
	if (!handle) {
		nugu_error("dlopen failed: '%s' plugin. %s", filepath,
			   dlerror());
		return NULL;
	}

	desc = dlsym(handle, NUGU_PLUGIN_SYMBOL);
	if (!desc) {
		nugu_error("dlsym failed: %s", dlerror());
		dlclose(handle);
		return NULL;
	}

	p = nugu_plugin_new(desc);
	if (!p) {
		dlclose(handle);
		return NULL;
	}

	p->handle = handle;

	return p;
}

EXPORT_API void nugu_plugin_free(NuguPlugin *p)
{
	g_return_if_fail(p != NULL);

	if (p->active && p->desc && p->desc->unload)
		p->desc->unload(p);

	if (p->handle)
		dlclose(p->handle);

	memset(p, 0, sizeof(struct _plugin));
	free(p);
}

EXPORT_API int nugu_plugin_add(NuguPlugin *p)
{
	g_return_val_if_fail(p != NULL, -1);

	if (g_list_find(_plugin_list, p))
		return 0;

	_plugin_list = g_list_append(_plugin_list, p);

	return 0;
}

EXPORT_API int nugu_plugin_remove(NuguPlugin *p)
{
	g_return_val_if_fail(p != NULL, -1);

	if (g_list_find(_plugin_list, p) == NULL)
		return -1;

	_plugin_list = g_list_remove(_plugin_list, p);

	return 0;
}

EXPORT_API int nugu_plugin_set_data(NuguPlugin *p, void *data)
{
	g_return_val_if_fail(p != NULL, -1);

	p->data = data;

	return 0;
}

EXPORT_API void *nugu_plugin_get_data(NuguPlugin *p)
{
	g_return_val_if_fail(p != NULL, NULL);

	return p->data;
}

EXPORT_API void *nugu_plugin_get_symbol(NuguPlugin *p, const char *symbol_name)
{
	g_return_val_if_fail(p != NULL, NULL);
	g_return_val_if_fail(symbol_name != NULL, NULL);

	return dlsym(p->handle, symbol_name);
}

EXPORT_API const struct nugu_plugin_desc *
nugu_plugin_get_description(NuguPlugin *p)
{
	g_return_val_if_fail(p != NULL, NULL);

	return p->desc;
}

EXPORT_API int nugu_plugin_load_directory(const char *dirpath)
{
	const gchar *file;
	gchar *filename;
	gchar *env_dirpath;
	const char *plugin_path;
	NuguPlugin *p;
	GDir *dir;

	env_dirpath = getenv(NUGU_ENV_PLUGIN_PATH);
	if (env_dirpath)
		plugin_path = (const char *)env_dirpath;
	else
		plugin_path = dirpath;

	g_return_val_if_fail(plugin_path != NULL, -1);

	dir = g_dir_open(plugin_path, 0, NULL);
	if (!dir) {
		nugu_error("g_dir_open(%s) failed", plugin_path);
		return -1;
	}

	while ((file = g_dir_read_name(dir)) != NULL) {
		if (g_str_has_prefix(file, "lib") == TRUE ||
		    g_str_has_suffix(file, ".so") == FALSE)
			continue;

		filename = g_build_filename(plugin_path, file, NULL);
		p = nugu_plugin_new_from_file(filename);
		if (!p) {
			g_free(filename);
			continue;
		}

		g_free(filename);

		if (nugu_plugin_add(p) < 0)
			nugu_plugin_free(p);
	}

	// add the comment below to exclude code checking
	// NOLINTNEXTLINE (clang-analyzer-unix.Malloc)
	g_dir_close(dir);

	if (!_plugin_list)
		return 0;
	else
		return g_list_length(_plugin_list);
}

static gint _sort_priority_cmp(gconstpointer a, gconstpointer b)
{
	return ((NuguPlugin *)a)->desc->priority -
	       ((NuguPlugin *)b)->desc->priority;
}

EXPORT_API int nugu_plugin_initialize(void)
{
	GList *cur;

	if (!_plugin_list)
		return 0;

	_plugin_list = g_list_sort(_plugin_list, _sort_priority_cmp);

	cur = _plugin_list;
	while (cur) {
		NuguPlugin *p = cur->data;

		if (p->desc->init(p) < 0) {
			nugu_plugin_free(p);
			cur->data = NULL;
		} else
			p->active = TRUE;

		cur = cur->next;
	}

	/* Remove failed plugins from managed list */
	cur = _plugin_list;
	while (cur) {
		if (cur->data != NULL) {
			cur = cur->next;
			continue;
		}

		cur = _plugin_list = g_list_remove_link(_plugin_list, cur);
	}

	if (!_plugin_list)
		return 0;
	else
		return g_list_length(_plugin_list);
}

EXPORT_API void nugu_plugin_deinitialize(void)
{
	GList *cur;

	if (_plugin_list == NULL)
		return;

	_plugin_list = g_list_reverse(_plugin_list);

	for (cur = _plugin_list; cur; cur = cur->next) {
		NuguPlugin *p = cur->data;

		if (p->active && p->desc && p->desc->unload) {
			p->desc->unload(p);
			p->active = FALSE;
		}
	}

	g_list_free_full(_plugin_list, (GDestroyNotify)nugu_plugin_free);
	_plugin_list = NULL;
}

EXPORT_API NuguPlugin *nugu_plugin_find(const char *name)
{
	GList *cur;

	g_return_val_if_fail(name != NULL, NULL);

	cur = _plugin_list;
	while (cur) {
		if (g_strcmp0(((NuguPlugin *)cur->data)->desc->name, name) == 0)
			return cur->data;

		cur = cur->next;
	}

	return NULL;
}
