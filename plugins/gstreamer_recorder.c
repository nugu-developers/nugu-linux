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
#include <pthread.h>

#include <glib.h>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <gst/pbutils/pbutils.h>

#include "base/nugu_log.h"
#include "base/nugu_plugin.h"
#include "base/nugu_recorder.h"

#define PLUGIN_DRIVER_NAME "gstreamer_recorder"
#define SAMPLE_SILENCE (0.0f)

typedef struct gst_handle GstreamerHandle;

struct gst_handle {
	GstElement *pipeline;
	GstElement *audio_source;
	GstElement *caps_filter;
	GstElement *audio_sink;
#ifdef __APPLE__
	GstElement *audioconvert;
	GstElement *resample;
#endif
	GstState cur_state;
	int samplerate;
	int samplebyte;
	int channel;
	gulong sig_msg_hid;
	void *rec;

#ifdef NUGU_ENV_DUMP_PATH_RECORDER
	int dump_fd;
#endif

#ifdef NUGU_ENV_RECORDING_FROM_FILE
	int src_fd;
#endif
};

static pthread_mutex_t lock;
static NuguRecorderDriver *rec_driver;
static int _uniq_id;
static int _handle_destroyed;

#ifdef NUGU_ENV_DUMP_PATH_RECORDER
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

static int _set_property_to_param(GstreamerHandle *gh, NuguAudioProperty prop)
{
	GstCaps *caps;
	char format[64];

	g_return_val_if_fail(gh != NULL, -1);

	switch (prop.samplerate) {
	case NUGU_AUDIO_SAMPLE_RATE_8K:
		gh->samplerate = 8000;
		break;
	case NUGU_AUDIO_SAMPLE_RATE_16K:
		gh->samplerate = 16000;
		break;
	case NUGU_AUDIO_SAMPLE_RATE_32K:
		gh->samplerate = 32000;
		break;
	case NUGU_AUDIO_SAMPLE_RATE_22K:
		gh->samplerate = 22050;
		break;
	case NUGU_AUDIO_SAMPLE_RATE_44K:
		gh->samplerate = 44100;
		break;
	default:
		gh->samplerate = 16000;
		break;
	}

	switch (prop.format) {
	case NUGU_AUDIO_FORMAT_S8:
		snprintf(format, 64, "S8");
		gh->samplebyte = 1;
		break;
	case NUGU_AUDIO_FORMAT_U8:
		snprintf(format, 64, "U8");
		gh->samplebyte = 1;
		break;
	case NUGU_AUDIO_FORMAT_S16_LE:
		snprintf(format, 64, "S16LE");
		gh->samplebyte = 2;
		break;
	case NUGU_AUDIO_FORMAT_S16_BE:
		snprintf(format, 64, "S16BE");
		gh->samplebyte = 2;
		break;
	case NUGU_AUDIO_FORMAT_S24_LE:
		snprintf(format, 64, "S24LE");
		gh->samplebyte = 3;
		break;
	case NUGU_AUDIO_FORMAT_S24_BE:
		snprintf(format, 64, "S24BE");
		gh->samplebyte = 3;
		break;
	case NUGU_AUDIO_FORMAT_S32_LE:
		snprintf(format, 64, "S32LE");
		gh->samplebyte = 4;
		break;
	case NUGU_AUDIO_FORMAT_S32_BE:
		snprintf(format, 64, "S32BE");
		gh->samplebyte = 4;
		break;
	default:
		nugu_error("not support the audio format(%d)", prop.format);
		return -1;
	}

	switch (prop.channel) {
	case 1:
	case 2:
	case 4:
	case 8:
		gh->channel = prop.channel;
		break;
	default:
		nugu_error("not support the audio channel(%d)", prop.channel);
		return -1;
	}

	caps = gst_caps_new_simple("audio/x-raw", "format", G_TYPE_STRING,
				   format, "rate", G_TYPE_INT, gh->samplerate,
				   "channels", G_TYPE_INT, gh->channel, NULL);
	g_object_set(G_OBJECT(gh->caps_filter), "caps", caps, NULL);
	gst_caps_unref(caps);

	return 0;
}

static void _recorder_push(GstreamerHandle *gh, const char *buf, int buf_size)
{
	nugu_recorder_push_frame(gh->rec, buf, buf_size);

#ifdef NUGU_ENV_DUMP_PATH_RECORDER
	if (gh->dump_fd != -1) {
		if (write(gh->dump_fd, buf, buf_size) < 0)
			nugu_error("write to fd-%d failed", gh->dump_fd);
	}
#endif
}

#ifdef NUGU_ENV_RECORDING_FROM_FILE
static int _recording_from_file(GstreamerHandle *gh, int buf_size)
{
	char *buf;
	ssize_t nread;

	buf = malloc(buf_size);
	if (!buf) {
		nugu_error_nomem();
		return -1;
	}

	nread = read(gh->src_fd, buf, buf_size);
	if (nread < 0) {
		nugu_error("read() failed: %s", strerror(errno));

		free(buf);
		close(gh->src_fd);
		gh->src_fd = -1;
		return -1;
	} else if (nread == 0) {
		nugu_dbg("read all pcm data from file. fill the SILENCE data");

		memset(buf, SAMPLE_SILENCE, buf_size);
		nread = buf_size;
	}

	_recorder_push(gh, buf, nread);

	free(buf);

	return 0;
}
#endif

static GstFlowReturn _new_sample_from_sink(GstElement *sink,
					   GstreamerHandle *gh)
{
	GstSample *sample;
	GstBuffer *buffer;
	GstMapInfo g_map;

	sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
	buffer = gst_sample_get_buffer(sample);
	if (buffer == NULL) {
		nugu_error("There is no buffer");
		goto exit;
	}

	if (!gst_buffer_map(buffer, &g_map, GST_MAP_READ)) {
		nugu_error("read map buffer failed");
		goto exit;
	}

#ifdef NUGU_ENV_RECORDING_FROM_FILE
	/* Use file source instead of real microphone data */
	if (gh->src_fd != -1) {
		_recording_from_file(gh, g_map.size);
		gst_buffer_unmap(buffer, &g_map);
		goto exit;
	}
#endif

	_recorder_push(gh, (const char *)g_map.data, g_map.size);
	gst_buffer_unmap(buffer, &g_map);

exit:
	gst_sample_unref(sample);
	return GST_FLOW_OK;
}

static void _recorder_message(GstBus *bus, GstMessage *msg, GstreamerHandle *gh)
{
	GstState old_state, new_state, pending_state;
	GError *err;
	gchar *debug;

	pthread_mutex_lock(&lock);

	if (_handle_destroyed) {
		nugu_warn("gstreamer handle is already destroyed");
		pthread_mutex_unlock(&lock);
		return;
	}

	switch (GST_MESSAGE_TYPE(msg)) {
	case GST_MESSAGE_ERROR:
		gst_message_parse_error(msg, &err, &debug);
		nugu_error("GST_MESSAGE_ERROR - %s", err->message);
		g_error_free(err);
		g_free(debug);
		break;

	case GST_MESSAGE_STATE_CHANGED:
		gst_message_parse_state_changed(msg, &old_state, &new_state,
						&pending_state);

		if (gh->cur_state == new_state && pending_state == 0)
			break;

		gh->cur_state = new_state;

		nugu_dbg("state - %d -> %d (pending: %d)", old_state, new_state,
			 pending_state);
		break;

	default:
		break;
	}

	pthread_mutex_unlock(&lock);
}

static void _connect_message_to_pipeline(GstreamerHandle *gh)
{
	GstBus *bus = gst_element_get_bus(gh->pipeline);

	gst_bus_add_signal_watch(bus);
	gh->sig_msg_hid = g_signal_connect(G_OBJECT(bus), "message",
					   G_CALLBACK(_recorder_message), gh);

	gst_object_unref(bus);
}

static void _disconnect_message_to_pipeline(GstreamerHandle *gh)
{
	GstBus *bus = gst_element_get_bus(gh->pipeline);

	if (gh->sig_msg_hid == 0)
		return;

	g_signal_handler_disconnect(G_OBJECT(bus), gh->sig_msg_hid);
	gh->sig_msg_hid = 0;

	gst_object_unref(bus);
}

static GstreamerHandle *_create(NuguRecorder *rec)
{
	GstreamerHandle *gh;
	char pipeline[128];
	char audio_source[128];
	char caps_filter[128];
	char audio_sink[128];
#ifdef __APPLE__
	char audio_convert[128];
	char audio_resample[128];
#endif

	gh = nugu_recorder_get_driver_data(rec);
	if (gh != NULL) {
		nugu_dbg("already created");
		return gh;
	}

	g_snprintf(pipeline, 128, "rec_pipeline#%d", _uniq_id);
	g_snprintf(audio_source, 128, "rec_audio_source#%d", _uniq_id);
	g_snprintf(caps_filter, 128, "rec_caps_filter#%d", _uniq_id);
	g_snprintf(audio_sink, 128, "rec_audio_sink#%d", _uniq_id);
#ifdef __APPLE__
	g_snprintf(audio_convert, 128, "rec_audio_convert#%d", _uniq_id);
	g_snprintf(audio_resample, 128, "rec_audio_resample#%d", _uniq_id);
#endif

	gh = (GstreamerHandle *)g_malloc0(sizeof(GstreamerHandle));
	if (!gh) {
		nugu_error_nomem();
		return NULL;
	}

	gh->pipeline = gst_pipeline_new(pipeline);
	gh->audio_source =
		gst_element_factory_make("autoaudiosrc", audio_source);
	gh->caps_filter = gst_element_factory_make("capsfilter", caps_filter);
#ifdef __APPLE__
	gh->audioconvert =
		gst_element_factory_make("audioconvert", audio_convert);
	gh->resample =
		gst_element_factory_make("audioresample", audio_resample);
#endif
	gh->audio_sink = gst_element_factory_make("appsink", audio_sink);

	g_object_set(gh->audio_sink, "emit-signals", TRUE, "sync", FALSE, NULL);
	g_signal_connect(gh->audio_sink, "new-sample",
			 G_CALLBACK(_new_sample_from_sink), gh);

#ifdef __APPLE__
	gst_bin_add_many(GST_BIN(gh->pipeline), gh->audio_source,
			 gh->audioconvert, gh->resample, gh->caps_filter,
			 gh->audio_sink, NULL);
	gst_element_link_many(gh->audio_source, gh->audioconvert, gh->resample,
			      gh->caps_filter, gh->audio_sink, NULL);
#else
	gst_bin_add_many(GST_BIN(gh->pipeline), gh->audio_source,
			 gh->caps_filter, gh->audio_sink, NULL);
	gst_element_link_many(gh->audio_source, gh->caps_filter, gh->audio_sink,
			      NULL);
#endif

	_connect_message_to_pipeline(gh);

	_handle_destroyed = 0;

	return gh;
}

static void _destroy(NuguRecorder *rec)
{
	GstreamerHandle *gh;

	gh = nugu_recorder_get_driver_data(rec);
	if (gh == NULL) {
		nugu_dbg("already destroyed");
		return;
	}

	_disconnect_message_to_pipeline(gh);

	if (gh->pipeline)
		g_object_unref(gh->pipeline);

	memset(gh, 0, sizeof(GstreamerHandle));
	g_free(gh);
	nugu_recorder_set_driver_data(rec, NULL);

	_handle_destroyed = 1;
}

static int _rec_start(NuguRecorderDriver *driver, NuguRecorder *rec,
		      NuguAudioProperty prop)
{
	GstreamerHandle *gh = nugu_recorder_get_driver_data(rec);
	int rec_5sec;
	int rec_100ms;

	g_return_val_if_fail(rec != NULL, -1);

	if (gh) {
		nugu_dbg("already start");
		return 0;
	}

	pthread_mutex_lock(&lock);

	gh = _create(rec);
	if (gh == NULL) {
		nugu_error("error: create gst handle failed");
		_destroy(rec);
		pthread_mutex_unlock(&lock);
		return -1;
	}

	if (_set_property_to_param(gh, prop) != 0) {
		nugu_error("error: set param failed");
		_destroy(rec);
		pthread_mutex_unlock(&lock);
		return -1;
	}

	gh->rec = (void *)rec;

	rec_100ms = gh->samplerate * gh->samplebyte / 10;
	rec_5sec = gh->samplerate * gh->samplebyte * 5 / rec_100ms;

	nugu_dbg("rec - %d, %d", rec_100ms, rec_5sec);
	nugu_recorder_set_frame_size(rec, rec_100ms, rec_5sec);

#ifdef NUGU_ENV_DUMP_PATH_RECORDER
	gh->dump_fd =
		_dumpfile_open(getenv(NUGU_ENV_DUMP_PATH_RECORDER), "gstrec");
#endif

#ifdef NUGU_ENV_RECORDING_FROM_FILE
	if (getenv(NUGU_ENV_RECORDING_FROM_FILE)) {
		gh->src_fd =
			open(getenv(NUGU_ENV_RECORDING_FROM_FILE), O_RDONLY);
		nugu_dbg("recording from file: '%s'",
			 getenv(NUGU_ENV_RECORDING_FROM_FILE));
	} else {
		gh->src_fd = -1;
	}
#endif

	gst_element_set_state(gh->pipeline, GST_STATE_PLAYING);

	nugu_recorder_set_driver_data(rec, gh);

	pthread_mutex_unlock(&lock);

	nugu_dbg("start done");
	return 0;
}

static int _rec_stop(NuguRecorderDriver *driver, NuguRecorder *rec)
{
	GstreamerHandle *gh = nugu_recorder_get_driver_data(rec);

	g_return_val_if_fail(rec != NULL, -1);

	if (gh == NULL) {
		nugu_dbg("already stop");
		return -1;
	}

	pthread_mutex_lock(&lock);

#ifdef NUGU_ENV_DUMP_PATH_RECORDER
	if (gh->dump_fd >= 0) {
		close(gh->dump_fd);
		gh->dump_fd = -1;
	}
#endif

#ifdef NUGU_ENV_RECORDING_FROM_FILE
	if (gh->src_fd >= 0) {
		close(gh->src_fd);
		gh->src_fd = -1;
	}
#endif

	gst_element_set_state(gh->pipeline, GST_STATE_NULL);

	_destroy(rec);

	nugu_recorder_set_driver_data(rec, NULL);

	pthread_mutex_unlock(&lock);

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

	if (gst_is_initialized() == FALSE)
		gst_init(NULL, NULL);

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

	pthread_mutex_init(&lock, NULL);

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
	"0.0.1", /* Plugin version */
	load, /* dlopen */
	unload, /* dlclose */
	init /* initialize */
);
