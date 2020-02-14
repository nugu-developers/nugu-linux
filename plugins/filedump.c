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

#include <glib.h>

#include "base/nugu_log.h"
#include "base/nugu_plugin.h"
#include "base/nugu_decoder.h"
#include "base/nugu_pcm.h"

#define DECODER_FILENAME_TPL "/tmp/filedump_decoder_XXXXXX"
#define PCM_FILENAME_TPL "/tmp/filedumpXXXXXX"

static NuguDecoderDriver *decoder_driver;
static NuguPcmDriver *pcm_driver;

static int _decoder_create(NuguDecoderDriver *driver, NuguDecoder *dec)
{
	int fd;
	char buf[255] = DECODER_FILENAME_TPL;

	nugu_dbg("new filedump decoder created");

	fd = g_mkstemp(buf);
	if (fd < 0)
		return -1;

	nugu_decoder_set_userdata(dec, GINT_TO_POINTER(fd));

	return 0;
}

static int _decoder_decode(NuguDecoderDriver *driver, NuguDecoder *dec,
			   const void *data, size_t data_len, NuguBuffer *buf)
{
	int fd;

	fd = GPOINTER_TO_INT(nugu_decoder_get_userdata(dec));
	if (write(fd, data, data_len) < 0)
		return -1;

	return 0;
}

static int _decoder_destroy(NuguDecoderDriver *driver, NuguDecoder *dec)
{
	int fd;

	fd = GPOINTER_TO_INT(nugu_decoder_get_userdata(dec));
	close(fd);

	return 0;
}

static struct nugu_decoder_driver_ops decoder_ops = {
	.create = _decoder_create,
	.decode = _decoder_decode,
	.destroy = _decoder_destroy
};

static int _pcm_start(NuguPcmDriver *driver, NuguPcm *pcm,
		      NuguAudioProperty prop)
{
	int fd;
	char buf[255] = PCM_FILENAME_TPL;

	fd = g_mkstemp(buf);
	if (fd < 0)
		return -1;

	nugu_dbg("pcm filedump open: name=%s, fd=%d", buf, fd);

	nugu_pcm_set_userdata(pcm, GINT_TO_POINTER(fd));

	return 0;
}

static int _pcm_push_data(NuguPcmDriver *driver, NuguPcm *pcm, const char *data,
			  size_t size, int is_last)
{
	int fd;

	fd = GPOINTER_TO_INT(nugu_pcm_get_userdata(pcm));

	if (write(fd, data, size) < 0)
		return -1;

	return 0;
}

static int _pcm_stop(NuguPcmDriver *driver, NuguPcm *pcm)
{
	int fd;

	fd = GPOINTER_TO_INT(nugu_pcm_get_userdata(pcm));

	nugu_dbg("pcm filedump close: fd=%d", fd);

	close(fd);

	return 0;
}

static struct nugu_pcm_driver_ops pcm_ops = {
	.start = _pcm_start,
	.push_data = _pcm_push_data,
	.stop = _pcm_stop
};

static int init(NuguPlugin *p)
{
	nugu_dbg("plugin-init '%s'", nugu_plugin_get_description(p)->name);

	decoder_driver = nugu_decoder_driver_new(
		"filedump", NUGU_DECODER_TYPE_CUSTOM, &decoder_ops);
	if (!decoder_driver)
		return -1;

	if (nugu_decoder_driver_register(decoder_driver) < 0) {
		nugu_decoder_driver_free(decoder_driver);
		return -1;
	}

	pcm_driver = nugu_pcm_driver_new("filedump", &pcm_ops);
	if (!pcm_driver) {
		nugu_decoder_driver_remove(decoder_driver);
		nugu_decoder_driver_free(decoder_driver);
		return -1;
	}

	if (nugu_pcm_driver_register(pcm_driver) != 0) {
		nugu_pcm_driver_free(pcm_driver);
		nugu_decoder_driver_remove(decoder_driver);
		nugu_decoder_driver_free(decoder_driver);
		return -1;
	}

	nugu_dbg("filedump decoder/pcm driver registered");

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
}

NUGU_PLUGIN_DEFINE("filedump",
	NUGU_PLUGIN_PRIORITY_LOW,
	"0.0.1",
	load,
	unload,
	init);
