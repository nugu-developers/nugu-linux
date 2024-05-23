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

#ifdef NUGU_PLUGIN_BUILTIN_GSTREAMER_PCM
#define NUGU_PLUGIN_BUILTIN
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#include <glib.h>
#include <gst/gst.h>
#include <gst/audio/audio-format.h>
#include <gst/app/gstappsrc.h>
#include <gst/pbutils/pbutils.h>

#include "base/nugu_log.h"
#include "base/nugu_plugin.h"
#include "base/nugu_pcm.h"
#include "base/nugu_prof.h"

#define GST_SET_VOLUME_MIN 0
#define GST_SET_VOLUME_MAX 1

//#define DEBUG_PCM

#define NOTIFY_STATUS_CHANGED(s)                                               \
	{                                                                      \
		if (pcm_param->pcm && pcm_param->status != s) {                \
			nugu_pcm_emit_status(pcm_param->pcm, s);               \
			pcm_param->status = s;                                 \
		}                                                              \
	}

/*
 * Pipeline:
 *   appsrc ! rawaudioparse format=pcm pcm-format=s16le sample-rate=22050
 *              num-channels=1 ! audioconvert ! volume ! autoaudiosink
 */
struct pa_audio_param {
	NuguPcm *pcm;

	GstElement *pipeline;
	GstElement *raw_parser;
	GstElement *convert;
	GstElement *volume;
	GstElement *audio_sink;
	GstAppSrc *app_src;

	int samplerate;
	int channel;
	GstAudioFormat format;

	size_t written;

	int stop;
	int pause;
	int done;

	int is_start;
	int is_first;
	int is_last;

	int uniq_id;
	gulong sig_msg_hid;

	int cur_volume;
	int new_volume;

#if defined(NUGU_ENV_DUMP_PATH_PCM)
	int dump_fd;
#endif
	enum nugu_media_status status;
};

static NuguPcmDriver *pcm_driver;
static int _uniq_id;
static const int INITIAL_VOLUME = -1;

#ifdef ENABLE_GSTREAMER_PLUGIN_VOLUME
static const gdouble VOLUME_ZERO = 0.0000001;
#endif

static int _pcm_stop(NuguPcmDriver *driver, NuguPcm *pcm);

#ifdef NUGU_ENV_DUMP_LINK_FILE_PCM
static void _dumpfile_link(const char *filename)
{
	char *link_file;
	int ret;

	if (!filename)
		return;

	link_file = getenv(NUGU_ENV_DUMP_LINK_FILE_PCM);
	if (!link_file)
		link_file = "tts_audio.pcm";

	unlink(link_file);
	ret = link(filename, link_file);
	if (ret < 0) {
		nugu_error("link(%s) failed: %s", link_file, strerror(errno));
		return;
	}

	nugu_dbg("link file: %s -> %s", link_file, filename);
}
#endif

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

#ifdef NUGU_ENV_DUMP_LINK_FILE_PCM
	_dumpfile_link(buf);
#endif
	g_free(buf);

	return fd;
}
#endif

#ifdef ENABLE_PULSEAUDIO
static void _set_audio_attribute(struct pa_audio_param *pcm_param, NuguPcm *pcm)
{
	int attr = nugu_pcm_get_audio_attribute(pcm);
	const char *media_role;
	GstStructure *s;

	if (attr == -1)
		attr = NUGU_AUDIO_ATTRIBUTE_MUSIC;

	media_role = nugu_audio_get_attribute_str(attr);

	s = gst_structure_new("properties", "media.role", G_TYPE_STRING,
			      media_role, NULL);

	g_object_set(G_OBJECT(pcm_param->audio_sink), "stream-properties", s,
		     NULL);
	gst_structure_free(s);
}
#endif

static void _cb_message(GstBus *bus, GstMessage *msg,
			struct pa_audio_param *pcm_param)
{
	switch (GST_MESSAGE_TYPE(msg)) {
	case GST_MESSAGE_ERROR: {
		GError *err;
		gchar *debug;

		gst_message_parse_error(msg, &err, &debug);
		nugu_error("Error: %s", err->message);

		g_error_free(err);
		g_free(debug);

		/* Set the pipeline to READY (which stops playback) */
		gst_element_set_state(pcm_param->pipeline, GST_STATE_READY);
		break;
	}
	case GST_MESSAGE_CLOCK_LOST:
		nugu_dbg("GST_MESSAGE_CLOCK_LOST");
		/* Get a new clock */
		gst_element_set_state(pcm_param->pipeline, GST_STATE_PAUSED);
		gst_element_set_state(pcm_param->pipeline, GST_STATE_PLAYING);
		break;

	case GST_MESSAGE_EOS:
		nugu_dbg("GST_MESSAGE_EOS");
		gst_element_set_state(pcm_param->pipeline, GST_STATE_READY);
		pcm_param->done = 1;
		nugu_pcm_emit_event(pcm_param->pcm,
				    NUGU_MEDIA_EVENT_END_OF_STREAM);
		NOTIFY_STATUS_CHANGED(NUGU_MEDIA_STATUS_STOPPED);
		break;
	default:
		/* Unhandled message */
		break;
	}
}

static void _connect_message_to_pipeline(struct pa_audio_param *pcm_param)
{
	GstBus *bus = gst_element_get_bus(pcm_param->pipeline);

	gst_bus_add_signal_watch(bus);
	g_signal_connect(G_OBJECT(bus), "message", G_CALLBACK(_cb_message),
			 pcm_param);

	gst_object_unref(bus);
}

static void _disconnect_message_to_pipeline(struct pa_audio_param *pcm_param)
{
	GstBus *bus = gst_element_get_bus(pcm_param->pipeline);

	if (pcm_param->sig_msg_hid == 0)
		return;

	g_signal_handler_disconnect(G_OBJECT(bus), pcm_param->sig_msg_hid);
	pcm_param->sig_msg_hid = 0;

	gst_object_unref(bus);
}

static int _set_property_to_param(struct pa_audio_param *pcm_param,
				  NuguAudioProperty prop)
{
	g_return_val_if_fail(pcm_param != NULL, -1);

	switch (prop.samplerate) {
	case NUGU_AUDIO_SAMPLE_RATE_8K:
		pcm_param->samplerate = 8000;
		break;
	case NUGU_AUDIO_SAMPLE_RATE_16K:
		pcm_param->samplerate = 16000;
		break;
	case NUGU_AUDIO_SAMPLE_RATE_32K:
		pcm_param->samplerate = 32000;
		break;
	case NUGU_AUDIO_SAMPLE_RATE_22K:
		pcm_param->samplerate = 22050;
		break;
	case NUGU_AUDIO_SAMPLE_RATE_44K:
		pcm_param->samplerate = 44100;
		break;
	default:
		nugu_error("not support the audio samplerate(%d)",
			   prop.samplerate);
		return -1;
	}

	switch (prop.format) {
	case NUGU_AUDIO_FORMAT_S8:
		pcm_param->format = GST_AUDIO_FORMAT_S8;
		break;
	case NUGU_AUDIO_FORMAT_U8:
		pcm_param->format = GST_AUDIO_FORMAT_U8;
		break;
	case NUGU_AUDIO_FORMAT_S16_LE:
		pcm_param->format = GST_AUDIO_FORMAT_S16LE;
		break;
	case NUGU_AUDIO_FORMAT_S16_BE:
		pcm_param->format = GST_AUDIO_FORMAT_S16BE;
		break;
	case NUGU_AUDIO_FORMAT_U16_LE:
		pcm_param->format = GST_AUDIO_FORMAT_U16LE;
		break;
	case NUGU_AUDIO_FORMAT_U16_BE:
		pcm_param->format = GST_AUDIO_FORMAT_S16BE;
		break;
	case NUGU_AUDIO_FORMAT_S24_LE:
		pcm_param->format = GST_AUDIO_FORMAT_S24LE;
		break;
	case NUGU_AUDIO_FORMAT_S24_BE:
		pcm_param->format = GST_AUDIO_FORMAT_S24BE;
		break;
	case NUGU_AUDIO_FORMAT_U24_LE:
		pcm_param->format = GST_AUDIO_FORMAT_U24LE;
		break;
	case NUGU_AUDIO_FORMAT_U24_BE:
		pcm_param->format = GST_AUDIO_FORMAT_U24BE;
		break;
	case NUGU_AUDIO_FORMAT_S32_LE:
		pcm_param->format = GST_AUDIO_FORMAT_S32LE;
		break;
	case NUGU_AUDIO_FORMAT_S32_BE:
		pcm_param->format = GST_AUDIO_FORMAT_S32BE;
		break;
	case NUGU_AUDIO_FORMAT_U32_LE:
		pcm_param->format = GST_AUDIO_FORMAT_U32LE;
		break;
	case NUGU_AUDIO_FORMAT_U32_BE:
		pcm_param->format = GST_AUDIO_FORMAT_U32BE;
		break;
	default:
		nugu_error("not support the audio format(%d)", prop.format);
		return -1;
	}

	switch (prop.channel) {
	case 1:
		pcm_param->channel = 1;
		break;
	case 2:
		pcm_param->channel = 2;
		break;
	case 4:
		pcm_param->channel = 4;
		break;
	case 8:
		pcm_param->channel = 8;
		break;
	default:
		nugu_error("not support audio channel(%d)", prop.channel);
		return -1;
	}

	nugu_dbg("format: %d, rate: %d, channels: %d", pcm_param->format,
		 pcm_param->samplerate, pcm_param->channel);

	return 0;
}

static void _pcm_drain_handler(GstElement *appsrc, guint unused_size,
			       struct pa_audio_param *param)
{
	if (!param->is_last) {
		nugu_dbg("Player still wait injecting audio raw data");
		return;
	}

	if (param->is_start)
		gst_app_src_end_of_stream(param->app_src);
	else
		nugu_dbg("Player status isn't playing");
}

static int _create_gst_elements(struct pa_audio_param *pcm_param)
{
	char app_source[128];
	char audio_sink[128];
	char volume[128];
	char pipeline[128];
	char raw_parser[128];
	char convert[128];

	if (!pcm_param) {
		nugu_error("pcm_param is null");
		return -1;
	}

	if (pcm_param->pipeline) {
		nugu_dbg("already create gst elements");
		return 0;
	}

#ifdef DEBUG_PCM
	nugu_info("_create_gst_elements: %d", pcm_param->uniq_id);
#endif

	g_snprintf(app_source, 128, "pcm_app_source#%d", pcm_param->uniq_id);
	g_snprintf(raw_parser, 128, "pcm_rawaudioparse%d", pcm_param->uniq_id);
	g_snprintf(convert, 128, "pcm_convert%d", pcm_param->uniq_id);
	g_snprintf(volume, 128, "pcm_volume#%d", pcm_param->uniq_id);
	g_snprintf(audio_sink, 128, "pcm_audio_sink#%d", pcm_param->uniq_id);
	g_snprintf(pipeline, 128, "pcm_pipeline#%d", pcm_param->uniq_id);

	pcm_param->pipeline = gst_pipeline_new(pipeline);
	if (!pcm_param->pipeline) {
		nugu_error("create pipeline(%s) failed", pipeline);
		goto error_out;
	}

	pcm_param->app_src =
		(GstAppSrc *)gst_element_factory_make("appsrc", app_source);
	if (!pcm_param->app_src) {
		nugu_error("create gst_element for 'appsrc' failed");
		goto error_out;
	}

	pcm_param->raw_parser =
		gst_element_factory_make("rawaudioparse", raw_parser);
	if (!pcm_param->raw_parser) {
		nugu_error("create gst_element for 'rawaudioparse' failed");
		goto error_out;
	}

	pcm_param->convert = gst_element_factory_make("audioconvert", convert);
	if (!pcm_param->convert) {
		nugu_error("create gst_element for 'audioconvert' failed");
		goto error_out;
	}

#ifdef ENABLE_GSTREAMER_PLUGIN_VOLUME
	pcm_param->volume = gst_element_factory_make("volume", volume);
	if (!pcm_param->volume) {
		nugu_error("create gst_element for 'volume' failed");
		goto error_out;
	}
#endif

#ifdef _WIN32
	pcm_param->audio_sink =
		gst_element_factory_make("directsoundsink", audio_sink);
	if (!pcm_param->audio_sink) {
		nugu_error("create gst_element for 'directsoundsink' failed");
		goto error_out;
	}
#else
#ifdef ENABLE_PULSEAUDIO
	pcm_param->audio_sink =
		gst_element_factory_make("pulsesink", audio_sink);
	if (!pcm_param->audio_sink) {
		nugu_error("create gst_element for 'pulsesink' failed");
		goto error_out;
	}
#else
	pcm_param->audio_sink =
		gst_element_factory_make("autoaudiosink", audio_sink);
	if (!pcm_param->audio_sink) {
		nugu_error("create gst_element for 'autoaudiosink' failed");
		goto error_out;
	}
#endif
#endif

#ifdef ENABLE_GSTREAMER_PLUGIN_VOLUME
	gst_bin_add_many(GST_BIN(pcm_param->pipeline),
			 (GstElement *)pcm_param->app_src,
			 pcm_param->raw_parser, pcm_param->convert,
			 pcm_param->volume, pcm_param->audio_sink, NULL);
	gst_element_link_many((GstElement *)pcm_param->app_src,
			      pcm_param->raw_parser, pcm_param->convert,
			      pcm_param->volume, pcm_param->audio_sink, NULL);
#else
	gst_bin_add_many(GST_BIN(pcm_param->pipeline),
			 (GstElement *)pcm_param->app_src,
			 pcm_param->raw_parser, pcm_param->convert,
			 pcm_param->audio_sink, NULL);
	gst_element_link_many((GstElement *)pcm_param->app_src,
			      pcm_param->raw_parser, pcm_param->convert,
			      pcm_param->audio_sink, NULL);
#endif

	g_signal_connect(pcm_param->app_src, "need-data",
			 G_CALLBACK(_pcm_drain_handler), pcm_param);

	g_object_set(G_OBJECT(pcm_param->raw_parser), "format", 0 /* PCM */,
		     NULL);
	g_object_set(G_OBJECT(pcm_param->raw_parser), "num-channels",
		     pcm_param->channel, NULL);
	g_object_set(G_OBJECT(pcm_param->raw_parser), "pcm-format",
		     pcm_param->format, NULL);
	g_object_set(G_OBJECT(pcm_param->raw_parser), "sample-rate",
		     pcm_param->samplerate, NULL);

	_connect_message_to_pipeline(pcm_param);

	return 0;

error_out:
	if (!pcm_param)
		return -1;

	if (pcm_param->app_src)
		g_object_unref(pcm_param->app_src);
	if (pcm_param->raw_parser)
		g_object_unref(pcm_param->raw_parser);
	if (pcm_param->convert)
		g_object_unref(pcm_param->convert);
	if (pcm_param->volume)
		g_object_unref(pcm_param->volume);
	if (pcm_param->audio_sink)
		g_object_unref(pcm_param->audio_sink);
	if (pcm_param->pipeline)
		g_object_unref(pcm_param->pipeline);

	memset(pcm_param, 0, sizeof(struct pa_audio_param));
	g_free(pcm_param);

	return -1;
}

static void _destroy_gst_elements(struct pa_audio_param *pcm_param)
{
	if (pcm_param) {
#ifdef DEBUG_PCM
		nugu_info("_destroy_gst_elements: %d", pcm_param->uniq_id);
#endif
		_disconnect_message_to_pipeline(pcm_param);

		if (pcm_param->pipeline) {
			gst_object_unref(pcm_param->pipeline);
			pcm_param->pipeline = NULL;
			pcm_param->cur_volume = INITIAL_VOLUME;
		}
	}
}

static void _change_volume(struct pa_audio_param *pcm_param)
{
#ifdef ENABLE_GSTREAMER_PLUGIN_VOLUME
	gdouble volume;

	if (!pcm_param) {
		nugu_error("pcm_param is null");
		return;
	}

	if (pcm_param->cur_volume == pcm_param->new_volume) {
		nugu_dbg("new_volume is same cur_volume");
		return;
	}

	pcm_param->cur_volume = pcm_param->new_volume;

	volume = (gdouble)pcm_param->cur_volume *
		 (GST_SET_VOLUME_MAX - GST_SET_VOLUME_MIN) /
		 (NUGU_SET_VOLUME_MAX - NUGU_SET_VOLUME_MIN);

	nugu_dbg("[id: %d] change volume: %f", pcm_param->uniq_id, volume);

	if (volume == 0)
		g_object_set(pcm_param->volume, "volume", VOLUME_ZERO, NULL);
	else
		g_object_set(pcm_param->volume, "volume", volume, NULL);
#endif
}

static int _pcm_create(NuguPcmDriver *driver, NuguPcm *pcm,
		       NuguAudioProperty prop)
{
	struct pa_audio_param *pcm_param;

	pcm_param = g_malloc0(sizeof(struct pa_audio_param));
	if (!pcm_param) {
		nugu_error("memory allocation is failed");
		return -1;
	}
	pcm_param->uniq_id = _uniq_id++;
	pcm_param->cur_volume = INITIAL_VOLUME;
	pcm_param->new_volume = NUGU_SET_VOLUME_DEFAULT;

#ifdef DEBUG_PCM
	nugu_info("#### pcm(%p) param is created(%p) ####", pcm, pcm_param);
#endif

	if (_set_property_to_param(pcm_param, prop) != 0) {
		nugu_error("set property to param failed");
		g_free(pcm_param);
		return -1;
	}

	pcm_param->pcm = pcm;
	pcm_param->status = NUGU_MEDIA_STATUS_STOPPED;

	nugu_pcm_set_driver_data(pcm, pcm_param);
	return 0;
}

static void _pcm_destroy(NuguPcmDriver *driver, NuguPcm *pcm)
{
	struct pa_audio_param *pcm_param = nugu_pcm_get_driver_data(pcm);

	_pcm_stop(driver, pcm);

	if (pcm_param)
		g_free(pcm_param);

	nugu_pcm_set_driver_data(pcm, NULL);

#ifdef DEBUG_PCM
	nugu_info("#### pcm(%p) param is destroyed(%p) ####", pcm, pcm_param);
#endif
}

static int _pcm_start(NuguPcmDriver *driver, NuguPcm *pcm)
{
	struct pa_audio_param *pcm_param = nugu_pcm_get_driver_data(pcm);
	GstStateChangeReturn ret;

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

	pcm_param->is_start = 1;
	pcm_param->is_first = 1;
	pcm_param->is_last = 0;
	pcm_param->pause = 0;
	pcm_param->stop = 0;
	pcm_param->written = 0;
	pcm_param->done = 0;

	if (_create_gst_elements(pcm_param) < 0) {
		nugu_error("failed to create gst elements");
		return -1;
	}

#ifdef ENABLE_PULSEAUDIO
	_set_audio_attribute(pcm_param, pcm);
#endif

	_change_volume(pcm_param);

	ret = gst_element_set_state(pcm_param->pipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		nugu_error("unable to set the pipeline to the playing state.");
		return -1;
	}

	NOTIFY_STATUS_CHANGED(NUGU_MEDIA_STATUS_READY);

	nugu_dbg("start done");
	return 0;
}

static int _pcm_stop(NuguPcmDriver *driver, NuguPcm *pcm)
{
	struct pa_audio_param *pcm_param = nugu_pcm_get_driver_data(pcm);
	GstStateChangeReturn ret;

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

#ifdef NUGU_ENV_DUMP_PATH_PCM
	if (pcm_param->dump_fd >= 0) {
		close(pcm_param->dump_fd);
		pcm_param->dump_fd = -1;
	}
#endif
	pcm_param->is_start = 0;

	if (pcm_param->pipeline) {
		ret = gst_element_set_state(pcm_param->pipeline,
					    GST_STATE_NULL);
		if (ret == GST_STATE_CHANGE_FAILURE) {
			nugu_error("failed to set state stop to pipeline.");
			return -1;
		}
		_destroy_gst_elements(pcm_param);
	}
	NOTIFY_STATUS_CHANGED(NUGU_MEDIA_STATUS_STOPPED);

	nugu_dbg("stop done");

	return 0;
}

static int _pcm_pause(NuguPcmDriver *driver, NuguPcm *pcm)
{
	struct pa_audio_param *pcm_param = nugu_pcm_get_driver_data(pcm);
	GstStateChangeReturn ret;

	g_return_val_if_fail(pcm != NULL, -1);

	if (pcm_param == NULL) {
		nugu_error("internal error");
		return -1;
	}

	if (pcm_param->pause) {
		nugu_info("pcm is already paused");
		return 0;
	}

	if (pcm_param->is_start) {
		ret = gst_element_set_state(pcm_param->pipeline,
					    GST_STATE_PAUSED);
		if (ret == GST_STATE_CHANGE_FAILURE) {
			nugu_error("unable to set the pipeline pause state.");
			return -1;
		}
	}

	pcm_param->pause = 1;
	NOTIFY_STATUS_CHANGED(NUGU_MEDIA_STATUS_PAUSED);

	nugu_dbg("pause done");

	return 0;
}

static int _pcm_resume(NuguPcmDriver *driver, NuguPcm *pcm)
{
	struct pa_audio_param *pcm_param = nugu_pcm_get_driver_data(pcm);
	GstStateChangeReturn ret;

	g_return_val_if_fail(pcm != NULL, -1);

	if (pcm_param == NULL) {
		nugu_error("internal error");
		return -1;
	}

	if (!pcm_param->pause) {
		nugu_dbg("pcm is not paused");
		return 0;
	}

	if (_create_gst_elements(pcm_param) < 0) {
		nugu_error("failed to create gst elements");
		return -1;
	}

	_change_volume(pcm_param);

	ret = gst_element_set_state(pcm_param->pipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		nugu_error("unable to set the pipeline to the resume state.");
		return -1;
	}

	pcm_param->pause = 0;
	if (!pcm_param->is_start) {
		pcm_param->is_start = 1;
		pcm_param->is_first = 1;
	}
	NOTIFY_STATUS_CHANGED(NUGU_MEDIA_STATUS_PLAYING);

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

	pcm_param->new_volume = volume;
	if (!pcm_param->is_start) {
		nugu_dbg("new_volume: %d", volume);
		return 0;
	}

	_change_volume(pcm_param);
	return 0;
}

static int _pcm_get_position(NuguPcmDriver *driver, NuguPcm *pcm)
{
	struct pa_audio_param *pcm_param = nugu_pcm_get_driver_data(pcm);
	gint64 current;

	g_return_val_if_fail(pcm != NULL, -1);

	if (pcm_param == NULL) {
		nugu_error("internal error");
		return -1;
	}

	if (pcm_param->is_start == 0) {
		nugu_error("pcm is not started");
		return -1;
	}

	if (pcm_param->status != NUGU_MEDIA_STATUS_PLAYING) {
		nugu_error("pcm player doesn't be played yet!!");
		return -1;
	}

	/* Query the current position of the stream */
	if (!gst_element_query_position(pcm_param->pipeline, GST_FORMAT_TIME,
					&current)) {
		nugu_error("Could not query current position!!");
		return -1;
	}

	return GST_TIME_AS_SECONDS(current);
}
static int _pcm_push_data(NuguPcmDriver *driver, NuguPcm *pcm, const char *data,
			  size_t size, int is_last)
{
	struct pa_audio_param *pcm_param = nugu_pcm_get_driver_data(pcm);
	GstBuffer *buf = NULL;
	GstFlowReturn ret;
	void *temp = NULL;

	if (pcm_param == NULL) {
		nugu_error("pcm is not started");
		return -1;
	}

	if (pcm_param->is_first) {
		nugu_prof_mark(NUGU_PROF_TYPE_TTS_FIRST_PCM_WRITE);
		pcm_param->is_first = 0;
		NOTIFY_STATUS_CHANGED(NUGU_MEDIA_STATUS_PLAYING);
	}

	if (!size && is_last) {
		pcm_param->is_last = is_last;

		if (pcm_param->written == 0) {
			nugu_dbg("empty pcm streaming");
			gst_app_src_end_of_stream(pcm_param->app_src);
		} else {
			nugu_dbg("just set is_last flag true");
		}
		return 0;
	}

	temp = g_malloc(size);
	if (temp == NULL) {
		nugu_error("heap memory allocation failed");
		return -1;
	}
	memcpy(temp, data, size);

#ifdef NUGU_ENV_DUMP_PATH_PCM
	if (pcm_param->dump_fd != -1) {
		if (write(pcm_param->dump_fd, temp, size) < 0)
			nugu_error("write to fd-%d failed", pcm_param->dump_fd);
	}
#endif

	buf = gst_buffer_new_wrapped(temp, size);
	if (buf == NULL) {
		nugu_error("gst buffer allocation failed");
		g_free(temp);
		return -1;
	}

	ret = gst_app_src_push_buffer(pcm_param->app_src, buf);
	if (ret != GST_FLOW_OK) {
		nugu_error("gst_app_src_push_buffer return %d", ret);
		gst_buffer_unref(buf);
		g_free(temp);
		return -1;
	}

#ifdef DEBUG_PCM
	nugu_dbg("write data => %d/%d(%d)", size, pcm_param->written, is_last);
#endif
	pcm_param->written += size;

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
	const struct nugu_plugin_desc *desc;

	desc = nugu_plugin_get_description(p);
	nugu_dbg("plugin-init '%s'", desc->name);

	if (gst_is_initialized() == FALSE)
		gst_init(NULL, NULL);

	pcm_driver = nugu_pcm_driver_new(desc->name, &pcm_ops);
	if (!pcm_driver) {
		nugu_error("nugu_pcm_driver_new() failed");
		return -1;
	}

	if (nugu_pcm_driver_register(pcm_driver) != 0) {
		nugu_pcm_driver_free(pcm_driver);
		pcm_driver = NULL;
		return -1;
	}

	nugu_dbg("'%s' plugin initialized", desc->name);

	return 0;
}

static int load(void)
{
	nugu_dbg("plugin-load");

	return 0;
}

static void unload(NuguPlugin *p)
{
	nugu_dbg("plugin-unload '%s'", nugu_plugin_get_description(p)->name);

	if (pcm_driver) {
		nugu_pcm_driver_remove(pcm_driver);
		nugu_pcm_driver_free(pcm_driver);
		pcm_driver = NULL;
	}

	nugu_dbg("'%s' plugin unloaded", nugu_plugin_get_description(p)->name);
}

NUGU_PLUGIN_DEFINE(
	/* NUGU SDK Plug-in description */
	gstreamer_pcm, /* Plugin name */
	NUGU_PLUGIN_PRIORITY_DEFAULT - 1, /* Plugin priority */
	"0.0.1", /* Plugin version */
	load, /* dlopen */
	unload, /* dlclose */
	init /* initialize */
);
