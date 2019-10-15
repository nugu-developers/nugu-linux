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
#include <pthread.h>

#include "nugu_log.h"
#include "nugu_player.h"
#include "nugu_dbus.h"

#define INTROSPECTION_PLAYER                                                   \
	"<node>"                                                               \
	"  <interface name='" NUGU_DBUS_SERVICE ".Player'>"                    \
	"    <method name='SET_SOURCE'>"                                       \
	"      <arg type='s' name='url' direction='in'/>"                      \
	"    </method>"                                                        \
	"    <method name='SEEK'>"                                             \
	"      <arg type='s' name='sec' direction='in'/>"                      \
	"    </method>"                                                        \
	"    <method name='START'>"                                            \
	"    </method>"                                                        \
	"    <method name='STOP'>"                                             \
	"    </method>"                                                        \
	"    <method name='PAUSE'>"                                            \
	"    </method>"                                                        \
	"    <method name='RESUME'>"                                           \
	"    </method>"                                                        \
	"    <method name='GET_DURATION'>"                                     \
	"      <arg type='i' name='duration' direction='out'/>"                \
	"    </method>"                                                        \
	"    <method name='GET_POSITION'>"                                     \
	"      <arg type='i' name='pos' direction='out'/>"                     \
	"    </method>"                                                        \
	"    <method name='SET_VOLUME'>"                                       \
	"      <arg type='s' name='vol' direction='in'/>"                      \
	"    </method>"                                                        \
	"    <method name='GET_VOLUME'>"                                       \
	"      <arg type='i' name='vol' direction='out'/>"                     \
	"    </method>"                                                        \
	"  </interface>"                                                       \
	"</node>"

#define OBJECT_PATH_PLAYER "/Player"

struct _nugu_player_driver {
	char *name;
	struct nugu_player_driver_ops *ops;
	int ref_count;
};

struct _nugu_player {
	char *name;
	NuguPlayerDriver *driver;
	enum nugu_media_status status;
	char *playurl;
	int volume;
	mediaEventCallback ecb;
	mediaStatusCallback scb;
	void *eud; /* user data for event callback */
	void *sud; /* user data for status callback */
	void *device;
	DObject *dbus;
};

static GList *_players;
static GList *_player_drivers;
static NuguPlayerDriver *_default_driver;

static void _player_method_call(GDBusConnection *connection,
				const gchar *sender, const gchar *object_path,
				const gchar *interface_name,
				const gchar *method_name, GVariant *parameters,
				GDBusMethodInvocation *invocation,
				gpointer user_data)
{
	nugu_dbg("method: '%s' from '%s'", method_name, sender);

	if (!g_strcmp0(method_name, "SET_SOURCE")) {
		const gchar *param_str;
		NuguPlayer *player = nugu_dbus_object_get_data(user_data);

		g_variant_get(parameters, "(&s)", &param_str);
		if (!param_str || strlen(param_str) == 0) {
			g_dbus_method_invocation_return_error(
				invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
				"invalid parameter");
			return;
		}
		if (nugu_player_set_source(player, param_str) != 0) {
			g_dbus_method_invocation_return_error(
				invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
				"failed to set source");
			return;
		}
		g_dbus_method_invocation_return_value(invocation,
						      g_variant_new("()"));
		return;
	} else if (!g_strcmp0(method_name, "SEEK")) {
		const gchar *param_str;
		NuguPlayer *player = nugu_dbus_object_get_data(user_data);

		g_variant_get(parameters, "(&s)", &param_str);
		if (!param_str || strlen(param_str) == 0) {
			g_dbus_method_invocation_return_error(
				invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
				"invalid parameter");
			return;
		}
		if (nugu_player_seek(player, atoi(param_str)) != 0) {
			g_dbus_method_invocation_return_error(
				invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
				"failed to seek");
			return;
		}
		g_dbus_method_invocation_return_value(invocation,
						      g_variant_new("()"));
		return;
	} else if (!g_strcmp0(method_name, "START")) {
		NuguPlayer *player = nugu_dbus_object_get_data(user_data);

		if (nugu_player_start(player) != 0) {
			g_dbus_method_invocation_return_error(
				invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
				"failed to start");
			return;
		}
		g_dbus_method_invocation_return_value(invocation,
						      g_variant_new("()"));
		return;
	} else if (!g_strcmp0(method_name, "STOP")) {
		NuguPlayer *player = nugu_dbus_object_get_data(user_data);

		if (nugu_player_stop(player) != 0) {
			g_dbus_method_invocation_return_error(
				invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
				"failed to stop");
			return;
		}
		g_dbus_method_invocation_return_value(invocation,
						      g_variant_new("()"));
		return;
	} else if (!g_strcmp0(method_name, "PAUSE")) {
		NuguPlayer *player = nugu_dbus_object_get_data(user_data);

		if (nugu_player_pause(player) != 0) {
			g_dbus_method_invocation_return_error(
				invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
				"failed to pause");
			return;
		}
		g_dbus_method_invocation_return_value(invocation,
						      g_variant_new("()"));
		return;
	} else if (!g_strcmp0(method_name, "RESUME")) {
		NuguPlayer *player = nugu_dbus_object_get_data(user_data);

		if (nugu_player_resume(player) != 0) {
			g_dbus_method_invocation_return_error(
				invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
				"failed to resume");
			return;
		}
		g_dbus_method_invocation_return_value(invocation,
						      g_variant_new("()"));
		return;
	} else if (!g_strcmp0(method_name, "GET_DURATION")) {
		int duration;
		NuguPlayer *player = nugu_dbus_object_get_data(user_data);

		duration = nugu_player_get_duration(player);
		if (duration == -1) {
			g_dbus_method_invocation_return_error(
				invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
				"failed to get duration");
			return;
		}
		g_dbus_method_invocation_return_value(
			invocation, g_variant_new("(i)", duration));
		return;
	} else if (!g_strcmp0(method_name, "GET_POSITION")) {
		int position;
		NuguPlayer *player = nugu_dbus_object_get_data(user_data);

		position = nugu_player_get_position(player);
		if (position == -1) {
			g_dbus_method_invocation_return_error(
				invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
				"failed to get position");
			return;
		}
		g_dbus_method_invocation_return_value(
			invocation, g_variant_new("(i)", position));
		return;
	} else if (!g_strcmp0(method_name, "GET_VOLUME")) {
		int volume;
		NuguPlayer *player = nugu_dbus_object_get_data(user_data);

		volume = nugu_player_get_volume(player);
		if (volume == -1) {
			g_dbus_method_invocation_return_error(
				invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
				"failed to get volume");
			return;
		}
		g_dbus_method_invocation_return_value(
			invocation, g_variant_new("(i)", volume));
		return;
	} else if (!g_strcmp0(method_name, "SET_VOLUME")) {
		const gchar *param_str;
		int volume;
		NuguPlayer *player = nugu_dbus_object_get_data(user_data);

		g_variant_get(parameters, "(&s)", &param_str);
		if (!param_str || strlen(param_str) == 0) {
			g_dbus_method_invocation_return_error(
				invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
				"invalid parameter");
			return;
		}

		volume = atoi(param_str);
		if (nugu_player_set_volume(player, volume) != 0) {
			g_dbus_method_invocation_return_error(
				invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
				"failed to set volume");
			return;
		}
		g_dbus_method_invocation_return_value(invocation,
						      g_variant_new("()"));
		return;
	}

	g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
					      G_DBUS_ERROR_FAILED,
					      "Internal error");
}

static const GDBusInterfaceVTable _player_ops = { .method_call =
							  _player_method_call,
						  .get_property = NULL,
						  .set_property = NULL };

EXPORT_API NuguPlayerDriver *
nugu_player_driver_new(const char *name, struct nugu_player_driver_ops *ops)
{
	NuguPlayerDriver *driver;

	g_return_val_if_fail(name != NULL, NULL);
	g_return_val_if_fail(ops != NULL, NULL);

	driver = g_malloc0(sizeof(struct _nugu_player_driver));
	driver->name = g_strdup(name);
	driver->ops = ops;
	driver->ref_count = 0;

	return driver;
}

EXPORT_API int nugu_player_driver_free(NuguPlayerDriver *driver)
{
	g_return_val_if_fail(driver != NULL, -1);

	if (driver->ref_count != 0) {
		nugu_error("player still using driver");
		return -1;
	}

	g_free(driver->name);

	memset(driver, 0, sizeof(struct _nugu_player_driver));
	g_free(driver);

	return 0;
}

EXPORT_API int nugu_player_driver_register(NuguPlayerDriver *driver)
{
	g_return_val_if_fail(driver != NULL, -1);

	if (nugu_player_driver_find(driver->name)) {
		nugu_error("'%s' player driver already exist.", driver->name);
		return -1;
	}

	_player_drivers = g_list_append(_player_drivers, driver);
	if (!_default_driver)
		nugu_player_driver_set_default(driver);
	return 0;
}

EXPORT_API int nugu_player_driver_remove(NuguPlayerDriver *driver)
{
	GList *l;

	g_return_val_if_fail(driver != NULL, -1);

	l = g_list_find(_player_drivers, driver);
	if (!l)
		return -1;

	if (_default_driver == driver)
		_default_driver = NULL;

	_player_drivers = g_list_remove_link(_player_drivers, l);

	if (_player_drivers && _default_driver == NULL)
		_default_driver = _player_drivers->data;

	return 0;
}

EXPORT_API int nugu_player_driver_set_default(NuguPlayerDriver *driver)
{
	g_return_val_if_fail(driver != NULL, -1);

	_default_driver = driver;

	return 0;
}

EXPORT_API NuguPlayerDriver *nugu_player_driver_get_default(void)
{
	return _default_driver;
}

EXPORT_API NuguPlayerDriver *nugu_player_driver_find(const char *name)
{
	GList *cur;

	g_return_val_if_fail(name != NULL, NULL);

	cur = _player_drivers;
	while (cur) {
		if (g_strcmp0(((NuguPlayerDriver *)cur->data)->name, name) == 0)
			return cur->data;

		cur = cur->next;
	}

	return NULL;
}

EXPORT_API NuguPlayer *nugu_player_new(const char *name,
				       NuguPlayerDriver *driver)
{
	NuguPlayer *player;

	g_return_val_if_fail(name != NULL, NULL);
	g_return_val_if_fail(driver != NULL, NULL);
	g_return_val_if_fail(driver->ops != NULL, NULL);
	g_return_val_if_fail(driver->ops->create != NULL, NULL);

	player = g_malloc0(sizeof(struct _nugu_player));
	player->name = g_strdup(name);
	player->driver = driver;
	player->ecb = NULL;
	player->scb = NULL;
	player->eud = NULL;
	player->sud = NULL;
	player->playurl = NULL;
	player->volume = NUGU_SET_VOLUME_DEFAULT;
	player->driver->ref_count++;
	player->device = player->driver->ops->create(player);
	player->status = MEDIA_STATUS_STOPPED;

	player->dbus = nugu_dbus_object_new(INTROSPECTION_PLAYER, &_player_ops);
	if (!player->dbus)
		nugu_warn("dbus object creation failed.");

	nugu_dbus_object_set_data(player->dbus, player);

	return player;
}

EXPORT_API void nugu_player_free(NuguPlayer *player)
{
	g_return_if_fail(player != NULL);
	g_return_if_fail(player->driver != NULL);
	g_return_if_fail(player->driver->ops != NULL);
	g_return_if_fail(player->driver->ops->destroy != NULL);

	if (player->dbus)
		nugu_dbus_object_free(player->dbus);

	player->driver->ref_count--;
	player->driver->ops->destroy(player->device, player);

	g_free(player->playurl);
	g_free(player->name);

	memset(player, 0, sizeof(struct _nugu_player));
	g_free(player);
}

EXPORT_API int nugu_player_add(NuguPlayer *player)
{
	char *object_path;

	g_return_val_if_fail(player != NULL, -1);

	if (nugu_player_find(player->name)) {
		nugu_error("'%s' player already exist.", player->name);
		return -1;
	}

	_players = g_list_append(_players, player);

	object_path =
		g_strdup_printf("%s/%s", OBJECT_PATH_PLAYER, player->name);
	nugu_dbus_object_export(player->dbus, object_path);
	g_free(object_path);

	return 0;
}

EXPORT_API int nugu_player_remove(NuguPlayer *player)
{
	GList *l;

	l = g_list_find(_players, player);
	if (!l)
		return -1;

	_players = g_list_remove_link(_players, l);

	nugu_dbus_object_unexport(player->dbus);

	return 0;
}

EXPORT_API NuguPlayer *nugu_player_find(const char *name)
{
	GList *cur;

	g_return_val_if_fail(name != NULL, NULL);

	cur = _players;
	while (cur) {
		if (g_strcmp0(((NuguPlayer *)cur->data)->name, name) == 0)
			return cur->data;

		cur = cur->next;
	}

	return NULL;
}

EXPORT_API int nugu_player_set_source(NuguPlayer *player, const char *url)
{
	g_return_val_if_fail(player != NULL, -1);
	g_return_val_if_fail(player->driver != NULL, -1);
	g_return_val_if_fail(url != NULL, -1);

	if (player->driver->ops->set_source == NULL) {
		nugu_error("Not supported");
		return -1;
	}

	if (player->playurl)
		g_free(player->playurl);

	player->playurl = g_strdup(url);

	return player->driver->ops->set_source(player->device, player, url);
}

EXPORT_API int nugu_player_start(NuguPlayer *player)
{
	g_return_val_if_fail(player != NULL, -1);
	g_return_val_if_fail(player->driver != NULL, -1);

	if (player->driver->ops->start == NULL) {
		nugu_error("Not supported");
		return -1;
	}
	if (nugu_player_set_volume(player, player->volume) != 0)
		return -1;

	return player->driver->ops->start(player->device, player);
}

EXPORT_API int nugu_player_stop(NuguPlayer *player)
{
	g_return_val_if_fail(player != NULL, -1);
	g_return_val_if_fail(player->driver != NULL, -1);

	if (player->driver->ops->stop == NULL) {
		nugu_error("Not supported");
		return -1;
	}
	return player->driver->ops->stop(player->device, player);
}

EXPORT_API int nugu_player_pause(NuguPlayer *player)
{
	g_return_val_if_fail(player != NULL, -1);
	g_return_val_if_fail(player->driver != NULL, -1);

	if (player->driver->ops->pause == NULL) {
		nugu_error("Not supported");
		return -1;
	}

	return player->driver->ops->pause(player->device, player);
}

EXPORT_API int nugu_player_resume(NuguPlayer *player)
{
	g_return_val_if_fail(player != NULL, -1);
	g_return_val_if_fail(player->driver != NULL, -1);

	if (player->driver->ops->resume == NULL) {
		nugu_error("Not supported");
		return -1;
	}

	return player->driver->ops->resume(player->device, player);
}

EXPORT_API int nugu_player_seek(NuguPlayer *player, int sec)
{
	g_return_val_if_fail(player != NULL, -1);
	g_return_val_if_fail(player->driver != NULL, -1);

	if (player->driver->ops->seek == NULL) {
		nugu_error("Not supported");
		return -1;
	}

	return player->driver->ops->seek(player->device, player, sec);
}

EXPORT_API int nugu_player_set_volume(NuguPlayer *player, int vol)
{
	g_return_val_if_fail(player != NULL, -1);
	g_return_val_if_fail(player->driver != NULL, -1);

	if (player->driver->ops->set_volume == NULL) {
		nugu_error("Not supported");
		return -1;
	}

	if (vol < NUGU_SET_VOLUME_MIN)
		player->volume = NUGU_SET_VOLUME_MIN;
	else if (vol > NUGU_SET_VOLUME_MAX)
		player->volume = NUGU_SET_VOLUME_MAX;
	else
		player->volume = vol;

	return player->driver->ops->set_volume(player->device, player,
					       player->volume);
}

EXPORT_API int nugu_player_get_volume(NuguPlayer *player)
{
	g_return_val_if_fail(player != NULL, -1);

	return player->volume;
}

EXPORT_API int nugu_player_get_duration(NuguPlayer *player)
{
	g_return_val_if_fail(player != NULL, -1);
	g_return_val_if_fail(player->driver != NULL, -1);

	if (player->driver->ops->get_duration == NULL) {
		nugu_error("Not supported");
		return -1;
	}
	return player->driver->ops->get_duration(player->device, player);
}

EXPORT_API int nugu_player_get_position(NuguPlayer *player)
{
	g_return_val_if_fail(player != NULL, -1);
	g_return_val_if_fail(player->driver != NULL, -1);

	if (player->driver->ops->get_position == NULL) {
		nugu_error("Not supported");
		return -1;
	}

	return player->driver->ops->get_position(player->device, player);
}

EXPORT_API enum nugu_media_status nugu_player_get_status(NuguPlayer *player)
{
	g_return_val_if_fail(player != NULL, -1);
	g_return_val_if_fail(player->driver != NULL, -1);

	if (player->driver->ops->get_position == NULL) {
		nugu_error("Not supported");
		return -1;
	}

	return player->status;
}

EXPORT_API void nugu_player_set_status_callback(NuguPlayer *player,
						mediaStatusCallback cb,
						void *userdata)
{
	g_return_if_fail(player != NULL);

	player->scb = cb;
	player->sud = userdata;
}

EXPORT_API void nugu_player_emit_status(NuguPlayer *player,
					enum nugu_media_status status)
{
	g_return_if_fail(player != NULL);

	if (player->status == status)
		return;

	player->status = status;

	if (player->scb != NULL)
		player->scb(status, player->sud);
}

EXPORT_API void nugu_player_set_event_callback(NuguPlayer *player,
					       mediaEventCallback cb,
					       void *userdata)
{
	g_return_if_fail(player != NULL);

	player->ecb = cb;
	player->eud = userdata;
}

EXPORT_API void nugu_player_emit_event(NuguPlayer *player,
				       enum nugu_media_event event)
{
	g_return_if_fail(player != NULL);

	if (event == MEDIA_EVENT_END_OF_STREAM)
		player->status = MEDIA_STATUS_STOPPED;

	if (player->ecb != NULL)
		player->ecb(event, player->eud);
}
