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

#include <glib.h>

#include "base/nugu_log.h"
#include "base/nugu_decoder.h"

#define DEFAULT_DECODE_BUFFER_SIZE 65536

struct _nugu_decoder {
	NuguDecoderDriver *driver;
	void *driver_data;

	NuguPcm *pcm;
	NuguBuffer *buf;
};

struct _nugu_decoder_driver {
	char *name;
	enum nugu_decoder_type type;
	struct nugu_decoder_driver_ops *ops;
	int ref_count;
};

static GList *_decoder_drivers;

EXPORT_API NuguDecoderDriver *
nugu_decoder_driver_new(const char *name, enum nugu_decoder_type type,
			struct nugu_decoder_driver_ops *ops)
{
	NuguDecoderDriver *driver;

	g_return_val_if_fail(name != NULL, NULL);
	g_return_val_if_fail(ops != NULL, NULL);

	driver = malloc(sizeof(struct _nugu_decoder_driver));
	driver->name = g_strdup(name);
	driver->type = type;
	driver->ops = ops;
	driver->ref_count = 0;

	return driver;
}

EXPORT_API int nugu_decoder_driver_free(NuguDecoderDriver *driver)
{
	g_return_val_if_fail(driver != NULL, -1);

	if (driver->ref_count != 0)
		return -1;

	g_free(driver->name);

	memset(driver, 0, sizeof(struct _nugu_decoder_driver));
	free(driver);

	return 0;
}

EXPORT_API int nugu_decoder_driver_register(NuguDecoderDriver *driver)
{
	g_return_val_if_fail(driver != NULL, -1);

	if (nugu_decoder_driver_find(driver->name)) {
		nugu_error("'%s' decoder driver already exist.", driver->name);
		return -1;
	}

	_decoder_drivers = g_list_append(_decoder_drivers, driver);

	return 0;
}

EXPORT_API int nugu_decoder_driver_remove(NuguDecoderDriver *driver)
{
	GList *l;

	l = g_list_find(_decoder_drivers, driver);
	if (!l)
		return -1;

	_decoder_drivers = g_list_remove_link(_decoder_drivers, l);

	return 0;
}

EXPORT_API NuguDecoderDriver *nugu_decoder_driver_find(const char *name)
{
	GList *cur;

	g_return_val_if_fail(name != NULL, NULL);

	cur = _decoder_drivers;
	while (cur) {
		if (g_strcmp0(((NuguDecoderDriver *)cur->data)->name, name) ==
		    0)
			return cur->data;

		cur = cur->next;
	}

	return NULL;
}

EXPORT_API NuguDecoderDriver *
nugu_decoder_driver_find_bytype(enum nugu_decoder_type type)
{
	GList *cur;

	cur = _decoder_drivers;
	while (cur) {
		if (((NuguDecoderDriver *)cur->data)->type == type)
			return cur->data;

		cur = cur->next;
	}

	return NULL;
}

EXPORT_API NuguDecoder *nugu_decoder_new(NuguDecoderDriver *driver,
					 NuguPcm *sink)
{
	NuguDecoder *dec;

	g_return_val_if_fail(driver != NULL, NULL);

	dec = malloc(sizeof(struct _nugu_decoder));
	dec->driver = driver;
	dec->pcm = sink;
	dec->buf = nugu_buffer_new(DEFAULT_DECODE_BUFFER_SIZE);
	dec->driver_data = NULL;

	driver->ref_count++;

	if (driver->ops->create == NULL)
		return dec;

	if (driver->ops->create(driver, dec) == 0)
		return dec;

	driver->ref_count--;
	memset(dec, 0, sizeof(struct _nugu_decoder));
	free(dec);

	return NULL;
}

EXPORT_API int nugu_decoder_free(NuguDecoder *dec)
{
	g_return_val_if_fail(dec != NULL, -1);

	if (dec->driver->ops->destroy &&
	    dec->driver->ops->destroy(dec->driver, dec) < 0)
		return -1;

	dec->driver->ref_count--;

	if (dec->buf)
		nugu_buffer_free(dec->buf, 1);

	memset(dec, 0, sizeof(struct _nugu_decoder));
	free(dec);

	return 0;
}

EXPORT_API NuguPcm *nugu_decoder_get_pcm(NuguDecoder *dec)
{
	g_return_val_if_fail(dec != NULL, NULL);

	return dec->pcm;
}

EXPORT_API int nugu_decoder_play(NuguDecoder *dec, const void *data,
				 size_t data_len)
{
	int ret;
	void *out;
	size_t out_length;

	g_return_val_if_fail(dec != NULL, -1);
	g_return_val_if_fail(data != NULL, -1);
	g_return_val_if_fail(data_len > 0, -1);
	g_return_val_if_fail(dec->driver != NULL, -1);

	if (dec->driver->ops->decode == NULL) {
		nugu_error("Not supported");
		return -1;
	}

	ret = dec->driver->ops->decode(dec->driver, dec, data, data_len,
				       dec->buf);
	if (ret != 0)
		return -1;

	if (!dec->pcm)
		return ret;

	out_length = nugu_buffer_get_size(dec->buf);
	out = nugu_buffer_pop(dec->buf, 0);
	if (!out)
		return -1;

	if (nugu_pcm_push_data(dec->pcm, out, out_length, 0) < 0) {
		free(out);
		return -1;
	}

	free(out);

	return 0;
}

EXPORT_API void *nugu_decoder_decode(NuguDecoder *dec, const void *data,
				     size_t data_len, size_t *output_len)
{
	int ret;
	void *out;

	g_return_val_if_fail(dec != NULL, NULL);
	g_return_val_if_fail(data != NULL, NULL);
	g_return_val_if_fail(data_len > 0, NULL);
	g_return_val_if_fail(dec->driver != NULL, NULL);
	g_return_val_if_fail(output_len != NULL, NULL);

	if (dec->driver->ops->decode == NULL) {
		nugu_error("Not supported");
		return NULL;
	}

	ret = dec->driver->ops->decode(dec->driver, dec, data, data_len,
				       dec->buf);
	if (ret != 0)
		return NULL;

	*output_len = nugu_buffer_get_size(dec->buf);

	out = nugu_buffer_pop(dec->buf, 0);
	if (!out)
		return NULL;

	return out;
}

EXPORT_API int nugu_decoder_set_driver_data(NuguDecoder *dec, void *data)
{
	g_return_val_if_fail(dec != NULL, -1);

	dec->driver_data = data;

	return 0;
}

EXPORT_API void *nugu_decoder_get_driver_data(NuguDecoder *dec)
{
	g_return_val_if_fail(dec != NULL, NULL);

	return dec->driver_data;
}
