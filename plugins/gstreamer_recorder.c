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

#include <string.h>
#include <pthread.h>
#include <stdio.h>

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
	GstState cur_state;
	int samplerate;
	int samplebyte;
	int channel;
	void *rec;
};

static NuguRecorderDriver *rec_driver;
static int _uniq_id;

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

	nugu_recorder_push_frame(gh->rec, (const char *)g_map.data, g_map.size);
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
}

static void _connect_message_to_pipeline(GstreamerHandle *gh)
{
	GstBus *bus = gst_element_get_bus(gh->pipeline);

	gst_bus_add_signal_watch(bus);
	g_signal_connect(G_OBJECT(bus), "message",
			 G_CALLBACK(_recorder_message), gh);

	gst_object_unref(bus);
}

static GstreamerHandle *_create(NuguRecorder *rec)
{
	GstreamerHandle *gh;
	char pipeline[128];
	char audio_source[128];
	char caps_filter[128];
	char audio_sink[128];

	gh = nugu_recorder_get_driver_data(rec);
	if (gh != NULL) {
		nugu_dbg("already created");
		return gh;
	}

	g_snprintf(pipeline, 128, "rec_pipeline#%d", _uniq_id);
	g_snprintf(audio_source, 128, "rec_audio_source#%d", _uniq_id);
	g_snprintf(caps_filter, 128, "rec_caps_filter#%d", _uniq_id);
	g_snprintf(audio_sink, 128, "rec_audio_sink#%d", _uniq_id);

	gh = (GstreamerHandle *)g_malloc0(sizeof(GstreamerHandle));
	if (!gh) {
		nugu_error_nomem();
		return NULL;
	}
	gh->pipeline = gst_pipeline_new(pipeline);
	gh->audio_source =
		gst_element_factory_make("autoaudiosrc", audio_source);
	gh->caps_filter = gst_element_factory_make("capsfilter", caps_filter);
	gh->audio_sink = gst_element_factory_make("appsink", audio_sink);

	g_object_set(gh->audio_sink, "emit-signals", TRUE, "sync", FALSE, NULL);
	g_signal_connect(gh->audio_sink, "new-sample",
			 G_CALLBACK(_new_sample_from_sink), gh);

	gst_bin_add_many(GST_BIN(gh->pipeline), gh->audio_source,
			 gh->caps_filter, gh->audio_sink, NULL);
	gst_element_link_many(gh->audio_source, gh->caps_filter, gh->audio_sink,
			      NULL);

	_connect_message_to_pipeline(gh);

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

	if (gh->pipeline)
		g_object_unref(gh->pipeline);

	memset(gh, 0, sizeof(GstreamerHandle));
	g_free(gh);
	nugu_recorder_set_driver_data(rec, NULL);
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

	gh = _create(rec);
	if (gh == NULL) {
		nugu_error("error: create gst handle failed");
		_destroy(rec);
		return -1;
	}

	if (_set_property_to_param(gh, prop) != 0) {
		nugu_error("error: set param failed");
		_destroy(rec);
		return -1;
	}

	gh->rec = (void *)rec;

	rec_100ms = gh->samplerate * gh->samplebyte / 10;
	rec_5sec = gh->samplerate * gh->samplebyte * 5 / rec_100ms;

	nugu_dbg("rec - %d, %d", rec_100ms, rec_5sec);
	nugu_recorder_set_frame_size(rec, rec_100ms, rec_5sec);

	gst_element_set_state(gh->pipeline, GST_STATE_PLAYING);

	nugu_recorder_set_driver_data(rec, gh);

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

	gst_element_set_state(gh->pipeline, GST_STATE_NULL);

	_destroy(rec);

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
