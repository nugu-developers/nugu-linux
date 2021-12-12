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

#include <glib.h>
#include <opus.h>
#include <ogg/ogg.h>

#include "base/nugu_log.h"
#include "base/nugu_plugin.h"
#include "base/nugu_encoder.h"
#include "base/nugu_pcm.h"

#define SAMPLERATE 16000
#define CHANNELS 1
#define FRAME_LENGTH 10 /* 10ms */

#define FRAME_SIZE ((SAMPLERATE / 1000) * FRAME_LENGTH) /* 160 */
#define PACKET_SIZE (FRAME_SIZE * 2 * CHANNELS) /* 320 */
#define READSIZE (FRAME_SIZE * 2)

#define MAX_PACKET_SIZE (PACKET_SIZE * 2)

struct opus_data {
	OpusEncoder *enc_handle;
	ogg_stream_state os;

	ogg_int64_t packetno;
	ogg_int64_t granulepos;
	opus_int16 frames[READSIZE];
	unsigned char encoded_buf[MAX_PACKET_SIZE];

	NuguBuffer *buf;

#ifdef NUGU_ENV_DUMP_PATH_ENCODER
	int dump_fd;
#endif
};

static NuguEncoderDriver *enc_driver;

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

	free(buf);

	return fd;
}
#endif

static void setup_opus_head(struct opus_data *od)
{
	NuguBuffer *header = nugu_buffer_new(512);
	uint8_t version = 1;
	uint8_t channel = 1;
	uint16_t preskip = 0;
	uint32_t sample_rate = 16000;
	uint16_t gain = 0;
	uint8_t channel_mapping = 0;
	ogg_packet p_head;

	nugu_buffer_add(header, "OpusHead", 8);
	nugu_buffer_add(header, &version, 1);
	nugu_buffer_add(header, &channel, 1);
	nugu_buffer_add(header, &preskip, 2);
	nugu_buffer_add(header, &sample_rate, 4);
	nugu_buffer_add(header, &gain, 2);
	nugu_buffer_add(header, &channel_mapping, 1);

	p_head.bytes = nugu_buffer_get_size(header);
	p_head.packet = nugu_buffer_pop(header, 0);
	p_head.b_o_s = 1;
	p_head.e_o_s = 0;
	p_head.granulepos = 0;
	p_head.packetno = od->packetno++;
	ogg_stream_packetin(&(od->os), &p_head);

	nugu_buffer_free(header, 1);
}

static void setup_opus_tags(struct opus_data *od)
{
	NuguBuffer *header = nugu_buffer_new(512);
	ogg_packet p_tags;
	const char *vendor = "skt_nugu";
	uint32_t vendor_len = strlen(vendor);
	uint32_t comment_list_len = 0;

	nugu_buffer_add(header, "OpusTags", 8);
	nugu_buffer_add(header, &vendor_len, 4);
	nugu_buffer_add(header, vendor, vendor_len);
	nugu_buffer_add(header, &comment_list_len, 4);

	p_tags.bytes = nugu_buffer_get_size(header);
	p_tags.packet = nugu_buffer_pop(header, 0);
	p_tags.b_o_s = 0;
	p_tags.e_o_s = 0;
	p_tags.granulepos = 0;
	p_tags.packetno = od->packetno++;
	ogg_stream_packetin(&(od->os), &p_tags);

	nugu_buffer_free(header, 1);
}

static void flush_stream(struct opus_data *od)
{
	unsigned int i;
	ogg_page og;

	i = ogg_stream_flush(&(od->os), &og);
	for (; i != 0; i = ogg_stream_flush(&(od->os), &og)) {
		nugu_buffer_add(od->buf, og.header, og.header_len);
		nugu_buffer_add(od->buf, og.body, og.body_len);
	}
}

int _encoder_create(NuguEncoderDriver *driver, NuguEncoder *enc,
		    NuguAudioProperty property)
{
	struct opus_data *od;
	int err;

	g_return_val_if_fail(driver != NULL, -1);
	g_return_val_if_fail(enc != NULL, -1);

	od = malloc(sizeof(struct opus_data));
	if (!od) {
		nugu_error_nomem();
		return -1;
	}

	nugu_encoder_set_driver_data(enc, od);

	nugu_dbg("new opus encoder created");

	od->buf = nugu_buffer_new(4096);
	od->packetno = 0;
	od->granulepos = 0;
	od->enc_handle = opus_encoder_create(SAMPLERATE, CHANNELS,
					     OPUS_APPLICATION_VOIP, &err);

#ifdef NUGU_ENV_DUMP_PATH_ENCODER
	od->dump_fd =
		_dumpfile_open(getenv(NUGU_ENV_DUMP_PATH_ENCODER), "opus");
#endif

	err = ogg_stream_init(&(od->os), rand());
	if (err < 0) {
		nugu_error("ogg_stream_init() failed\n");
		return -1;
	}

	setup_opus_head(od);
	setup_opus_tags(od);
	flush_stream(od);

	return 0;
}

int do_encode(struct opus_data *od, int is_last, const unsigned char *buf,
	      size_t buf_len)
{
	int enc_len;
	size_t samples = buf_len / 2;
	size_t i = 0;
	ogg_packet op;

	if (buf) {
		for (; i < samples; i++)
			od->frames[i] = (0xFF & buf[2 * i + 1]) << 8 |
					(0xFF & buf[2 * i]);
	}

	/* If samples < FRAME_SIZE, fill empty data */
	if (samples < FRAME_SIZE)
		samples = FRAME_SIZE;

	for (; i < FRAME_SIZE; i++)
		od->frames[i] = 0;

	enc_len = opus_encode(od->enc_handle, od->frames, samples,
			      od->encoded_buf, MAX_PACKET_SIZE);
	if (enc_len < 0) {
		nugu_error("opus_encode() failed: %s", opus_strerror(enc_len));
		return -1;
	}

	od->granulepos += samples * (48000 / SAMPLERATE);

	op.packet = od->encoded_buf;
	op.bytes = enc_len;
	op.b_o_s = 0;
	op.e_o_s = is_last;
	op.packetno = od->packetno;
	op.granulepos = od->granulepos;

	ogg_stream_packetin(&(od->os), &op);

	return enc_len;
}

int _encoder_encode(NuguEncoderDriver *driver, NuguEncoder *enc, int is_last,
		    const void *data, size_t data_len, NuguBuffer *out_buf)
{
	struct opus_data *od;
	const unsigned char *buf = data;
	size_t i;

	od = nugu_encoder_get_driver_data(enc);
	if (!od)
		return -1;

	if (data_len == 0) {
		do_encode(od, is_last, NULL, 0);
	} else {
		for (i = 0; i < data_len; i += READSIZE)
			do_encode(od, is_last, buf + i, READSIZE);

		od->packetno++;
	}

	flush_stream(od);

	i = nugu_buffer_get_size(od->buf);
	if (i <= 0)
		return 0;

	nugu_dbg("OPUS encoded %zd bytes (PCM %d)", i, data_len);

	nugu_buffer_add(out_buf, nugu_buffer_peek(od->buf), i);

#ifdef NUGU_ENV_DUMP_PATH_ENCODER
	if (od->dump_fd != -1) {
		if (write(od->dump_fd, nugu_buffer_peek(od->buf), i) < 0)
			nugu_error("write to fd-%d failed", od->dump_fd);
	}
#endif

	nugu_buffer_clear(od->buf);

	return 0;
}

int _encoder_destroy(NuguEncoderDriver *driver, NuguEncoder *enc)
{
	struct opus_data *od;

	od = nugu_encoder_get_driver_data(enc);
	if (!od) {
		nugu_error("internal error");
		return -1;
	}

#ifdef NUGU_ENV_DUMP_PATH_ENCODER
	if (od->dump_fd >= 0) {
		close(od->dump_fd);
		od->dump_fd = -1;
	}
#endif

	opus_encoder_destroy(od->enc_handle);
	ogg_stream_clear(&(od->os));
	nugu_buffer_free(od->buf, 1);

	free(od);
	nugu_encoder_set_driver_data(enc, NULL);

	nugu_dbg("opus encoder destroyed");

	return 0;
}

static struct nugu_encoder_driver_ops encoder_ops = {
	.create = _encoder_create,
	.encode = _encoder_encode,
	.destroy = _encoder_destroy
};

static int init(NuguPlugin *p)
{
	nugu_dbg("plugin-init '%s'", nugu_plugin_get_description(p)->name);

	enc_driver = nugu_encoder_driver_new("opus", NUGU_ENCODER_TYPE_OPUS,
					     &encoder_ops);
	if (!enc_driver)
		return -1;

	if (nugu_encoder_driver_register(enc_driver) < 0) {
		nugu_encoder_driver_free(enc_driver);
		enc_driver = NULL;
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
	nugu_encoder_driver_remove(enc_driver);
	nugu_encoder_driver_free(enc_driver);
}

NUGU_PLUGIN_DEFINE("opusenc",
	NUGU_PLUGIN_PRIORITY_DEFAULT,
	"0.0.1",
	load,
	unload,
	init);
