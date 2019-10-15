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

#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include <string.h>
#include "nugu_log.h"
#include "nugu_plugin.h"
#include "nugu_player.h"

#define PLUGIN_DRIVER_NAME "gstreamer"
#define GST_SET_VOLUME_MIN 0
#define GST_SET_VOLUME_MAX 1

#define NOTIFY_STATUS_CHANGED(s)                                               \
	{                                                                      \
		if (gh->player && gh->status != s) {                           \
			nugu_player_emit_status(gh->player, s);                \
			gh->status = s;                                        \
		}                                                              \
	}

#define NOTIFY_EVENT(e)                                                        \
	{                                                                      \
		if (gh->player)                                                \
			nugu_player_emit_event(gh->player, e);                 \
	}

typedef struct gst_handle GstreamerHandle;

struct gst_handle {
	GstElement *pipeline;
	GstElement *audio_src;
	GstElement *audio_convert;
	GstElement *audio_sink;
	GstElement *caps_filter;
	GstElement *volume;
	GstDiscoverer *discoverer;
	gboolean is_live;
	gboolean seekable;
	gboolean seek_done;
	int seek_reserve;
	int duration;
	int buffering;
	char *playurl;
	NuguPlayer *player;
	enum nugu_media_status status;
};

static void _discovered_cb(GstDiscoverer *discoverer, GstDiscovererInfo *info,
			   GError *err, GstreamerHandle *gh);
static void _cb_message(GstBus *bus, GstMessage *msg, GstreamerHandle *gh);
static void _connect_message_to_pipeline(GstreamerHandle *gh);
static void _pad_added_handler(GstElement *src, GstPad *new_src_pad,
			       GstreamerHandle *gh);
static void *_create(NuguPlayer *player);
static void _destroy(void *device, NuguPlayer *player);
static int _set_source(void *device, NuguPlayer *player, const char *url);
static int _start(void *device, NuguPlayer *player);
static int _stop(void *device, NuguPlayer *player);
static int _pause(void *device, NuguPlayer *player);
static int _resume(void *device, NuguPlayer *player);
static int _seek(void *device, NuguPlayer *player, int sec);
static int _set_volume(void *device, NuguPlayer *player, int vol);
static int _get_duration(void *device, NuguPlayer *player);
static int _get_position(void *device, NuguPlayer *player);

static NuguPlayerDriver *driver;
static int _uniq_id;
static const gdouble VOLUME_ZERO = 0.0000001;

void _discovered_cb(GstDiscoverer *discoverer, GstDiscovererInfo *info,
		    GError *err, GstreamerHandle *gh)
{
	const gchar *discoverer_uri = NULL;
	GstDiscovererResult discoverer_result;

	discoverer_uri = gst_discoverer_info_get_uri(info);
	discoverer_result = gst_discoverer_info_get_result(info);

	switch (discoverer_result) {
	case GST_DISCOVERER_URI_INVALID:
		nugu_dbg("Invalid URI '%s'", discoverer_uri);
		break;
	case GST_DISCOVERER_ERROR:
		nugu_dbg("Discoverer error: %s", err->message);
		break;
	case GST_DISCOVERER_TIMEOUT:
		nugu_dbg("Timeout");
		break;
	case GST_DISCOVERER_BUSY:
		nugu_dbg("Busy");
		break;
	case GST_DISCOVERER_MISSING_PLUGINS: {
		const GstStructure *s;
		gchar *str;

		s = gst_discoverer_info_get_misc(info);
		str = gst_structure_to_string(s);

		nugu_dbg("Missing plugins: %s", str);
		g_free(str);
		break;
	}
	case GST_DISCOVERER_OK:
		nugu_dbg("Discovered '%s'", discoverer_uri);
		break;

	default:
		break;
	}

	if (discoverer_result != GST_DISCOVERER_OK) {
		if (gh->status == MEDIA_STATUS_PLAYING) {
			nugu_dbg("Gstreamer is already played!!");
			return;
		}
		if (discoverer_result == GST_DISCOVERER_URI_INVALID) {
			NOTIFY_EVENT(MEDIA_EVENT_MEDIA_INVALID);
		} else {
			NOTIFY_EVENT(MEDIA_EVENT_MEDIA_LOAD_FAILED);
		}

		nugu_dbg("This URI cannot be played");
		return;
	}

	gh->duration =
		GST_TIME_AS_SECONDS(gst_discoverer_info_get_duration(info));
	gh->seekable = gst_discoverer_info_get_seekable(info);

	nugu_dbg("duration: %d, seekable: %d", gh->duration, gh->seekable);

	nugu_dbg("[gstreamer]Duration: %" GST_TIME_FORMAT "",
		 GST_TIME_ARGS(gst_discoverer_info_get_duration(info)));

	if (gh->status == MEDIA_STATUS_STOPPED)
		NOTIFY_STATUS_CHANGED(MEDIA_STATUS_READY);

	NOTIFY_EVENT(MEDIA_EVENT_MEDIA_LOADED);
}

void _cb_message(GstBus *bus, GstMessage *msg, GstreamerHandle *gh)
{
	switch (GST_MESSAGE_TYPE(msg)) {
	case GST_MESSAGE_ERROR: {
		GError *err;
		gchar *debug;

		gst_message_parse_error(msg, &err, &debug);
		nugu_dbg("GST_MESSAGE_ERROR: %s", err->message);

		if (gh->status != MEDIA_STATUS_STOPPED) {
			NOTIFY_EVENT(MEDIA_EVENT_MEDIA_LOAD_FAILED);
			NOTIFY_STATUS_CHANGED(MEDIA_STATUS_STOPPED);
		}

		g_error_free(err);
		g_free(debug);

		gst_element_set_state(gh->pipeline, GST_STATE_READY);
		break;
	}
	case GST_MESSAGE_STATE_CHANGED: {
		GstState old_state, new_state, pending_state;
		gboolean need_seek = FALSE;

		gst_message_parse_state_changed(msg, &old_state, &new_state,
						&pending_state);

		if (GST_MESSAGE_SRC(msg) != GST_OBJECT(gh->pipeline))
			break;

		nugu_dbg("State set to %s -> %s",
			 gst_element_state_get_name(old_state),
			 gst_element_state_get_name(new_state));

		if (new_state == GST_STATE_PLAYING) {
			if (gh->status == MEDIA_STATUS_READY ||
			    gh->status == MEDIA_STATUS_STOPPED ||
			    gh->status == MEDIA_STATUS_PAUSED) {
				NOTIFY_STATUS_CHANGED(MEDIA_STATUS_PLAYING);

				if (gh->seek_reserve)
					need_seek = TRUE;
			} else if (gh->status == MEDIA_STATUS_PAUSED &&
				   gh->seek_done) {
				gst_element_set_state(gh->pipeline,
						      GST_STATE_PAUSED);
				gh->seek_done = FALSE;
			}
		}

		if (need_seek) {
			if (gst_element_seek_simple(
				    gh->pipeline, GST_FORMAT_TIME,
				    (GstSeekFlags)(GST_SEEK_FLAG_FLUSH |
						   GST_SEEK_FLAG_KEY_UNIT),
				    gh->seek_reserve * GST_SECOND))
				nugu_dbg("seek is success. status: %d",
					 gh->status);
			else
				nugu_dbg("seek is failed. status: %d",
					 gh->status);

			gh->seek_reserve = 0;
		}
		break;
	}
	case GST_MESSAGE_EOS:
		nugu_dbg("GST_MESSAGE_EOS");
		gst_element_set_state(gh->pipeline, GST_STATE_READY);
		NOTIFY_EVENT(MEDIA_EVENT_END_OF_STREAM);
		NOTIFY_STATUS_CHANGED(MEDIA_STATUS_STOPPED);
		break;

	case GST_MESSAGE_BUFFERING: {
		gint percent = 0;

		if (gh->is_live)
			break;

		gst_message_parse_buffering(msg, &percent);

		if (gh->buffering < percent) {
			nugu_dbg("Buffering (%3d%%)", percent);
			gh->buffering = percent;
		}
		break;
	}

	case GST_MESSAGE_CLOCK_LOST:
		nugu_dbg("GST_MESSAGE_CLOCK_LOST");
		/* Get a new clock */
		gst_element_set_state(gh->pipeline, GST_STATE_PAUSED);
		gst_element_set_state(gh->pipeline, GST_STATE_PLAYING);
		break;

	default:
		break;
	}
}

void _connect_message_to_pipeline(GstreamerHandle *gh)
{
	GstBus *bus = gst_element_get_bus(gh->pipeline);

	gst_bus_add_signal_watch(bus);
	g_signal_connect(G_OBJECT(bus), "message", G_CALLBACK(_cb_message), gh);

	gst_object_unref(bus);
}

void _pad_added_handler(GstElement *src, GstPad *new_src_pad,
			GstreamerHandle *gh)
{
	GstPad *sink_pad =
		gst_element_get_static_pad(gh->audio_convert, "sink");

	nugu_dbg("Received new source pad '%s' <--- '%s':",
		 GST_PAD_NAME(new_src_pad), GST_ELEMENT_NAME(src));

	if (gst_pad_is_linked(sink_pad)) {
		nugu_error("We are already linked. Ignoring.");
	} else {
		GstCaps *new_pad_caps = NULL;
		const gchar *new_pad_type = NULL;
		GstPadLinkReturn ret;

		/* Check the new pad's type */
		new_pad_caps = gst_pad_get_current_caps(new_src_pad);
		new_pad_type = gst_structure_get_name(
			gst_caps_get_structure(new_pad_caps, 0));

		if (!g_str_has_prefix(new_pad_type, "audio/x-raw")) {
			nugu_error("It has type '%s' which is not raw audio.",
				   new_pad_type);

			if (new_pad_caps != NULL)
				gst_caps_unref(new_pad_caps);

			gst_object_unref(sink_pad);
			return;
		}

		/* Attempt the link */
		ret = gst_pad_link(new_src_pad, sink_pad);
		if (GST_PAD_LINK_FAILED(ret))
			nugu_error("Type is '%s' but link failed.",
				   new_pad_type);
		else
			nugu_dbg("Link succeeded (type '%s').", new_pad_type);
	}
	gst_object_unref(sink_pad);
}

void *_create(NuguPlayer *player)
{
	GstreamerHandle *gh;
	char caps_filter[128];
	char audio_source[128];
	char audio_convert[128];
	char audio_sink[128];
	char volume[128];
	char discoverer[128];
	char pipeline[128];
	GError *err;

	g_return_val_if_fail(player != NULL, NULL);

	g_snprintf(caps_filter, 128, "caps_filter#%d", _uniq_id);
	g_snprintf(audio_source, 128, "audio_source#%d", _uniq_id);
	g_snprintf(audio_convert, 128, "audio_convert#%d", _uniq_id);
	g_snprintf(audio_sink, 128, "audio_sink#%d", _uniq_id);
	g_snprintf(volume, 128, "volume#%d", _uniq_id);
	g_snprintf(discoverer, 128, "discoverer#%d", _uniq_id);
	g_snprintf(pipeline, 128, "pipeline#%d", _uniq_id);

	gh = (GstreamerHandle *)g_malloc0(sizeof(GstreamerHandle));
	gh->pipeline = gst_pipeline_new(pipeline);
	gh->audio_src = gst_element_factory_make("uridecodebin", audio_source);
	gh->audio_convert =
		gst_element_factory_make("audioconvert", audio_convert);
	gh->audio_sink = gst_element_factory_make("autoaudiosink", audio_sink);
	gh->volume = gst_element_factory_make("volume", volume);
	gh->discoverer =
		gst_discoverer_new(NUGU_SET_LOADING_TIMEOUT * GST_SECOND, &err);
	gh->is_live = FALSE;
	gh->seekable = FALSE;
	gh->seek_done = FALSE;
	gh->seek_reserve = 0;
	gh->buffering = -1;
	gh->duration = -1;
	gh->status = MEDIA_STATUS_STOPPED;
	gh->player = player;

	if (!gh->pipeline || !gh->audio_src || !gh->audio_convert ||
	    !gh->audio_sink || !gh->volume || !gh->discoverer) {
		nugu_error("create gst elements failed");
		memset(gh, 0, sizeof(GstreamerHandle));
		g_free(gh);
		return NULL;
	}
	gst_bin_add_many(GST_BIN(gh->pipeline), gh->audio_src,
			 gh->audio_convert, gh->volume, gh->audio_sink, NULL);
	gst_element_link_many(gh->audio_convert, gh->volume, gh->audio_sink,
			      NULL);
	_connect_message_to_pipeline(gh);

	/* Connect to the pad-added signal */
	g_signal_connect(gh->audio_src, "pad-added",
			 G_CALLBACK(_pad_added_handler), gh);

	/* Connect to the discoverer signal */
	g_signal_connect(gh->discoverer, "discovered",
			 G_CALLBACK(_discovered_cb), gh);

	_uniq_id++;
	return gh;
}

void _destroy(void *device, NuguPlayer *player)
{
	GstreamerHandle *gh;

	g_return_if_fail(device != NULL);

	gh = (GstreamerHandle *)device;

	if (player == gh->player)
		_stop(gh, gh->player);

	if (gh->playurl)
		g_free(gh->playurl);

	if (gh->discoverer)
		g_object_unref(gh->discoverer);

	if (gh->pipeline)
		g_object_unref(gh->pipeline);

	memset(gh, 0, sizeof(GstreamerHandle));
	g_free(gh);
}

int _set_source(void *device, NuguPlayer *player, const char *url)
{
	GstreamerHandle *gh;

	g_return_val_if_fail(device != NULL, -1);
	g_return_val_if_fail(player != NULL, -1);
	g_return_val_if_fail(url != NULL, -1);

	gh = (GstreamerHandle *)device;

	gh->is_live = FALSE;
	gh->seekable = FALSE;
	gh->seek_done = FALSE;
	gh->seek_reserve = 0;
	gh->duration = -1;
	gh->buffering = -1;

	if (gh->playurl)
		g_free(gh->playurl);

	gh->playurl = g_malloc0(strlen(url) + 1);
	memcpy(gh->playurl, url, strlen(url));
	nugu_dbg("playurl:%s", gh->playurl);

	nugu_player_emit_event(player, MEDIA_EVENT_MEDIA_SOURCE_CHANGED);
	_stop(device, player);

	return 0;
}

int _start(void *device, NuguPlayer *player)
{
	GstreamerHandle *gh;
	GstStateChangeReturn ret;

	g_return_val_if_fail(device != NULL, -1);
	g_return_val_if_fail(player != NULL, -1);

	gh = (GstreamerHandle *)device;

	/* Start the discoverer process (nothing to do yet) */
	gst_discoverer_stop(gh->discoverer);
	gst_discoverer_start(gh->discoverer);

	/* Add a request to process asynchronously the URI passed
	 * through the command line
	 */
	if (!gst_discoverer_discover_uri_async(gh->discoverer, gh->playurl))
		nugu_dbg("Failed to start discovering URI '%s'", gh->playurl);

	g_object_set(gh->audio_src, "uri", gh->playurl, NULL);

	ret = gst_element_set_state(gh->pipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		nugu_dbg("unable to set the pipeline to the playing state.");
		return -1;
	} else if (ret == GST_STATE_CHANGE_NO_PREROLL) {
		nugu_dbg("the media is live!!");
		gh->is_live = TRUE;
	}

	return 0;
}

int _stop(void *device, NuguPlayer *player)
{
	GstreamerHandle *gh;
	GstStateChangeReturn ret;

	g_return_val_if_fail(device != NULL, -1);
	g_return_val_if_fail(player != NULL, -1);

	gh = (GstreamerHandle *)device;
	gh->is_live = FALSE;
	gh->buffering = -1;

	if (gh->pipeline == NULL)
		nugu_dbg("pipeline is not valid!!");
	else {
		gst_discoverer_stop(gh->discoverer);

		ret = gst_element_set_state(gh->pipeline, GST_STATE_NULL);
		if (ret == GST_STATE_CHANGE_FAILURE) {
			nugu_error("failed to set state stop to pipeline.");
			return -1;
		}
	}

	NOTIFY_STATUS_CHANGED(MEDIA_STATUS_STOPPED);

	return 0;
}

int _pause(void *device, NuguPlayer *player)
{
	GstreamerHandle *gh;
	GstStateChangeReturn ret;

	g_return_val_if_fail(device != NULL, -1);
	g_return_val_if_fail(player != NULL, -1);

	gh = (GstreamerHandle *)device;

	if (gh->pipeline == NULL) {
		nugu_error("pipeline is not valid!!");
		return -1;
	}

	ret = gst_element_set_state(gh->pipeline, GST_STATE_PAUSED);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		nugu_error("unable to set the pipeline to the pause state.");
		return -1;
	}

	NOTIFY_STATUS_CHANGED(MEDIA_STATUS_PAUSED);

	return 0;
}

int _resume(void *device, NuguPlayer *player)
{
	GstreamerHandle *gh;
	GstStateChangeReturn ret;

	g_return_val_if_fail(device != NULL, -1);
	g_return_val_if_fail(player != NULL, -1);

	gh = (GstreamerHandle *)device;

	if (gh->pipeline == NULL) {
		nugu_error("pipeline is not valid!!");
		return -1;
	}

	ret = gst_element_set_state(gh->pipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		nugu_error("unable to set the pipeline to the resume state.");
		return -1;
	}

	NOTIFY_STATUS_CHANGED(MEDIA_STATUS_PLAYING);

	return 0;
}

int _seek(void *device, NuguPlayer *player, int sec)
{
	GstreamerHandle *gh;

	g_return_val_if_fail(device != NULL, -1);
	g_return_val_if_fail(player != NULL, -1);

	gh = (GstreamerHandle *)device;
	gh->buffering = -1;

	if (gh->pipeline == NULL) {
		nugu_error("pipeline is not valid!!");
		return -1;
	}

	if (gh->status == MEDIA_STATUS_READY ||
	    gh->status == MEDIA_STATUS_STOPPED) {
		/* wait to play & seek */
		nugu_dbg("seek is reserved (%d sec)", sec);
		gh->seek_reserve = sec;
	} else {
		if (!gh->seekable) {
			nugu_dbg("this media is not supported seek function");
			return -1;
		}

		if (!gst_element_seek_simple(
			    gh->pipeline, GST_FORMAT_TIME,
			    (GstSeekFlags)(GST_SEEK_FLAG_FLUSH |
					   GST_SEEK_FLAG_KEY_UNIT),
			    sec * GST_SECOND)) {
			nugu_dbg("seek is failed. status: %d", gh->status);
			return -1;
		}

		nugu_dbg("seek is success. status: %d", gh->status);
		/* seek & pause */
		gh->seek_done = TRUE;
	}

	return 0;
}

int _set_volume(void *device, NuguPlayer *player, int vol)
{
	GstreamerHandle *gh;
	gdouble volume;

	g_return_val_if_fail(device != NULL, -1);
	g_return_val_if_fail(player != NULL, -1);
	g_return_val_if_fail(vol >= 0, -1);

	gh = (GstreamerHandle *)device;

	volume = (gdouble)vol * (GST_SET_VOLUME_MAX - GST_SET_VOLUME_MIN) /
		 (NUGU_SET_VOLUME_MAX - NUGU_SET_VOLUME_MIN);

	nugu_dbg("volume: %f", volume);

	if (volume == 0)
		g_object_set(gh->volume, "volume", VOLUME_ZERO, NULL);
	else
		g_object_set(gh->volume, "volume", volume, NULL);

	return 0;
}

int _get_duration(void *device, NuguPlayer *player)
{
	GstreamerHandle *gh;

	g_return_val_if_fail(device != NULL, -1);
	g_return_val_if_fail(player != NULL, -1);

	gh = (GstreamerHandle *)device;
	return gh->duration;
}

int _get_position(void *device, NuguPlayer *player)
{
	GstreamerHandle *gh;
	gint64 current;

	g_return_val_if_fail(device != NULL, -1);
	g_return_val_if_fail(player != NULL, -1);

	gh = (GstreamerHandle *)device;

	if (gh->pipeline == NULL) {
		nugu_error("pipeline is not valid!!");
		return -1;
	}

	/* Query the current position of the stream */
	if (!gst_element_query_position(gh->pipeline, GST_FORMAT_TIME,
					&current)) {
		nugu_error("Could not query current position!!");
		return -1;
	}
	return GST_TIME_AS_SECONDS(current);
}

static struct nugu_player_driver_ops player_ops = { .create = _create,
						    .destroy = _destroy,
						    .set_source = _set_source,
						    .start = _start,
						    .stop = _stop,
						    .pause = _pause,
						    .resume = _resume,
						    .seek = _seek,
						    .set_volume = _set_volume,
						    .get_duration =
							    _get_duration,
						    .get_position =
							    _get_position };

static int init(NuguPlugin *p)
{
	nugu_dbg("'%s' plugin initialized",
		 nugu_plugin_get_description(p)->name);

	gst_init(NULL, NULL);

	driver = nugu_player_driver_new(PLUGIN_DRIVER_NAME, &player_ops);
	g_return_val_if_fail(driver != NULL, -1);
	if (nugu_player_driver_register(driver) != 0) {
		nugu_player_driver_free(driver);
		driver = NULL;
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

	if (driver) {
		nugu_player_driver_remove(driver);
		nugu_player_driver_free(driver);
		driver = NULL;
	}

	nugu_dbg("'%s' plugin unloaded done",
		 nugu_plugin_get_description(p)->name);
}

NUGU_PLUGIN_DEFINE(PLUGIN_DRIVER_NAME,
	NUGU_PLUGIN_PRIORITY_DEFAULT,
	"0.0.1",
	load,
	unload,
	init);
