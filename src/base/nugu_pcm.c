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
#include "base/nugu_buffer.h"
#include "base/nugu_pcm.h"

struct _nugu_pcm_driver {
	char *name;
	struct nugu_pcm_driver_ops *ops;
	int ref_count;
};

struct _nugu_pcm {
	char *name;
	NuguPcmDriver *driver;
	enum nugu_media_status status;
	NuguAudioProperty property;
	NuguMediaEventCallback ecb;
	NuguMediaStatusCallback scb;
	void *eud; /* user data for event callback */
	void *sud; /* user data for status callback */
	void *dud; /* user data for driver */
	NuguBuffer *buf;
	int is_last;
	int volume;
	size_t total_size;

	pthread_mutex_t mutex;
};

static GList *_pcms;
static GList *_pcm_drivers;
static NuguPcmDriver *_default_driver;

EXPORT_API NuguPcmDriver *nugu_pcm_driver_new(const char *name,
					      struct nugu_pcm_driver_ops *ops)
{
	NuguPcmDriver *driver;

	g_return_val_if_fail(name != NULL, NULL);
	g_return_val_if_fail(ops != NULL, NULL);

	driver = g_malloc0(sizeof(struct _nugu_pcm_driver));
	if (!driver) {
		nugu_error_nomem();
		return NULL;
	}

	driver->name = g_strdup(name);
	driver->ops = ops;
	driver->ref_count = 0;

	return driver;
}

EXPORT_API int nugu_pcm_driver_free(NuguPcmDriver *driver)
{
	g_return_val_if_fail(driver != NULL, -1);

	if (driver->ref_count != 0) {
		nugu_error("pcm still using driver");
		return -1;
	}

	if (_default_driver == driver)
		_default_driver = NULL;

	g_free(driver->name);

	memset(driver, 0, sizeof(struct _nugu_pcm_driver));
	g_free(driver);

	return 0;
}

EXPORT_API int nugu_pcm_driver_register(NuguPcmDriver *driver)
{
	g_return_val_if_fail(driver != NULL, -1);

	if (nugu_pcm_driver_find(driver->name)) {
		nugu_error("'%s' pcm driver already exist.", driver->name);
		return -1;
	}

	_pcm_drivers = g_list_append(_pcm_drivers, driver);
	if (!_default_driver)
		nugu_pcm_driver_set_default(driver);
	return 0;
}

EXPORT_API int nugu_pcm_driver_remove(NuguPcmDriver *driver)
{
	GList *l;

	g_return_val_if_fail(driver != NULL, -1);

	l = g_list_find(_pcm_drivers, driver);
	if (!l)
		return -1;

	if (_default_driver == driver)
		_default_driver = NULL;

	_pcm_drivers = g_list_remove_link(_pcm_drivers, l);

	if (_pcm_drivers && _default_driver == NULL)
		_default_driver = _pcm_drivers->data;

	return 0;
}

EXPORT_API int nugu_pcm_driver_set_default(NuguPcmDriver *driver)
{
	g_return_val_if_fail(driver != NULL, -1);

	_default_driver = driver;

	return 0;
}

EXPORT_API NuguPcmDriver *nugu_pcm_driver_get_default(void)
{
	return _default_driver;
}

EXPORT_API NuguPcmDriver *nugu_pcm_driver_find(const char *name)
{
	GList *cur;

	g_return_val_if_fail(name != NULL, NULL);

	cur = _pcm_drivers;
	while (cur) {
		if (g_strcmp0(((NuguPcmDriver *)cur->data)->name, name) == 0)
			return cur->data;

		cur = cur->next;
	}

	return NULL;
}

EXPORT_API NuguPcm *nugu_pcm_new(const char *name, NuguPcmDriver *driver)
{
	NuguPcm *pcm;

	g_return_val_if_fail(name != NULL, NULL);
	g_return_val_if_fail(driver != NULL, NULL);

	pcm = g_malloc0(sizeof(struct _nugu_pcm));
	if (!pcm) {
		nugu_error_nomem();
		return NULL;
	}

	pcm->name = g_strdup(name);
	pcm->driver = driver;
	pcm->buf = nugu_buffer_new(0);
	pcm->is_last = 0;
	pcm->scb = NULL;
	pcm->ecb = NULL;
	pcm->sud = NULL;
	pcm->eud = NULL;
	pcm->dud = NULL;
	pcm->status = NUGU_MEDIA_STATUS_STOPPED;
	pcm->volume = NUGU_SET_VOLUME_MAX;
	pcm->total_size = 0;

	if (pcm->buf == NULL) {
		nugu_error("buffer new is internal error");
		g_free(pcm->name);
		free(pcm);
		return NULL;
	}

	pcm->driver->ref_count++;

	pthread_mutex_init(&pcm->mutex, NULL);

	return pcm;
}

EXPORT_API void nugu_pcm_free(NuguPcm *pcm)
{
	g_return_if_fail(pcm != NULL);
	g_return_if_fail(pcm->driver != NULL);

	pcm->driver->ref_count--;

	g_free(pcm->name);
	nugu_buffer_free(pcm->buf, 1);

	pthread_mutex_destroy(&pcm->mutex);

	memset(pcm, 0, sizeof(struct _nugu_pcm));
	g_free(pcm);
}

EXPORT_API int nugu_pcm_add(NuguPcm *pcm)
{
	g_return_val_if_fail(pcm != NULL, -1);

	if (nugu_pcm_find(pcm->name)) {
		nugu_error("'%s' pcm already exist.", pcm->name);
		return -1;
	}

	_pcms = g_list_append(_pcms, pcm);

	return 0;
}

EXPORT_API int nugu_pcm_remove(NuguPcm *pcm)
{
	GList *l;

	l = g_list_find(_pcms, pcm);
	if (!l)
		return -1;

	_pcms = g_list_remove_link(_pcms, l);

	return 0;
}

EXPORT_API NuguPcm *nugu_pcm_find(const char *name)
{
	GList *cur;

	g_return_val_if_fail(name != NULL, NULL);

	cur = _pcms;
	while (cur) {
		if (g_strcmp0(((NuguPcm *)cur->data)->name, name) == 0)
			return cur->data;

		cur = cur->next;
	}

	return NULL;
}

EXPORT_API int nugu_pcm_set_property(NuguPcm *pcm, NuguAudioProperty property)
{
	g_return_val_if_fail(pcm != NULL, -1);

	memcpy(&pcm->property, &property, sizeof(NuguAudioProperty));

	return 0;
}

EXPORT_API int nugu_pcm_start(NuguPcm *pcm)
{
	g_return_val_if_fail(pcm != NULL, -1);
	g_return_val_if_fail(pcm->driver != NULL, -1);

	if (pcm->driver->ops->start == NULL) {
		nugu_error("Not supported");
		return -1;
	}
	nugu_pcm_clear_buffer(pcm);

	return pcm->driver->ops->start(pcm->driver, pcm, pcm->property);
}

EXPORT_API int nugu_pcm_stop(NuguPcm *pcm)
{
	g_return_val_if_fail(pcm != NULL, -1);
	g_return_val_if_fail(pcm->driver != NULL, -1);

	if (pcm->driver->ops->stop == NULL) {
		nugu_error("Not supported");
		return -1;
	}
	nugu_pcm_clear_buffer(pcm);

	return pcm->driver->ops->stop(pcm->driver, pcm);
}

EXPORT_API int nugu_pcm_pause(NuguPcm *pcm)
{
	g_return_val_if_fail(pcm != NULL, -1);
	g_return_val_if_fail(pcm->driver != NULL, -1);

	if (pcm->driver->ops->pause == NULL) {
		nugu_error("Not supported");
		return -1;
	}

	return pcm->driver->ops->pause(pcm->driver, pcm);
}

EXPORT_API int nugu_pcm_resume(NuguPcm *pcm)
{
	g_return_val_if_fail(pcm != NULL, -1);
	g_return_val_if_fail(pcm->driver != NULL, -1);

	if (pcm->driver->ops->resume == NULL) {
		nugu_error("Not supported");
		return -1;
	}

	return pcm->driver->ops->resume(pcm->driver, pcm);
}

EXPORT_API int nugu_pcm_set_volume(NuguPcm *pcm, int volume)
{
	g_return_val_if_fail(pcm != NULL, -1);

	if (volume < NUGU_SET_VOLUME_MIN)
		pcm->volume = NUGU_SET_VOLUME_MIN;
	else if (volume > NUGU_SET_VOLUME_MAX)
		pcm->volume = NUGU_SET_VOLUME_MAX;
	else
		pcm->volume = volume;

	nugu_dbg("change volume: %d", pcm->volume);

	return 0;
}

EXPORT_API int nugu_pcm_get_volume(NuguPcm *pcm)
{
	g_return_val_if_fail(pcm != NULL, -1);

	return pcm->volume;
}

EXPORT_API int nugu_pcm_get_duration(NuguPcm *pcm)
{
	g_return_val_if_fail(pcm != NULL, -1);
	int samplerate;

	switch (pcm->property.samplerate) {
	case NUGU_AUDIO_SAMPLE_RATE_8K:
		samplerate = 8000;
		break;
	case NUGU_AUDIO_SAMPLE_RATE_16K:
		samplerate = 16000;
		break;
	case NUGU_AUDIO_SAMPLE_RATE_32K:
		samplerate = 32000;
		break;
	case NUGU_AUDIO_SAMPLE_RATE_22K:
		samplerate = 22050;
		break;
	case NUGU_AUDIO_SAMPLE_RATE_44K:
		samplerate = 44100;
		break;
	default:
		samplerate = 16000;
		break;
	}

	return (pcm->total_size / samplerate);
}

EXPORT_API int nugu_pcm_get_position(NuguPcm *pcm)
{
	g_return_val_if_fail(pcm != NULL, -1);
	g_return_val_if_fail(pcm->driver != NULL, -1);

	if (pcm->driver->ops->get_position == NULL) {
		nugu_error("Not supported");
		return -1;
	}

	return pcm->driver->ops->get_position(pcm->driver, pcm);
}

EXPORT_API void nugu_pcm_set_status_callback(NuguPcm *pcm,
					     NuguMediaStatusCallback cb,
					     void *userdata)
{
	g_return_if_fail(pcm != NULL);

	pcm->scb = cb;
	pcm->sud = userdata;
}

EXPORT_API void nugu_pcm_emit_status(NuguPcm *pcm,
				     enum nugu_media_status status)
{
	g_return_if_fail(pcm != NULL);

	if (pcm->status == status)
		return;

	pcm->status = status;

	if (pcm->scb != NULL)
		pcm->scb(status, pcm->sud);
}

EXPORT_API enum nugu_media_status nugu_pcm_get_status(NuguPcm *pcm)
{
	g_return_val_if_fail(pcm != NULL, NUGU_MEDIA_STATUS_STOPPED);

	return pcm->status;
}

EXPORT_API void nugu_pcm_set_event_callback(NuguPcm *pcm,
					    NuguMediaEventCallback cb,
					    void *userdata)
{
	g_return_if_fail(pcm != NULL);

	pcm->ecb = cb;
	pcm->eud = userdata;
}

EXPORT_API void nugu_pcm_emit_event(NuguPcm *pcm, enum nugu_media_event event)
{
	g_return_if_fail(pcm != NULL);

	if (event == NUGU_MEDIA_EVENT_END_OF_STREAM)
		pcm->status = NUGU_MEDIA_STATUS_STOPPED;

	if (pcm->ecb != NULL)
		pcm->ecb(event, pcm->eud);
}

EXPORT_API void nugu_pcm_set_userdata(NuguPcm *pcm, void *userdata)
{
	g_return_if_fail(pcm != NULL);

	pcm->dud = userdata;
}

EXPORT_API void *nugu_pcm_get_userdata(NuguPcm *pcm)
{
	g_return_val_if_fail(pcm != NULL, NULL);

	return pcm->dud;
}

EXPORT_API void nugu_pcm_clear_buffer(NuguPcm *pcm)
{
	g_return_if_fail(pcm != NULL);
	g_return_if_fail(pcm->buf != NULL);

	pthread_mutex_lock(&pcm->mutex);

	nugu_buffer_clear(pcm->buf);
	pcm->total_size = 0;
	pcm->is_last = 0;

	pthread_mutex_unlock(&pcm->mutex);
}

EXPORT_API int nugu_pcm_push_data(NuguPcm *pcm, const char *data, size_t size,
				  int is_last)
{
	int ret;

	g_return_val_if_fail(pcm != NULL, -1);
	g_return_val_if_fail(pcm->buf != NULL, -1);
	g_return_val_if_fail(data != NULL, -1);
	g_return_val_if_fail(size > 0, -1);
	g_return_val_if_fail(pcm->driver != NULL, -1);

	pthread_mutex_lock(&pcm->mutex);

	pcm->total_size += size;
	if (!pcm->is_last)
		pcm->is_last = is_last;

	ret = nugu_buffer_add(pcm->buf, (void *)data, size);

	pthread_mutex_unlock(&pcm->mutex);

	if (pcm->driver->ops->push_data)
		pcm->driver->ops->push_data(pcm->driver, pcm, data, size,
					    pcm->is_last);
	return ret;
}

EXPORT_API int nugu_pcm_push_data_done(NuguPcm *pcm)
{
	g_return_val_if_fail(pcm != NULL, -1);
	g_return_val_if_fail(pcm->driver != NULL, -1);

	pthread_mutex_lock(&pcm->mutex);

	pcm->is_last = 1;

	pthread_mutex_unlock(&pcm->mutex);

	if (pcm->driver->ops->push_data)
		pcm->driver->ops->push_data(pcm->driver, pcm, NULL, 0, 1);

	return 0;
}

EXPORT_API int nugu_pcm_get_data(NuguPcm *pcm, char *data, size_t size)
{
	size_t data_size;
	const char *ptr;
	float vol;
	int ret;
	size_t i;

	g_return_val_if_fail(pcm != NULL, -1);
	g_return_val_if_fail(pcm->buf != NULL, -1);
	g_return_val_if_fail(data != NULL, -1);
	g_return_val_if_fail(size != 0, -1);

	pthread_mutex_lock(&pcm->mutex);

	data_size = nugu_buffer_get_size(pcm->buf);
	if (data_size < size)
		size = data_size;

	ptr = nugu_buffer_peek(pcm->buf);
	if (ptr == NULL) {
		pthread_mutex_unlock(&pcm->mutex);

		nugu_error("buffer peek is internal error");
		return -1;
	}

	vol = (float)pcm->volume / NUGU_SET_VOLUME_MAX;

	for (i = 0; i < size; i++)
		*(data + i) = *(ptr + i) * vol;

	ret = nugu_buffer_shift_left(pcm->buf, size);
	if (ret == -1) {
		pthread_mutex_unlock(&pcm->mutex);

		nugu_error("buffer shift left is internal error");
		return -1;
	}

	pthread_mutex_unlock(&pcm->mutex);

	return size;
}

EXPORT_API size_t nugu_pcm_get_data_size(NuguPcm *pcm)
{
	size_t ret;

	g_return_val_if_fail(pcm != NULL, -1);

	pthread_mutex_lock(&pcm->mutex);

	ret = nugu_buffer_get_size(pcm->buf);

	pthread_mutex_unlock(&pcm->mutex);

	return ret;
}

EXPORT_API int nugu_pcm_receive_is_last_data(NuguPcm *pcm)
{
	g_return_val_if_fail(pcm != NULL, -1);

	return pcm->is_last;
}
