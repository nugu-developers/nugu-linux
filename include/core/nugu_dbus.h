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

#ifndef __NUGU_DBUS_H__
#define __NUGU_DBUS_H__

#include <glib.h>
#include <gio/gio.h>

#ifndef NUGU_DBUS_SERVICE
#define NUGU_DBUS_SERVICE "com.sktnugu.nugud"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _dobject DObject;

enum nugu_dbus_name_status {
	DBUS_NAME_ACQUIRED,
	DBUS_NAME_LOST
};

typedef void (*nugu_dbus_register_callback)(enum nugu_dbus_name_status status);

int nugu_dbus_register(nugu_dbus_register_callback cb);
void nugu_dbus_unregister(void);
GDBusConnection *nugu_dbus_get(void);

DObject *nugu_dbus_object_new(const char *introspection,
			      const GDBusInterfaceVTable *ops);
int nugu_dbus_object_export(DObject *dobj, const char *path);
int nugu_dbus_object_unexport(DObject *dobj);
void nugu_dbus_object_free(DObject *dobj);
int nugu_dbus_object_set_data(DObject *dobj, void *data);
void *nugu_dbus_object_get_data(DObject *dobj);
const char *nugu_dbus_object_peek_path(DObject *dobj);

#ifdef __cplusplus
}
#endif

#endif
