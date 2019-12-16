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
#include <unistd.h>

#include <glib.h>

#include "base/nugu_recorder.h"

#define SET_DEFAULT_AUDIO_PROPERTY(property)                                   \
	property.samplerate = AUDIO_SAMPLE_RATE_16K;                           \
	property.format = AUDIO_FORMAT_S16_LE;                                 \
	property.channel = 1

#define DEFAULT_PLUGIN_NAME "portaudio"

#define SET_AUDIO_FRAME_SIZE(value) value
#define SET_AUDIO_MAX_FRAMES 10
#define MOCK_AUDIO_FRAMES "abcde"

static pthread_t _tid;
static int _size;

struct _th_handler {
	GMainLoop *loop;
	NuguRecorder *rec;
};

static struct _th_handler _handle;

static int dummy_start(NuguRecorderDriver *driver, NuguRecorder *rec,
		       NuguAudioProperty property)
{
	(void)driver;

	g_assert(property.samplerate == AUDIO_SAMPLE_RATE_16K);
	g_assert(property.format == AUDIO_FORMAT_S16_LE);
	g_assert(property.channel == 1);

	g_assert(nugu_recorder_push_frame(rec, MOCK_AUDIO_FRAMES,
					  strlen(MOCK_AUDIO_FRAMES)) == 0);
	return 0;
}

static int dummy_stop(NuguRecorderDriver *driver, NuguRecorder *rec)
{
	(void)driver;
	(void)rec;
	return 0;
}

static struct nugu_recorder_driver_ops recorder_driver_ops = {
	.start = dummy_start,
	.stop = dummy_stop
};

static int timeout_start(NuguRecorderDriver *driver, NuguRecorder *rec,
			 NuguAudioProperty property)
{
	(void)driver;
	(void)rec;

	g_assert(property.samplerate == AUDIO_SAMPLE_RATE_16K);
	g_assert(property.format == AUDIO_FORMAT_S16_LE);
	g_assert(property.channel == 1);

	return 0;
}

static int timeout_stop(NuguRecorderDriver *driver, NuguRecorder *rec)
{
	(void)driver;
	(void)rec;
	return 0;
}

static struct nugu_recorder_driver_ops timeout_driver_ops = {
	.start = timeout_start,
	.stop = timeout_stop
};

static gint _push_data(void *p)
{
	int *count = (int *)p;
	char data[SET_AUDIO_MAX_FRAMES];
	int i;

	for (i = 0; i < *count; i++)
		data[i] = 'a' + i;

	g_assert(nugu_recorder_push_frame(_handle.rec, data, *count) == 0);
	return FALSE;
}

static void _push_data_later(int size, int timeout)
{
	_size = size;
	g_timeout_add(timeout, _push_data, &_size);
}

static void test_recorder_default(void)
{
	NuguAudioProperty property;
	NuguRecorderDriver *rec_drv;
	NuguRecorder *rec;
	char temp;
	int size;

	SET_DEFAULT_AUDIO_PROPERTY(property);

	rec_drv = nugu_recorder_driver_new(DEFAULT_PLUGIN_NAME,
					   &recorder_driver_ops);
	nugu_recorder_driver_register(rec_drv);

	rec = nugu_recorder_new("rec_timeout", rec_drv);
	nugu_recorder_add(rec);

	g_assert(nugu_recorder_set_frame_size(rec, SET_AUDIO_FRAME_SIZE(1),
					      SET_AUDIO_MAX_FRAMES) == 0);

	g_assert(nugu_recorder_set_property(rec, property) == 0);
	g_assert(nugu_recorder_start(rec) == 0);

	g_assert(nugu_recorder_get_frame_count(rec) == 5);
	g_assert(nugu_recorder_get_frame(rec, &temp, &size) == 0);
	g_assert(temp == 'a' && size == 1);
	g_assert(nugu_recorder_get_frame_count(rec) == 4);

	g_assert(nugu_recorder_stop(rec) == 0);

	g_assert(nugu_recorder_get_frame_count(rec) == 0);

	nugu_recorder_remove(rec);
	nugu_recorder_free(rec);
	nugu_recorder_driver_remove(rec_drv);
	nugu_recorder_driver_free(rec_drv);
}

static void *_runner(void *p)
{
	NuguAudioProperty property;
	char temp[SET_AUDIO_MAX_FRAMES];
	int size;

	SET_DEFAULT_AUDIO_PROPERTY(property);

	g_assert(nugu_recorder_set_frame_size(_handle.rec,
					      SET_AUDIO_FRAME_SIZE(2),
					      SET_AUDIO_MAX_FRAMES) == 0);

	g_assert(nugu_recorder_set_property(_handle.rec, property) == 0);

	// wait to read until pushing data
	g_assert(nugu_recorder_start(_handle.rec) == 0);

	_push_data_later(4, 500);
	g_assert(nugu_recorder_get_frame_count(_handle.rec) == 0);
	g_assert(nugu_recorder_get_frame_timeout(_handle.rec, temp, &size, 0) ==
		 0);
	g_assert_cmpmem(temp, 2, "ab", 2);
	g_assert(size == SET_AUDIO_FRAME_SIZE(2));
	g_assert(nugu_recorder_get_frame_timeout(_handle.rec, temp, &size, 0) ==
		 0);
	g_assert(nugu_recorder_get_frame_count(_handle.rec) == 0);

	g_assert(nugu_recorder_stop(_handle.rec) == 0);

	// unblocking 100 ms later
	g_assert(nugu_recorder_start(_handle.rec) == 0);

	_push_data_later(4, 500);
	g_assert(nugu_recorder_get_frame_count(_handle.rec) == 0);
	g_assert(nugu_recorder_get_frame_timeout(_handle.rec, temp, &size,
						 100) == 0);
	g_assert(size == 0);

	g_assert(nugu_recorder_stop(_handle.rec) == 0);

	g_main_loop_quit(_handle.loop);
	return NULL;
}

static void test_recorder_timeout(void)
{
	NuguRecorderDriver *rec_drv;
	GMainLoop *loop;
	NuguRecorder *rec;

	rec_drv = nugu_recorder_driver_new(DEFAULT_PLUGIN_NAME,
					   &timeout_driver_ops);
	nugu_recorder_driver_register(rec_drv);
	rec = nugu_recorder_new("rec_timeout", rec_drv);
	nugu_recorder_add(rec);

	loop = g_main_loop_new(NULL, FALSE);

	_handle.loop = loop;
	_handle.rec = rec;
	pthread_create(&_tid, NULL, _runner, NULL);

	g_main_loop_run(loop);
	g_main_loop_unref(loop);

	nugu_recorder_remove(rec);
	nugu_recorder_free(rec);
	nugu_recorder_driver_remove(rec_drv);
	nugu_recorder_driver_free(rec_drv);
}

int main(int argc, char *argv[])
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
	g_type_init();
#endif

	g_test_init(&argc, &argv, NULL);
	g_log_set_always_fatal((GLogLevelFlags)G_LOG_FATAL_MASK);

	g_test_add_func("/recorder/default", test_recorder_default);
	g_test_add_func("/recorder/timeout", test_recorder_timeout);
	return g_test_run();
}
