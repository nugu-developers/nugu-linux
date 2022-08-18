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

#include "base/nugu_player.h"

static enum nugu_media_status _status;
static enum nugu_media_event _event;
static int _volume;
static int _position;
static int _duration;
static int _seek_pos;
static const char *_playurl;

#define CHECK_STATUS(s)                                                        \
	{                                                                      \
		_status = s;                                                   \
	}
#define CHECK_EVENT(e)                                                         \
	{                                                                      \
		_event = e;                                                    \
	}
#define CHECK_VOLUME(v)                                                        \
	{                                                                      \
		_volume = v;                                                   \
	}

#define RIGHT_PLAYURL "file:///tmp/right.mp3"
#define WRONG_PLAYURL "file:///tmp/nofile"

#define DUMMY_DURATION 60
#define DUMMY_POSITION 0
#define DUMMY_SEEK_ONE_HALF (DUMMY_DURATION / 2)
#define DUMMY_SEEK_ONE_THIRD (DUMMY_DURATION / 3)
#define DUMMY_VOLUME NUGU_SET_VOLUME_MAX

static int dummy_create(NuguPlayerDriver *driver, NuguPlayer *player)
{
	(void)driver;
	(void)player;

	return 0;
}

static void dummy_destroy(NuguPlayerDriver *driver, NuguPlayer *player)
{
	(void)driver;
	(void)player;
}

static int dummy_start(NuguPlayerDriver *driver, NuguPlayer *player)
{
	(void)driver;

	if (g_strcmp0(_playurl, WRONG_PLAYURL) == 0)
		nugu_player_emit_event(player,
				       NUGU_MEDIA_EVENT_MEDIA_LOAD_FAILED);
	else {
		nugu_player_emit_event(player, NUGU_MEDIA_EVENT_MEDIA_LOADED);
		nugu_player_emit_status(player, NUGU_MEDIA_STATUS_PLAYING);
	}

	_seek_pos = 0;
	_position = 0;

	return 0;
}

static int dummy_stop(NuguPlayerDriver *driver, NuguPlayer *player)
{
	(void)driver;

	nugu_player_emit_status(player, NUGU_MEDIA_STATUS_STOPPED);

	return 0;
}

static int dummy_pause(NuguPlayerDriver *driver, NuguPlayer *player)
{
	(void)driver;

	nugu_player_emit_status(player, NUGU_MEDIA_STATUS_PAUSED);
	return 0;
}

static int dummy_resume(NuguPlayerDriver *driver, NuguPlayer *player)
{
	(void)driver;

	nugu_player_emit_status(player, NUGU_MEDIA_STATUS_PLAYING);

	return 0;
}

static int dummy_seek(NuguPlayerDriver *driver, NuguPlayer *player, int sec)
{
	(void)driver;

	_seek_pos += sec;

	if (_seek_pos > _duration) {
		_seek_pos = _duration;
		nugu_player_emit_event(player, NUGU_MEDIA_EVENT_END_OF_STREAM);
	} else if (_seek_pos < 0) {
		_seek_pos = 0;
	}

	return 0;
}
static int dummy_set_source(NuguPlayerDriver *driver, NuguPlayer *player,
			    const char *url)
{
	nugu_player_emit_event(player, NUGU_MEDIA_EVENT_MEDIA_SOURCE_CHANGED);
	dummy_stop(driver, player);

	_playurl = url;

	return 0;
}
static int dummy_set_volume(NuguPlayerDriver *driver, NuguPlayer *player,
			    int vol)
{
	(void)driver;
	(void)player;

	g_assert(vol == _volume);

	return 0;
}

static int dummy_get_duration(NuguPlayerDriver *driver, NuguPlayer *player)
{
	(void)driver;
	(void)player;

	if (_status == NUGU_MEDIA_STATUS_STOPPED)
		_duration = -1;
	else
		_duration = DUMMY_DURATION;

	return _duration;
}

static int dummy_get_position(NuguPlayerDriver *driver, NuguPlayer *player)
{
	(void)driver;
	(void)player;

	if (_status == NUGU_MEDIA_STATUS_STOPPED)
		_position = -1;
	else if (_seek_pos)
		_position = _seek_pos;
	else
		_position = DUMMY_POSITION;

	return _position;
}

static struct nugu_player_driver_ops dummy_driver_ops = {
	.create = dummy_create,
	.destroy = dummy_destroy,
	.set_source = dummy_set_source,
	.start = dummy_start,
	.stop = dummy_stop,
	.pause = dummy_pause,
	.resume = dummy_resume,
	.seek = dummy_seek,
	.set_volume = dummy_set_volume,
	.get_duration = dummy_get_duration,
	.get_position = dummy_get_position
};

static void player_status_callback(enum nugu_media_status status,
				   void *userdata)
{
	(void)userdata;
	g_assert(status == _status);
}

static void player_event_callback(enum nugu_media_event event, void *userdata)
{
	(void)userdata;
	g_assert(event == _event);
}

static void test_player_default(void)
{
	NuguPlayerDriver *driver;
	NuguPlayer *player;

	driver = nugu_player_driver_new("dummy", &dummy_driver_ops);
	g_assert(driver != NULL);
	g_assert(nugu_player_driver_register(driver) == 0);

	player = nugu_player_new("fileplayer", driver);
	g_assert(nugu_player_add(player) == 0);

	nugu_player_set_status_callback(player, player_status_callback, NULL);
	nugu_player_set_event_callback(player, player_event_callback, NULL);

	CHECK_EVENT(NUGU_MEDIA_EVENT_MEDIA_SOURCE_CHANGED);
	CHECK_STATUS(NUGU_MEDIA_STATUS_STOPPED);
	nugu_player_set_source(player, RIGHT_PLAYURL);

	CHECK_EVENT(NUGU_MEDIA_EVENT_MEDIA_LOADED);
	CHECK_STATUS(NUGU_MEDIA_STATUS_PLAYING);
	CHECK_VOLUME(NUGU_SET_VOLUME_DEFAULT);
	g_assert(nugu_player_start(player) == 0);

	CHECK_STATUS(NUGU_MEDIA_STATUS_PAUSED);
	g_assert(nugu_player_pause(player) == 0);

	CHECK_STATUS(NUGU_MEDIA_STATUS_PLAYING);
	g_assert(nugu_player_resume(player) == 0);

	CHECK_STATUS(NUGU_MEDIA_STATUS_STOPPED);
	g_assert(nugu_player_stop(player) == 0);

	nugu_player_remove(player);
	nugu_player_free(player);

	nugu_player_driver_remove(driver);
}

static void test_player_info(void)
{
	NuguPlayerDriver *driver;
	NuguPlayer *player;

	driver = nugu_player_driver_new("dummy", &dummy_driver_ops);
	g_assert(driver != NULL);
	g_assert(nugu_player_driver_register(driver) == 0);

	player = nugu_player_new("fileplayer", driver);
	g_assert(nugu_player_add(player) == 0);

	g_assert(nugu_player_get_duration(player) == -1);
	g_assert(nugu_player_get_position(player) == -1);

	nugu_player_set_source(player, RIGHT_PLAYURL);

	CHECK_VOLUME(NUGU_SET_VOLUME_DEFAULT);
	CHECK_STATUS(NUGU_MEDIA_STATUS_PLAYING);
	g_assert(nugu_player_start(player) == 0);

	g_assert(nugu_player_get_duration(player) == DUMMY_DURATION);
	g_assert(nugu_player_get_position(player) == DUMMY_POSITION);

	g_assert(nugu_player_stop(player) == 0);

	nugu_player_remove(player);
	nugu_player_free(player);

	nugu_player_driver_remove(driver);
}

static void test_player_probe(void)
{
	NuguPlayerDriver *driver;
	NuguPlayer *player;

	driver = nugu_player_driver_new("dummy", &dummy_driver_ops);
	g_assert(driver != NULL);
	g_assert(nugu_player_driver_register(driver) == 0);

	player = nugu_player_new("fileplayer", driver);
	g_assert(nugu_player_add(player) == 0);

	nugu_player_set_status_callback(player, player_status_callback, NULL);
	nugu_player_set_event_callback(player, player_event_callback, NULL);

	/* set available media content source */
	CHECK_EVENT(NUGU_MEDIA_EVENT_MEDIA_SOURCE_CHANGED);
	CHECK_STATUS(NUGU_MEDIA_STATUS_STOPPED);
	nugu_player_set_source(player, RIGHT_PLAYURL);

	CHECK_EVENT(NUGU_MEDIA_EVENT_MEDIA_LOADED);
	CHECK_STATUS(NUGU_MEDIA_STATUS_PLAYING);
	CHECK_VOLUME(NUGU_SET_VOLUME_DEFAULT);
	g_assert(nugu_player_start(player) == 0);

	/* set unavailable media content source */
	CHECK_EVENT(NUGU_MEDIA_EVENT_MEDIA_SOURCE_CHANGED);
	CHECK_STATUS(NUGU_MEDIA_STATUS_STOPPED);
	nugu_player_set_source(player, WRONG_PLAYURL);

	CHECK_EVENT(NUGU_MEDIA_EVENT_MEDIA_LOAD_FAILED);
	g_assert(nugu_player_start(player) == 0);

	nugu_player_remove(player);
	nugu_player_free(player);

	nugu_player_driver_remove(driver);
}

static void test_nugu_player_seek(void)
{
	NuguPlayerDriver *driver;
	NuguPlayer *player;

	driver = nugu_player_driver_new("dummy", &dummy_driver_ops);
	g_assert(driver != NULL);
	g_assert(nugu_player_driver_register(driver) == 0);

	player = nugu_player_new("fileplayer", driver);
	g_assert(nugu_player_add(player) == 0);

	nugu_player_set_status_callback(player, player_status_callback, NULL);
	nugu_player_set_event_callback(player, player_event_callback, NULL);

	CHECK_EVENT(NUGU_MEDIA_EVENT_MEDIA_SOURCE_CHANGED);
	CHECK_STATUS(NUGU_MEDIA_STATUS_STOPPED);
	nugu_player_set_source(player, RIGHT_PLAYURL);

	/* test for positive seek */
	CHECK_EVENT(NUGU_MEDIA_EVENT_MEDIA_LOADED);
	CHECK_STATUS(NUGU_MEDIA_STATUS_PLAYING);
	CHECK_VOLUME(NUGU_SET_VOLUME_DEFAULT);
	g_assert(nugu_player_start(player) == 0);

	g_assert(nugu_player_seek(player, DUMMY_SEEK_ONE_THIRD) == 0);
	g_assert(nugu_player_get_position(player) == DUMMY_SEEK_ONE_THIRD);

	g_assert(nugu_player_seek(player, DUMMY_SEEK_ONE_THIRD) == 0);
	g_assert(nugu_player_get_position(player) == 2 * DUMMY_SEEK_ONE_THIRD);

	CHECK_EVENT(NUGU_MEDIA_EVENT_END_OF_STREAM);
	g_assert(nugu_player_seek(player, DUMMY_SEEK_ONE_THIRD) == 0);
	g_assert(nugu_player_get_position(player) == DUMMY_DURATION);

	CHECK_STATUS(NUGU_MEDIA_STATUS_STOPPED);
	g_assert(nugu_player_stop(player) == 0);

	/* test for negative seek */
	CHECK_EVENT(NUGU_MEDIA_EVENT_MEDIA_LOADED);
	CHECK_STATUS(NUGU_MEDIA_STATUS_PLAYING);
	CHECK_VOLUME(NUGU_SET_VOLUME_DEFAULT);
	g_assert(nugu_player_start(player) == 0);

	g_assert(nugu_player_get_position(player) == DUMMY_POSITION);

	g_assert(nugu_player_seek(player, DUMMY_SEEK_ONE_HALF) == 0);
	g_assert(nugu_player_get_position(player) == DUMMY_SEEK_ONE_HALF);

	g_assert(nugu_player_seek(player, -DUMMY_SEEK_ONE_THIRD) == 0);
	g_assert(nugu_player_get_position(player) ==
		 DUMMY_SEEK_ONE_HALF - DUMMY_SEEK_ONE_THIRD);

	g_assert(nugu_player_seek(player, -DUMMY_SEEK_ONE_THIRD) == 0);
	g_assert(nugu_player_get_position(player) == 0);

	CHECK_STATUS(NUGU_MEDIA_STATUS_STOPPED);
	g_assert(nugu_player_stop(player) == 0);

	/* test for positive seek when palyer is paused */
	CHECK_EVENT(NUGU_MEDIA_EVENT_MEDIA_LOADED);
	CHECK_STATUS(NUGU_MEDIA_STATUS_PLAYING);
	CHECK_VOLUME(NUGU_SET_VOLUME_DEFAULT);
	g_assert(nugu_player_start(player) == 0);

	CHECK_STATUS(NUGU_MEDIA_STATUS_PAUSED);
	g_assert(nugu_player_pause(player) == 0);

	g_assert(nugu_player_get_position(player) == DUMMY_POSITION);

	g_assert(nugu_player_seek(player, DUMMY_SEEK_ONE_HALF) == 0);
	g_assert(nugu_player_get_position(player) == DUMMY_SEEK_ONE_HALF);

	g_assert(nugu_player_get_status(player) == NUGU_MEDIA_STATUS_PAUSED);

	CHECK_STATUS(NUGU_MEDIA_STATUS_PLAYING);
	g_assert(nugu_player_resume(player) == 0);

	CHECK_STATUS(NUGU_MEDIA_STATUS_STOPPED);
	g_assert(nugu_player_stop(player) == 0);

	nugu_player_remove(player);
	nugu_player_free(player);

	nugu_player_driver_remove(driver);
}

static void test_player_volume(void)
{
	NuguPlayerDriver *driver;
	NuguPlayer *player;
	int i;

	driver = nugu_player_driver_new("dummy", &dummy_driver_ops);
	g_assert(driver != NULL);
	g_assert(nugu_player_driver_register(driver) == 0);

	player = nugu_player_new("fileplayer", driver);
	g_assert(nugu_player_add(player) == 0);

	nugu_player_set_status_callback(player, player_status_callback, NULL);
	nugu_player_set_event_callback(player, player_event_callback, NULL);

	CHECK_EVENT(NUGU_MEDIA_EVENT_MEDIA_SOURCE_CHANGED);
	CHECK_STATUS(NUGU_MEDIA_STATUS_STOPPED);
	nugu_player_set_source(player, RIGHT_PLAYURL);

	g_assert(nugu_player_get_volume(player) == NUGU_SET_VOLUME_DEFAULT);

	CHECK_EVENT(NUGU_MEDIA_EVENT_MEDIA_LOADED);
	CHECK_STATUS(NUGU_MEDIA_STATUS_PLAYING);
	CHECK_VOLUME(NUGU_SET_VOLUME_DEFAULT);
	g_assert(nugu_player_start(player) == 0);

	/* change negative volume to set minimum volume */
	CHECK_VOLUME(NUGU_SET_VOLUME_MIN);
	g_assert(nugu_player_set_volume(player, -NUGU_SET_VOLUME_MAX) == 0);
	g_assert(nugu_player_get_volume(player) == NUGU_SET_VOLUME_MIN);

	/* change over maximum volume to set maximum volume */
	CHECK_VOLUME(NUGU_SET_VOLUME_MAX);
	g_assert(nugu_player_set_volume(player, 2 * NUGU_SET_VOLUME_MAX) == 0);
	g_assert(nugu_player_get_volume(player) == NUGU_SET_VOLUME_MAX);

	for (i = NUGU_SET_VOLUME_MIN; i <= NUGU_SET_VOLUME_MAX; i++) {
		CHECK_VOLUME(i);
		g_assert(nugu_player_set_volume(player, i) == 0);
		g_assert(nugu_player_get_volume(player) == i);
	}

	CHECK_STATUS(NUGU_MEDIA_STATUS_STOPPED);
	g_assert(nugu_player_stop(player) == 0);

	nugu_player_remove(player);
	nugu_player_free(player);

	nugu_player_driver_remove(driver);
}

static void test_player_audio_attribute(void)
{
	NuguPlayerDriver *driver;
	NuguPlayer *music_player, *alarm_player;

	driver = nugu_player_driver_new("dummy", &dummy_driver_ops);
	g_assert(driver != NULL);
	g_assert(nugu_player_driver_register(driver) == 0);

	music_player = nugu_player_new("music_player", driver);
	alarm_player = nugu_player_new("alarm_player", driver);
	g_assert(nugu_player_add(music_player) == 0);
	g_assert(nugu_player_add(alarm_player) == 0);

	// default player audio attribute: music (string: music)
	g_assert(nugu_player_get_audio_attribute(music_player) ==
		 NUGU_AUDIO_ATTRIBUTE_MUSIC);
	g_assert(nugu_player_get_audio_attribute(alarm_player) ==
		 NUGU_AUDIO_ATTRIBUTE_MUSIC);
	g_assert_cmpstr(nugu_player_get_audio_attribute_str(music_player), ==,
			NUGU_AUDIO_ATTRIBUTE_MUSIC_DEFAULT_STRING);

	// change to alarm
	nugu_player_set_audio_attribute(alarm_player,
					NUGU_AUDIO_ATTRIBUTE_ALARM);
	g_assert(nugu_player_get_audio_attribute(music_player) ==
		 NUGU_AUDIO_ATTRIBUTE_MUSIC);
	g_assert(nugu_player_get_audio_attribute(alarm_player) ==
		 NUGU_AUDIO_ATTRIBUTE_ALARM);
	g_assert_cmpstr(nugu_player_get_audio_attribute_str(alarm_player), ==,
			NUGU_AUDIO_ATTRIBUTE_ALARM_DEFAULT_STRING);

	nugu_player_remove(music_player);
	nugu_player_remove(alarm_player);

	nugu_player_free(music_player);
	nugu_player_free(alarm_player);

	nugu_player_driver_remove(driver);
}

int main(int argc, char *argv[])
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
	g_type_init();
#endif

	g_test_init(&argc, &argv, NULL);
	g_log_set_always_fatal((GLogLevelFlags)G_LOG_FATAL_MASK);

	g_test_add_func("/player/default", test_player_default);
	g_test_add_func("/player/info", test_player_info);
	g_test_add_func("/player/probe", test_player_probe);
	g_test_add_func("/player/seek", test_nugu_player_seek);
	g_test_add_func("/player/volume", test_player_volume);
	g_test_add_func("/player/audio_attribute", test_player_audio_attribute);

	return g_test_run();
}
