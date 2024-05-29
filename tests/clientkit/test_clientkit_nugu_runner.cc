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

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <pthread.h>

#include "clientkit/nugu_runner.hh"

using namespace NuguClientKit;

#define TIMER_1S 1
#define TIMER_500MS 500
#define TIMER_100MS 100
#define TIMEOUT_IMMEDIATELY 1

#ifdef _WIN32
void usleep(unsigned int usec)
{
    Sleep(usec / 1000); // msec
}
#endif

class TestExecutor : public NuguRunner {
public:
    TestExecutor() = default;
    ~TestExecutor() = default;

    void neverCallFunction()
    {
        g_assert(false);
    }

    bool callBoolFunction()
    {
        return true;
    }

    int callIntegerFunction()
    {
        return 1;
    }

    std::string callStringFunction()
    {
        return "ok";
    }

    bool runMethod(ExecuteType type, int timeout = 0)
    {
        ret_bool = false;

        std::string tag;

        switch (type) {
        case ExecuteType::Auto:
            tag = __func__ + std::string("_") + "Auto";
            break;
        case ExecuteType::Queued:
            tag = __func__ + std::string("_") + "Queued";
            break;
        case ExecuteType::Blocking:
            tag = __func__ + std::string("_") + "Blocking";
            break;
        }

        if (!invokeMethod(
                tag, [&, type, timeout]() {
                    // if invokeMethod is released by timeout, this method should not be called.
                    if (type == ExecuteType::Blocking && timeout == TIMEOUT_IMMEDIATELY)
                        neverCallFunction();

                    ret_bool = callBoolFunction();
                },
                type, timeout))
            return false;

        if (type == ExecuteType::Blocking)
            return ret_bool;
        else
            return true;
    }

    bool runMethodReturnBool()
    {
        return runMethod(ExecuteType::Blocking);
    }

    int runMethodReturnInteger()
    {
        ret_int = -1;

        if (!invokeMethod(
                __func__, [&]() {
                    ret_int = callIntegerFunction();
                },
                ExecuteType::Blocking))
            return -1;

        return ret_int;
    }

    std::string runMethodReturnString()
    {
        ret_str = "";

        if (!invokeMethod(
                __func__, [&]() {
                    ret_str = callStringFunction();
                },
                ExecuteType::Blocking))
            return "";

        return ret_str;
    }

    void runMethodWithSleep()
    {
        invokeMethod(
            __func__, [&]() {
                // delay for 1ms
                usleep(1 * 1000);
            },
            ExecuteType::Auto);
    }

public:
    bool ret_bool = false;
    int ret_int = 0;
    std::string ret_str;
};

typedef struct _nuguFixture {
    GMainLoop* loop;
} nuguFixture;

static void setup(nuguFixture* fixture, gconstpointer user_data)
{
    fixture->loop = g_main_loop_new(NULL, FALSE);
}

static void teardown(nuguFixture* fixture, gconstpointer user_data)
{
    g_main_loop_unref(fixture->loop);
}

#define G_TEST_ADD_FUNC(name, func) \
    g_test_add(name, nuguFixture, NULL, setup, func, teardown);

static gboolean _execute_method_runner_with_queue_on_nuguloop(gpointer userdata)
{
    TestExecutor test_executor;
    GMainLoop* loop = (GMainLoop*)userdata;

    g_assert(test_executor.ret_bool == false);
    // should be executed next idle time
    g_assert(test_executor.runMethod(ExecuteType::Queued) == true);
    g_assert(test_executor.ret_bool == false);

    // terminate loop before the job is executed.
    g_main_loop_quit(loop);
    return FALSE;
}

static gboolean _execute_method_runner_with_auto_on_nuguloop(gpointer userdata)
{
    TestExecutor test_executor;
    GMainLoop* loop = (GMainLoop*)userdata;

    g_assert(test_executor.ret_bool == false);
    // should be executed immediately
    g_assert(test_executor.runMethod(ExecuteType::Auto) == true);
    g_assert(test_executor.ret_bool == true);

    // should be executed immediately
    g_assert(test_executor.runMethod(ExecuteType::Blocking) == true);

    g_main_loop_quit(loop);
    return FALSE;
}

static void* _execute_method_runner_on_another_thread(gpointer userdata)
{
    TestExecutor test_executor;
    GMainLoop* loop = (GMainLoop*)userdata;

    g_assert(test_executor.runMethod(ExecuteType::Auto) == true);
    g_assert(test_executor.runMethod(ExecuteType::Blocking) == true);

    g_main_loop_quit(loop);
    return NULL;
}

static void* _execute_method_runner_blocking_on_another_thread(gpointer userdata)
{
    TestExecutor test_executor;
    GMainLoop* loop = (GMainLoop*)userdata;

    g_assert(test_executor.runMethod(ExecuteType::Blocking, TIMEOUT_IMMEDIATELY) == false);
    g_assert(test_executor.runMethod(ExecuteType::Blocking) == true);

    g_main_loop_quit(loop);
    return NULL;
}

static void* _execute_method_type_runner_on_another_thread(gpointer userdata)
{
    TestExecutor test_executor;
    GMainLoop* loop = (GMainLoop*)userdata;

    g_assert(test_executor.runMethodReturnBool() == true);
    g_assert(test_executor.runMethodReturnInteger() == 1);
    g_assert(test_executor.runMethodReturnString() == "ok");

    g_main_loop_quit(loop);
    return NULL;
}

static void* _execute_method_runner_multiple_job_on_another_thread(gpointer userdata)
{
    TestExecutor test_executor;
    GMainLoop* loop = (GMainLoop*)userdata;

    for (int i = 0; i < 100; i++) {
        test_executor.runMethodWithSleep();
    }

    // wait for 1sec
    usleep(1 * 1000 * 1000);

    g_main_loop_quit(loop);
    return NULL;
}

static void test_nugu_runner_reserve_execute_method_next_idle_time_on_nuguloop(nuguFixture* fixture, gconstpointer ignored)
{
    GMainLoop* loop = fixture->loop;

    g_timeout_add(TIMER_500MS, _execute_method_runner_with_queue_on_nuguloop, (gpointer)loop);

    g_main_loop_run(loop);
}

static void test_nugu_runner_execute_method_immediately_on_nuguloop(nuguFixture* fixture, gconstpointer ignored)
{
    GMainLoop* loop = fixture->loop;

    g_timeout_add(TIMER_500MS, _execute_method_runner_with_auto_on_nuguloop, (gpointer)loop);

    g_main_loop_run(loop);
}

static void test_nugu_runner_execute_method_on_another_thread(nuguFixture* fixture, gconstpointer ignored)
{
    GMainLoop* loop = fixture->loop;
    pthread_t tid;

    pthread_create(&tid, NULL, _execute_method_runner_on_another_thread, (gpointer)loop);

    g_main_loop_run(loop);
}

static void test_nugu_runner_execute_method_blocking_on_another_thread(nuguFixture* fixture, gconstpointer ignored)
{
    GMainLoop* loop = fixture->loop;
    pthread_t tid;

    pthread_create(&tid, NULL, _execute_method_runner_blocking_on_another_thread, (gpointer)loop);

    // wait for 2sec
    usleep(2 * 1000 * 1000);

    g_main_loop_run(loop);
}

static void test_nugu_runner_execute_method_return_type(nuguFixture* fixture, gconstpointer ignored)
{
    GMainLoop* loop = fixture->loop;
    pthread_t tid;

    pthread_create(&tid, NULL, _execute_method_type_runner_on_another_thread, (gpointer)loop);

    g_main_loop_run(loop);
}

static void test_nugu_runner_execute_multiple_method_on_anther_thread(nuguFixture* fixture, gconstpointer ignored)
{
    GMainLoop* loop = fixture->loop;
    pthread_t tid;

    pthread_create(&tid, NULL, _execute_method_runner_multiple_job_on_another_thread, (gpointer)loop);

    g_main_loop_run(loop);
}

int main(int argc, char* argv[])
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
    g_type_init();
#endif

    g_test_init(&argc, &argv, (void*)NULL);
    g_log_set_always_fatal((GLogLevelFlags)G_LOG_FATAL_MASK);

    G_TEST_ADD_FUNC("/app/NuguRunnerReserveExecuteMethodNextIdleTime", test_nugu_runner_reserve_execute_method_next_idle_time_on_nuguloop);
    G_TEST_ADD_FUNC("/app/NuguRunnerReserveExecuteMethodImmediately", test_nugu_runner_execute_method_immediately_on_nuguloop);
    G_TEST_ADD_FUNC("/app/NuguRunnerExecuteMethodOnAnotherThread", test_nugu_runner_execute_method_on_another_thread);
    G_TEST_ADD_FUNC("/app/NuguRunnerExecuteMethodReturnType", test_nugu_runner_execute_method_return_type);
    G_TEST_ADD_FUNC("/app/NuguRunnerExecuteMultipleMethodOnAnotherThread", test_nugu_runner_execute_multiple_method_on_anther_thread);

    if (getenv("UNIT_TEST_ONLY_LOCAL") != NULL)
        G_TEST_ADD_FUNC("/app/NuguRunnerExecuteMethodBlockingOnAnotherThread", test_nugu_runner_execute_method_blocking_on_another_thread);

    return g_test_run();
}
