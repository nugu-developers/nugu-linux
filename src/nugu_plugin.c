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
#include <dlfcn.h>

#include "nugu_log.h"
#include "nugu_plugin.h"
#include "nugu_dbus.h"

#define INTROSPECTION_PLUGIN_MANAGER                                           \
	"<node>"                                                               \
	"  <interface name='" NUGU_DBUS_SERVICE ".PluginManager'>"             \
	"    <method name='AddPlugin'>"                                        \
	"      <arg type='s' name='path' direction='in'/>"                     \
	"    </method>"                                                        \
	"    <method name='RemovePlugin'>"                                     \
	"      <arg type='s' name='name' direction='in'/>"                     \
	"    </method>"                                                        \
	"    <signal name='PluginAdded'>"                                      \
	"      <arg type='s' name='path'/>"                                    \
	"    </signal>"                                                        \
	"    <signal name='PluginRemoved'>"                                    \
	"      <arg type='s' name='path'/>"                                    \
	"    </signal>"                                                        \
	"  </interface>"                                                       \
	"</node>"

#define INTROSPECTION_PLUGIN                                                   \
	"<node>"                                                               \
	"  <interface name='" NUGU_DBUS_SERVICE ".Plugin'>"                    \
	"    <property type='s' name='version' access='read'/>"                \
	"    <property type='u' name='priority' access='read'/>"               \
	"  </interface>"                                                       \
	"</node>"

#define OBJECT_PATH_PLUGIN_MANAGER "/Plugins"

struct _plugin {
	const struct nugu_plugin_desc *desc;
	void *data;
	void *handle;
	gboolean active;
	DObject *dbus;
};

static GList *_plugin_list;
static DObject *_dobj_manager;

static GVariant *
_plugin_get_property(GDBusConnection *connection, const gchar *sender,
		     const gchar *object_path, const gchar *interface_name,
		     const gchar *property_name, GError **e, gpointer user_data)
{
	NuguPlugin *p = nugu_dbus_object_get_data(user_data);

	nugu_dbg("property get: '%s' from '%s'", property_name, sender);

	if (!g_strcmp0(property_name, "version"))
		return g_variant_new_string(p->desc->version);
	else if (!g_strcmp0(property_name, "priority"))
		return g_variant_new_uint32(p->desc->priority);

	g_set_error(e, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_PROPERTY,
		    "Property '%s' not initialized", property_name);

	return NULL;
}

static const GDBusInterfaceVTable _plugin_ops = { .method_call = NULL,
						  .get_property =
							  _plugin_get_property,
						  .set_property = NULL };

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
		error_nomem();
		return NULL;
	}

	p->desc = desc;
	p->data = NULL;
	p->handle = NULL;
	p->active = FALSE;

	p->dbus = nugu_dbus_object_new(INTROSPECTION_PLUGIN, &_plugin_ops);
	if (!p->dbus)
		nugu_warn("dbus object creation failed.");

	nugu_dbus_object_set_data(p->dbus, p);

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

	if (p->dbus)
		nugu_dbus_object_free(p->dbus);

	memset(p, 0, sizeof(struct _plugin));
	free(p);
}

EXPORT_API int nugu_plugin_add(NuguPlugin *p)
{
	char *object_path;

	g_return_val_if_fail(p != NULL, -1);

	if (g_list_find(_plugin_list, p))
		return 0;

	_plugin_list = g_list_append(_plugin_list, p);

	object_path = g_strdup_printf("%s/%s", OBJECT_PATH_PLUGIN_MANAGER,
				      p->desc->name);
	nugu_dbus_object_export(p->dbus, object_path);
	g_free(object_path);

	return 0;
}

EXPORT_API int nugu_plugin_remove(NuguPlugin *p)
{
	g_return_val_if_fail(p != NULL, -1);

	if (g_list_find(_plugin_list, p) == NULL)
		return -1;

	_plugin_list = g_list_remove(_plugin_list, p);

	nugu_dbus_object_unexport(p->dbus);

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
	NuguPlugin *p;
	GDir *dir;

	g_return_val_if_fail(dirpath != NULL, -1);

	dir = g_dir_open(dirpath, 0, NULL);
	if (!dir) {
		nugu_error("g_dir_open(%s) failed", dirpath);
		return -1;
	}

	while ((file = g_dir_read_name(dir)) != NULL) {
		if (g_str_has_prefix(file, "lib") == TRUE ||
		    g_str_has_suffix(file, ".so") == FALSE)
			continue;

		filename = g_build_filename(dirpath, file, NULL);
		p = nugu_plugin_new_from_file(filename);
		if (!p) {
			g_free(filename);
			continue;
		}

		g_free(filename);

		if (nugu_plugin_add(p) < 0)
			nugu_plugin_free(p);
	}

	g_dir_close(dir);

	return 0;
}

static gint _sort_priority_cmp(gconstpointer a, gconstpointer b)
{
	return ((NuguPlugin *)a)->desc->priority -
	       ((NuguPlugin *)b)->desc->priority;
}

static void _plugin_manager_method_call(
	GDBusConnection *connection, const gchar *sender,
	const gchar *object_path, const gchar *interface_name,
	const gchar *method_name, GVariant *parameters,
	GDBusMethodInvocation *invocation, gpointer user_data)
{
	const gchar *param_str;

	nugu_dbg("method: '%s' from '%s'", method_name, sender);

	g_variant_get(parameters, "(&s)", &param_str);
	if (!param_str || strlen(param_str) == 0) {
		g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
						      G_DBUS_ERROR_FAILED,
						      "invalid parameter");
		return;
	}

	if (!g_strcmp0(method_name, "AddPlugin")) {
		NuguPlugin *p;

		p = nugu_plugin_new_from_file(param_str);
		if (!p) {
			g_dbus_method_invocation_return_error(
				invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
				"Plugin load '%s' failed.", param_str);
			return;
		}

		if (p->desc->init(p) < 0) {
			nugu_plugin_free(p);
			g_dbus_method_invocation_return_error(
				invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
				"Plugin initialize failed.");
			return;
		}

		nugu_plugin_add(p);
		p->active = TRUE;

		g_dbus_method_invocation_return_value(invocation,
						      g_variant_new("()"));
		return;
	} else if (!g_strcmp0(method_name, "RemovePlugin")) {
		NuguPlugin *p;

		p = nugu_plugin_find(param_str);
		if (!p) {
			g_dbus_method_invocation_return_error(
				invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
				"Can't find '%s' plugin", param_str);
			return;
		}

		nugu_plugin_remove(p);
		nugu_plugin_free(p);
		g_dbus_method_invocation_return_value(invocation,
						      g_variant_new("()"));
		return;
	}

	g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
					      G_DBUS_ERROR_FAILED,
					      "Internal error");
}

static const GDBusInterfaceVTable _plugin_manager_ops = {
	.method_call = _plugin_manager_method_call,
	.get_property = NULL,
	.set_property = NULL
};

EXPORT_API void nugu_plugin_initialize(void)
{
	GList *cur;

	_dobj_manager = nugu_dbus_object_new(INTROSPECTION_PLUGIN_MANAGER,
					     &_plugin_manager_ops);
	if (!_dobj_manager) {
		nugu_error("can't create dbus object for Plugins");
		return;
	}

	nugu_dbus_object_export(_dobj_manager, OBJECT_PATH_PLUGIN_MANAGER);

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
}

EXPORT_API void nugu_plugin_deinitialize(void)
{
	GList *cur;

	if (_dobj_manager) {
		nugu_dbus_object_free(_dobj_manager);
		_dobj_manager = NULL;
	}

	if (_plugin_list == NULL)
		return;

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
