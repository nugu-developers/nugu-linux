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
	gboolean singleshot;
	NuguTimeoutCallback cb;
	void *userdata;
};

static gboolean _nugu_timer_callback(gpointer userdata)
{
	NuguTimer *timer = (NuguTimer *)userdata;

	timer->cb(timer->userdata);

	return !timer->singleshot;
}

NuguTimer *nugu_timer_new(long interval)
{
	NuguTimer *timer;

	timer = g_malloc0(sizeof(struct _nugu_timer));
	if (!timer) {
		nugu_error_nomem();
		return NULL;
	}

	timer->source = NULL;
	timer->interval = interval;
	timer->singleshot = FALSE;
	timer->cb = NULL;
	timer->userdata = NULL;

	return timer;
}

void nugu_timer_delete(NuguTimer *timer)
{
	g_return_if_fail(timer != NULL);

	if (timer->source) {
		g_source_destroy(timer->source);
		g_source_unref(timer->source);
	}

	memset(timer, 0, sizeof(struct _nugu_timer));
	g_free(timer);
}

void nugu_timer_set_interval(NuguTimer *timer, long interval)
{
	g_return_if_fail(timer != NULL);
	g_return_if_fail(interval > 0);

	timer->interval = interval;
}

long nugu_timer_get_interval(NuguTimer *timer)
{
	g_return_val_if_fail(timer != NULL, -1);

	return timer->interval;
}

void nugu_timer_set_singleshot(NuguTimer *timer, int singleshot)
{
	g_return_if_fail(timer != NULL);
	g_return_if_fail(singleshot >= 0);

	timer->singleshot = singleshot == 0 ? FALSE : TRUE;
}

int nugu_timer_get_singleshot(NuguTimer *timer)
{
	g_return_val_if_fail(timer != NULL, FALSE);

	return timer->singleshot;
}

void nugu_timer_start(NuguTimer *timer)
{
	g_return_if_fail(timer != NULL);
	g_return_if_fail(timer->cb != NULL);
	g_return_if_fail(timer->interval > 0);

	nugu_timer_stop(timer);

	timer->source = g_timeout_source_new(timer->interval);
	g_source_set_callback(timer->source, _nugu_timer_callback,
			      (gpointer)timer, NULL);
	g_source_attach(timer->source, g_main_context_default());
}

void nugu_timer_stop(NuguTimer *timer)
{
	g_return_if_fail(timer != NULL);

	if (timer->source) {
		g_source_destroy(timer->source);
		g_source_unref(timer->source);
		timer->source = NULL;
	}
}

void nugu_timer_set_callback(NuguTimer *timer, NuguTimeoutCallback callback,
			     void *userdata)
{
	g_return_if_fail(timer != NULL);
	g_return_if_fail(callback != NULL);

	timer->cb = callback;
	timer->userdata = userdata;
}
