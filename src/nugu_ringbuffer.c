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
#include <pthread.h>

#include <glib.h>

#include "base/nugu_log.h"
#include "base/nugu_ringbuffer.h"

//#define DEBUG_RINGBUFFER

struct _nugu_ring_buffer {
	unsigned char *buf;
	int item_size;
	int max_items;
	int read_index;
	int count;
	unsigned long woffset;
	pthread_mutex_t mutex;
};

static void _calculate_count(NuguRingBuffer *buf, int write_item, int size)
{
	int write_index = buf->woffset / buf->item_size;
	int new_write_index = (buf->woffset + size) / buf->item_size;

	if ((write_item + buf->count) > buf->max_items)
		buf->count = buf->max_items;
	else if (buf->read_index > write_index)
		buf->count =
			buf->max_items - (buf->read_index - new_write_index);
	else
		buf->count = new_write_index - buf->read_index;
}

EXPORT_API NuguRingBuffer *nugu_ring_buffer_new(int item_size, int max_items)
{
	NuguRingBuffer *buffer;

	g_return_val_if_fail(item_size > 0, NULL);
	g_return_val_if_fail(max_items > 0, NULL);

	buffer = (NuguRingBuffer *)calloc(1, sizeof(struct _nugu_ring_buffer));
	buffer->buf = (unsigned char *)calloc(1, item_size * max_items);
	buffer->item_size = item_size;
	buffer->max_items = max_items;
	buffer->read_index = 0;
	buffer->woffset = 0;
	buffer->count = 0;

	pthread_mutex_init(&buffer->mutex, NULL);

	return buffer;
}
EXPORT_API void nugu_ring_buffer_free(NuguRingBuffer *buf)
{
	g_return_if_fail(buf != NULL);
	g_return_if_fail(buf->buf != NULL);

	pthread_mutex_destroy(&buf->mutex);
	free(buf->buf);
	free(buf);
}

EXPORT_API int nugu_ring_buffer_resize(NuguRingBuffer *buf, int item_size,
				       int max_items)
{
	g_return_val_if_fail(buf != NULL, -1);
	g_return_val_if_fail(buf->buf != NULL, -1);
	g_return_val_if_fail(item_size > 0, -1);
	g_return_val_if_fail(max_items > 0, -1);

	if (item_size == buf->item_size && max_items == buf->max_items)
		return 0;

	pthread_mutex_lock(&buf->mutex);

	free(buf->buf);
	buf->buf = (unsigned char *)calloc(1, item_size * max_items);
	buf->item_size = item_size;
	buf->max_items = max_items;
	buf->read_index = 0;
	buf->woffset = 0;
	buf->count = 0;

	pthread_mutex_unlock(&buf->mutex);

	return 0;
}

EXPORT_API int nugu_ring_buffer_push_data(NuguRingBuffer *buf, const char *data,
					  int size)
{
	unsigned long buf_size;
	int write_item = 0;
	int count = 0;
	int temp, temp2, temp3;

	g_return_val_if_fail(buf != NULL, -1);
	g_return_val_if_fail(data != NULL, -1);
	g_return_val_if_fail(size > 0, -1);

	buf_size = buf->max_items * buf->item_size;
	if (buf_size < (unsigned long)size) {
		nugu_error("Should be setting more space for ring buffer!!!");
		return -1;
	}
	// Remainder write offset over item_size
	temp = buf->woffset - (buf->woffset / buf->item_size * buf->item_size);
	// Remainder data offset over item_size
	temp2 = size % buf->item_size;
	// Remainder sum write offset and data offset
	temp3 = (temp + temp2) % buf->item_size;
	write_item = size / buf->item_size + (temp + temp2) / buf->item_size;

	if (temp3)
		write_item += 1 /* reserved for writing next item */;

	count = buf->count;
	_calculate_count(buf, write_item, size);

#ifdef DEBUG_RINGBUFFER
	nugu_dbg("[0-%d] write %d (r: %d, w: %d, c: %d)", buf->max_items,
		 write_item, buf->read_index, buf->woffset / buf->item_size,
		 buf->count);
#endif

	if ((write_item + count) > buf->max_items) {
		pthread_mutex_lock(&buf->mutex);

#ifdef DEBUG_RINGBUFFER
		nugu_dbg("ring buffer is full. reduce %d", write_item);
#endif
		buf->read_index += write_item;

		if (buf->read_index >= buf->max_items)
			buf->read_index = buf->read_index - buf->max_items;

		pthread_mutex_unlock(&buf->mutex);
	}

	if ((buf->woffset + size) >= buf_size) {
		unsigned long diff;

		diff = buf_size - buf->woffset;
		if (diff)
			memcpy(buf->buf + buf->woffset, data, diff);
		memcpy(buf->buf, data + diff, size - diff);
		buf->woffset = size - diff;
	} else {
		memcpy(buf->buf + buf->woffset, data, size);
		buf->woffset += size;
	}

#ifdef DEBUG_RINGBUFFER
	nugu_dbg("[0-%d] write %d done (r:%d, w:%d, c:%d)", buf->max_items,
		 write_item, buf->read_index, buf->woffset / buf->item_size,
		 buf->count);
#endif

	return 0;
}

EXPORT_API int nugu_ring_buffer_read_item(NuguRingBuffer *buf, char *item,
					  int *size)
{
	g_return_val_if_fail(buf != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(size != NULL, -1);

	if (nugu_ring_buffer_get_count(buf) <= 0) {
		*size = 0;
		return 0;
	}

	pthread_mutex_lock(&buf->mutex);

	*size = buf->item_size;
	memcpy(item, buf->buf + (buf->read_index * buf->item_size), *size);

	buf->read_index++;
	buf->count--;
	if (buf->read_index == buf->max_items)
		buf->read_index = 0;

	pthread_mutex_unlock(&buf->mutex);

	return 0;
}

EXPORT_API int nugu_ring_buffer_get_count(NuguRingBuffer *buf)
{
	g_return_val_if_fail(buf != NULL, -1);
	return buf->count;
}

EXPORT_API int nugu_ring_buffer_get_maxcount(NuguRingBuffer *buf)
{
	g_return_val_if_fail(buf != NULL, -1);
	return buf->max_items;
}

EXPORT_API int nugu_ring_buffer_get_item_size(NuguRingBuffer *buf)
{
	g_return_val_if_fail(buf != NULL, -1);
	return buf->item_size;
}

EXPORT_API void nugu_ring_buffer_clear_items(NuguRingBuffer *buf)
{
	g_return_if_fail(buf != NULL);

	pthread_mutex_lock(&buf->mutex);

	buf->read_index = 0;
	buf->woffset = 0;
	buf->count = 0;

	pthread_mutex_unlock(&buf->mutex);
}
