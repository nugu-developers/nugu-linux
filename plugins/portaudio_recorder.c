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
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#include <alsa/error.h>
#include <glib.h>
#include <portaudio.h>

#include "base/nugu_log.h"
#include "base/nugu_plugin.h"
#include "base/nugu_recorder.h"

#define PLUGIN_DRIVER_NAME "portaudio_recorder"
#define SAMPLE_SILENCE (0.0f)

struct pa_audio_param {
	PaStream *stream;
	int samplerate;
	int samplebyte;
	PaSampleFormat format;
	int channel;
	int stop;
	int pause;
	int done;
	size_t write_data;
	void *data;
	int is_start;
	int is_first;

#if defined(NUGU_ENV_DUMP_PATH_RECORDER)
	int dump_fd;
#endif

#ifdef NUGU_ENV_RECORDING_FROM_FILE
	int src_fd;
#endif
};

static NuguRecorderDriver *rec_driver;

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

	free(buf);

	return fd;
}
#endif

static int _set_property_to_param(struct pa_audio_param *param,
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
		param->format = paInt8;
		param->samplebyte = 1;
		break;
	case NUGU_AUDIO_FORMAT_U8:
		param->format = paUInt8;
		param->samplebyte = 1;
		break;
	case NUGU_AUDIO_FORMAT_S16_LE:
	case NUGU_AUDIO_FORMAT_S16_BE:
		param->format = paInt16;
		param->samplebyte = 2;
		break;
	case NUGU_AUDIO_FORMAT_S24_LE:
	case NUGU_AUDIO_FORMAT_S24_BE:
		param->format = paInt24;
		param->samplebyte = 3;
		break;
	case NUGU_AUDIO_FORMAT_S32_LE:
	case NUGU_AUDIO_FORMAT_S32_BE:
		param->format = paFloat32;
		param->samplebyte = 4;
		break;
	default:
		nugu_error("not support the audio format(%d)", prop.format);
		return -1;
	}
	param->channel = prop.channel;

	return 0;
}

static void _recorder_push(struct pa_audio_param *param, const char *buf,
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
static int _recording_from_file(struct pa_audio_param *param, int buf_size)
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

/* This routine will be called by the PortAudio engine when audio is needed.
 * It may be called at interrupt level on some machines so don't do anything
 * that could mess up the system like calling malloc() or free().
 */
static int _recordCallback(const void *inputBuffer, void *outputBuffer,
			   unsigned long framesPerBuffer,
			   const PaStreamCallbackTimeInfo *timeInfo,
			   PaStreamCallbackFlags statusFlags, void *userData)
{
	struct pa_audio_param *param = (struct pa_audio_param *)userData;
	int buf_size = framesPerBuffer * param->samplebyte;
	int finished = paContinue;

	if (param->stop)
		finished = paComplete;

	/* Fill the buffer to SILENCE data */
	if (inputBuffer == NULL) {
		char *buf;

		buf = malloc(buf_size);
		if (!buf) {
			nugu_error_nomem();
			return finished;
		}

		memset(buf, SAMPLE_SILENCE, buf_size);

		_recorder_push(param, buf, buf_size);

		free(buf);

		return finished;
	}

#ifdef NUGU_ENV_RECORDING_FROM_FILE
	/* Use file source instead of real microphone data */
	if (param->src_fd != -1) {
		if (_recording_from_file(param, buf_size) < 0)
			return paComplete;
		else
			return finished;
	}
#endif

	_recorder_push(param, inputBuffer, buf_size);

	return finished;
}

static int _rec_start(NuguRecorderDriver *driver, NuguRecorder *rec,
		      NuguAudioProperty prop)
{
	PaStreamParameters input_param;
	PaError err = paNoError;
	struct pa_audio_param *rec_param = nugu_recorder_get_driver_data(rec);
	int rec_5sec;
	int rec_100ms;

	if (rec_param) {
		nugu_dbg("already start");
		return 0;
	}

	rec_param = (struct pa_audio_param *)g_malloc0(
		sizeof(struct pa_audio_param));

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

	/* default input device */
	input_param.device = Pa_GetDefaultInputDevice();
	if (input_param.device == paNoDevice) {
		nugu_error("no default input device");
		g_free(rec_param);
		return -1;
	}
	input_param.channelCount = rec_param->channel;
	input_param.sampleFormat = rec_param->format;
	input_param.suggestedLatency =
		Pa_GetDeviceInfo(input_param.device)->defaultLowInputLatency;
	input_param.hostApiSpecificStreamInfo = NULL;

	err = Pa_OpenStream(&rec_param->stream, &input_param,
			    NULL, /* &outputParameters, */
			    rec_param->samplerate, rec_100ms,
			    paClipOff, /* don't bother clipping them */
			    _recordCallback, rec_param);
	if (err != paNoError) {
		nugu_error("Pa_OpenStream return fail");
		g_free(rec_param);
		return -1;
	}

	rec_param->stop = 0;
	err = Pa_StartStream(rec_param->stream);
	if (err != paNoError) {
		nugu_error("Pa_OpenStream return fail");
		g_free(rec_param);
		return -1;
	}

#ifdef NUGU_ENV_DUMP_PATH_RECORDER
	rec_param->dump_fd =
		_dumpfile_open(getenv(NUGU_ENV_DUMP_PATH_RECORDER), "parec");
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
	PaError err = paNoError;
	struct pa_audio_param *rec_param = nugu_recorder_get_driver_data(rec);

	if (rec_param == NULL) {
		nugu_dbg("already stop");
		return 0;
	}

	rec_param->stop = 1;
	while (Pa_IsStreamActive(rec_param->stream) == 1)
		Pa_Sleep(10);

	err = Pa_CloseStream(rec_param->stream);
	if (err != paNoError)
		nugu_error("Pa_CloseStream return fail");

#ifdef NUGU_ENV_DUMP_PATH_RECORDER
	if (rec_param->dump_fd >= 0) {
		close(rec_param->dump_fd);
		rec_param->dump_fd = -1;
	}
#endif

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

	if (!nugu_plugin_find("portaudio")) {
		nugu_error("portaudio plugin is not initialized");
		return -1;
	}

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

	nugu_recorder_driver_set_default(rec_driver);

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
	/* NUGU SDK Plug-in description */
	PLUGIN_DRIVER_NAME, /* Plugin name */
	NUGU_PLUGIN_PRIORITY_DEFAULT, /* Plugin priority */
	"0.0.2", /* Plugin version */
	load, /* dlopen */
	unload, /* dlclose */
	init /* initialize */
);
