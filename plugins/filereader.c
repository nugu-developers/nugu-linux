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
#include <pthread.h>

#include "base/nugu_log.h"
#include "base/nugu_plugin.h"
#include "base/nugu_recorder.h"

#define SAMPLE_SILENCE (0.0f)

struct audio_param {
	int samplerate;
	int samplebyte;
	int channel;
	void *data;

	int dump_fd;
	int src_fd;
	int idle_id;
};

static NuguRecorderDriver *rec_driver;
static pthread_mutex_t mutex;

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

	_dumpfile_link(buf);
	g_free(buf);

	return fd;
}

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

	if (param->dump_fd != -1) {
		if (write(param->dump_fd, buf, buf_size) < 0)
			nugu_error("write to fd-%d failed", param->dump_fd);
	}
}

static long _get_new_audio_size(struct audio_param *param)
{
	long offset, total;
	long audio_size = 0;

	offset = lseek(param->src_fd, 0L, SEEK_CUR);
	total = lseek(param->src_fd, 0L, SEEK_END);
	if (total > 0 && offset >= 0 && total > offset) {
		audio_size = total - offset;
		lseek(param->src_fd, offset, SEEK_SET);
	}
	return audio_size;
}

static int _read_and_push_audio_from_file(struct audio_param *param)
{
	char *buf;
	ssize_t nread;
	long buf_size;

	buf_size = _get_new_audio_size(param);
	if (!buf_size)
		return 0;

	buf = malloc(buf_size);
	if (!buf) {
		nugu_error_nomem();
		return -1;
	}

	nread = read(param->src_fd, buf, buf_size);
	if (nread < 0) {
		nugu_error("read() failed: %s", strerror(errno));
		free(buf);
		return -1;
	} else if (nread == 0) {
		nugu_dbg("read zero pcm data from file");
		free(buf);
		return 0;
	}

	nugu_info("read => %d", nread);
	_recorder_push(param, buf, nread);

	free(buf);
	return 0;
}

static gboolean _record_callback(gpointer userdata)
{
	NuguRecorder *rec = (NuguRecorder *)userdata;
	struct audio_param *rec_param;

	pthread_mutex_lock(&mutex);
	rec_param = nugu_recorder_get_driver_data(rec);
	if (rec_param == NULL || rec_param->src_fd == -1) {
		nugu_dbg("record is stopped");
		pthread_mutex_unlock(&mutex);
		return FALSE;
	}

	if (_read_and_push_audio_from_file(rec_param) < 0) {
		nugu_error("fail to read and push audio");
		close(rec_param->src_fd);
		rec_param->src_fd = -1;
		rec_param->idle_id = 0;

		pthread_mutex_unlock(&mutex);
		return FALSE;
	}

	pthread_mutex_unlock(&mutex);
	return TRUE;
}

static int _rec_start(NuguRecorderDriver *driver, NuguRecorder *rec,
		      NuguAudioProperty prop)
{
	struct audio_param *rec_param = nugu_recorder_get_driver_data(rec);
	char *rec_file;
	int rec_5sec;
	int rec_100ms; // 3200 byte
	int src_fd = -1;

	if (rec_param) {
		nugu_dbg("already start");
		return 0;
	}

	rec_file = getenv(NUGU_ENV_RECORDING_FROM_FILE);
	if (rec_file == NULL) {
		nugu_error("can't get recording filename");
		return -1;
	}

	src_fd = open(rec_file, O_RDONLY);
	if (src_fd == -1) {
		nugu_error("can't open the file: '%s'", rec_file);
		return -1;
	}
	nugu_dbg("recording from file: '%s'", rec_file);

	rec_param = (struct audio_param *)g_malloc0(sizeof(struct audio_param));

	if (_set_property_to_param(rec_param, prop) != 0) {
		g_free(rec_param);
		close(src_fd);
		return -1;
	}

	rec_param->data = (void *)rec;
	rec_100ms = rec_param->samplerate * rec_param->samplebyte / 10;
	rec_5sec =
		rec_param->samplerate * rec_param->samplebyte * 5 / rec_100ms;
	nugu_dbg("rec - %d, %d", rec_100ms, rec_5sec);
	nugu_recorder_set_frame_size(rec, rec_100ms, rec_5sec);

	rec_param->dump_fd =
		_dumpfile_open(getenv(NUGU_ENV_DUMP_PATH_RECORDER), "rec");
	rec_param->src_fd = src_fd;

	nugu_recorder_set_driver_data(rec, rec_param);

	rec_param->idle_id = g_idle_add(_record_callback, (gpointer)rec);

	nugu_dbg("start done");
	return 0;
}

static int _rec_stop(NuguRecorderDriver *driver, NuguRecorder *rec)
{
	struct audio_param *rec_param = nugu_recorder_get_driver_data(rec);

	pthread_mutex_lock(&mutex);

	if (rec_param == NULL) {
		nugu_dbg("already stop");
		pthread_mutex_unlock(&mutex);
		return 0;
	}

	if (rec_param->dump_fd >= 0) {
		close(rec_param->dump_fd);
		rec_param->dump_fd = -1;
	}

	if (rec_param->src_fd >= 0) {
		close(rec_param->src_fd);
		rec_param->src_fd = -1;
	}

	if (rec_param->idle_id) {
		g_source_remove(rec_param->idle_id);
		rec_param->idle_id = 0;
	}

	g_free(rec_param);
	nugu_recorder_set_driver_data(rec, NULL);

	pthread_mutex_unlock(&mutex);

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
	const struct nugu_plugin_desc *desc;

	desc = nugu_plugin_get_description(p);
	nugu_dbg("'%s' plugin initialized", desc->name);

	if (!getenv(NUGU_ENV_RECORDING_FROM_FILE)) {
		nugu_error("must set environment => NUGU_RECORDING_FROM_FILE");
		return -1;
	}

	rec_driver = nugu_recorder_driver_new(desc->name, &rec_ops);
	if (!rec_driver) {
		nugu_error("nugu_recorder_driver_new() failed");
		return -1;
	}

	if (nugu_recorder_driver_register(rec_driver) != 0) {
		nugu_recorder_driver_free(rec_driver);
		rec_driver = NULL;
		return -1;
	}

	pthread_mutex_init(&mutex, NULL);

	nugu_dbg("'%s' plugin initialized done", desc->name);

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
	"filereader",
	NUGU_PLUGIN_PRIORITY_LOW,
	"0.0.1",
	load,
	unload,
	init
);
