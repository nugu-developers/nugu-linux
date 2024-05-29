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

#ifdef NUGU_PLUGIN_BUILTIN_FILEDUMP
#define NUGU_PLUGIN_BUILTIN
#endif

#ifdef _WIN32
#ifdef NUGU_ENV_DUMP_LINK_FILE_PCM
#undef NUGU_ENV_DUMP_LINK_FILE_PCM
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include <glib.h>

#include "base/nugu_log.h"
#include "base/nugu_plugin.h"
#include "base/nugu_decoder.h"
#include "base/nugu_pcm.h"

static NuguDecoderDriver *decoder_driver;
static NuguPcmDriver *pcm_driver;

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

static int _dumpfile_open(const char *path, const char *prefix)
{
	char ymd[32];
	char hms[32];
	time_t now;
	struct tm now_tm;
	char *buf = NULL;
	int fd;

	now = time(NULL);
#ifdef _WIN32
	localtime_s(&now_tm, &now);
#else
	localtime_r(&now, &now_tm);
#endif

	snprintf(ymd, sizeof(ymd), "%04d%02d%02d", now_tm.tm_year + 1900,
		 now_tm.tm_mon + 1, now_tm.tm_mday);
	snprintf(hms, sizeof(hms), "%02d%02d%02d", now_tm.tm_hour,
		 now_tm.tm_min, now_tm.tm_sec);

	if (path)
		buf = g_strdup_printf("%s/%s_%s_%s.dat", path, prefix, ymd,
				      hms);
	else
		buf = g_strdup_printf("/tmp/%s_%s_%s.dat", prefix, ymd, hms);

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

static int _decoder_create(NuguDecoderDriver *driver, NuguDecoder *dec)
{
	int fd;

	nugu_dbg("new filedump decoder created");

#ifdef NUGU_ENV_DUMP_PATH_DECODER
	fd = _dumpfile_open(getenv(NUGU_ENV_DUMP_PATH_DECODER), "decoder");
#else
	fd = _dumpfile_open(NULL, "decoder");
#endif

	if (fd < 0)
		return -1;

	nugu_decoder_set_driver_data(dec, GINT_TO_POINTER(fd));

	return 0;
}

static int _decoder_decode(NuguDecoderDriver *driver, NuguDecoder *dec,
			   const void *data, size_t data_len, NuguBuffer *buf)
{
	int fd;

	if (nugu_decoder_get_driver_data(dec) == NULL)
		return -1;

	fd = GPOINTER_TO_INT(nugu_decoder_get_driver_data(dec));
	if (write(fd, data, data_len) < 0)
		return -1;

	return 0;
}

static int _decoder_destroy(NuguDecoderDriver *driver, NuguDecoder *dec)
{
	int fd;

	fd = GPOINTER_TO_INT(nugu_decoder_get_driver_data(dec));
	close(fd);

	nugu_decoder_set_driver_data(dec, NULL);

	nugu_dbg("decoder filedump done");

	return 0;
}

static struct nugu_decoder_driver_ops decoder_ops = {
	.create = _decoder_create,
	.decode = _decoder_decode,
	.destroy = _decoder_destroy
};

static int _pcm_create(NuguPcmDriver *driver, NuguPcm *pcm,
		       NuguAudioProperty prop)
{
	return 0;
}

static void _pcm_destroy(NuguPcmDriver *driver, NuguPcm *pcm)
{
}

static int _pcm_start(NuguPcmDriver *driver, NuguPcm *pcm)
{
	int fd;

#ifdef NUGU_ENV_DUMP_PATH_PCM
	fd = _dumpfile_open(getenv(NUGU_ENV_DUMP_PATH_PCM), "pcm");
#else
	fd = _dumpfile_open(NULL, "pcm");
#endif

	if (fd < 0)
		return -1;

	nugu_pcm_set_driver_data(pcm, GINT_TO_POINTER(fd));

	nugu_pcm_emit_status(pcm, NUGU_MEDIA_STATUS_READY);
	nugu_pcm_emit_status(pcm, NUGU_MEDIA_STATUS_PLAYING);

	return 0;
}

static int _pcm_push_data(NuguPcmDriver *driver, NuguPcm *pcm, const char *data,
			  size_t size, int is_last)
{
	int fd;

	if (nugu_pcm_get_driver_data(pcm) == NULL)
		return -1;

	fd = GPOINTER_TO_INT(nugu_pcm_get_driver_data(pcm));
	if (data != NULL && size > 0) {
		if (write(fd, data, size) < 0)
			return -1;
	}

	if (is_last)
		nugu_pcm_emit_event(pcm, NUGU_MEDIA_EVENT_END_OF_STREAM);

	return 0;
}

static int _pcm_stop(NuguPcmDriver *driver, NuguPcm *pcm)
{
	int fd;

	if (nugu_pcm_get_driver_data(pcm) == NULL)
		return 0;

	fd = GPOINTER_TO_INT(nugu_pcm_get_driver_data(pcm));
	close(fd);

	nugu_pcm_set_driver_data(pcm, NULL);

	nugu_dbg("pcm filedump close: fd=%d", fd);

	nugu_pcm_emit_status(pcm, NUGU_MEDIA_STATUS_STOPPED);

	return 0;
}

static struct nugu_pcm_driver_ops pcm_ops = {
	.create = _pcm_create,
	.destroy = _pcm_destroy,
	.start = _pcm_start,
	.push_data = _pcm_push_data,
	.stop = _pcm_stop
};

static int init(NuguPlugin *p)
{
	const struct nugu_plugin_desc *desc;

	desc = nugu_plugin_get_description(p);
	nugu_dbg("plugin-init '%s'", desc->name);

	decoder_driver = nugu_decoder_driver_new(
		desc->name, NUGU_DECODER_TYPE_OPUS, &decoder_ops);
	if (!decoder_driver)
		return -1;

	if (nugu_decoder_driver_register(decoder_driver) < 0) {
		nugu_decoder_driver_free(decoder_driver);
		decoder_driver = NULL;
		return -1;
	}

	pcm_driver = nugu_pcm_driver_new(desc->name, &pcm_ops);
	if (!pcm_driver) {
		nugu_decoder_driver_remove(decoder_driver);
		nugu_decoder_driver_free(decoder_driver);
		decoder_driver = NULL;
		return -1;
	}

	if (nugu_pcm_driver_register(pcm_driver) != 0) {
		nugu_pcm_driver_free(pcm_driver);
		pcm_driver = NULL;

		nugu_decoder_driver_remove(decoder_driver);
		nugu_decoder_driver_free(decoder_driver);
		decoder_driver = NULL;
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

	if (decoder_driver) {
		nugu_decoder_driver_remove(decoder_driver);
		nugu_decoder_driver_free(decoder_driver);
		decoder_driver = NULL;
	}

	if (pcm_driver) {
		nugu_pcm_driver_remove(pcm_driver);
		nugu_pcm_driver_free(pcm_driver);
		pcm_driver = NULL;
	}

	nugu_dbg("'%s' plugin unloaded", nugu_plugin_get_description(p)->name);
}

NUGU_PLUGIN_DEFINE(filedump,
	NUGU_PLUGIN_PRIORITY_LOW,
	"0.0.1",
	load,
	unload,
	init);
