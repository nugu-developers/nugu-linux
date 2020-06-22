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
#include "base/nugu_recorder.h"
#include "base/nugu_pcm.h"
#include "base/nugu_prof.h"

#define PLUGIN_DRIVER_NAME "portaudio"
#define SAMPLE_SILENCE (0.0f)
#define FRAME_PER_BUFFER 512

//#define DEBUG_PCM
//#define ASYNC_PLAYBACK

#ifndef ASYNC_PLAYBACK
enum SYNC_FLAG {
	SYNC_FLAG_NOT_RUNNING,
	SYNC_FLAG_READY,
	SYNC_FLAG_DATA,
	SYNC_FLAG_EXIT
};
#endif

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
	int dump_fd;

#ifndef ASYNC_PLAYBACK
	pthread_t tid;
	pthread_mutex_t lock;
	pthread_cond_t cond;
	enum SYNC_FLAG flag;
	int running;
#endif
};

static NuguRecorderDriver *rec_driver;
static NuguPcmDriver *pcm_driver;

#if defined(NUGU_ENV_DUMP_PATH_RECORDER) || defined(NUGU_ENV_DUMP_PATH_PCM)
static int _dumpfile_open(const char *path, const char *prefix)
{
	char ymd[9];
	char hms[7];
	time_t now;
	struct tm now_tm;
	char *buf = NULL;
	int fd;

	if (!path)
		return -1;

	now = time(NULL);
	localtime_r(&now, &now_tm);

	snprintf(ymd, 9, "%04d%02d%02d", now_tm.tm_year + 1900,
		 now_tm.tm_mon + 1, now_tm.tm_mday);
	snprintf(hms, 7, "%02d%02d%02d", now_tm.tm_hour, now_tm.tm_min,
		 now_tm.tm_sec);

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
	NuguRecorder *rec = (NuguRecorder *)param->data;
	char *buf = (char *)inputBuffer;
	int finished = paContinue;
	int buf_size = framesPerBuffer * param->samplebyte;

	(void)outputBuffer; /* Prevent unused variable warnings. */
	(void)timeInfo;
	(void)statusFlags;

	if (inputBuffer == NULL) {
		buf = malloc(buf_size);
		memset(buf, SAMPLE_SILENCE, buf_size);
	}

	nugu_recorder_push_frame(rec, buf, buf_size);

	if (param->dump_fd != -1) {
		if (write(param->dump_fd, buf, buf_size) < 0)
			nugu_error("write to fd-%d failed", param->dump_fd);
	}

	if (inputBuffer == NULL)
		free(buf);

	if (param->stop)
		finished = paComplete;

	return finished;
}

static gboolean _playerEndOfStream(void *userdata)
{
	NuguPcm *pcm = (NuguPcm *)userdata;

	if (pcm)
		nugu_pcm_emit_event(pcm, NUGU_MEDIA_EVENT_END_OF_STREAM);
	else
		nugu_error("wrong parameter");

	return FALSE;
}

#ifdef ASYNC_PLAYBACK
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
	NuguPcm *pcm = (NuguPcm *)param->data;
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
		param->write_data += buf_size;

#ifndef DUMP_ON_PCM_PUSH_DATA
		if (param->dump_fd != -1) {
			if (write(param->dump_fd, buf, buf_size) < 0)
				nugu_error("write to fd-%d failed",
					   param->dump_fd);
		}
#endif
	} else if (nugu_pcm_receive_is_last_data(pcm)) {
		// send event to the main loop thread
		if (!param->done) {
			g_timeout_add(guard_time, _playerEndOfStream,
				      (gpointer)pcm);
			param->done = 1;
		}
	}

	if (param->stop)
		finished = paComplete;

	return finished;
}
#endif

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
#else
	rec_param->dump_fd = -1;
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

	if (rec_param->dump_fd >= 0) {
		close(rec_param->dump_fd);
		rec_param->dump_fd = -1;
	}

	g_free(rec_param);
	nugu_recorder_set_driver_data(rec, NULL);

	nugu_dbg("stop done");

	return 0;
}

#ifndef ASYNC_PLAYBACK
static void *_play_loop(void *data)
{
	NuguPcm *pcm = data;
	struct pa_audio_param *pcm_param = nugu_pcm_get_driver_data(pcm);
	char *buf = NULL;
	size_t buf_size = 0;
	PaError err;
	int is_last = 0;
	size_t data_size;

	pthread_mutex_lock(&pcm_param->lock);
	pcm_param->flag = SYNC_FLAG_READY;
	pthread_cond_signal(&pcm_param->cond);
	pthread_mutex_unlock(&pcm_param->lock);

	while (pcm_param->running) {
		/* wait for new data */
		pthread_mutex_lock(&pcm_param->lock);
		if (pcm_param->flag == SYNC_FLAG_READY)
			pthread_cond_wait(&pcm_param->cond, &pcm_param->lock);

		if (pcm_param->flag == SYNC_FLAG_EXIT) {
			pthread_mutex_unlock(&pcm_param->lock);
			break;
		} else if (pcm_param->flag == SYNC_FLAG_DATA) {

			is_last = nugu_pcm_receive_is_last_data(pcm);

			data_size = nugu_pcm_get_data_size(pcm);
			if (data_size > 0) {
				/* resize the buffer */
				if (data_size > buf_size) {
					if (buf)
						free(buf);

					buf_size = data_size;
					buf = malloc(buf_size);
				}

				nugu_pcm_get_data(pcm, buf, buf_size);
				pcm_param->write_data += buf_size;
			}

			pcm_param->flag = SYNC_FLAG_READY;
		}
		pthread_mutex_unlock(&pcm_param->lock);

		if (pcm_param->is_first) {
			nugu_prof_mark(NUGU_PROF_TYPE_TTS_FIRST_PCM_WRITE);
			pcm_param->is_first = 0;
		}

		if (buf) {
			nugu_dbg("Pa_WriteStream %zd bytes", data_size);
			err = Pa_WriteStream(pcm_param->stream, buf,
					     data_size / pcm_param->samplebyte);
			if (err != paNoError) {
				nugu_error("Pa_WriteStream return fail: %d",
					   err);
				continue;
			}

			if (is_last) {
				nugu_dbg("last pcm data");
				g_idle_add(_playerEndOfStream, pcm);
			}
		}
	}

	if (buf)
		free(buf);

	nugu_dbg("_play_loop finished");

	return NULL;
}
#endif

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

	pcm_param->data = (void *)pcm;

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

#ifdef ASYNC_PLAYBACK
	err = Pa_OpenStream(&pcm_param->stream, NULL, /* no input */
			    &output_param, pcm_param->samplerate,
			    FRAME_PER_BUFFER,
			    paClipOff, /* don't bother clipping them */
			    _playbackCallback, pcm_param);
#else
	err = Pa_OpenStream(&pcm_param->stream, NULL, /* no input */
			    &output_param, pcm_param->samplerate,
			    FRAME_PER_BUFFER,
			    paClipOff, /* don't bother clipping them */
			    NULL, NULL);
#endif
	if (err != paNoError) {
		nugu_error("Pa_OpenStream return fail");
		g_free(pcm_param);
		return -1;
	}

	nugu_pcm_set_driver_data(pcm, pcm_param);

#ifndef ASYNC_PLAYBACK
	pcm_param->flag = SYNC_FLAG_NOT_RUNNING;
	pcm_param->running = 1;

	pthread_mutex_init(&pcm_param->lock, NULL);
	pthread_cond_init(&pcm_param->cond, NULL);
	pthread_create(&pcm_param->tid, NULL, _play_loop, pcm);

	/* Wait for thread creation */
	pthread_mutex_lock(&pcm_param->lock);
	if (pcm_param->flag == SYNC_FLAG_NOT_RUNNING)
		pthread_cond_wait(&pcm_param->cond, &pcm_param->lock);
	pthread_mutex_unlock(&pcm_param->lock);
#endif

	return 0;
}

static void _pcm_destroy(NuguPcmDriver *driver, NuguPcm *pcm)
{
	PaError err = paNoError;
	struct pa_audio_param *pcm_param = nugu_pcm_get_driver_data(pcm);

#ifndef ASYNC_PLAYBACK
	if (pcm_param->running) {
		pcm_param->running = 0;
		pthread_mutex_lock(&pcm_param->lock);
		pcm_param->flag = SYNC_FLAG_EXIT;
		pthread_cond_signal(&pcm_param->cond);
		pthread_mutex_unlock(&pcm_param->lock);
	}

	pthread_join(pcm_param->tid, NULL);
#endif

	err = Pa_CloseStream(pcm_param->stream);
	if (err != paNoError)
		nugu_error("Pa_CloseStream return fail(%d)", err);

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
#else
	pcm_param->dump_fd = -1;
#endif

	nugu_pcm_emit_status(pcm, NUGU_MEDIA_STATUS_READY);

	pcm_param->is_start = 1;
	pcm_param->is_first = 1;
	pcm_param->pause = 0;
	pcm_param->stop = 0;
	pcm_param->write_data = 0;
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

#ifdef ASYNC_PLAYBACK
	while (Pa_IsStreamActive(pcm_param->stream) == 1)
		Pa_Sleep(10);
#endif

	if (pcm_param->dump_fd >= 0) {
		close(pcm_param->dump_fd);
		pcm_param->dump_fd = -1;
	}

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

	return (pcm_param->write_data / pcm_param->samplerate);
}

static void snd_error_log(const char *file, int line, const char *function,
			  int err, const char *fmt, ...)
{
	char msg[4096];
	va_list arg;

	va_start(arg, fmt);
	vsnprintf(msg, 4096, fmt, arg);
	va_end(arg);

	nugu_log_print(NUGU_LOG_MODULE_AUDIO, NUGU_LOG_LEVEL_DEBUG, NULL, NULL,
		       -1, "[ALSA] <%s:%d> err=%d, %s", file, line, err, msg);
}

static int _pcm_push_data(NuguPcmDriver *driver, NuguPcm *pcm, const char *data,
			  size_t size, int is_last)
{
	struct pa_audio_param *pcm_param = nugu_pcm_get_driver_data(pcm);
	int playing_flag = 0;

	if (pcm_param == NULL)
		nugu_error("pcm is not started");

	if (pcm_param->is_first == 1)
		playing_flag = 1;

#ifndef ASYNC_PLAYBACK
	if (pcm_param->dump_fd != -1) {
		if (write(pcm_param->dump_fd, data, size) < 0)
			nugu_error("write pcm data failed");
	}

	pthread_mutex_lock(&pcm_param->lock);
	pcm_param->flag = SYNC_FLAG_DATA;
	pthread_cond_signal(&pcm_param->cond);
	pthread_mutex_unlock(&pcm_param->lock);
#endif

	if (playing_flag)
		nugu_pcm_emit_status(pcm, NUGU_MEDIA_STATUS_PLAYING);

	return 0;
}

static struct nugu_recorder_driver_ops rec_ops = {
	/* nugu_recorder_driver */
	.start = _rec_start, /* nugu_recorder_start() */
	.stop = _rec_stop /* nugu_recorder_stop() */
};

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

	if (Pa_Initialize() != paNoError) {
		nugu_error("initialize is failed");
		return -1;
	}

	rec_driver = nugu_recorder_driver_new(PLUGIN_DRIVER_NAME, &rec_ops);
	g_return_val_if_fail(rec_driver != NULL, -1);
	if (nugu_recorder_driver_register(rec_driver) != 0) {
		nugu_recorder_driver_free(rec_driver);
		rec_driver = NULL;
		return -1;
	}

	pcm_driver = nugu_pcm_driver_new(PLUGIN_DRIVER_NAME, &pcm_ops);
	g_return_val_if_fail(pcm_driver != NULL, -1);
	if (nugu_pcm_driver_register(pcm_driver) != 0) {
		nugu_recorder_driver_free(rec_driver);
		nugu_pcm_driver_free(pcm_driver);
		rec_driver = NULL;
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
	snd_lib_error_set_handler(snd_error_log);

	return 0;
}

static void unload(NuguPlugin *p)
{
	nugu_dbg("'%s' plugin unloaded", nugu_plugin_get_description(p)->name);

	Pa_Terminate();

	if (rec_driver) {
		nugu_recorder_driver_remove(rec_driver);
		nugu_recorder_driver_free(rec_driver);
		rec_driver = NULL;
	}
	if (pcm_driver) {
		nugu_pcm_driver_remove(pcm_driver);
		nugu_pcm_driver_free(pcm_driver);
		pcm_driver = NULL;
	}

	snd_lib_error_set_handler(NULL);

	nugu_dbg("'%s' plugin unloaded done",
		 nugu_plugin_get_description(p)->name);
}

NUGU_PLUGIN_DEFINE(
	/* NUGU SDK Plug-in description */
	PLUGIN_DRIVER_NAME, /* Plugin name */
	NUGU_PLUGIN_PRIORITY_DEFAULT, /* Plugin priority */
	"0.0.1", /* Plugin version */
	load, /* dlopen */
	unload, /* dlclose */
	init /* initialize */
);
