/*
 * Copyright (c) 2019 SK Telecom Co., Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License")
 * {
 * }
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
#include <string.h>

#include <glib.h>

#include "base/nugu_timer.h"

struct _nugu_timer {
    GSource* source;
    long interval;
    int fake_count;
    gboolean singleshot;
    NuguTimeoutCallback cb;
    void* userdata;
    int start;
};

GList* timers;

static gboolean _nugu_timer_callback(gpointer userdata)
{
    NuguTimer* timer = (NuguTimer*)userdata;

    timer->cb(timer->userdata);

    return !timer->singleshot;
}

static void fake_timer_action(gpointer data, gpointer user_data)
{
    NuguTimer* timer = (NuguTimer*)data;

    if (!timer)
        return;

    if (timer->start == 0)
        return;

    timer->fake_count++;
    if (timer->fake_count >= timer->interval) {
        gboolean ret = _nugu_timer_callback(timer);
        if (!ret)
            timer->start = 0;
        else
            timer->fake_count = 0;
    }
}

void fake_timer_elapse()
{
    g_list_foreach(timers, fake_timer_action, NULL);
}

NuguTimer* nugu_timer_new(long interval)
{
    NuguTimer* timer;

    timer = (NuguTimer*)g_malloc0(sizeof(struct _nugu_timer));
    timer->source = NULL;
    timer->interval = interval;
    timer->fake_count = 0;
    timer->singleshot = FALSE;
    timer->cb = NULL;
    timer->userdata = NULL;
    timer->start = 0;

    return timer;
}

void nugu_timer_delete(NuguTimer* timer)
{
    nugu_timer_stop(timer);
    memset(timer, 0, sizeof(struct _nugu_timer));
    g_free(timer);
}

void nugu_timer_set_interval(NuguTimer* timer, long interval)
{
    timer->interval = interval;
}

long nugu_timer_get_interval(NuguTimer* timer)
{
    return timer->interval;
}

void nugu_timer_set_singleshot(NuguTimer* timer, int singleshot)
{
    timer->singleshot = singleshot == 0 ? FALSE : TRUE;
}

int nugu_timer_get_singleshot(NuguTimer* timer)
{
    return timer->singleshot;
}

void nugu_timer_start(NuguTimer* timer)
{
    nugu_timer_stop(timer);
    timer->start = 1;

    timers = g_list_append(timers, timer);
}

void nugu_timer_stop(NuguTimer* timer)
{
    timer->fake_count = 0;

    timers = g_list_remove(timers, timer);
}

void nugu_timer_set_callback(NuguTimer* timer, NuguTimeoutCallback callback,
    void* userdata)
{
    timer->cb = callback;
    timer->userdata = userdata;
}