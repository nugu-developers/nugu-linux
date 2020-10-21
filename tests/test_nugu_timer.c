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

#include "base/nugu_timer.h"

#define TIME_UNIT_100MS 100
#define CHECK_SOFT_TIMER_COUNT_FIVE 5
#define CHECK_SOFT_TIMER_COUNT_THREE 3
#define CHECK_SOFT_TIMER_COUNT_ZERO 0
#define CHECK_SOFT_TIMER_COUNT_ONE 1

static void count_timeout(void *param)
{
	int *count = (int *)param;
	(*count)++;
}

static void quit_timeout(void *param)
{
	g_main_loop_quit((GMainLoop *)param);
}

static void test_timer_default(void)
{
	GMainLoop *loop;
	NuguTimer *t1, *t2;
	int t1_count = 0;

	loop = g_main_loop_new(NULL, FALSE);

	t1 = nugu_timer_new(TIME_UNIT_100MS * CHECK_SOFT_TIMER_COUNT_ONE);
	g_assert(t1 != NULL);

	nugu_timer_set_callback(t1, count_timeout, (void *)&t1_count);

	t2 = nugu_timer_new(TIME_UNIT_100MS * CHECK_SOFT_TIMER_COUNT_FIVE);
	g_assert(t2 != NULL);

	nugu_timer_set_callback(t2, quit_timeout, (void *)loop);

	nugu_timer_start(t1);
	nugu_timer_start(t2);

	g_main_loop_run(loop);
	g_main_loop_unref(loop);

	nugu_timer_stop(t1);
	nugu_timer_stop(t2);

	g_assert(t1_count >= CHECK_SOFT_TIMER_COUNT_THREE);

	nugu_timer_delete(t1);
	nugu_timer_delete(t2);
}

static void test_timer_set_zero(void)
{
	GMainLoop *loop;
	NuguTimer *t1, *t2;
	int t1_count = 0;

	loop = g_main_loop_new(NULL, FALSE);

	t1 = nugu_timer_new(TIME_UNIT_100MS * CHECK_SOFT_TIMER_COUNT_ZERO);
	g_assert(t1 != NULL);

	nugu_timer_set_callback(t1, count_timeout, (void *)&t1_count);

	t2 = nugu_timer_new(TIME_UNIT_100MS * CHECK_SOFT_TIMER_COUNT_THREE);
	g_assert(t2 != NULL);

	nugu_timer_set_callback(t2, quit_timeout, (void *)loop);

	nugu_timer_start(t1);
	nugu_timer_start(t2);

	g_main_loop_run(loop);
	g_main_loop_unref(loop);

	nugu_timer_stop(t1);
	nugu_timer_stop(t2);

	g_assert(t1_count == CHECK_SOFT_TIMER_COUNT_ZERO);

	nugu_timer_delete(t1);
	nugu_timer_delete(t2);
}

static void test_timer_set_singleshot(void)
{
	GMainLoop *loop;
	NuguTimer *t1, *t2;
	int t1_count = 0;

	loop = g_main_loop_new(NULL, FALSE);

	t1 = nugu_timer_new(TIME_UNIT_100MS * CHECK_SOFT_TIMER_COUNT_ONE);
	g_assert(t1 != NULL);

	nugu_timer_set_callback(t1, count_timeout, (void *)&t1_count);

	t2 = nugu_timer_new(TIME_UNIT_100MS * CHECK_SOFT_TIMER_COUNT_THREE);
	g_assert(t2 != NULL);

	nugu_timer_set_callback(t2, quit_timeout, (void *)loop);

	nugu_timer_set_singleshot(t1, 1);

	nugu_timer_start(t1);
	nugu_timer_start(t2);

	g_main_loop_run(loop);
	g_main_loop_unref(loop);

	nugu_timer_stop(t1);
	nugu_timer_stop(t2);

	g_assert(t1_count == CHECK_SOFT_TIMER_COUNT_ONE);

	nugu_timer_delete(t1);
	nugu_timer_delete(t2);
}

int main(int argc, char *argv[])
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
	g_type_init();
#endif

	g_test_init(&argc, &argv, NULL);
	g_log_set_always_fatal((GLogLevelFlags)G_LOG_FATAL_MASK);

	g_test_add_func("/timer/timer_default", test_timer_default);
	g_test_add_func("/timer/timer_set_zero", test_timer_set_zero);
	g_test_add_func("/timer/timer_set_singleshot",
			test_timer_set_singleshot);

	return g_test_run();
}
