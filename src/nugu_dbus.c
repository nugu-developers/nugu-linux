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

#include <glib.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>

#include "nugu_log.h"
#include "nugu_dbus.h"

struct _dobject {
	GDBusNodeInfo *intro;
	guint export_id;
	char *path;
	const GDBusInterfaceVTable *ops;
	void *data;
};

static guint _id;
static GDBusConnection *_dbus_conn;

static void _bus_acquired(GDBusConnection *conn, const gchar *name,
			  gpointer user_data)
{
	nugu_dbg("dbus bus acquired");
	_dbus_conn = conn;
}

static void _name_acquired(GDBusConnection *connection, const gchar *name,
			   gpointer user_data)
{
	nugu_dbus_register_callback cb = user_data;

	nugu_dbg("dbus name('%s') acquired", name);

	if (cb)
		cb(DBUS_NAME_ACQUIRED);
}

static void _name_lost(GDBusConnection *connection, const gchar *name,
		       gpointer user_data)
{
	nugu_dbus_register_callback cb = user_data;

	nugu_dbg("dbus name('%s') lost", name);

	if (cb)
		cb(DBUS_NAME_LOST);
}

EXPORT_API GDBusConnection *nugu_dbus_get(void)
{
	return _dbus_conn;
}

EXPORT_API int nugu_dbus_register(nugu_dbus_register_callback cb)
{
	g_return_val_if_fail(cb != NULL, -1);

	_id = g_bus_own_name(G_BUS_TYPE_SYSTEM, NUGU_DBUS_SERVICE,
			     G_BUS_NAME_OWNER_FLAGS_REPLACE, _bus_acquired,
			     _name_acquired, _name_lost, cb, NULL);
	if (_id == 0) {
		nugu_error("g_bus_own_name() failed");
		return -1;
	}

	return 0;
}

EXPORT_API void nugu_dbus_unregister(void)
{
	if (_id == 0)
		return;

	g_bus_unown_name(_id);
	_id = 0;
}

EXPORT_API DObject *nugu_dbus_object_new(const char *introspection,
					 const GDBusInterfaceVTable *ops)
{
	DObject *dobj;
	GError *e = NULL;
	GDBusNodeInfo *intro;

	g_return_val_if_fail(introspection != NULL, NULL);
	g_return_val_if_fail(ops != NULL, NULL);

	intro = g_dbus_node_info_new_for_xml(introspection, &e);
	if (!intro) {
		nugu_error("g_dbus_node_info_new_for_xml() failed. %s",
			   e->message);
		g_error_free(e);
		return NULL;
	}

	dobj = malloc(sizeof(struct _dobject));
	if (!dobj) {
		error_nomem();
		g_dbus_node_info_unref(intro);
		return NULL;
	}

	dobj->intro = intro;
	dobj->ops = ops;
	dobj->export_id = 0;
	dobj->path = NULL;

	return dobj;
}

EXPORT_API int nugu_dbus_object_export(DObject *dobj, const char *path)
{
	GError *e = NULL;

	g_return_val_if_fail(dobj != NULL, -1);
	g_return_val_if_fail(path != NULL, -1);

	if (!_dbus_conn)
		return -1;

	if (dobj->export_id != 0)
		return 0;

	if (g_variant_is_object_path(path) == FALSE) {
		nugu_error("invalid name('%s'). allow only [A-Z][a-z][0-9]_",
			   path);
		return -1;
	}

	dobj->export_id =
		g_dbus_connection_register_object(_dbus_conn, path,
						  dobj->intro->interfaces[0],
						  dobj->ops, dobj, NULL, &e);
	if (!dobj->export_id) {
		nugu_error("Error: %s", e->message);
		g_error_free(e);
		return -1;
	}

	dobj->path = g_strdup(path);

	return 0;
}

EXPORT_API int nugu_dbus_object_unexport(DObject *dobj)
{
	g_return_val_if_fail(dobj != NULL, -1);

	if (!_dbus_conn)
		return -1;

	if (dobj->export_id == 0)
		return 0;

	if (g_dbus_connection_unregister_object(_dbus_conn, dobj->export_id) ==
	    FALSE) {
		nugu_error("unregister_object failed");
		return -1;
	}

	g_free(dobj->path);
	dobj->path = NULL;
	dobj->export_id = 0;

	return 0;
}

EXPORT_API void nugu_dbus_object_free(DObject *dobj)
{
	g_return_if_fail(dobj != NULL);

	if (dobj->export_id)
		g_dbus_connection_unregister_object(_dbus_conn,
						    dobj->export_id);

	if (dobj->intro)
		g_dbus_node_info_unref(dobj->intro);

	g_free(dobj->path);

	memset(dobj, 0, sizeof(struct _dobject));
	free(dobj);
}

EXPORT_API int nugu_dbus_object_set_data(DObject *dobj, void *data)
{
	g_return_val_if_fail(dobj != NULL, -1);

	dobj->data = data;

	return 0;
}

EXPORT_API void *nugu_dbus_object_get_data(DObject *dobj)
{
	g_return_val_if_fail(dobj != NULL, NULL);

	return dobj->data;
}

EXPORT_API const char *nugu_dbus_object_peek_path(DObject *dobj)
{
	g_return_val_if_fail(dobj != NULL, NULL);

	return dobj->path;
}
