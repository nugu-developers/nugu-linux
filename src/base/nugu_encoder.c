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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "base/nugu_log.h"
#include "base/nugu_encoder.h"

#define DEFAULT_ENCODE_BUFFER_SIZE 4096

struct _nugu_encoder {
	NuguEncoderDriver *driver;
	void *driver_data;

	NuguBuffer *buf;
};

struct _nugu_encoder_driver {
	char *name;
	enum nugu_encoder_type type;
	struct nugu_encoder_driver_ops *ops;
	int ref_count;
};

static GList *_encoder_drivers;

EXPORT_API NuguEncoderDriver *
nugu_encoder_driver_new(const char *name, enum nugu_encoder_type type,
			struct nugu_encoder_driver_ops *ops)
{
	NuguEncoderDriver *driver;

	g_return_val_if_fail(name != NULL, NULL);
	g_return_val_if_fail(ops != NULL, NULL);

	driver = malloc(sizeof(struct _nugu_encoder_driver));
	driver->name = g_strdup(name);
	driver->type = type;
	driver->ops = ops;
	driver->ref_count = 0;

	return driver;
}

EXPORT_API int nugu_encoder_driver_free(NuguEncoderDriver *driver)
{
	g_return_val_if_fail(driver != NULL, -1);

	if (driver->ref_count != 0)
		return -1;

	g_free(driver->name);

	memset(driver, 0, sizeof(struct _nugu_encoder_driver));
	free(driver);

	return 0;
}

EXPORT_API int nugu_encoder_driver_register(NuguEncoderDriver *driver)
{
	g_return_val_if_fail(driver != NULL, -1);

	if (nugu_encoder_driver_find(driver->name)) {
		nugu_error("'%s' encoder driver already exist.", driver->name);
		return -1;
	}

	_encoder_drivers = g_list_append(_encoder_drivers, driver);

	return 0;
}

EXPORT_API int nugu_encoder_driver_remove(NuguEncoderDriver *driver)
{
	GList *l;

	l = g_list_find(_encoder_drivers, driver);
	if (!l)
		return -1;

	_encoder_drivers = g_list_remove_link(_encoder_drivers, l);

	return 0;
}

EXPORT_API NuguEncoderDriver *nugu_encoder_driver_find(const char *name)
{
	GList *cur;

	g_return_val_if_fail(name != NULL, NULL);

	cur = _encoder_drivers;
	while (cur) {
		if (g_strcmp0(((NuguEncoderDriver *)cur->data)->name, name) ==
		    0)
			return cur->data;

		cur = cur->next;
	}

	return NULL;
}

EXPORT_API NuguEncoderDriver *
nugu_encoder_driver_find_bytype(enum nugu_encoder_type type)
{
	GList *cur;

	cur = _encoder_drivers;
	while (cur) {
		if (((NuguEncoderDriver *)cur->data)->type == type)
			return cur->data;

		cur = cur->next;
	}

	return NULL;
}

EXPORT_API NuguEncoder *nugu_encoder_new(NuguEncoderDriver *driver,
					 NuguAudioProperty property)
{
	NuguEncoder *enc;

	g_return_val_if_fail(driver != NULL, NULL);

	enc = malloc(sizeof(struct _nugu_encoder));
	enc->driver = driver;
	enc->buf = nugu_buffer_new(DEFAULT_ENCODE_BUFFER_SIZE);
	enc->driver_data = NULL;

	driver->ref_count++;

	if (driver->ops->create == NULL)
		return enc;

	if (driver->ops->create(driver, enc, property) == 0)
		return enc;

	nugu_error("create() failed from driver");

	driver->ref_count--;
	nugu_buffer_free(enc->buf, 1);
	memset(enc, 0, sizeof(struct _nugu_encoder));
	free(enc);

	return NULL;
}

EXPORT_API int nugu_encoder_free(NuguEncoder *enc)
{
	g_return_val_if_fail(enc != NULL, -1);

	if (enc->driver->ops->destroy &&
	    enc->driver->ops->destroy(enc->driver, enc) < 0)
		return -1;

	enc->driver->ref_count--;

	if (enc->buf)
		nugu_buffer_free(enc->buf, 1);

	memset(enc, 0, sizeof(struct _nugu_encoder));
	free(enc);

	return 0;
}

EXPORT_API void *nugu_encoder_encode(NuguEncoder *enc, const void *data,
				     size_t data_len, size_t *output_len)
{
	int ret;
	void *out;

	g_return_val_if_fail(enc != NULL, NULL);
	g_return_val_if_fail(data != NULL, NULL);
	g_return_val_if_fail(data_len > 0, NULL);
	g_return_val_if_fail(enc->driver != NULL, NULL);
	g_return_val_if_fail(output_len != NULL, NULL);

	if (enc->driver->ops->encode == NULL) {
		nugu_error("Not supported");
		return NULL;
	}

	ret = enc->driver->ops->encode(enc->driver, enc, data, data_len,
				       enc->buf);
	if (ret != 0)
		return NULL;

	*output_len = nugu_buffer_get_size(enc->buf);

	out = nugu_buffer_pop(enc->buf, 0);
	if (!out)
		return NULL;

	return out;
}

EXPORT_API int nugu_encoder_set_driver_data(NuguEncoder *enc, void *data)
{
	g_return_val_if_fail(enc != NULL, -1);

	enc->driver_data = data;

	return 0;
}

EXPORT_API void *nugu_encoder_get_driver_data(NuguEncoder *enc)
{
	g_return_val_if_fail(enc != NULL, NULL);

	return enc->driver_data;
}
