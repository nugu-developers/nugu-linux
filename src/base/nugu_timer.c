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

#include <string.h>
#include <stdio.h>

#include <glib.h>

#include "base/nugu_log.h"
#include "base/nugu_timer.h"

struct _nugu_timer {
	GSource *source;
	long interval;
	int repeat;
	int count;
	NuguTimeoutCallback cb;
	void *userdata;
};

static gboolean _nugu_timer_callback(gpointer userdata)
{
	NuguTimer *timer = (NuguTimer *)userdata;

	timer->count++;
	timer->cb(timer->userdata);

	if (timer->repeat > timer->count)
		return TRUE;
	else
		return FALSE;
}

EXPORT_API NuguTimer *nugu_timer_new(long interval, int repeat)
{
	NuguTimer *timer;

	timer = g_malloc0(sizeof(struct _nugu_timer));
	if (!timer) {
		nugu_error_nomem();
		return NULL;
	}

	timer->source = NULL;
	timer->interval = interval;
	timer->repeat = repeat;
	timer->count = 0;
	timer->cb = NULL;
	timer->userdata = NULL;

	return timer;
}

EXPORT_API void nugu_timer_delete(NuguTimer *timer)
{
	g_return_if_fail(timer != NULL);

	if (timer->source) {
		g_source_destroy(timer->source);
		g_source_unref(timer->source);
	}

	memset(timer, 0, sizeof(struct _nugu_timer));
	g_free(timer);
}

EXPORT_API void nugu_timer_set_interval(NuguTimer *timer, long interval)
{
	g_return_if_fail(timer != NULL);
	g_return_if_fail(interval > 0);

	timer->interval = interval;
}

EXPORT_API long nugu_timer_get_interval(NuguTimer *timer)
{
	g_return_val_if_fail(timer != NULL, -1);

	return timer->interval;
}

EXPORT_API void nugu_timer_set_repeat(NuguTimer *timer, int repeat)
{
	g_return_if_fail(timer != NULL);
	g_return_if_fail(repeat >= 0);

	timer->repeat = repeat;
}

EXPORT_API int nugu_timer_get_repeat(NuguTimer *timer)
{
	g_return_val_if_fail(timer != NULL, -1);

	return timer->repeat;
}

EXPORT_API int nugu_timer_get_count(NuguTimer *timer)
{
	g_return_val_if_fail(timer != NULL, -1);

	return timer->count;
}

EXPORT_API void nugu_timer_start(NuguTimer *timer)
{
	g_return_if_fail(timer != NULL);
	g_return_if_fail(timer->cb != NULL);

	if (!timer->repeat)
		return;

	nugu_timer_stop(timer);

	timer->source = g_timeout_source_new(timer->interval);
	g_source_set_callback(timer->source, _nugu_timer_callback,
			      (gpointer)timer, NULL);
	g_source_attach(timer->source, g_main_context_default());
}

EXPORT_API void nugu_timer_stop(NuguTimer *timer)
{
	g_return_if_fail(timer != NULL);

	if (timer->source) {
		g_source_destroy(timer->source);
		g_source_unref(timer->source);
		timer->source = NULL;
	}

	timer->count = 0;
}

EXPORT_API void nugu_timer_set_callback(NuguTimer *timer,
					NuguTimeoutCallback callback,
					void *userdata)
{
	g_return_if_fail(timer != NULL);
	g_return_if_fail(callback != NULL);

	timer->cb = callback;
	timer->userdata = userdata;
}
