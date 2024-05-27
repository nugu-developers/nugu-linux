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
#include <glib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "base/nugu_log.h"
#include "base/nugu_plugin.h"

#ifndef _WIN32
#include "builtin.h"
#endif

#ifdef _WIN32
#if 0
#define DYNLIB_HANDLE HMODULE
#define DYNLIB_LOAD(a) LoadLibrary(a)
#define DYNLIB_GETSYM(a, b) GetProcAddress(a, b)
#define DYNLIB_UNLOAD(a) FreeLibrary(a)
#define DYNLIB_ERROR() "dlerror()"
#endif
#else
#define DYNLIB_HANDLE void *
#define DYNLIB_LOAD(a) dlopen(a, RTLD_NOW | RTLD_LOCAL)
#define DYNLIB_GETSYM(a, b) dlsym(a, b)
#define DYNLIB_UNLOAD(a) dlclose(a)
#define DYNLIB_ERROR() dlerror()
#endif

struct _plugin {
	const struct nugu_plugin_desc *desc;
	void *data;
#ifndef _WIN32
	DYNLIB_HANDLE handle;
#endif
	char *filename;
	gboolean active;
};

static GList *_plugin_list;

NuguPlugin *nugu_plugin_new(struct nugu_plugin_desc *desc)
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
#ifndef _WIN32
	p->handle = NULL;
#endif
	p->active = FALSE;
	p->filename = NULL;

	return p;
}

NuguPlugin *nugu_plugin_new_from_file(const char *filepath)
{
#ifndef _WIN32
	void *handle;
	struct nugu_plugin_desc *desc;
	NuguPlugin *p;

	g_return_val_if_fail(filepath != NULL, NULL);

	handle = DYNLIB_LOAD(filepath);
	if (!handle) {
		nugu_error("dlopen failed: '%s' plugin. %s", filepath,
			   DYNLIB_ERROR());
		return NULL;
	}

	desc = DYNLIB_GETSYM(handle, NUGU_PLUGIN_SYMBOL);
	if (!desc) {
		nugu_error("dlsym failed: %s", DYNLIB_ERROR());
		DYNLIB_UNLOAD(handle);
		return NULL;
	}

	p = nugu_plugin_new(desc);
	if (!p) {
		DYNLIB_UNLOAD(handle);
		return NULL;
	}

	p->filename = strdup(filepath);
	p->handle = handle;

	return p;
#else
	return NULL;
#endif
}

void nugu_plugin_free(NuguPlugin *p)
{
	g_return_if_fail(p != NULL);

	if (p->active && p->desc && p->desc->unload)
		p->desc->unload(p);

#ifndef _WIN32
	if (p->handle)
		DYNLIB_UNLOAD(p->handle);
#endif

	if (p->filename)
		free(p->filename);

	memset(p, 0, sizeof(struct _plugin));
	free(p);
}

int nugu_plugin_add(NuguPlugin *p)
{
	GList *cur;

	g_return_val_if_fail(p != NULL, -1);

	for (cur = _plugin_list; cur; cur = cur->next) {
		NuguPlugin *tmp;

		tmp = cur->data;
		if (!tmp)
			continue;

		if (tmp == p) {
			nugu_error("plugin '%s' already registered",
				   p->desc->name);
			return 1;
		}

		if (tmp->filename != NULL && p->filename != NULL) {
			if (g_strcmp0(tmp->filename, p->filename) == 0) {
				nugu_error(
					"plugin file '%s' already registered",
					p->filename);
				return -1;
			}
		}
	}

	_plugin_list = g_list_append(_plugin_list, p);

	return 0;
}

int nugu_plugin_remove(NuguPlugin *p)
{
	g_return_val_if_fail(p != NULL, -1);

	if (g_list_find(_plugin_list, p) == NULL)
		return -1;

	_plugin_list = g_list_remove(_plugin_list, p);

	return 0;
}

int nugu_plugin_set_data(NuguPlugin *p, void *data)
{
	g_return_val_if_fail(p != NULL, -1);

	p->data = data;

	return 0;
}

void *nugu_plugin_get_data(NuguPlugin *p)
{
	g_return_val_if_fail(p != NULL, NULL);

	return p->data;
}

void *nugu_plugin_get_symbol(NuguPlugin *p, const char *symbol_name)
{
	g_return_val_if_fail(p != NULL, NULL);
	g_return_val_if_fail(symbol_name != NULL, NULL);

#ifndef _WIN32
	return DYNLIB_GETSYM(p->handle, symbol_name);
#else
	return NULL;
#endif
}

const struct nugu_plugin_desc *nugu_plugin_get_description(NuguPlugin *p)
{
	g_return_val_if_fail(p != NULL, NULL);

	return p->desc;
}

int nugu_plugin_load_directory(const char *dirpath)
{
#ifndef _WIN32
	const gchar *file;
	gchar *filename;
	gchar *env_dirpath;
	const char *plugin_path;
	NuguPlugin *p;
	GDir *dir;

	env_dirpath = getenv(NUGU_ENV_PLUGIN_PATH);
	if (env_dirpath) {
		plugin_path = (const char *)env_dirpath;
		nugu_info("use environment variable path '%s'", plugin_path);
	} else {
		plugin_path = dirpath;
	}

	g_return_val_if_fail(plugin_path != NULL, -1);

	dir = g_dir_open(plugin_path, 0, NULL);
	if (!dir) {
		nugu_error("g_dir_open(%s) failed", plugin_path);
		return -1;
	}

	while ((file = g_dir_read_name(dir)) != NULL) {
		if (g_str_has_prefix(file, "lib") == TRUE ||
		    g_str_has_suffix(file, NUGU_PLUGIN_FILE_EXTENSION) == FALSE)
			continue;

		filename = g_build_filename(plugin_path, file, NULL);
		p = nugu_plugin_new_from_file(filename);
		if (!p) {
			g_free(filename);
			continue;
		}

		g_free(filename);

		if (nugu_plugin_add(p) != 0)
			nugu_plugin_free(p);
	}

	// add the comment below to exclude code checking
	// NOLINTNEXTLINE (clang-analyzer-unix.Malloc)
	g_dir_close(dir);

	if (!_plugin_list)
		return 0;
	else
		return g_list_length(_plugin_list);
#else
	return 0;
#endif
}

int nugu_plugin_load_builtin(void)
{
/* FIXME: Build error 
 * 9>c:\workspace\git\nugu-linux\src\base\nugu_plugin.c : fatal error C1001: 내부 컴파일러 오류가 발생했습니다. [C:\workspace\git\nugu-linux\build\src\libnugu.vcxproj]
(컴파일러 파일 'D:\a\_work\1\s\src\vctools\Compiler\Utc\src\p2\main.c', 줄 235)
 이 문제를 해결하려면 위 목록에 나오는 위치 부근의 프로그램을 단순화하거나 변경하세요.
가능한 경우 https://developercommunity.visualstudio.com에서 재현해 주세요.
자세한 내용을 보려면 Visual C++ [도움말] 메뉴에서 [기술 지원] 명령을
 선택하거나 기술 지원 도움말 파일을 참조하세요.
  link!RaiseException()+0x69
  link!RaiseException()+0x69
  link!CloseTypeServerPDB()+0x9a5d7
  link!CloseTypeServerPDB()+0x12152b
  link!DllGetObjHandler()+0x1ffc
  link!DllGetObjHandler()+0x1a65
  link!DllGetObjHandler()+0x16c1
  link!InvokeCompilerPassW()+0x3d9
  link!InvokeCompilerPass()+0x7b57
 */
#ifndef _WIN32
	size_t length;
	size_t i;
	NuguPlugin *p;

	length = sizeof(_builtin_list) / sizeof(struct nugu_plugin_desc *);
	nugu_info("built-in plugin count: %d", length);

	for (i = 0; i < length; i++) {
		p = nugu_plugin_new(_builtin_list[i]);
		if (!p) {
			nugu_error("nugu_plugin_new() failed");
			continue;
		}

		if (nugu_plugin_add(p) != 0)
			nugu_plugin_free(p);
	}

	return g_list_length(_plugin_list);
#else
	return 0;
#endif
}

static gint _sort_priority_cmp(gconstpointer a, gconstpointer b)
{
	return ((NuguPlugin *)a)->desc->priority -
	       ((NuguPlugin *)b)->desc->priority;
}

int nugu_plugin_initialize(void)
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

void nugu_plugin_deinitialize(void)
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

NuguPlugin *nugu_plugin_find(const char *name)
{
	GList *cur;

	g_return_val_if_fail(name != NULL, NULL);

	cur = _plugin_list;
	while (cur) {
		NuguPlugin *p = cur->data;

		if (p && g_strcmp0(p->desc->name, name) == 0)
			return cur->data;

		cur = cur->next;
	}

	return NULL;
}
