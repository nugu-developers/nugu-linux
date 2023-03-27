/*
 * Copyright (c) 2023 SK Telecom Co., Ltd. All rights reserved.
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
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <glib.h>

#include "base/nugu_log.h"
#include "base/nugu_plugin.h"
#include "base/nugu_recorder.h"

#define PLUGIN_DRIVER_NAME "filereader"
#define SAMPLE_SILENCE (0.0f)
#define AUDIO_SAMPLING_100MS (100)

struct audio_param {
	GSource *source;
	int samplerate;
	int samplebyte;
	int channel;
	void *data;

#if defined(NUGU_ENV_DUMP_PATH_RECORDER)
	int dump_fd;
#endif

#ifdef NUGU_ENV_RECORDING_FROM_FILE
	int src_fd;
#endif
};

static NuguRecorderDriver *rec_driver;

#ifdef NUGU_ENV_DUMP_LINK_FILE_RECORDER
static void _dumpfile_link(const char *filename)
{
	char *link_file;
	int ret;

	if (!filename)
		return;

	link_file = getenv(NUGU_ENV_DUMP_LINK_FILE_RECORDER);
	if (!link_file)
		link_file = "rec_audio.pcm";

	unlink(link_file);
	ret = link(filename, link_file);
	if (ret < 0) {
		nugu_error("link(%s) failed: %s", link_file, strerror(errno));
		return;
	}

	nugu_dbg("link file: %s -> %s", link_file, filename);
}
#endif

#if defined(NUGU_ENV_DUMP_PATH_RECORDER)
static int _dumpfile_open(const char *path, const char *prefix)
{
	char ymd[32];
	char hms[32];
	time_t now;
	struct tm now_tm;
	char *buf = NULL;
	int fd;

	if (!path)
		return -1;

	now = time(NULL);
	localtime_r(&now, &now_tm);

	snprintf(ymd, sizeof(ymd), "%04d%02d%02d", now_tm.tm_year + 1900,
		 now_tm.tm_mon + 1, now_tm.tm_mday);
	snprintf(hms, sizeof(hms), "%02d%02d%02d", now_tm.tm_hour,
		 now_tm.tm_min, now_tm.tm_sec);

	buf = g_strdup_printf("%s/%s_%s_%s.dat", path, prefix, ymd, hms);

	fd = open(buf, O_CREAT | O_WRONLY, 0644);
	if (fd < 0)
		nugu_error("open(%s) failed: %s", buf, strerror(errno));

	nugu_dbg("%s filedump to '%s' (fd=%d)", prefix, buf, fd);

#ifdef NUGU_ENV_DUMP_LINK_FILE_RECORDER
	_dumpfile_link(buf);
#endif
	free(buf);

	return fd;
}
#endif

static int _set_property_to_param(struct audio_param *param,
				  NuguAudioProperty prop)
{
	g_return_val_if_fail(param != NULL, -1);

	switch (prop.samplerate) {
	case NUGU_AUDIO_SAMPLE_RATE_8K:
		param->samplerate = 8000;
		break;
	case NUGU_AUDIO_SAMPLE_RATE_16K:
		param->samplerate = 16000;
		break;
	case NUGU_AUDIO_SAMPLE_RATE_32K:
		param->samplerate = 32000;
		break;
	case NUGU_AUDIO_SAMPLE_RATE_22K:
		param->samplerate = 22050;
		break;
	case NUGU_AUDIO_SAMPLE_RATE_44K:
		param->samplerate = 44100;
		break;
	default:
		param->samplerate = 16000;
		break;
	}

	switch (prop.format) {
	case NUGU_AUDIO_FORMAT_S8:
		param->samplebyte = 1;
		break;
	case NUGU_AUDIO_FORMAT_U8:
		param->samplebyte = 1;
		break;
	case NUGU_AUDIO_FORMAT_S16_LE:
	case NUGU_AUDIO_FORMAT_S16_BE:
		param->samplebyte = 2;
		break;
	case NUGU_AUDIO_FORMAT_S24_LE:
	case NUGU_AUDIO_FORMAT_S24_BE:
		param->samplebyte = 3;
		break;
	case NUGU_AUDIO_FORMAT_S32_LE:
	case NUGU_AUDIO_FORMAT_S32_BE:
		param->samplebyte = 4;
		break;
	default:
		nugu_error("not support the audio format(%d)", prop.format);
		return -1;
	}
	param->channel = prop.channel;

	return 0;
}

static void _recorder_push(struct audio_param *param, const char *buf,
			   int buf_size)
{
	nugu_recorder_push_frame(param->data, buf, buf_size);

#ifdef NUGU_ENV_DUMP_PATH_RECORDER
	if (param->dump_fd != -1) {
		if (write(param->dump_fd, buf, buf_size) < 0)
			nugu_error("write to fd-%d failed", param->dump_fd);
	}
#endif
}

#ifdef NUGU_ENV_RECORDING_FROM_FILE
static int _recording_from_file(struct audio_param *param, int buf_size)
{
	char *buf;
	ssize_t nread;

	buf = malloc(buf_size);
	if (!buf) {
		nugu_error_nomem();
		return -1;
	}

	nread = read(param->src_fd, buf, buf_size);
	if (nread < 0) {
		nugu_error("read() failed: %s", strerror(errno));

		free(buf);
		close(param->src_fd);
		param->src_fd = -1;
		return -1;
	} else if (nread == 0) {
		nugu_dbg("read all pcm data from file. fill the SILENCE data");

		memset(buf, SAMPLE_SILENCE, buf_size);
		nread = buf_size;
	}

	_recorder_push(param, buf, nread);

	free(buf);

	return 0;
}
#endif

static gboolean _record_callback(gpointer userdata)
{
	struct audio_param *rec_param = (struct audio_param *)userdata;
	int buf_size = rec_param->samplerate * rec_param->samplebyte / 10;

#ifdef NUGU_ENV_RECORDING_FROM_FILE
	/* Use file source instead of real microphone data */
	if (rec_param->src_fd != -1) {
		if (_recording_from_file(rec_param, buf_size) < 0)
			return FALSE;
		else
			return TRUE;
	}
#endif

	return FALSE;
}

static int _rec_start(NuguRecorderDriver *driver, NuguRecorder *rec,
		      NuguAudioProperty prop)
{
	struct audio_param *rec_param = nugu_recorder_get_driver_data(rec);
	int rec_5sec;
	int rec_100ms; // 3200 byte

	if (rec_param) {
		nugu_dbg("already start");
		return 0;
	}

	rec_param = (struct audio_param *)g_malloc0(sizeof(struct audio_param));

	if (_set_property_to_param(rec_param, prop) != 0) {
		g_free(rec_param);
		return -1;
	}

	rec_param->data = (void *)rec;
	rec_100ms = rec_param->samplerate * rec_param->samplebyte / 10;
	rec_5sec =
		rec_param->samplerate * rec_param->samplebyte * 5 / rec_100ms;
	nugu_dbg("rec - %d, %d", rec_100ms, rec_5sec);
	nugu_recorder_set_frame_size(rec, rec_100ms, rec_5sec);

	// timeout
	rec_param->source = g_timeout_source_new(AUDIO_SAMPLING_100MS);
	g_source_set_callback(rec_param->source, _record_callback,
			      (gpointer)rec_param, NULL);
	g_source_attach(rec_param->source, g_main_context_default());
	g_source_unref(rec_param->source);

#ifdef NUGU_ENV_DUMP_PATH_RECORDER
	rec_param->dump_fd =
		_dumpfile_open(getenv(NUGU_ENV_DUMP_PATH_RECORDER), "rec");
#endif

#ifdef NUGU_ENV_RECORDING_FROM_FILE
	if (getenv(NUGU_ENV_RECORDING_FROM_FILE)) {
		rec_param->src_fd =
			open(getenv(NUGU_ENV_RECORDING_FROM_FILE), O_RDONLY);
		nugu_dbg("recording from file: '%s'",
			 getenv(NUGU_ENV_RECORDING_FROM_FILE));
	} else {
		rec_param->src_fd = -1;
	}
#endif

	nugu_recorder_set_driver_data(rec, rec_param);

	nugu_dbg("start done");
	return 0;
}

static int _rec_stop(NuguRecorderDriver *driver, NuguRecorder *rec)
{
	struct audio_param *rec_param = nugu_recorder_get_driver_data(rec);

	if (rec_param == NULL) {
		nugu_dbg("already stop");
		return 0;
	}

	if (rec_param->source) {
		g_source_destroy(rec_param->source);
		rec_param->source = NULL;
	}

#ifdef NUGU_ENV_RECORDING_FROM_FILE
	if (rec_param->src_fd >= 0) {
		close(rec_param->src_fd);
		rec_param->src_fd = -1;
	}
#endif

	g_free(rec_param);
	nugu_recorder_set_driver_data(rec, NULL);

	nugu_dbg("stop done");

	return 0;
}

static struct nugu_recorder_driver_ops rec_ops = {
	/* nugu_recorder_driver */
	.start = _rec_start, /* nugu_recorder_start() */
	.stop = _rec_stop /* nugu_recorder_stop() */
};

static int init(NuguPlugin *p)
{
	nugu_dbg("'%s' plugin initialized",
		 nugu_plugin_get_description(p)->name);

	rec_driver = nugu_recorder_driver_new(PLUGIN_DRIVER_NAME, &rec_ops);
	if (!rec_driver) {
		nugu_error("nugu_recorder_driver_new() failed");
		return -1;
	}

	if (nugu_recorder_driver_register(rec_driver) != 0) {
		nugu_recorder_driver_free(rec_driver);
		rec_driver = NULL;
		return -1;
	}

	nugu_dbg("'%s' plugin initialized done",
		 nugu_plugin_get_description(p)->name);

	return 0;
}

static int load(void)
{
	nugu_dbg("plugin loaded");

	return 0;
}

static void unload(NuguPlugin *p)
{
	nugu_dbg("'%s' plugin unloaded", nugu_plugin_get_description(p)->name);

	if (rec_driver) {
		nugu_recorder_driver_remove(rec_driver);
		nugu_recorder_driver_free(rec_driver);
		rec_driver = NULL;
	}

	nugu_dbg("'%s' plugin unloaded done",
		 nugu_plugin_get_description(p)->name);
}

NUGU_PLUGIN_DEFINE(
	PLUGIN_DRIVER_NAME,
	NUGU_PLUGIN_PRIORITY_LOW,
	"0.0.1",
	load,
	unload,
	init
);
