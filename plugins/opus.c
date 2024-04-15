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

#ifdef NUGU_PLUGIN_BUILTIN_OPUS
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
#include <opus.h>

#include "base/nugu_log.h"
#include "base/nugu_plugin.h"
#include "base/nugu_decoder.h"
#include "base/nugu_pcm.h"

#define SAMPLING_RATES 24000
#define CHANNELS 1
#define PCM_SAMPLES 480

#define READINT(v)                                                             \
	(((unsigned int)v[0] << 24) | ((unsigned int)v[1] << 16) |             \
	 ((unsigned int)v[2] << 8) | (unsigned int)v[3])

struct opus_data {
	OpusDecoder *handle;
	int dump_fd;
};

static NuguDecoderDriver *driver;

static char *opus_error_defines[] = {
	"OK",
	"One or more invalid/out of range arguments",
	"Not enough bytes allocated in the buffer",
	"An internal error was detected",
	"The compressed data passed is corrupted",
	"Invalid/unsupported request number",
	"An encoder or decoder structure is invalid or already freed",
	"Memory allocation has failed"
};

static void dump_opus_error(int error_code)
{
	nugu_warn("opus error(%d): %s", error_code,
		  opus_error_defines[error_code * -1]);
}

#ifdef NUGU_ENV_DUMP_LINK_FILE_DECODER
static void _dumpfile_link(const char *filename)
{
	char *link_file;
	int ret;

	if (!filename)
		return;

	link_file = getenv(NUGU_ENV_DUMP_LINK_FILE_DECODER);
	if (!link_file)
		link_file = "tts_audio.opus";

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

#ifdef NUGU_ENV_DUMP_LINK_FILE_DECODER
	_dumpfile_link(buf);
#endif
	g_free(buf);

	return fd;
}

static int _decoder_create(NuguDecoderDriver *driver, NuguDecoder *dec)
{
	struct opus_data *od;
	int err = 0;

	od = malloc(sizeof(struct opus_data));
	if (!od) {
		nugu_error_nomem();
		return -1;
	}

	od->handle = opus_decoder_create(SAMPLING_RATES, CHANNELS, &err);
	if (err != OPUS_OK) {
		nugu_error("opus_decoder_create() failed. (%d)", err);
		free(od);
		return -1;
	}

#ifdef NUGU_ENV_DUMP_PATH_DECODER
	od->dump_fd =
		_dumpfile_open(getenv(NUGU_ENV_DUMP_PATH_DECODER), "opus");
#else
	od->dump_fd = -1;
#endif

	nugu_decoder_set_driver_data(dec, od);

	nugu_dbg("new opus 22K decoder (16bit mono pcm) created");

	return 0;
}

static int _decoder_decode(NuguDecoderDriver *driver, NuguDecoder *dec,
			   const void *data, size_t data_len,
			   NuguBuffer *out_buf)
{
	struct opus_data *od;
	const unsigned char *packet = data;
	int nsamples;
	uint32_t enc_final_range;
	uint32_t dec_final_range;
	opus_int16 sample[PCM_SAMPLES * CHANNELS];
	char plain_pcm[PCM_SAMPLES * CHANNELS * 2];
	int i;

	od = nugu_decoder_get_driver_data(dec);

	if (od->dump_fd != -1) {
		if (write(od->dump_fd, data, data_len) < 0)
			nugu_error("write to fd-%d failed", od->dump_fd);
	}

	/**
	 * opus 1 frame
	 *   := 168 bytes (4 bytes[len] + 4 bytes[range] + 160 bytes[payload])
	 * decoding result
	 *   := 480 samples (16bit) == 960 bytes
	 */
	while (packet < (unsigned char *)data + data_len) {
		int len = READINT(packet);

		packet += 4;

		if (len > 160 || len == 0) {
			nugu_error("invalid payload length(%d)", len);
			continue;
		}

		enc_final_range = READINT(packet);
		packet += 4;

		nsamples = opus_decode(od->handle, packet, len, sample,
				       PCM_SAMPLES, 0);
		if (nsamples <= 0) {
			dump_opus_error(nsamples);
			break;
		}

		packet += len;

		opus_decoder_ctl(od->handle,
				 OPUS_GET_FINAL_RANGE(&dec_final_range));
		if (enc_final_range != dec_final_range) {
			nugu_error("range coder status mismatch (0x%x != 0x%X)",
				   enc_final_range, dec_final_range);
			continue;
		}

		for (i = 0; i < nsamples; i++) {
			plain_pcm[2 * i] = sample[i] & 0xFF;
			plain_pcm[2 * i + 1] = (sample[i] & 0xFF00) >> 8;
		}

		nugu_buffer_add(out_buf, plain_pcm, nsamples * 2);
	}

	return 0;
}

static int _decoder_destroy(NuguDecoderDriver *driver, NuguDecoder *dec)
{
	struct opus_data *od;

	od = nugu_decoder_get_driver_data(dec);
	if (!od) {
		nugu_error("internal error");
		return -1;
	}

	if (od->dump_fd >= 0) {
		close(od->dump_fd);
		od->dump_fd = -1;
	}

	opus_decoder_destroy(od->handle);

	free(od);
	nugu_decoder_set_driver_data(dec, NULL);

	nugu_dbg("opus decoder destroyed");

	return 0;
}

static struct nugu_decoder_driver_ops decoder_ops = {
	.create = _decoder_create,
	.decode = _decoder_decode,
	.destroy = _decoder_destroy
};

static int init(NuguPlugin *p)
{
	const struct nugu_plugin_desc *desc;

	desc = nugu_plugin_get_description(p);
	nugu_dbg("plugin-init '%s'", desc->name);

	driver = nugu_decoder_driver_new(desc->name, NUGU_DECODER_TYPE_OPUS,
					 &decoder_ops);
	if (!driver)
		return -1;

	if (nugu_decoder_driver_register(driver) < 0) {
		nugu_decoder_driver_free(driver);
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
		nugu_decoder_driver_remove(driver);
		nugu_decoder_driver_free(driver);
		driver = NULL;
	}

	nugu_dbg("'%s' plugin unloaded", nugu_plugin_get_description(p)->name);
}

NUGU_PLUGIN_DEFINE(opus,
	NUGU_PLUGIN_PRIORITY_DEFAULT,
	"0.0.1",
	load,
	unload,
	init);
