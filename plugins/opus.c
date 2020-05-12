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

#include <unistd.h>
#include <stdlib.h>

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

static NuguDecoderDriver *driver;

#ifdef DECODER_FILE_DUMP
static Pcm *_tmp_filedump;
#endif

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

static int _decoder_create(NuguDecoderDriver *driver, NuguDecoder *dec)
{
	OpusDecoder *handle;
	int err = 0;

	handle = opus_decoder_create(SAMPLING_RATES, CHANNELS, &err);
	if (err != OPUS_OK) {
		nugu_error("opus_decoder_create() failed. (%d)", err);
		return -1;
	}

	nugu_decoder_set_driver_data(dec, handle);

	nugu_dbg("new opus 22K decoder (16bit mono pcm) created");

#ifdef DECODER_FILE_DUMP
	if (_tmp_filedump)
		nugu_pcm_free(_tmp_filedump);

	_tmp_filedump = nugu_pcm_new("filedump", pcm_driver_find("filedump"));
	nugu_pcm_start(_tmp_filedump);
#endif
	return 0;
}

static int _decoder_decode(NuguDecoderDriver *driver, NuguDecoder *dec,
			   const void *data, size_t data_len,
			   NuguBuffer *out_buf)
{
	OpusDecoder *handle;
	const unsigned char *packet = data;
	int nsamples;
	int len;
	uint32_t enc_final_range;
	uint32_t dec_final_range;
	opus_int16 sample[PCM_SAMPLES * CHANNELS];
	char plain_pcm[PCM_SAMPLES * CHANNELS * 2];
	int i;

	handle = nugu_decoder_get_driver_data(dec);

	/**
	 * opus 1 frame
	 *   := 168 bytes (4 bytes[len] + 4 bytes[range] + 160 bytes[payload])
	 * decoding result
	 *   := 480 samples (16bit) == 960 bytes
	 */
	while (packet < (unsigned char *)data + data_len) {
		len = READINT(packet);
		packet += 4;

		if (len > 160 || len == 0) {
			nugu_error("invalid payload length(%d)", len);
			continue;
		}
		enc_final_range = READINT(packet);
		packet += 4;

		nsamples = opus_decode(handle, packet, len, sample, PCM_SAMPLES,
				       0);
		if (nsamples <= 0) {
			dump_opus_error(nsamples);
			break;
		}

		packet += len;

		opus_decoder_ctl(handle,
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

#ifdef DECODER_FILE_DUMP
		if (_tmp_filedump)
			nugu_pcm_push_data(_tmp_filedump, plain_pcm,
					   nsamples * 2, 0);
#endif

		nugu_buffer_add(out_buf, plain_pcm, nsamples * 2);
	}

	return 0;
}

static int _decoder_destroy(NuguDecoderDriver *driver, NuguDecoder *dec)
{
	OpusDecoder *handle;

	handle = nugu_decoder_get_driver_data(dec);

	opus_decoder_destroy(handle);

	nugu_dbg("opus decoder destroyed");

#ifdef DECODER_FILE_DUMP
	if (_tmp_filedump) {
		pcm_free(_tmp_filedump);
		_tmp_filedump = NULL;
	}
#endif

	return 0;
}

static struct nugu_decoder_driver_ops decoder_ops = {
	.create = _decoder_create,
	.decode = _decoder_decode,
	.destroy = _decoder_destroy
};

static int init(NuguPlugin *p)
{
	nugu_dbg("plugin-init '%s'", nugu_plugin_get_description(p)->name);

	driver = nugu_decoder_driver_new("opus", NUGU_DECODER_TYPE_OPUS,
					 &decoder_ops);
	if (!driver)
		return -1;

	if (nugu_decoder_driver_register(driver) < 0) {
		nugu_decoder_driver_free(driver);
		return -1;
	}

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
}

NUGU_PLUGIN_DEFINE("opus",
	NUGU_PLUGIN_PRIORITY_DEFAULT,
	"0.0.1",
	load,
	unload,
	init);
