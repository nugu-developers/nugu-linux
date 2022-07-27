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
#include <string.h>

#include <glib.h>

#include "base/nugu_log.h"
#include "base/nugu_buffer.h"

#ifndef CONFIG_DEFAULT_BUFFER_SIZE
#define CONFIG_DEFAULT_BUFFER_SIZE 1024
#endif

struct _nugu_buffer {
	size_t alloc_size;
	size_t index;
	unsigned char *data;
};

EXPORT_API NuguBuffer *nugu_buffer_new(size_t default_size)
{
	NuguBuffer *buf;

	buf = calloc(1, sizeof(struct _nugu_buffer));
	if (!buf) {
		nugu_error_nomem();
		return NULL;
	}

	buf->index = 0;
	buf->alloc_size = default_size;

	if (default_size == 0)
		buf->alloc_size = CONFIG_DEFAULT_BUFFER_SIZE;

	buf->data = calloc(1, buf->alloc_size);
	if (!buf->data) {
		free(buf);
		nugu_error_nomem();
		return NULL;
	}

	return buf;
}

EXPORT_API void *nugu_buffer_free(NuguBuffer *buf, int is_data_free)
{
	g_return_val_if_fail(buf != NULL, NULL);

	if (is_data_free == 0) {
		void *return_data = buf->data;

		memset(buf, 0, sizeof(struct _nugu_buffer));
		free(buf);

		return return_data;
	}

	memset(buf->data, 0, buf->alloc_size);
	free(buf->data);

	memset(buf, 0, sizeof(struct _nugu_buffer));
	free(buf);

	return NULL;
}

static int _buffer_resize(NuguBuffer *buf, size_t needed)
{
	size_t new_size = buf->alloc_size;
	void *tmp;

	if (buf->alloc_size < needed)
		new_size += needed * 2;
	else
		new_size += buf->alloc_size;

	tmp = realloc(buf->data, new_size);
	if (!tmp) {
		nugu_error_nomem();
		return -EFAULT;
	}

	buf->data = tmp;
	buf->alloc_size = new_size;

	return 0;
}

EXPORT_API size_t nugu_buffer_add(NuguBuffer *buf, const void *data,
				  size_t data_len)
{
	g_return_val_if_fail(buf != NULL, 0);
	g_return_val_if_fail(data != NULL, 0);
	g_return_val_if_fail(data_len > 0, 0);

	if (buf->alloc_size - buf->index < data_len) {
		if (_buffer_resize(buf, data_len) < 0)
			return 0;
	}

	memcpy(buf->data + buf->index, data, data_len);
	buf->index += data_len;

	return data_len;
}

EXPORT_API size_t nugu_buffer_add_byte(NuguBuffer *buf, unsigned char byte)
{
	g_return_val_if_fail(buf != NULL, 0);

	if (buf->alloc_size - buf->index < 1) {
		if (_buffer_resize(buf, 1) < 0)
			return 0;
	}

	buf->data[buf->index] = byte;
	buf->index += 1;

	return 1;
}

EXPORT_API int nugu_buffer_set_byte(NuguBuffer *buf, size_t pos,
				    unsigned char byte)
{
	g_return_val_if_fail(buf != NULL, -1);

	if (pos > buf->index)
		return -1;

	buf->data[pos] = byte;

	return 0;
}

EXPORT_API const void *nugu_buffer_peek(NuguBuffer *buf)
{
	g_return_val_if_fail(buf != NULL, NULL);

	return buf->data;
}

EXPORT_API size_t nugu_buffer_get_size(NuguBuffer *buf)
{
	g_return_val_if_fail(buf != NULL, 0);

	return buf->index;
}

EXPORT_API size_t nugu_buffer_get_alloc_size(NuguBuffer *buf)
{
	g_return_val_if_fail(buf != NULL, 0);

	return buf->alloc_size;
}

EXPORT_API size_t nugu_buffer_find_byte(NuguBuffer *buf, unsigned char want)
{
	unsigned char *pos;
	unsigned char *last_pos;

	g_return_val_if_fail(buf != NULL, -1);

	pos = buf->data;
	last_pos = buf->data + buf->index;

	while (pos != last_pos) {
		if (*pos == want)
			return pos - buf->data;

		pos++;
	}

	return NUGU_BUFFER_NOT_FOUND;
}

EXPORT_API unsigned char nugu_buffer_peek_byte(NuguBuffer *buf, size_t pos)
{
	g_return_val_if_fail(buf != NULL, 0);

	if (pos >= buf->index)
		return 0;

	return *(buf->data + pos);
}

EXPORT_API int nugu_buffer_clear(NuguBuffer *buf)
{
	g_return_val_if_fail(buf != NULL, -1);

	memset(buf->data, 0, buf->alloc_size);

	buf->index = 0;

	return 0;
}

EXPORT_API int nugu_buffer_clear_from(NuguBuffer *buf, size_t pos)
{
	g_return_val_if_fail(buf != NULL, -1);
	g_return_val_if_fail(pos <= buf->index, -1);

	memset(buf->data + pos, 0, buf->alloc_size - pos);

	buf->index = buf->index - (buf->index - pos);

	return 0;
}

EXPORT_API int nugu_buffer_shift_left(NuguBuffer *buf, size_t size)
{
	unsigned char *dst;
	unsigned char *src;

	g_return_val_if_fail(buf != NULL, -1);

	if (size >= buf->index)
		return nugu_buffer_clear(buf);

	dst = buf->data;
	src = buf->data + size;

	while (src != buf->data + buf->index) {
		*dst = *src;
		src++;
		dst++;
	}

	while (dst != buf->data + buf->index) {
		*dst = '\0';
		dst++;
	}

	buf->index = buf->index - size;

	return 0;
}

EXPORT_API void *nugu_buffer_pop(NuguBuffer *buf, size_t size)
{
	unsigned char *tmp;

	g_return_val_if_fail(buf != NULL, NULL);

	if (size == 0 || size > buf->index)
		size = buf->index;

	tmp = malloc(size + 1);
	if (!tmp) {
		nugu_error_nomem();
		return NULL;
	}

	memcpy(tmp, buf->data, size);
	tmp[size] = '\0';
	nugu_buffer_shift_left(buf, size);

	return tmp;
}
