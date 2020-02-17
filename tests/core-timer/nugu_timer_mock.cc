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
    int repeat;
    int count;
    NuguTimeoutCallback cb;
    void* userdata;
    int start;
};

GList* timers;
static int fake_timer_count;

static gboolean _nugu_timer_callback(gpointer userdata)
{
    NuguTimer* timer = (NuguTimer*)userdata;

    timer->count++;
    timer->cb(timer->userdata);

    if (timer->repeat > timer->count)
        return TRUE;
    else
        return FALSE;
}

static void fake_timer_action(gpointer data, gpointer user_data)
{
    NuguTimer* timer = (NuguTimer*)data;

    if (!timer)
        return;

    if (timer->start == 0)
        return;

    if (timer->repeat < timer->count)
        return;

    timer->fake_count++;
    int interval = timer->interval / 1000;

    if (timer->fake_count >= interval) {
        if (_nugu_timer_callback(timer) == FALSE)
            timer->start = 0;
        timer->fake_count = 0;
    }
}

void fake_timer_start()
{
    fake_timer_count = 0;
}

void fake_timer_elapse()
{
    g_list_foreach(timers, fake_timer_action, NULL);
}

NuguTimer* nugu_timer_new(long interval, int repeat)
{
    NuguTimer* timer;

    timer = (NuguTimer*)g_malloc0(sizeof(struct _nugu_timer));
    timer->source = NULL;
    timer->interval = interval;
    timer->repeat = repeat;
    timer->fake_count = 0;
    timer->count = 0;
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

void nugu_timer_set_repeat(NuguTimer* timer, int repeat)
{
    timer->repeat = repeat;
}

int nugu_timer_get_repeat(NuguTimer* timer)
{
    return timer->repeat;
}

int nugu_timer_get_count(NuguTimer* timer)
{
    return timer->count;
}

void nugu_timer_start(NuguTimer* timer)
{
    if (!timer->repeat)
        return;

    nugu_timer_stop(timer);
    timer->start = 1;

    timers = g_list_append(timers, timer);
}

void nugu_timer_stop(NuguTimer* timer)
{
    timer->count = 0;
    timer->fake_count = 0;
    timer->start = 0;

    timers = g_list_remove(timers, timer);
}

void nugu_timer_set_callback(NuguTimer* timer, NuguTimeoutCallback callback,
    void* userdata)
{
    timer->cb = callback;
    timer->userdata = userdata;
}