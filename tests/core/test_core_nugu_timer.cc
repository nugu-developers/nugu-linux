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
#include "mock/nugu_timer_mock.h"
#include "nugu_timer.hh"

using namespace NuguCore;

#define TIMER_ELAPSE_SEC(t)                              \
    {                                                    \
        for (int elap_sec = 0; elap_sec < t; elap_sec++) \
            fake_timer_elapse();                         \
    }

#define TIMEOUT_UNIT_SEC (1 * NUGU_TIMER_UNIT_SEC)

typedef struct _ntimerFixture {
    NUGUTimer* timer1;
    NUGUTimer* timer2;
} ntimerFixture;

static void setup(ntimerFixture* fixture, gconstpointer user_data)
{
    fixture->timer1 = new NUGUTimer();
    fixture->timer1->setInterval(TIMEOUT_UNIT_SEC);

    fixture->timer2 = new NUGUTimer();
    fixture->timer2->setInterval(TIMEOUT_UNIT_SEC);
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
    int count = 0;
    int expect_count = 0;

    timer->setCallback([&]() {
        count++;
        g_assert(count == expect_count);
    });

    /* Trigger Timer Once after 2 Seconds */
    timer->setInterval(TIMEOUT_UNIT_SEC * 2);
    timer->start();
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
    expect_count = 1;
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
    expect_count = 2;
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
}

static void test_nugutimer_single_shot(ntimerFixture* fixture, gconstpointer ignored)
{
    NUGUTimer* timer = fixture->timer1;
    int count = 0;
    int expect_count = 0;

    timer->setCallback([&]() {
        count++;
        g_assert(count == expect_count);
    });

    /* Trigger Timer Just Once during 5 Seconds */
    timer->setInterval(TIMEOUT_UNIT_SEC);
    timer->setSingleShot(1);
    timer->start();

    expect_count = 1;
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
}

static void test_nugutimer_multi_trigger(ntimerFixture* fixture, gconstpointer ignored)
{
    NUGUTimer* timer1 = fixture->timer1;
    NUGUTimer* timer2 = fixture->timer2;
    int count1 = 0;
    int expect_count1 = 0;
    int count2 = 0;
    int expect_count2 = 0;

    timer1->setCallback([&]() {
        count1++;
        g_assert(count1 == expect_count1);
    });
    timer2->setCallback([&]() {
        count2++;
        g_assert(count2 == expect_count2);
    });

    /* Trigger 4 times(Timer1) and 2 times(Timer2) after 4 Second */
    timer1->setInterval(TIMEOUT_UNIT_SEC);
    timer1->start();
    timer2->setInterval(TIMEOUT_UNIT_SEC * 2);
    timer2->start();
    expect_count1 = 1;
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
    expect_count1 = 2;
    expect_count2 = 1;
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
    expect_count1 = 3;
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
    expect_count1 = 4;
    expect_count2 = 2;
    TIMER_ELAPSE_SEC(TIMEOUT_UNIT_SEC);
}

int main(int argc, char* argv[])
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
    g_type_init();
#endif

    g_test_init(&argc, &argv, (void*)NULL);
    g_log_set_always_fatal((GLogLevelFlags)G_LOG_FATAL_MASK);

    G_TEST_ADD_FUNC("/core/NuguTimer/Trigger", test_nugutimer_trigger);
    G_TEST_ADD_FUNC("/core/NuguTimer/SingleShot", test_nugutimer_single_shot);
    G_TEST_ADD_FUNC("/core/NuguTimer/MultiTrigger", test_nugutimer_multi_trigger);

    return g_test_run();
}
