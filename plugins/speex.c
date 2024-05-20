/*
 * Copyright (c) 2021 SK Telecom Co., Ltd. All rights reserved.
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

#ifdef NUGU_PLUGIN_BUILTIN_SPEEX
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

#ifdef ENABLE_VENDOR_LIBRARY
#include <speex.h>
#endif

#include "base/nugu_log.h"
#include "base/nugu_plugin.h"
#include "base/nugu_encoder.h"
#include "base/nugu_pcm.h"

struct encoder_data {
	void *buffer;
	int buffer_size;
#ifdef NUGU_ENV_DUMP_PATH_ENCODER
	int dump_fd;
#endif
};

static NuguEncoderDriver *driver;

#ifdef NUGU_ENV_DUMP_PATH_ENCODER
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

	g_free(buf);

	return fd;
}
#endif

static int _encoder_create(NuguEncoderDriver *driver, NuguEncoder *enc,
			   NuguAudioProperty property)
{
	int sample_rate;
	struct encoder_data *ed;

	g_return_val_if_fail(driver != NULL, -1);
	g_return_val_if_fail(enc != NULL, -1);

	switch (property.samplerate) {
	case NUGU_AUDIO_SAMPLE_RATE_8K:
		sample_rate = 8000;
		break;
	case NUGU_AUDIO_SAMPLE_RATE_16K:
		sample_rate = 16000;
		break;
	case NUGU_AUDIO_SAMPLE_RATE_32K:
		sample_rate = 32000;
		break;
	case NUGU_AUDIO_SAMPLE_RATE_22K:
		sample_rate = 22050;
		break;
	case NUGU_AUDIO_SAMPLE_RATE_44K:
		sample_rate = 44100;
		break;
	default:
		nugu_error("not supported sample-rate type(%d)",
			   property.samplerate);
		return -1;
	}

	if (epd_speex_start(sample_rate) < 0) {
		nugu_error("epd_speex_start() failed");
		return -1;
	}

	ed = malloc(sizeof(struct encoder_data));
	if (!ed) {
		nugu_error_nomem();
		epd_speex_release();
		return -1;
	}

#ifdef NUGU_ENV_DUMP_PATH_ENCODER
	ed->dump_fd =
		_dumpfile_open(getenv(NUGU_ENV_DUMP_PATH_ENCODER), "speex");
#endif

	ed->buffer = NULL;
	ed->buffer_size = 0;
	nugu_encoder_set_driver_data(enc, ed);

	nugu_dbg("new speex %d encoder created", sample_rate);

	return 0;
}

static int _encoder_encode(NuguEncoderDriver *driver, NuguEncoder *enc,
			   int is_end, const void *data, size_t data_len,
			   NuguBuffer *out_buf)
{
	struct encoder_data *ed;
	int encoded_size;

	g_return_val_if_fail(driver != NULL, -1);
	g_return_val_if_fail(enc != NULL, -1);
	g_return_val_if_fail(data != NULL, -1);
	g_return_val_if_fail(out_buf != NULL, -1);

	if (data_len == 0)
		return 0;

	ed = nugu_encoder_get_driver_data(enc);
	if (!ed)
		return -1;

	encoded_size = epd_speex_run(data, data_len);
	if (encoded_size < 0) {
		nugu_error("epd_speex_run() failed");
		return -1;
	}

	nugu_dbg("SPEEX encoded %d bytes (PCM %d)", encoded_size, data_len);
	if (encoded_size == 0)
		return 0;

	if (encoded_size > ed->buffer_size) {
		ed->buffer_size = encoded_size;
		if (ed->buffer)
			ed->buffer = (char *)realloc(ed->buffer, encoded_size);
		else
			ed->buffer = (char *)malloc(encoded_size);
	}

	epd_speex_get_encoded_data(ed->buffer, encoded_size);
	nugu_buffer_add(out_buf, ed->buffer, encoded_size);

#ifdef NUGU_ENV_DUMP_PATH_ENCODER
	if (ed->dump_fd != -1) {
		if (write(ed->dump_fd, ed->buffer, encoded_size) < 0)
			nugu_error("write to fd-%d failed", ed->dump_fd);
	}
#endif

	return 0;
}

static int _encoder_destroy(NuguEncoderDriver *driver, NuguEncoder *enc)
{
	struct encoder_data *ed;

	g_return_val_if_fail(driver != NULL, -1);
	g_return_val_if_fail(enc != NULL, -1);

	if (epd_speex_release() < 0) {
		nugu_error("epd_speex_release() failed");
		return -1;
	}

	ed = nugu_encoder_get_driver_data(enc);
	if (ed) {
		if (ed->buffer)
			free(ed->buffer);
#ifdef NUGU_ENV_DUMP_PATH_ENCODER
		if (ed->dump_fd >= 0) {
			close(ed->dump_fd);
			ed->dump_fd = -1;
		}
#endif
		free(ed);
	}

	nugu_encoder_set_driver_data(enc, NULL);

	return 0;
}

static struct nugu_encoder_driver_ops encoder_ops = {
	.create = _encoder_create,
	.encode = _encoder_encode,
	.destroy = _encoder_destroy
};

static int init(NuguPlugin *p)
{
	const struct nugu_plugin_desc *desc;

	desc = nugu_plugin_get_description(p);
	nugu_dbg("plugin-init '%s'", desc->name);

	driver = nugu_encoder_driver_new(desc->name, NUGU_ENCODER_TYPE_SPEEX,
					 &encoder_ops);
	if (!driver)
		return -1;

	if (nugu_encoder_driver_register(driver) < 0) {
		nugu_encoder_driver_free(driver);
		driver = NULL;
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

	if (driver) {
		nugu_encoder_driver_remove(driver);
		nugu_encoder_driver_free(driver);
		driver = NULL;
	}

	nugu_dbg("'%s' plugin unloaded", nugu_plugin_get_description(p)->name);
}

NUGU_PLUGIN_DEFINE(speex,
	NUGU_PLUGIN_PRIORITY_DEFAULT,
	"0.0.1",
	load,
	unload,
	init);
