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
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#include <alsa/error.h>
#include <glib.h>
#include <portaudio.h>

#include "base/nugu_log.h"
#include "base/nugu_plugin.h"
#include "base/nugu_pcm.h"
#include "base/nugu_prof.h"

#define PLUGIN_DRIVER_NAME "portaudio_pcm_async"
#define FRAME_PER_BUFFER 512
#define SAMPLE_SILENCE (0.0f)

//#define DEBUG_PCM

struct pa_audio_param {
	PaStream *stream;
	NuguPcm *pcm;

	int samplerate;
	int samplebyte;
	PaSampleFormat format;
	int channel;

	size_t written;

	int stop;
	int pause;
	int done;

	int is_start;
	int is_first;

	guint timer;

#if defined(NUGU_ENV_DUMP_PATH_PCM)
	int dump_fd;
#endif
};

static NuguPcmDriver *pcm_driver;

#if defined(NUGU_ENV_DUMP_PATH_PCM)
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

static gboolean _playerEndOfStream(void *userdata)
{
	struct pa_audio_param *param = userdata;

	if (!param) {
		nugu_error("wrong parameter");
		return FALSE;
	}

	param->timer = 0;

	if (param->pcm)
		nugu_pcm_emit_event(param->pcm, NUGU_MEDIA_EVENT_END_OF_STREAM);
	else
		nugu_error("wrong parameter");

	return FALSE;
}

/* This routine will be called by the PortAudio engine when audio is needed.
 * It may be called at interrupt level on some machines so don't do anything
 * that could mess up the system like calling malloc() or free().
 */
static int _playbackCallback(const void *inputBuffer, void *outputBuffer,
			     unsigned long framesPerBuffer,
			     const PaStreamCallbackTimeInfo *timeInfo,
			     PaStreamCallbackFlags statusFlags, void *userData)
{
	struct pa_audio_param *param = (struct pa_audio_param *)userData;
	NuguPcm *pcm = param->pcm;
	char *buf = (char *)outputBuffer;
	int finished = paContinue;
	int buf_size = framesPerBuffer * param->samplebyte;
	int guard_time = 500;

	(void)inputBuffer; /* Prevent unused variable warnings. */
	(void)timeInfo;
	(void)statusFlags;

	memset(buf, SAMPLE_SILENCE, buf_size);

#ifdef DEBUG_PCM
	nugu_info("#### pcm(%p) param is used(%p) - %d ####", pcm, param,
		  nugu_pcm_get_data_size(pcm));
#endif

	if (param->pause)
		return finished;

	if (nugu_pcm_get_data_size(pcm) > 0) {
		if (param->is_first) {
			nugu_prof_mark(NUGU_PROF_TYPE_TTS_FIRST_PCM_WRITE);
			param->is_first = 0;
		}

		nugu_pcm_get_data(pcm, buf, buf_size);
		param->written += buf_size;

#ifdef NUGU_ENV_DUMP_PATH_PCM
		if (param->dump_fd != -1) {
			if (write(param->dump_fd, buf, buf_size) < 0)
				nugu_error("write to fd-%d failed",
					   param->dump_fd);
		}
#endif
	} else if (nugu_pcm_receive_is_last_data(pcm)) {
		// send event to the main loop thread
		if (!param->done) {
			if (param->timer != 0)
				g_source_remove(param->timer);

			param->timer = g_timeout_add(guard_time,
						     _playerEndOfStream, param);
			param->done = 1;
		}
	}

	if (param->stop)
		finished = paComplete;

	return finished;
}

static int _pcm_create(NuguPcmDriver *driver, NuguPcm *pcm,
		       NuguAudioProperty prop)
{
	PaStreamParameters output_param;
	PaError err = paNoError;
	struct pa_audio_param *pcm_param;
	PaHostApiIndex hostapi_count;
	PaDeviceIndex dev_count;
	int i;

	pcm_param = g_malloc0(sizeof(struct pa_audio_param));

#ifdef DEBUG_PCM
	nugu_info("#### pcm(%p) param is created(%p) ####", pcm, pcm_param);
#endif

	if (_set_property_to_param(pcm_param, prop) != 0) {
		g_free(pcm_param);
		return -1;
	}

	hostapi_count = Pa_GetHostApiCount();
	nugu_dbg("Pa_GetHostApiCount() = %d", hostapi_count);
	for (i = 0; i < hostapi_count; i++) {
		const struct PaHostApiInfo *hostapi = Pa_GetHostApiInfo(i);

		if (hostapi)
			nugu_dbg("[%d] type=%d, name=%s, deviceCount=%d", i,
				 hostapi->type, hostapi->name,
				 hostapi->deviceCount);
	}

	dev_count = Pa_GetDeviceCount();
	nugu_dbg("Pa_GetDeviceCount() = %d", dev_count);
	for (i = 0; i < dev_count; i++) {
		const struct PaDeviceInfo *device = Pa_GetDeviceInfo(i);

		if (device)
			nugu_dbg("[%d] name: '%s', hostApi: %d", i,
				 device->name, device->hostApi);
	}

	pcm_param->pcm = pcm;

	/* default output device */
	output_param.device = Pa_GetDefaultOutputDevice();
	nugu_dbg("Pa_GetDefaultOutputDevice() = %d", output_param.device);
	if (output_param.device == paNoDevice) {
		nugu_error("no default output device");
		g_free(pcm_param);
		return -1;
	}

	output_param.channelCount = pcm_param->channel;
	output_param.sampleFormat = pcm_param->format;
	output_param.suggestedLatency =
		Pa_GetDeviceInfo(output_param.device)->defaultLowOutputLatency;
	output_param.hostApiSpecificStreamInfo = NULL;

	err = Pa_OpenStream(&pcm_param->stream, NULL, /* no input */
			    &output_param, pcm_param->samplerate,
			    FRAME_PER_BUFFER,
			    paClipOff, /* don't bother clipping them */
			    _playbackCallback, pcm_param);
	if (err != paNoError) {
		nugu_error("Pa_OpenStream return fail");
		g_free(pcm_param);
		return -1;
	}

	nugu_pcm_set_driver_data(pcm, pcm_param);

	return 0;
}

static void _pcm_destroy(NuguPcmDriver *driver, NuguPcm *pcm)
{
	PaError err = paNoError;
	struct pa_audio_param *pcm_param = nugu_pcm_get_driver_data(pcm);

	err = Pa_CloseStream(pcm_param->stream);
	if (err != paNoError)
		nugu_error("Pa_CloseStream return fail(%d)", err);

	if (pcm_param->timer) {
		nugu_dbg("remove pending timer");
		g_source_remove(pcm_param->timer);
		pcm_param->timer = 0;
	}

	free(pcm_param);
	nugu_pcm_set_driver_data(pcm, NULL);

#ifdef DEBUG_PCM
	nugu_info("#### pcm(%p) param is destroyed(%p) ####", pcm, pcm_param);
#endif
}

static int _pcm_start(NuguPcmDriver *driver, NuguPcm *pcm)
{
	PaError err = paNoError;
	struct pa_audio_param *pcm_param = nugu_pcm_get_driver_data(pcm);

	g_return_val_if_fail(pcm != NULL, -1);

	if (pcm_param == NULL) {
		nugu_error("internal error");
		return -1;
	}

	if (pcm_param->is_start) {
		nugu_dbg("already started");
		return 0;
	}

#ifdef NUGU_ENV_DUMP_PATH_PCM
	pcm_param->dump_fd =
		_dumpfile_open(getenv(NUGU_ENV_DUMP_PATH_PCM), "papcm");
#endif

	nugu_pcm_emit_status(pcm, NUGU_MEDIA_STATUS_READY);

	pcm_param->is_start = 1;
	pcm_param->is_first = 1;
	pcm_param->pause = 0;
	pcm_param->stop = 0;
	pcm_param->written = 0;
	pcm_param->done = 0;

	err = Pa_StartStream(pcm_param->stream);
	if (err != paNoError) {
		nugu_error("Pa_OpenStream return fail");
		g_free(pcm_param);
		return -1;
	}

	nugu_dbg("start done");
	return 0;
}

static int _pcm_stop(NuguPcmDriver *driver, NuguPcm *pcm)
{
	PaError err = paNoError;
	struct pa_audio_param *pcm_param = nugu_pcm_get_driver_data(pcm);

	g_return_val_if_fail(pcm != NULL, -1);

	if (pcm_param == NULL) {
		nugu_error("internal error");
		return -1;
	}

	if (pcm_param->is_start == 0) {
		nugu_dbg("already stopped");
		return 0;
	}

	pcm_param->pause = 0;
	pcm_param->stop = 1;

	while (Pa_IsStreamActive(pcm_param->stream) == 1)
		Pa_Sleep(10);

#ifdef NUGU_ENV_DUMP_PATH_PCM
	if (pcm_param->dump_fd >= 0) {
		close(pcm_param->dump_fd);
		pcm_param->dump_fd = -1;
	}
#endif

	nugu_dbg("Pa_StopStream");
	err = Pa_StopStream(pcm_param->stream);
	if (err != paNoError && err != paStreamIsStopped) {
		nugu_error("Pa_StopStream return fail(%d)", err);
		return -1;
	}

	pcm_param->is_start = 0;
	nugu_pcm_emit_status(pcm, NUGU_MEDIA_STATUS_STOPPED);

	nugu_dbg("stop done");

	return 0;
}

static int _pcm_pause(NuguPcmDriver *driver, NuguPcm *pcm)
{
	struct pa_audio_param *pcm_param = nugu_pcm_get_driver_data(pcm);

	g_return_val_if_fail(pcm != NULL, -1);

	if (pcm_param == NULL) {
		nugu_error("internal error");
		return -1;
	}

	if (pcm_param->pause) {
		nugu_dbg("pcm is already paused");
		return 0;
	}

	pcm_param->pause = 1;
	nugu_pcm_emit_status(pcm, NUGU_MEDIA_STATUS_PAUSED);

	nugu_dbg("pause done");

	return 0;
}

static int _pcm_resume(NuguPcmDriver *driver, NuguPcm *pcm)
{
	struct pa_audio_param *pcm_param = nugu_pcm_get_driver_data(pcm);

	g_return_val_if_fail(pcm != NULL, -1);

	if (pcm_param == NULL) {
		nugu_error("internal error");
		return -1;
	}

	if (!pcm_param->pause) {
		nugu_dbg("pcm is not paused");
		return 0;
	}

	pcm_param->pause = 0;
	nugu_pcm_emit_status(pcm, NUGU_MEDIA_STATUS_PLAYING);

	nugu_dbg("resume done");

	return 0;
}

static int _pcm_set_volume(NuguPcmDriver *driver, NuguPcm *pcm, int volume)
{
	struct pa_audio_param *pcm_param = nugu_pcm_get_driver_data(pcm);

	g_return_val_if_fail(pcm != NULL, -1);

	if (pcm_param == NULL) {
		nugu_error("internal error");
		return -1;
	}

	nugu_warn("This device is not support to set volume yet");
	return -1;
}

static int _pcm_get_position(NuguPcmDriver *driver, NuguPcm *pcm)
{
	struct pa_audio_param *pcm_param = nugu_pcm_get_driver_data(pcm);

	g_return_val_if_fail(pcm != NULL, -1);

	if (pcm_param == NULL) {
		nugu_error("internal error");
		return -1;
	}

	if (pcm_param->is_start == 0) {
		nugu_error("pcm is not started");
		return -1;
	}

	return (pcm_param->written / pcm_param->samplerate);
}

static int _pcm_push_data(NuguPcmDriver *driver, NuguPcm *pcm, const char *data,
			  size_t size, int is_last)
{
	struct pa_audio_param *pcm_param = nugu_pcm_get_driver_data(pcm);
	int playing_flag = 0;

	if (pcm_param == NULL) {
		nugu_error("pcm is not started");
		return -1;
	}

	if (pcm_param->is_first == 1)
		playing_flag = 1;

	if (playing_flag)
		nugu_pcm_emit_status(pcm, NUGU_MEDIA_STATUS_PLAYING);

	return 0;
}

static struct nugu_pcm_driver_ops pcm_ops = {
	/* nugu_pcm_driver */
	.create = _pcm_create, /* nugu_pcm_new() */
	.destroy = _pcm_destroy, /* nugu_pcm_free() */
	.start = _pcm_start, /* nugu_pcm_start() */
	.stop = _pcm_stop, /* nugu_pcm_stop() */
	.pause = _pcm_pause, /* nugu_pcm_pause() */
	.resume = _pcm_resume, /* nugu_pcm_resume() */
	.push_data = _pcm_push_data, /* nugu_pcm_push_data() */
	.set_volume = _pcm_set_volume, /* nugu_pcm_set_volume() */
	.get_position = _pcm_get_position /* nugu_pcm_get_position() */
};

static int init(NuguPlugin *p)
{
	nugu_dbg("'%s' plugin initialized",
		 nugu_plugin_get_description(p)->name);

	if (!nugu_plugin_find("portaudio")) {
		nugu_error("portaudio plugin is not initialized");
		return -1;
	}

	pcm_driver = nugu_pcm_driver_new(PLUGIN_DRIVER_NAME, &pcm_ops);
	if (!pcm_driver) {
		nugu_error("nugu_pcm_driver_new() failed");
		return -1;
	}

	if (nugu_pcm_driver_register(pcm_driver) != 0) {
		nugu_error("nugu_pcm_driver_register() failed");
		nugu_pcm_driver_free(pcm_driver);
		pcm_driver = NULL;
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

	if (pcm_driver) {
		nugu_pcm_driver_remove(pcm_driver);
		nugu_pcm_driver_free(pcm_driver);
		pcm_driver = NULL;
	}

	nugu_dbg("'%s' plugin unloaded done",
		 nugu_plugin_get_description(p)->name);
}

NUGU_PLUGIN_DEFINE(
	/* NUGU SDK Plug-in description */
	PLUGIN_DRIVER_NAME, /* Plugin name */
	NUGU_PLUGIN_PRIORITY_DEFAULT + 2, /* Plugin priority */
	"0.0.2", /* Plugin version */
	load, /* dlopen */
	unload, /* dlclose */
	init /* initialize */
);
