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
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "base/nugu_log.h"
#include "base/nugu_player.h"
#include "mock/media_player_mock.h"

#define PLUGIN_DRIVER_NAME "test_player_driver"

#define NOTIFY_STATUS_CHANGED(s)                                               \
	{                                                                      \
		if (dh->player && dh->status != s) {                           \
			nugu_player_emit_status(dh->player, s);                \
			dh->status = s;                                        \
		}                                                              \
	}

#define NOTIFY_EVENT(e)                                                        \
	{                                                                      \
		if (dh->player)                                                \
			nugu_player_emit_event(dh->player, e);                 \
	}

typedef struct drv_handle DriverHandle;

struct drv_handle {
	int duration;
	int position;
	char *playurl;
	NuguPlayer *player;
	enum nugu_media_status status;
};

static NuguPlayerDriver *driver;

static int _create(NuguPlayerDriver *driver, NuguPlayer *player)
{
	DriverHandle *dh;

	g_return_val_if_fail(player != NULL, -1);

	dh = (DriverHandle *)g_malloc0(sizeof(DriverHandle));
	if (!dh) {
		nugu_error_nomem();
		return -1;
	}

	dh->duration = -1;
	dh->position = 0;
	dh->player = player;
	dh->status = NUGU_MEDIA_STATUS_STOPPED;

	nugu_player_set_driver_data(player, dh);

	return 0;
}

static int _start(NuguPlayerDriver *driver, NuguPlayer *player)
{
	DriverHandle *dh;

	g_return_val_if_fail(player != NULL, -1);

	dh = (DriverHandle *)nugu_player_get_driver_data(player);
	if (!dh) {
		nugu_error("invalid player (no driver data)");
		return -1;
	}

	NOTIFY_STATUS_CHANGED(NUGU_MEDIA_STATUS_PLAYING);
	return 0;
}

static int _stop(NuguPlayerDriver *driver, NuguPlayer *player)
{
	DriverHandle *dh;

	g_return_val_if_fail(player != NULL, -1);

	dh = (DriverHandle *)nugu_player_get_driver_data(player);
	if (!dh) {
		nugu_error("invalid player (no driver data)");
		return -1;
	}

	dh->position = 0;

	NOTIFY_STATUS_CHANGED(NUGU_MEDIA_STATUS_STOPPED);
	return 0;
}

static void _destroy(NuguPlayerDriver *driver, NuguPlayer *player)
{
	DriverHandle *dh;

	g_return_if_fail(player != NULL);

	dh = (DriverHandle *)nugu_player_get_driver_data(player);
	if (!dh) {
		nugu_error("invalid player (no driver data)");
		return;
	}

	if (player == dh->player)
		_stop(driver, dh->player);

	if (dh->playurl)
		g_free(dh->playurl);

	memset(dh, 0, sizeof(DriverHandle));
	g_free(dh);
}

static int _set_source(NuguPlayerDriver *driver, NuguPlayer *player,
		       const char *url)
{
	DriverHandle *dh;

	g_return_val_if_fail(player != NULL, -1);
	g_return_val_if_fail(url != NULL, -1);

	dh = (DriverHandle *)nugu_player_get_driver_data(player);
	if (!dh) {
		nugu_error("invalid player (no driver data)");
		return -1;
	}

	dh->duration = -1;

	if (dh->playurl)
		g_free(dh->playurl);

	dh->playurl = (char *)g_malloc0(strlen(url) + 1);
	memcpy(dh->playurl, url, strlen(url));

	NOTIFY_EVENT(NUGU_MEDIA_EVENT_MEDIA_SOURCE_CHANGED);
	_stop(driver, player);

	return 0;
}

static int _pause(NuguPlayerDriver *driver, NuguPlayer *player)
{
	DriverHandle *dh;

	g_return_val_if_fail(player != NULL, -1);

	dh = (DriverHandle *)nugu_player_get_driver_data(player);
	if (!dh) {
		nugu_error("invalid player (no driver data)");
		return -1;
	}

	NOTIFY_STATUS_CHANGED(NUGU_MEDIA_STATUS_PAUSED);

	return 0;
}

static int _resume(NuguPlayerDriver *driver, NuguPlayer *player)
{
	DriverHandle *dh;

	g_return_val_if_fail(player != NULL, -1);

	dh = (DriverHandle *)nugu_player_get_driver_data(player);
	if (!dh) {
		nugu_error("invalid player (no driver data)");
		return -1;
	}

	NOTIFY_STATUS_CHANGED(NUGU_MEDIA_STATUS_PLAYING);

	return 0;
}

static int _seek(NuguPlayerDriver *driver, NuguPlayer *player, int msec)
{
	nugu_warn("Not support yet");
	return 0;
}

static int _set_volume(NuguPlayerDriver *driver, NuguPlayer *player, int vol)
{
	nugu_warn("Not support yet");
	return 0;
}

static int _get_duration(NuguPlayerDriver *driver, NuguPlayer *player)
{
	DriverHandle *dh;

	g_return_val_if_fail(player != NULL, -1);

	dh = (DriverHandle *)nugu_player_get_driver_data(player);
	if (!dh) {
		nugu_error("invalid player (no driver data)");
		return -1;
	}

	return dh->duration / 1000;
}

static int _get_position(NuguPlayerDriver *driver, NuguPlayer *player)
{
	DriverHandle *dh;

	g_return_val_if_fail(player != NULL, -1);

	dh = (DriverHandle *)nugu_player_get_driver_data(player);
	if (!dh) {
		nugu_error("invalid player (no driver data)");
		return -1;
	}

	return dh->position / 1000;
}

static struct nugu_player_driver_ops player_ops = {
	.create = _create,
	.destroy = _destroy,
	.set_source = _set_source,
	.start = _start,
	.stop = _stop,
	.pause = _pause,
	.resume = _resume,
	.seek = _seek,
	.set_volume = _set_volume,
	.get_duration = _get_duration,
	.get_position = _get_position
};

int test_player_driver_initialize(void)
{
	driver = nugu_player_driver_new(PLUGIN_DRIVER_NAME, &player_ops);
	if (!driver) {
		nugu_error("nugu_player_driver_new() failed");
		return -1;
	}

	if (nugu_player_driver_register(driver) != 0) {
		nugu_player_driver_free(driver);
		driver = NULL;
		return -1;
	}

	return 0;
}

void test_player_driver_deinitialize(void)
{
	if (driver) {
		nugu_player_driver_remove(driver);
		nugu_player_driver_free(driver);
		driver = NULL;
	}
}

void test_player_notify_event(NuguPlayer *player, enum nugu_media_event event)
{
	DriverHandle *dh;

	g_return_if_fail(player != NULL);

	dh = (DriverHandle *)nugu_player_get_driver_data(player);
	if (!dh) {
		nugu_error("invalid player (no driver data)");
		return;
	}

	NOTIFY_EVENT(event);

	if (event == NUGU_MEDIA_EVENT_END_OF_STREAM)
		NOTIFY_STATUS_CHANGED(NUGU_MEDIA_STATUS_STOPPED);
}

int test_player_set_duration(NuguPlayer *player, int duration)
{
	DriverHandle *dh;

	g_return_val_if_fail(player != NULL, -1);

	dh = (DriverHandle *)nugu_player_get_driver_data(player);
	if (!dh) {
		nugu_error("invalid player (no driver data)");
		return -1;
	}

	dh->duration = duration;
	return 0;
}

int test_player_set_position(NuguPlayer *player, int position)
{
	DriverHandle *dh;

	g_return_val_if_fail(player != NULL, -1);

	dh = (DriverHandle *)nugu_player_get_driver_data(player);
	if (!dh) {
		nugu_error("invalid player (no driver data)");
		return -1;
	}

	dh->position = position;

	return 0;
}
