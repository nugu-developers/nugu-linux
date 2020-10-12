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
#include "nugu_timer.hh"

using namespace NuguCore;

extern void fake_timer_start();
extern void fake_timer_elapse();

#define TIMER_ELAPSE_SEC(t)                              \
    {                                                    \
        for (int elap_sec = 0; elap_sec < t; elap_sec++) \
            fake_timer_elapse();                         \
    }

#define TIMEOUT_UNIT_SEC (1 * NUGU_TIMER_UNIT_SEC)

typedef struct {
    NUGUTimer* timer1;
    NUGUTimer* timer2;
} ntimerFixture;

static void setup(ntimerFixture* fixture, gconstpointer user_data)
{
    fake_timer_start();

    fixture->timer1 = new NUGUTimer();
    fixture->timer1->setInterval(TIMEOUT_UNIT_SEC);
    fixture->timer1->setRepeat(1);

    fixture->timer2 = new NUGUTimer();
    fixture->timer2->setInterval(TIMEOUT_UNIT_SEC);
    fixture->timer2->setRepeat(1);
}

static void teardown(ntimerFixture* fixture, gconstpointer user_data)
{
    delete fixture->timer1;
    delete fixture->timer2;
}

#define G_TEST_ADD_FUNC(name, func) \
    g_test_add(name, ntimerFixture, NULL, setup, func, teardown);

static void test_nugutimer_trigger(ntimerFixture* fixture, gconstpointer ignored)
{
    NUGUTimer* timer = fixture->timer1;

    timer->setCallback([&](int count, int repeat) {
        g_assert(count == 1 && repeat == 1);
    });

    /* Trigger Timer Once after 1 Second */
    timer->setInterval(TIMEOUT_UNIT_SEC);
    timer->start();
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
    g_assert(timer->getCount() == 1);

    /* Trigger Timer Once after 2 Seconds */
    timer->setInterval(TIMEOUT_UNIT_SEC * 2);
    timer->start();
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
    g_assert(timer->getCount() == 0);
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
    g_assert(timer->getCount() == 1);
}

static void test_nugutimer_repeat(ntimerFixture* fixture, gconstpointer ignored)
{
    NUGUTimer* timer = fixture->timer1;
    int expect_repeat = 1;
    int expect_count = 1;

    timer->setCallback([&](int count, int repeat) {
        g_assert(count == expect_count && repeat == expect_repeat);
    });

    /* Trigger Timer just Once after 2 Second */
    expect_repeat = 1;
    timer->setInterval(TIMEOUT_UNIT_SEC);
    timer->setRepeat(expect_repeat);
    timer->start();

    expect_count = 1;
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
    g_assert(timer->getCount() == (unsigned int)expect_count);
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
    g_assert(timer->getCount() == (unsigned int)expect_count);

    /* Trigger Timer Twice Once after 3 Seconds */
    expect_repeat = 2;
    timer->setInterval(TIMEOUT_UNIT_SEC);
    timer->setRepeat(expect_repeat);
    timer->start();

    expect_count = 1;
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
    g_assert(timer->getCount() == (unsigned int)expect_count);
    expect_count = 2;
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
    g_assert(timer->getCount() == (unsigned int)expect_count);
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
    g_assert(timer->getCount() == (unsigned int)expect_count);
}

static void test_nugutimer_loop(ntimerFixture* fixture, gconstpointer ignored)
{
    NUGUTimer* timer = fixture->timer1;
    int expect_repeat = 1;
    int expect_count = 1;

    timer->setCallback([&](int count, int repeat) {
        g_assert(count == expect_count && repeat == expect_repeat);
    });

    /* Trigger Timer every time */
    expect_repeat = 1;
    timer->setInterval(TIMEOUT_UNIT_SEC);
    timer->setRepeat(expect_repeat);
    timer->setLoop(true);
    timer->start();

    expect_count = 1;
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
    g_assert(timer->getCount() == (unsigned int)expect_count);
    expect_count = 2;
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
    g_assert(timer->getCount() == (unsigned int)expect_count);
    expect_count = 3;
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
    g_assert(timer->getCount() == (unsigned int)expect_count);
    expect_count = 4;
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
    g_assert(timer->getCount() == (unsigned int)expect_count);
    expect_count = 5;
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
    g_assert(timer->getCount() == (unsigned int)expect_count);
}

static void test_nugutimer_multi_trigger(ntimerFixture* fixture, gconstpointer ignored)
{
    NUGUTimer* timer1 = fixture->timer1;
    NUGUTimer* timer2 = fixture->timer2;

    timer1->setCallback([&](int count, int repeat) {
        g_assert(count == 1 && repeat == 1);
    });
    timer2->setCallback([&](int count, int repeat) {
        g_assert(count == 1 && repeat == 1);
    });
    /* Trigger Two Timer Once after 1 Second */
    timer1->setInterval(TIMEOUT_UNIT_SEC);
    timer1->start();
    timer2->setInterval(TIMEOUT_UNIT_SEC);
    timer2->start();
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
    g_assert(timer1->getCount() == 1);
    g_assert(timer2->getCount() == 1);

    /* Trigger Two Timer Once after 2 Seconds */
    timer1->setInterval(TIMEOUT_UNIT_SEC * 2);
    timer1->start();
    timer2->setInterval(TIMEOUT_UNIT_SEC * 2);
    timer2->start();
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
    g_assert(timer1->getCount() == 0);
    g_assert(timer2->getCount() == 0);
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
    g_assert(timer1->getCount() == 1);
    g_assert(timer2->getCount() == 1);
}

static void test_nugutimer_multi_repeat(ntimerFixture* fixture, gconstpointer ignored)
{
    NUGUTimer* timer1 = fixture->timer1;
    NUGUTimer* timer2 = fixture->timer2;

    int expect_repeat1 = 1;
    int expect_count1 = 1;

    int expect_repeat2 = 1;
    int expect_count2 = 1;

    timer1->setCallback([&](int count, int repeat) {
        g_assert(count == expect_count1 && repeat == expect_repeat1);
    });

    timer2->setCallback([&](int count, int repeat) {
        g_assert(count == expect_count2 && repeat == expect_repeat2);
    });

    /* Trigger Two Timer just Once after 2 Seconds */
    expect_repeat1 = 1;
    timer1->setInterval(TIMEOUT_UNIT_SEC);
    timer1->setRepeat(expect_repeat1);
    timer1->start();

    expect_repeat2 = 1;
    timer2->setInterval(TIMEOUT_UNIT_SEC);
    timer2->setRepeat(expect_repeat2);
    timer2->start();

    expect_count1 = 1;
    expect_count2 = 1;
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
    g_assert(timer1->getCount() == (unsigned int)expect_count1);
    g_assert(timer2->getCount() == (unsigned int)expect_count2);
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
    g_assert(timer1->getCount() == (unsigned int)expect_count1);
    g_assert(timer2->getCount() == (unsigned int)expect_count2);

    /* Trigger Timer1 Twice and Timer2 Once after 3 Seconds */
    expect_repeat1 = 1;
    timer1->setInterval(TIMEOUT_UNIT_SEC);
    timer1->setRepeat(expect_repeat1);
    timer1->start();

    expect_repeat2 = 2;
    timer2->setInterval(TIMEOUT_UNIT_SEC);
    timer2->setRepeat(expect_repeat2);
    timer2->start();

    expect_count1 = 1;
    expect_count2 = 1;
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
    g_assert(timer1->getCount() == (unsigned int)expect_count1);
    g_assert(timer2->getCount() == (unsigned int)expect_count2);
    expect_count2 = 2;
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
    g_assert(timer1->getCount() == (unsigned int)expect_count1);
    g_assert(timer2->getCount() == (unsigned int)expect_count2);
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
    g_assert(timer1->getCount() == (unsigned int)expect_count1);
    g_assert(timer2->getCount() == (unsigned int)expect_count2);
}

int main(int argc, char* argv[])
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
    g_type_init();
#endif

    g_test_init(&argc, &argv, (void*)NULL);
    g_log_set_always_fatal((GLogLevelFlags)G_LOG_FATAL_MASK);

    G_TEST_ADD_FUNC("/core/NuguTimer/Trigger", test_nugutimer_trigger);
    G_TEST_ADD_FUNC("/core/NuguTimer/Repeat", test_nugutimer_repeat);
    G_TEST_ADD_FUNC("/core/NuguTimer/Loop", test_nugutimer_loop);
    G_TEST_ADD_FUNC("/core/NuguTimer/MultiTrigger", test_nugutimer_multi_trigger);
    G_TEST_ADD_FUNC("/core/NuguTimer/MultiRepeat", test_nugutimer_multi_repeat);

    return g_test_run();
}
