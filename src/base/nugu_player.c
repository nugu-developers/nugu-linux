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
#include <pthread.h>

#include <glib.h>

#include "base/nugu_log.h"
#include "base/nugu_player.h"

struct _nugu_player_driver {
	char *name;
	struct nugu_player_driver_ops *ops;
	int ref_count;
};

struct _nugu_player {
	char *name;
	NuguPlayerDriver *driver;
	void *driver_data;

	enum nugu_media_status status;
	enum nugu_audio_attribute attr;
	char *playurl;
	int volume;
	NuguMediaEventCallback ecb;
	NuguMediaStatusCallback scb;
	void *eud; /* user data for event callback */
	void *sud; /* user data for status callback */
};

static GList *_players;
static GList *_player_drivers;
static NuguPlayerDriver *_default_driver;

EXPORT_API NuguPlayerDriver *
nugu_player_driver_new(const char *name, struct nugu_player_driver_ops *ops)
{
	NuguPlayerDriver *driver;

	g_return_val_if_fail(name != NULL, NULL);
	g_return_val_if_fail(ops != NULL, NULL);

	driver = g_malloc0(sizeof(struct _nugu_player_driver));
	if (!driver) {
		nugu_error_nomem();
		return NULL;
	}

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

	nugu_info("set default player driver: '%s'", driver->name);

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

	player = g_malloc0(sizeof(struct _nugu_player));
	if (!player) {
		nugu_error_nomem();
		return NULL;
	}

	player->name = g_strdup(name);
	player->driver = driver;
	player->volume = NUGU_SET_VOLUME_DEFAULT;
	player->status = NUGU_MEDIA_STATUS_STOPPED;
	player->attr = NUGU_AUDIO_ATTRIBUTE_MUSIC;

	if (driver == NULL || driver->ops == NULL ||
	    driver->ops->create == NULL) {
		nugu_warn("Not supported");
		return player;
	}

	if (player->driver->ops->create(player->driver, player) < 0) {
		nugu_error("can't create nugu_player");
		g_free(player->name);
		g_free(player);
		return NULL;
	}

	player->driver->ref_count++;

	return player;
}

EXPORT_API void nugu_player_free(NuguPlayer *player)
{
	g_return_if_fail(player != NULL);

	if (player->driver) {
		player->driver->ref_count--;

		if (player->driver->ops && player->driver->ops->destroy)
			player->driver->ops->destroy(player->driver, player);
	}

	g_free(player->playurl);
	g_free(player->name);

	memset(player, 0, sizeof(struct _nugu_player));
	g_free(player);
}

EXPORT_API int nugu_player_add(NuguPlayer *player)
{
	g_return_val_if_fail(player != NULL, -1);

	if (nugu_player_find(player->name)) {
		nugu_error("'%s' player already exist.", player->name);
		return -1;
	}

	_players = g_list_append(_players, player);

	return 0;
}

EXPORT_API int nugu_player_remove(NuguPlayer *player)
{
	GList *l;

	l = g_list_find(_players, player);
	if (!l)
		return -1;

	_players = g_list_remove_link(_players, l);

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

EXPORT_API int nugu_player_set_audio_attribute(NuguPlayer *player,
					       NuguAudioAttribute attr)
{
	g_return_val_if_fail(player != NULL, -1);

	player->attr = attr;
	return 0;
}

EXPORT_API int nugu_player_get_audio_attribute(NuguPlayer *player)
{
	g_return_val_if_fail(player != NULL, -1);

	return player->attr;
}

EXPORT_API const char *nugu_player_get_audio_attribute_str(NuguPlayer *player)
{
	g_return_val_if_fail(player != NULL, NULL);

	return nugu_audio_get_attribute_str(player->attr);
}

EXPORT_API int nugu_player_set_source(NuguPlayer *player, const char *url)
{
	g_return_val_if_fail(player != NULL, -1);
	g_return_val_if_fail(url != NULL, -1);

	if (player->driver == NULL || player->driver->ops == NULL ||
	    player->driver->ops->set_source == NULL) {
		nugu_error("Not supported");
		return -1;
	}

	if (player->playurl)
		g_free(player->playurl);

	player->playurl = g_strdup(url);

	return player->driver->ops->set_source(player->driver, player, url);
}

EXPORT_API int nugu_player_start(NuguPlayer *player)
{
	g_return_val_if_fail(player != NULL, -1);

	if (player->driver == NULL || player->driver->ops == NULL ||
	    player->driver->ops->start == NULL) {
		nugu_error("Not supported");
		return -1;
	}
	if (nugu_player_set_volume(player, player->volume) != 0)
		return -1;

	return player->driver->ops->start(player->driver, player);
}

EXPORT_API int nugu_player_stop(NuguPlayer *player)
{
	g_return_val_if_fail(player != NULL, -1);

	if (player->driver == NULL || player->driver->ops == NULL ||
	    player->driver->ops->stop == NULL) {
		nugu_error("Not supported");
		return -1;
	}
	return player->driver->ops->stop(player->driver, player);
}

EXPORT_API int nugu_player_pause(NuguPlayer *player)
{
	g_return_val_if_fail(player != NULL, -1);

	if (player->driver == NULL || player->driver->ops == NULL ||
	    player->driver->ops->pause == NULL) {
		nugu_error("Not supported");
		return -1;
	}

	return player->driver->ops->pause(player->driver, player);
}

EXPORT_API int nugu_player_resume(NuguPlayer *player)
{
	g_return_val_if_fail(player != NULL, -1);

	if (player->driver == NULL || player->driver->ops == NULL ||
	    player->driver->ops->resume == NULL) {
		nugu_error("Not supported");
		return -1;
	}

	return player->driver->ops->resume(player->driver, player);
}

EXPORT_API int nugu_player_seek(NuguPlayer *player, int sec)
{
	g_return_val_if_fail(player != NULL, -1);

	if (player->driver == NULL || player->driver->ops == NULL ||
	    player->driver->ops->seek == NULL) {
		nugu_error("Not supported");
		return -1;
	}

	return player->driver->ops->seek(player->driver, player, sec);
}

EXPORT_API int nugu_player_set_volume(NuguPlayer *player, int vol)
{
	g_return_val_if_fail(player != NULL, -1);

	if (player->driver == NULL || player->driver->ops == NULL ||
	    player->driver->ops->set_volume == NULL) {
		nugu_error("Not supported");
		return -1;
	}

	if (vol < NUGU_SET_VOLUME_MIN)
		player->volume = NUGU_SET_VOLUME_MIN;
	else if (vol > NUGU_SET_VOLUME_MAX)
		player->volume = NUGU_SET_VOLUME_MAX;
	else
		player->volume = vol;

	return player->driver->ops->set_volume(player->driver, player,
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

	if (player->driver == NULL || player->driver->ops == NULL ||
	    player->driver->ops->get_duration == NULL) {
		nugu_error("Not supported");
		return -1;
	}
	return player->driver->ops->get_duration(player->driver, player);
}

EXPORT_API int nugu_player_get_position(NuguPlayer *player)
{
	g_return_val_if_fail(player != NULL, -1);

	if (player->driver == NULL || player->driver->ops == NULL ||
	    player->driver->ops->get_position == NULL) {
		nugu_error("Not supported");
		return -1;
	}

	return player->driver->ops->get_position(player->driver, player);
}

EXPORT_API enum nugu_media_status nugu_player_get_status(NuguPlayer *player)
{
	g_return_val_if_fail(player != NULL, NUGU_MEDIA_STATUS_STOPPED);

	return player->status;
}

EXPORT_API void nugu_player_set_status_callback(NuguPlayer *player,
						NuguMediaStatusCallback cb,
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
					       NuguMediaEventCallback cb,
					       void *userdata)
{
	g_return_if_fail(player != NULL);

	player->ecb = cb;
	player->eud = userdata;
}

EXPORT_API void nugu_player_emit_event(NuguPlayer *player,
				       enum nugu_media_event event)
{
	gboolean emit_status_callback = FALSE;

	g_return_if_fail(player != NULL);

	if (event == NUGU_MEDIA_EVENT_END_OF_STREAM) {
		if (player->status != NUGU_MEDIA_STATUS_STOPPED) {
			player->status = NUGU_MEDIA_STATUS_STOPPED;
			emit_status_callback = TRUE;
		}
	}

	if (player->ecb != NULL)
		player->ecb(event, player->eud);

	if (emit_status_callback && player->scb)
		player->scb(player->status, player->sud);
}

EXPORT_API int nugu_player_set_driver_data(NuguPlayer *player, void *data)
{
	g_return_val_if_fail(player != NULL, -1);

	player->driver_data = data;

	return 0;
}

EXPORT_API void *nugu_player_get_driver_data(NuguPlayer *player)
{
	g_return_val_if_fail(player != NULL, NULL);

	return player->driver_data;
}
