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
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>

#include <glib.h>

#include "base/nugu_log.h"
#include "base/nugu_ringbuffer.h"
#include "base/nugu_recorder.h"

//#define RECORDER_FILE_DUMP

struct _nugu_recorder_driver {
	char *name;
	struct nugu_recorder_driver_ops *ops;
};

struct _nugu_recorder {
	char *name;
	NuguRecorderDriver *driver;
	NuguAudioProperty property;
	NuguRingBuffer *buf;
	int is_recording;
	void *userdata;
	pthread_cond_t cond;
	pthread_mutex_t lock;
#ifdef RECORDER_FILE_DUMP
	FILE *file;
#endif
};

static GList *_recorders;
static GList *_recorder_drivers;
static NuguRecorderDriver *_default_driver;

static void _recorder_release_condition(NuguRecorder *rec)
{
	g_return_if_fail(rec != NULL);

	pthread_mutex_lock(&rec->lock);
	pthread_cond_signal(&rec->cond);
	pthread_mutex_unlock(&rec->lock);
}

EXPORT_API NuguRecorderDriver *
nugu_recorder_driver_new(const char *name, struct nugu_recorder_driver_ops *ops)
{
	NuguRecorderDriver *driver;

	g_return_val_if_fail(name != NULL, NULL);

	driver = malloc(sizeof(struct _nugu_recorder_driver));
	driver->name = g_strdup(name);
	driver->ops = ops;

	return driver;
}

EXPORT_API int nugu_recorder_driver_free(NuguRecorderDriver *driver)
{
	g_return_val_if_fail(driver != NULL, -1);

	if (_default_driver == driver)
		_default_driver = NULL;

	free(driver->name);

	memset(driver, 0, sizeof(struct _nugu_recorder_driver));
	free(driver);

	return 0;
}

EXPORT_API int nugu_recorder_driver_register(NuguRecorderDriver *driver)
{
	g_return_val_if_fail(driver != NULL, -1);

	if (nugu_recorder_driver_find(driver->name)) {
		nugu_error("'%s' recorder driver already exist.", driver->name);
		return -1;
	}

	_recorder_drivers = g_list_append(_recorder_drivers, driver);
	if (!_default_driver)
		nugu_recorder_driver_set_default(driver);
	return 0;
}

EXPORT_API int nugu_recorder_driver_remove(NuguRecorderDriver *driver)
{
	GList *l;

	g_return_val_if_fail(driver != NULL, -1);

	l = g_list_find(_recorder_drivers, driver);
	if (!l)
		return -1;

	if (_default_driver == driver)
		_default_driver = NULL;

	_recorder_drivers = g_list_remove_link(_recorder_drivers, l);

	if (_recorder_drivers && _default_driver == NULL)
		_default_driver = _recorder_drivers->data;

	return 0;
}

EXPORT_API int nugu_recorder_driver_set_default(NuguRecorderDriver *driver)
{
	g_return_val_if_fail(driver != NULL, -1);

	_default_driver = driver;

	return 0;
}

EXPORT_API NuguRecorderDriver *nugu_recorder_driver_get_default(void)
{
	return _default_driver;
}

EXPORT_API NuguRecorderDriver *nugu_recorder_driver_find(const char *name)
{
	GList *cur;

	g_return_val_if_fail(name != NULL, NULL);

	cur = _recorder_drivers;
	while (cur) {
		if (g_strcmp0(((NuguRecorderDriver *)cur->data)->name, name) ==
		    0)
			return cur->data;

		cur = cur->next;
	}

	return NULL;
}

EXPORT_API NuguRecorder *nugu_recorder_new(const char *name,
					   NuguRecorderDriver *driver)
{
	NuguRecorder *rec;

	g_return_val_if_fail(name != NULL, NULL);
	g_return_val_if_fail(driver != NULL, NULL);

	rec = g_malloc0(sizeof(struct _nugu_recorder));
	rec->name = g_strdup(name);
	rec->driver = driver;
	rec->buf = nugu_ring_buffer_new(NUGU_RECORDER_FRAME_SIZE,
					NUGU_RECORDER_MAX_FRAMES);
	rec->is_recording = 0;
	pthread_mutex_init(&rec->lock, NULL);
	pthread_cond_init(&rec->cond, NULL);

#ifdef RECORDER_FILE_DUMP
	rec->file = fopen(name, "w");
#endif
	if (rec->buf == NULL) {
		nugu_error("buffer new is internal error");
		g_free(rec->name);
		g_free(rec);
		return NULL;
	}

	return rec;
}

EXPORT_API void nugu_recorder_free(NuguRecorder *rec)
{
	g_return_if_fail(rec != NULL);
	g_return_if_fail(rec->driver != NULL);

	g_free(rec->name);
	nugu_ring_buffer_free(rec->buf);

	pthread_mutex_destroy(&rec->lock);
	pthread_cond_destroy(&rec->cond);

#ifdef RECORDER_FILE_DUMP
	fclose(rec->file);
#endif
	memset(rec, 0, sizeof(struct _nugu_recorder));
	g_free(rec);
}

EXPORT_API int nugu_recorder_add(NuguRecorder *rec)
{
	g_return_val_if_fail(rec != NULL, -1);

	if (nugu_recorder_find(rec->name)) {
		nugu_error("'%s' rec already exist.", rec->name);
		return -1;
	}

	_recorders = g_list_append(_recorders, rec);

	return 0;
}

EXPORT_API int nugu_recorder_remove(NuguRecorder *rec)
{
	GList *l;

	l = g_list_find(_recorders, rec);
	if (!l)
		return -1;

	_recorders = g_list_remove_link(_recorders, l);

	return 0;
}

EXPORT_API NuguRecorder *nugu_recorder_find(const char *name)
{
	GList *cur;

	g_return_val_if_fail(name != NULL, NULL);

	cur = _recorders;
	while (cur) {
		if (g_strcmp0(((NuguRecorder *)cur->data)->name, name) == 0)
			return cur->data;

		cur = cur->next;
	}

	return NULL;
}

EXPORT_API int nugu_recorder_set_property(NuguRecorder *rec,
					  NuguAudioProperty property)
{
	g_return_val_if_fail(rec != NULL, -1);

	memcpy(&rec->property, &property, sizeof(NuguAudioProperty));

	return 0;
}

EXPORT_API int nugu_recorder_start(NuguRecorder *rec)
{
	g_return_val_if_fail(rec != NULL, -1);
	g_return_val_if_fail(rec->driver != NULL, -1);

	if (rec->driver->ops->start == NULL) {
		nugu_error("Not supported");
		return -1;
	}
	nugu_ring_buffer_clear_items(rec->buf);
	rec->is_recording = 1;

	return rec->driver->ops->start(rec->driver, rec, rec->property);
}

EXPORT_API int nugu_recorder_stop(NuguRecorder *rec)
{
	g_return_val_if_fail(rec != NULL, -1);
	g_return_val_if_fail(rec->driver != NULL, -1);

	if (rec->driver->ops->stop == NULL) {
		nugu_error("Not supported");
		return -1;
	}
	nugu_ring_buffer_clear_items(rec->buf);
	_recorder_release_condition(rec);
	rec->is_recording = 0;

	return rec->driver->ops->stop(rec->driver, rec);
}

EXPORT_API int nugu_recorder_is_recording(NuguRecorder *rec)
{
	g_return_val_if_fail(rec != NULL, -1);

	return rec->is_recording;
}

EXPORT_API void nugu_recorder_set_userdata(NuguRecorder *rec, void *userdata)
{
	g_return_if_fail(rec != NULL);

	rec->userdata = userdata;
}

EXPORT_API void *nugu_recorder_get_userdata(NuguRecorder *rec)
{
	g_return_val_if_fail(rec != NULL, NULL);

	return rec->userdata;
}

EXPORT_API int nugu_recorder_get_frame_size(NuguRecorder *rec, int *size,
					    int *max)
{
	g_return_val_if_fail(rec != NULL, -1);
	g_return_val_if_fail(rec->buf != NULL, -1);
	g_return_val_if_fail(size != NULL, -1);
	g_return_val_if_fail(max != NULL, -1);

	*size = nugu_ring_buffer_get_item_size(rec->buf);
	*max = nugu_ring_buffer_get_maxcount(rec->buf);

	return 0;
}

EXPORT_API int nugu_recorder_set_frame_size(NuguRecorder *rec, int size,
					    int max)
{
	g_return_val_if_fail(rec != NULL, -1);
	g_return_val_if_fail(rec->buf != NULL, -1);
	g_return_val_if_fail(size > 0, -1);
	g_return_val_if_fail(max > 0, -1);

	return nugu_ring_buffer_resize(rec->buf, size, max);
}

EXPORT_API int nugu_recorder_push_frame(NuguRecorder *rec, const char *data,
					int size)
{
	int ret;

	g_return_val_if_fail(rec != NULL, -1);
	g_return_val_if_fail(rec->buf != NULL, -1);
	g_return_val_if_fail(data != NULL, -1);
	g_return_val_if_fail(size > 0, -1);

#ifdef RECORDER_FILE_DUMP
	fwrite(data, size, 1, rec->file);
#endif
	ret = nugu_ring_buffer_push_data(rec->buf, data, size);

	if (nugu_recorder_get_frame_count(rec))
		_recorder_release_condition(rec);

	return ret;
}

EXPORT_API int nugu_recorder_get_frame(NuguRecorder *rec, char *data, int *size)
{
	g_return_val_if_fail(rec != NULL, -1);
	g_return_val_if_fail(data != NULL, -1);
	g_return_val_if_fail(size != NULL, -1);

	return nugu_ring_buffer_read_item(rec->buf, data, size);
}

EXPORT_API int nugu_recorder_get_frame_timeout(NuguRecorder *rec, char *data,
					       int *size, int timeout)
{
	struct timeval curtime;
	struct timespec spec;
	int status = 0;

	g_return_val_if_fail(rec != NULL, -1);
	g_return_val_if_fail(data != NULL, -1);
	g_return_val_if_fail(size != NULL, -1);

	if (nugu_recorder_get_frame_count(rec) == 0) {
		pthread_mutex_lock(&rec->lock);
		if (timeout) {
			gettimeofday(&curtime, NULL);
			spec.tv_sec = curtime.tv_sec + timeout / 1000;
			spec.tv_nsec = curtime.tv_usec * 1000 + timeout % 1000;
			status = pthread_cond_timedwait(&rec->cond, &rec->lock,
							&spec);
		} else
			pthread_cond_wait(&rec->cond, &rec->lock);
		pthread_mutex_unlock(&rec->lock);
	}

	if (status == ETIMEDOUT)
		nugu_dbg("timeout");

	return nugu_ring_buffer_read_item(rec->buf, data, size);
}

EXPORT_API int nugu_recorder_get_frame_count(NuguRecorder *rec)
{
	g_return_val_if_fail(rec != NULL, 0);

	return nugu_ring_buffer_get_count(rec->buf);
}
