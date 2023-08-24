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

#include <chrono>
#include <condition_variable>
#include <glib.h>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "nugu_timer.hh"
#include "routine_manager.hh"

using namespace NuguCore;

const unsigned int TIMER_DOWNSCALE_FACTOR = 10;
std::mutex mutex;
std::condition_variable cv;

using DirectiveElement = struct _DirectiveElement {
    std::string name_space;
    std::string name;
    std::string dialog_id;
    std::string groups;
};

enum class TestType {
    Normal,
    PostDelay,
    ActionTimeout,
    DefaultActionTimeout,
    BreakAction,
    CountableAction,
    LastActionProcessed
};

class FakeTimer final : public NUGUTimer {
public:
    FakeTimer(std::mutex& mutex, std::condition_variable& cv)
        : _mutex(mutex)
        , _cv(cv)
    {
    }

    virtual ~FakeTimer()
    {
        stop();
    }

    void start(unsigned int sec = 0) override
    {
        if (is_started)
            return;

        finishWorker();
        setStarted(true);

        worker = std::thread { [&] {
            std::this_thread::sleep_for(std::chrono::microseconds(getInterval() / TIMER_DOWNSCALE_FACTOR));

            if (is_started)
                notifyCallback();

            setStarted(false);
            _cv.notify_one();
        } };
    }

    void stop() override
    {
        setStarted(false);
        finishWorker();
    }

    bool isStarted()
    {
        return is_started;
    }

private:
    void setStarted(bool is_started)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        this->is_started = is_started;
    }

    void finishWorker()
    {
        if (worker.joinable())
            worker.join();
    }

    std::thread worker;
    bool is_started = false;
    std::mutex& _mutex;
    std::condition_variable& _cv;
};

class RoutineTestHelper {
public:
    void composeActions(std::vector<RoutineManager::RoutineAction>&& test_datas)
    {
        actions.clear();

        for (const auto& element : test_datas) {
            NJson::Value action;
            action["type"] = element.type;
            action["playServiceId"] = element.play_service_id;

            if (!element.token.empty())
                action["token"] = element.token;

            if (!element.text.empty())
                action["text"] = element.text;

            if (!element.data.isNull())
                action["data"] = element.data;

            if (element.post_delay_msec > 0)
                action["postDelayInMilliseconds"] = element.post_delay_msec;

            if (element.mute_delay_msec > 0)
                action["muteDelayInMilliseconds"] = element.mute_delay_msec;

            if (element.action_timeout_msec > 0)
                action["actionTimeoutInMilliseconds"] = element.action_timeout_msec;

            actions.append(action);
        }
    }

    NJson::Value getActions()
    {
        return actions;
    }

    size_t getActionCount()
    {
        return actions.size();
    }

    std::string getToken()
    {
        return token;
    }

    std::string getActionToken(unsigned int index)
    {
        for (NJson::Value::ArrayIndex i = 0; i < actions.size(); i++) {
            if (i == index - 1)
                return actions[i]["token"].asString();
        }

        return "";
    }

private:
    NJson::Value actions;
    std::string token = "token";
};

class RoutineManagerListener : public IRoutineManagerListener {
public:
    void onActivity(RoutineActivity activity) override
    {
        this->activity = activity;
    }

    void onActionTimeout(bool last_action = false) override
    {
        is_action_timeout = true;
        is_last_action = last_action;
    }

    RoutineActivity getActivity()
    {
        return activity;
    }

    bool isActionTimeout()
    {
        return is_action_timeout;
    }

    bool isProcessedInLastAction()
    {
        return is_last_action;
    }

    void reset()
    {
        is_action_timeout = false;
        is_last_action = false;
    }

private:
    RoutineActivity activity = RoutineActivity::IDLE;
    bool is_action_timeout = false;
    bool is_last_action = false;
};

static NuguDirective* createDirective(DirectiveElement&& element)
{
    return nugu_directive_new(element.name_space.c_str(), element.name.c_str(), "1.0",
        "message_1", element.dialog_id.c_str(), "referrer_1", "{}", element.groups.c_str());
}

static void changeDialogId(NuguDirective*& ndir, std::string&& dialog_id)
{
    std::string name_space = nugu_directive_peek_namespace(ndir);
    std::string name = nugu_directive_peek_namespace(ndir);
    std::string groups = nugu_directive_peek_groups(ndir);

    nugu_directive_unref(ndir);

    ndir = createDirective({ name_space, name, dialog_id, groups });
}

typedef struct _TestFixture {
    std::shared_ptr<RoutineManager> routine_manager;
    std::shared_ptr<RoutineTestHelper> routine_helper;
    std::shared_ptr<RoutineManagerListener> routine_manager_listener;
    std::shared_ptr<RoutineManagerListener> routine_manager_listener_snd;
    FakeTimer* fake_timer;
    NuguDirective* ndir_info;
    NuguDirective* ndir_info_disp;
    NuguDirective* ndir_info_tts;
    NuguDirective* ndir_routine;
    NuguDirective* ndir_routine_stop;
    NuguDirective* ndir_dm;
} TestFixture;

static void setup(TestFixture* fixture, gconstpointer user_data)
{
    fixture->routine_manager = std::make_shared<RoutineManager>();
    fixture->routine_helper = std::make_shared<RoutineTestHelper>();
    fixture->routine_manager_listener = std::make_shared<RoutineManagerListener>();
    fixture->routine_manager_listener_snd = std::make_shared<RoutineManagerListener>();
    fixture->fake_timer = new FakeTimer(mutex, cv);

    fixture->routine_manager->setTimer(fixture->fake_timer);
    fixture->routine_manager->setTextRequester([](const std::string& token, const std::string& text, const std::string& ps_id) {
        return ps_id == "ps_id_1" ? "dialog_1" : "dialog_2";
    });
    fixture->routine_manager->setDataRequester([](const std::string& ps_id, const NJson::Value& data) {
        return "dialog_3";
    });
    fixture->routine_helper->composeActions({
        { "TEXT", "ps_id_1", "token_1", "text_1", NJson::nullValue },
        { "TEXT", "ps_id_2", "token_2", "text_2", NJson::nullValue },
        { "DATA", "ps_id_3", "token_3", "", NJson::Value("DATA_VALUE") },
    });

    fixture->ndir_info = createDirective({ "TTS", "Speak", "dialog_1",
        "{ \"directives\": [\"TTS.Speak\"] }" });
    fixture->ndir_info_disp = createDirective({ "Display", "FullText2", "dialog_2",
        "{ \"directives\": [\"Display.FullText2\", \"TTS.Speak\"] }" });
    fixture->ndir_info_tts = createDirective({ "TTS", "Speak", "dialog_2",
        "{ \"directives\": [\"Display.FullText2\", \"TTS.Speak\"] }" });
    fixture->ndir_routine = createDirective({ "Routine", "Continue", "dialog_1",
        "{ \"directives\": [\"TTS.Speak\", \"Routine.Continue\"] }" });
    fixture->ndir_routine_stop = createDirective({ "Routine", "Stop", "dialog_1",
        "{ \"directives\": [\"TTS.Stop\", \"Routine.Stop\"] }" });
    fixture->ndir_dm = createDirective({ "ASR", "ExpectSpeech", "dialog_1",
        "{ \"directives\": [\"TTS.Speak\", \"ASR.ExpectSpeech\"] }" });
}

static void teardown(TestFixture* fixture, gconstpointer user_data)
{
    nugu_directive_unref(fixture->ndir_info);
    nugu_directive_unref(fixture->ndir_info_disp);
    nugu_directive_unref(fixture->ndir_info_tts);
    nugu_directive_unref(fixture->ndir_routine);
    nugu_directive_unref(fixture->ndir_routine_stop);
    nugu_directive_unref(fixture->ndir_dm);

    fixture->routine_manager_listener.reset();
    fixture->routine_manager_listener_snd.reset();
    fixture->routine_manager.reset();
    fixture->routine_helper.reset();
}

static void onTimeElapsed(TestFixture* fixture)
{
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&] { return !fixture->fake_timer->isStarted(); });
}

#define G_TEST_ADD_FUNC(name, func) \
    g_test_add(name, TestFixture, nullptr, setup, func, teardown);

static void test_routine_manager_listener(TestFixture* fixture, gconstpointer ignored)
{
    fixture->routine_manager->addListener(nullptr);
    g_assert(fixture->routine_manager->getListenerCount() == 0);

    fixture->routine_manager->addListener(fixture->routine_manager_listener.get());
    g_assert(fixture->routine_manager->getListenerCount() == 1);

    // check duplicate addition
    fixture->routine_manager->addListener(fixture->routine_manager_listener.get());
    g_assert(fixture->routine_manager->getListenerCount() == 1);
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::IDLE);

    fixture->routine_manager->addListener(fixture->routine_manager_listener_snd.get());
    g_assert(fixture->routine_manager->getListenerCount() == 2);

    g_assert(fixture->routine_manager->start(fixture->routine_helper->getToken(), fixture->routine_helper->getActions()));
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::PLAYING);

    fixture->routine_manager->removeListener(nullptr);
    g_assert(fixture->routine_manager->getListenerCount() == 2);

    fixture->routine_manager->removeListener(fixture->routine_manager_listener.get());
    g_assert(fixture->routine_manager->getListenerCount() == 1);

    // check removing again
    fixture->routine_manager->removeListener(fixture->routine_manager_listener.get());
    g_assert(fixture->routine_manager->getListenerCount() == 1);

    fixture->routine_manager->removeListener(fixture->routine_manager_listener_snd.get());
    g_assert(fixture->routine_manager->getListenerCount() == 0);
}

static void test_routine_manager_validation(TestFixture* fixture, gconstpointer ignored)
{
    const auto& action_container = fixture->routine_manager->getActionContainer();

    g_assert(!fixture->routine_manager->start(fixture->routine_helper->getToken(), NJson::arrayValue));
    g_assert(action_container.empty() && fixture->routine_manager->getCurrentActionIndex() == 0);

    // remove previous actions when calling start
    g_assert(fixture->routine_manager->start(fixture->routine_helper->getToken(), fixture->routine_helper->getActions()));
    g_assert(fixture->routine_manager->start(fixture->routine_helper->getToken(), fixture->routine_helper->getActions()));

    g_assert(action_container.size() == 3);
}

static void test_routine_manager_check_action_valid(TestFixture* fixture, gconstpointer ignored)
{
    fixture->routine_helper->composeActions({
        // valid
        { "TEXT", "ps_id_1", "token_1", "text_1", NJson::nullValue },
        // invalid (no type)
        { "", "ps_id_2", "token_2", "text_2", NJson::nullValue },
        // valid
        { "DATA", "ps_id_3", "token_3", "", NJson::Value("DATA_VALUE") },
        // invalid (no playServiceId)
        { "DATA", "", "token_4", "", NJson::Value("DATA_VALUE") },
        // invalid (no data)
        { "DATA", "", "token_4", "", NJson::nullValue },
    });

    auto actions = fixture->routine_helper->getActions();
    int valid_action_count = 0;

    for (const auto& action : actions)
        if (fixture->routine_manager->isActionValid(action))
            valid_action_count++;

    g_assert(valid_action_count == 2);
}

static void test_routine_manager_compose_action_queue(TestFixture* fixture, gconstpointer ignored)
{
    fixture->routine_helper->composeActions({
        { "", "ps_id_0", "token_0", "text_0", NJson::nullValue },
        { "", "ps_id_0", "token_0", "", NJson::nullValue },

        { "TEXT", "ps_id_1", "token_1", "text_1", NJson::nullValue },
        { "TEXT", "", "", "text_2", NJson::nullValue },
        { "TEXT", "", "", "text_3", NJson::nullValue },

        { "DATA", "ps_id_2", "token_2", "", NJson::Value("DATA_VALUE") },
        { "DATA", "ps_id_2", "token_2", "", NJson::nullValue },
        { "DATA", "", "", "", NJson::Value("DATA_VALUE") },
        { "DATA", "", "", "", NJson::Value("DATA_VALUE") },
    });

    g_assert(fixture->routine_manager->start(fixture->routine_helper->getToken(), fixture->routine_helper->getActions()));
    g_assert(fixture->routine_manager->getActionContainer().size() == 4);
}

static void test_routine_manager_normal(TestFixture* fixture, gconstpointer ignored)
{
    fixture->routine_manager->addListener(fixture->routine_manager_listener.get());
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::IDLE);

    g_assert(fixture->routine_manager->start(fixture->routine_helper->getToken(), fixture->routine_helper->getActions()));
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::PLAYING);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 1);
    g_assert(fixture->routine_manager->isActionProgress("dialog_1"));

    fixture->routine_manager->finish();
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::PLAYING);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 2);
    g_assert(!fixture->routine_manager->isActionProgress("dialog_1"));
    g_assert(fixture->routine_manager->isActionProgress("dialog_2"));

    fixture->routine_manager->finish();
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::PLAYING);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 3);
    g_assert(!fixture->routine_manager->isActionProgress("dialog_2"));
    g_assert(fixture->routine_manager->isActionProgress("dialog_3"));

    fixture->routine_manager->finish();
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::FINISHED);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 0);

    // It assume starting another routine
    g_assert(fixture->routine_manager->start(fixture->routine_helper->getToken(), fixture->routine_helper->getActions()));
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 1);
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::PLAYING);
}

static void sub_test_routine_manager_start_routine(TestFixture* fixture, TestType test_type = TestType::Normal)
{
    std::vector<RoutineManager::RoutineAction> test_datas;

    if (test_type == TestType::PostDelay)
        test_datas = {
            { "TEXT", "ps_id_1", "token_1", "text_1", NJson::nullValue, 1000 },
            { "TEXT", "ps_id_2", "token_2", "text_2", NJson::nullValue, 2000 },
            { "DATA", "ps_id_3", "token_3", "", NJson::Value("DATA_VALUE") },
            { "DATA", "ps_id_4", "token_4", "", NJson::Value("DATA_VALUE") },
        };
    else if (test_type == TestType::ActionTimeout)
        test_datas = {
            { "TEXT", "ps_id_1", "token_1", "text_1", NJson::nullValue },
            { "DATA", "ps_id_2", "token_2", "", NJson::Value("DATA_VALUE"), 0, 0, 2000 },
            { "TEXT", "ps_id_3", "token_3", "text_3", NJson::nullValue },
            { "DATA", "ps_id_4", "token_4", "", NJson::Value("DATA_VALUE"), 0, 0, 2000 },
        };
    else if (test_type == TestType::DefaultActionTimeout)
        test_datas = {
            { "TEXT", "ps_id_1", "token_1", "text_1", NJson::nullValue },
            { "DATA", "ps_id_2", "token_2", "", NJson::Value("DATA_VALUE"), 0, 0, 2000 },
            { "TEXT", "ps_id_3", "token_3", "text_3", NJson::nullValue },
        };
    else if (test_type == TestType::BreakAction)
        test_datas = {
            { "TEXT", "ps_id_1", "token_1", "text_1", NJson::nullValue },
            { "BREAK", "ps_id_2", "token_2", "", NJson::nullValue, 0, 1000 },
            { "DATA", "ps_id_3", "token_3", "", NJson::Value("DATA_VALUE") },
            { "BREAK", "ps_id_4", "token_4", "", NJson::nullValue, 0, 0 },
            { "DATA", "ps_id_5", "token_5", "", NJson::Value("DATA_VALUE") },
        };
    else if (test_type == TestType::CountableAction)
        test_datas = {
            { "TEXT", "ps_id_1", "token_1", "text_1", NJson::nullValue },
            { "BREAK", "ps_id_2", "token_2", "text_2", NJson::nullValue, 0, 1000 },
            { "DATA", "ps_id_3", "token_3", "", NJson::Value("DATA_VALUE") },
        };
    else if (test_type == TestType::LastActionProcessed)
        test_datas = {
            { "TEXT", "ps_id_1", "token_1", "text_1", NJson::nullValue },
            { "TEXT", "ps_id_2", "token_2", "text_2", NJson::nullValue },
        };

    if (!test_datas.empty())
        fixture->routine_helper->composeActions(std::move(test_datas));

    fixture->routine_manager->addListener(fixture->routine_manager_listener.get());
    g_assert(fixture->routine_manager->start(fixture->routine_helper->getToken(), fixture->routine_helper->getActions()));
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::PLAYING);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 1);
}

static void test_routine_manager_interrupt(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_routine_manager_start_routine(fixture);

    g_assert(fixture->routine_manager->isRoutineProgress());

    fixture->routine_manager->interrupt();
    g_assert(fixture->routine_manager->isInterrupted());
    g_assert(!fixture->routine_manager->isRoutineProgress());
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::INTERRUPTED);

    fixture->routine_manager->finish();
    g_assert(fixture->routine_manager->isInterrupted());
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 1);

    fixture->routine_manager->stop();
    g_assert(!fixture->routine_manager->isInterrupted());
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::STOPPED);
}

static void test_routine_manager_move(TestFixture* fixture, gconstpointer ignored)
{
    fixture->routine_manager->addListener(fixture->routine_manager_listener.get());

    auto checkActionProgress = [&](const unsigned int& index, std::string&& cur_dialog_id = "", std::string&& prev_dialog_id = "") {
        g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::PLAYING);
        g_assert(fixture->routine_manager->getCurrentActionIndex() == index);

        if (!cur_dialog_id.empty())
            g_assert(fixture->routine_manager->isActionProgress(cur_dialog_id));

        if (!prev_dialog_id.empty())
            g_assert(!fixture->routine_manager->isActionProgress(prev_dialog_id));
    };

    auto checkRoutineActivity = [&](const RoutineActivity activity) {
        g_assert(fixture->routine_manager_listener->getActivity() == activity);
        g_assert(fixture->routine_manager->getCurrentActionIndex() == 0);
    };

    g_assert(fixture->routine_manager->start(fixture->routine_helper->getToken(), fixture->routine_helper->getActions()));
    checkActionProgress(1, "dialog_1");

    fixture->routine_manager->finish();
    checkActionProgress(2, "dialog_2", "dialog_1");

    fixture->routine_manager->finish();
    checkActionProgress(3, "dialog_3", "dialog_2");

    // move from action 3 to 2
    g_assert(fixture->routine_manager->move(2));
    checkActionProgress(2, "dialog_2", "dialog_3");

    fixture->routine_manager->finish();
    checkActionProgress(3, "dialog_3", "dialog_2");

    // move from action 3 to 1
    g_assert(fixture->routine_manager->move(1));
    checkActionProgress(1, "dialog_1", "dialog_3");

    fixture->routine_manager->finish();
    checkActionProgress(2, "dialog_2", "dialog_1");

    // move from action 2 to 3
    g_assert(fixture->routine_manager->move(3));
    checkActionProgress(3, "dialog_3", "dialog_2");

    // move to index of greater than action count (maintain action: 3)
    g_assert(!fixture->routine_manager->move(4));
    checkActionProgress(3, "dialog_3");

    // move to index of less than action count (maintain action: 3)
    g_assert(!fixture->routine_manager->move(-1));
    checkActionProgress(3, "dialog_3");

    // try to move when routine is finished (not progress)
    fixture->routine_manager->finish();
    checkRoutineActivity(RoutineActivity::FINISHED);

    g_assert(!fixture->routine_manager->move(1));
    checkRoutineActivity(RoutineActivity::FINISHED);

    // try to move when routine stopped (not progress)
    g_assert(fixture->routine_manager->start(fixture->routine_helper->getToken(), fixture->routine_helper->getActions()));
    checkActionProgress(1);

    fixture->routine_manager->stop();
    checkRoutineActivity(RoutineActivity::STOPPED);

    g_assert(!fixture->routine_manager->move(1));
    checkRoutineActivity(RoutineActivity::STOPPED);

    // try to move when routine is interrupted (progress)
    g_assert(fixture->routine_manager->start(fixture->routine_helper->getToken(), fixture->routine_helper->getActions()));
    fixture->routine_manager->finish();
    fixture->routine_manager->finish();
    checkActionProgress(3, "dialog_3"); // handle last action but, routine is not finished yet.

    g_assert(fixture->routine_manager->move(2));
    checkActionProgress(2, "dialog_2", "dialog_3");

    fixture->routine_manager->interrupt();
    g_assert(fixture->routine_manager->isInterrupted());
    g_assert(!fixture->routine_manager->isRoutineProgress());
    g_assert(fixture->routine_manager->isRoutineAlive()); // It still alive because routine is not finished.
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::INTERRUPTED);

    g_assert(fixture->routine_manager->move(3));
    checkActionProgress(3, "dialog_3", "dialog_1");
}

static void test_routine_manager_stop(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_routine_manager_start_routine(fixture);

    const auto& action_container = fixture->routine_manager->getActionContainer();

    fixture->routine_manager->stop();
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::STOPPED);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 0);
    g_assert(fixture->routine_manager->getCountableActionIndex() == 0);
    g_assert(action_container.empty());
}

static void test_routine_manager_stop_in_post_delayed(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_routine_manager_start_routine(fixture, TestType::PostDelay);

    // hold by interrupt
    fixture->routine_manager->finish();
    fixture->routine_manager->interrupt();
    onTimeElapsed(fixture);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 1);

    // stop
    fixture->routine_manager->stop();
    g_assert(!fixture->routine_manager->hasPostDelayed());
}

static void test_routine_manager_set_pending_stop(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_routine_manager_start_routine(fixture);

    fixture->routine_manager->setPendingStop(fixture->ndir_routine);
    g_assert(!fixture->routine_manager->hasPendingStop());

    // reset flag in next action progress
    fixture->routine_manager->setPendingStop(fixture->ndir_routine_stop);
    g_assert(fixture->routine_manager->hasPendingStop());
    fixture->routine_manager->finish();
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::PLAYING);
    g_assert(!fixture->routine_manager->hasPendingStop());

    // reset flag in move
    fixture->routine_manager->setPendingStop(fixture->ndir_routine_stop);
    g_assert(fixture->routine_manager->hasPendingStop());
    g_assert(fixture->routine_manager->move(3));
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::PLAYING);
    g_assert(!fixture->routine_manager->hasPendingStop());

    // reset flag in finish
    fixture->routine_manager->setPendingStop(fixture->ndir_routine_stop);
    g_assert(fixture->routine_manager->hasPendingStop());
    fixture->routine_manager->finish();
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::FINISHED);
    g_assert(!fixture->routine_manager->hasPendingStop());

    // reset flag in stop
    sub_test_routine_manager_start_routine(fixture);
    fixture->routine_manager->setPendingStop(fixture->ndir_routine_stop);
    g_assert(fixture->routine_manager->hasPendingStop());
    fixture->routine_manager->stop();
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::STOPPED);
    g_assert(!fixture->routine_manager->hasPendingStop());
}

static void test_routine_manager_continue(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_routine_manager_start_routine(fixture);

    // execute next second action
    fixture->routine_manager->finish();
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::PLAYING);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 2);

    fixture->routine_manager->interrupt();
    g_assert(fixture->routine_manager->isInterrupted());

    fixture->routine_manager->resume();
    g_assert(!fixture->routine_manager->isInterrupted());
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::PLAYING);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 3);

    fixture->routine_manager->finish();
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::FINISHED);
}

static void test_routine_manager_continue_in_post_delayed(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_routine_manager_start_routine(fixture, TestType::PostDelay);

    // hold by interrupt
    fixture->routine_manager->finish();
    fixture->routine_manager->interrupt();
    onTimeElapsed(fixture);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 1);

    // resume
    fixture->routine_manager->resume();
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 1);
    onTimeElapsed(fixture);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 2);
}

static void test_routine_manager_post_delay_among_actions(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_routine_manager_start_routine(fixture, TestType::PostDelay);

    // apply delay about 1000 msec
    fixture->routine_manager->finish();
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 1);
    onTimeElapsed(fixture);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 2);

    // apply delay about 2000 msec
    fixture->routine_manager->finish();
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 2);
    onTimeElapsed(fixture);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 3);

    // no delay
    fixture->routine_manager->finish();
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 4);
}

static void test_routine_manager_handle_action_timeout(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_routine_manager_start_routine(fixture, TestType::ActionTimeout);

    fixture->routine_manager->finish();
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::SUSPENDED);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 2);
    g_assert(fixture->routine_manager->isActionTimeout());

    // interrupt not possible, if the routine is suspended.
    fixture->routine_manager->interrupt();
    g_assert(fixture->routine_manager_listener->getActivity() != RoutineActivity::INTERRUPTED);

    // finish is prevented when handling action timeout
    fixture->routine_manager->finish();
    g_assert(fixture->routine_manager_listener->getActivity() != RoutineActivity::PLAYING);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 2);
    g_assert(fixture->routine_manager->isActionTimeout());

    // move is possible when handling action timeout
    g_assert(fixture->routine_manager->move(3));
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::PLAYING);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 3);
    g_assert(!fixture->routine_manager->isActionTimeout());
    g_assert(!fixture->fake_timer->isStarted()); // timer stopped

    // assume media start is notified and start action timeout timer
    g_assert(fixture->routine_manager->move(2));
    onTimeElapsed(fixture);
    g_assert(fixture->routine_manager_listener->isActionTimeout());
    g_assert(!fixture->routine_manager_listener->isProcessedInLastAction());
    g_assert(fixture->routine_manager->move(3));
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::PLAYING);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 3);

    // process action timeout in last action
    g_assert(fixture->routine_manager->move(4));
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::SUSPENDED);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 4);
    fixture->routine_manager_listener->reset();
    onTimeElapsed(fixture);
    g_assert(fixture->routine_manager_listener->isActionTimeout());
    g_assert(fixture->routine_manager_listener->isProcessedInLastAction());

    // stop in processing action timeout
    g_assert(fixture->routine_manager->move(2));
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::SUSPENDED);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 2);
    g_assert(fixture->routine_manager->isActionTimeout());

    fixture->routine_manager->stop();
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::STOPPED);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 0);
    g_assert(!fixture->routine_manager->isActionTimeout());
}

static void test_routine_manager_use_preset_action_timeout(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_routine_manager_start_routine(fixture, TestType::DefaultActionTimeout);

    // process media in middle action (no action timeout) -> use default
    g_assert(!fixture->routine_manager->isActionTimeout());
    fixture->routine_manager->presetActionTimeout();
    g_assert(fixture->routine_manager->isActionTimeout());
    g_assert(fixture->fake_timer->getInterval() == 5000); // set default (5ms)

    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::SUSPENDED);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 1);

    onTimeElapsed(fixture);
    g_assert(fixture->routine_manager->move(2));

    // process media in middle action (has action timeout) -> use received data
    g_assert(fixture->routine_manager->isActionTimeout());
    fixture->routine_manager->presetActionTimeout();
    g_assert(fixture->fake_timer->getInterval() == 2000);

    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::SUSPENDED);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 2);

    onTimeElapsed(fixture);
    g_assert(fixture->routine_manager->move(3));

    // process media in last action (no action timeout) -> nothing
    g_assert(!fixture->routine_manager->isActionTimeout());
    fixture->routine_manager->presetActionTimeout();
    g_assert(!fixture->routine_manager->isActionTimeout());
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::PLAYING);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 3);
}

static void test_routine_manager_handle_break_action(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_routine_manager_start_routine(fixture, TestType::BreakAction);

    // SUSPENDED as handling BREAK action (activate mute delay timer)
    fixture->routine_manager->finish();
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::SUSPENDED);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 2);

    // interrupt/finish/move is prevented when handling BREAK action
    fixture->routine_manager->interrupt();
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::SUSPENDED);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 2);
    fixture->routine_manager->finish();
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::SUSPENDED);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 2);
    g_assert(!fixture->routine_manager->move(3));

    // progress next action after mute delay timeout
    onTimeElapsed(fixture);
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::PLAYING);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 3);

    // Because action-2 type is BREAK, it pass to action-3
    g_assert(fixture->routine_manager->move(2));
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::PLAYING);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 3);

    // Because action-4 has no mute delay, it progress next action immediately
    fixture->routine_manager->finish();
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::PLAYING);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 5);

    // stop routine in mute delay
    g_assert(fixture->routine_manager->move(1));
    fixture->routine_manager->finish();
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::SUSPENDED);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 2);
    g_assert(fixture->routine_manager->isMuteDelayed());

    fixture->routine_manager->stop();
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::STOPPED);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 0);
    g_assert(!fixture->routine_manager->isMuteDelayed());
}

static void test_routine_manager_check_has_routine_directive(TestFixture* fixture, gconstpointer ignored)
{
    g_assert(!fixture->routine_manager->hasRoutineDirective(nullptr));
    g_assert(!fixture->routine_manager->hasRoutineDirective(fixture->ndir_info));
    g_assert(fixture->routine_manager->hasRoutineDirective(fixture->ndir_routine));
}

static void test_routine_manager_check_routine_alive(TestFixture* fixture, gconstpointer ignored)
{
    g_assert(!fixture->routine_manager->isRoutineAlive());

    sub_test_routine_manager_start_routine(fixture);
    g_assert(fixture->routine_manager->isRoutineAlive());

    fixture->routine_manager->interrupt();
    g_assert(fixture->routine_manager->isRoutineAlive());

    fixture->routine_manager->stop();
    g_assert(!fixture->routine_manager->isRoutineAlive());
}

static void test_routine_manager_check_condition_to_stop(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_routine_manager_start_routine(fixture);

    g_assert(!fixture->routine_manager->isConditionToStop(nullptr));

    // The received directive is result of routine action. (not stop)
    g_assert(fixture->routine_manager->isRoutineAlive());
    g_assert(fixture->routine_manager->isActionProgress("dialog_1"));
    g_assert(!fixture->routine_manager->isConditionToStop(fixture->ndir_info));
    g_assert(!fixture->routine_manager->isConditionToStop(fixture->ndir_routine));

    // The received directive has ASR.ExpectSpeech in progressing middle action. (stop)
    g_assert(fixture->routine_manager->isConditionToStop(fixture->ndir_dm));

    // The received directive is for another play, not routine action. (stop if routine directive not exist)
    fixture->routine_manager->finish();
    g_assert(fixture->routine_manager->isActionProgress("dialog_2"));
    g_assert(fixture->routine_manager->isConditionToStop(fixture->ndir_info));
    g_assert(!fixture->routine_manager->isConditionToStop(fixture->ndir_routine));

    // progress last action
    fixture->routine_manager->finish();
    g_assert(fixture->routine_manager->isRoutineAlive());
    g_assert(fixture->routine_manager->isActionProgress("dialog_3"));

    // The received directive has ASR.ExpectSpeech in progressing last action. (not stop)
    changeDialogId(fixture->ndir_dm, "dialog_3");
    g_assert(!fixture->routine_manager->isConditionToStop(fixture->ndir_dm));

    // The routine is not activated. (not stop)
    fixture->routine_manager->stop();
    g_assert(!fixture->routine_manager->isRoutineAlive());
    g_assert(!fixture->routine_manager->isConditionToStop(fixture->ndir_info));

    // check suspended case (not stop)
    sub_test_routine_manager_start_routine(fixture, TestType::BreakAction);

    fixture->routine_manager->finish();
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::SUSPENDED);
    g_assert(!fixture->routine_manager->isConditionToStop(fixture->ndir_info));

    onTimeElapsed(fixture);
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::PLAYING);
    g_assert(fixture->routine_manager->isConditionToStop(fixture->ndir_info));
}

static void test_routine_manager_check_condition_to_finish_action(TestFixture* fixture, gconstpointer ignored)
{
    fixture->routine_manager->addListener(fixture->routine_manager_listener.get());
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::IDLE);

    // check validation
    g_assert(!fixture->routine_manager->isConditionToFinishAction(nullptr));

    // progress first action (dialog_1)
    g_assert(fixture->routine_manager->start(fixture->routine_helper->getToken(), fixture->routine_helper->getActions()));
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::PLAYING);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 1);

    // assume to handle [TTS.Speak] directive (dialog_1)
    g_assert(fixture->routine_manager->isConditionToFinishAction(fixture->ndir_info));
    fixture->routine_manager->finish();

    // progress second action (dialog_2)
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::PLAYING);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 2);

    // assume to handle [Display.FullText2, TTS.Speak] directives (dialog_2)
    g_assert(!fixture->routine_manager->isConditionToFinishAction(fixture->ndir_info)); // previous dialog
    g_assert(!fixture->routine_manager->isConditionToFinishAction(fixture->ndir_info_disp)); // Display
    g_assert(fixture->routine_manager->isConditionToFinishAction(fixture->ndir_info_tts)); // TTS

    fixture->routine_manager->stop();
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::STOPPED);
    g_assert(!fixture->routine_manager->isConditionToFinishAction(fixture->ndir_info_tts));
}

static void test_routine_manager_check_condition_to_cancel(TestFixture* fixture, gconstpointer ignored)
{
    // condition to cancel (has Routine directive except Routine.Stop)
    g_assert(fixture->routine_manager->isConditionToCancel(fixture->ndir_routine));

    // condition not to cancel
    g_assert(!fixture->routine_manager->isConditionToCancel(nullptr));
    g_assert(!fixture->routine_manager->isConditionToCancel(fixture->ndir_routine_stop));
    g_assert(!fixture->routine_manager->isConditionToCancel(fixture->ndir_info));
}

static void test_routine_manager_check_has_to_skip_media(TestFixture* fixture, gconstpointer ignored)
{
    // not skip in routine inactive
    g_assert(!fixture->routine_manager->hasToSkipMedia("dialog_1"));

    sub_test_routine_manager_start_routine(fixture, TestType::ActionTimeout);
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::PLAYING);

    // not skip in current action progress
    g_assert(!fixture->routine_manager->hasToSkipMedia("dialog_1"));
    // skip in another dialog progress
    g_assert(fixture->routine_manager->hasToSkipMedia("dialog_4"));

    // not skip in routine suspended
    fixture->routine_manager->finish();
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::SUSPENDED);
    g_assert(!fixture->routine_manager->hasToSkipMedia("dialog_4"));
    onTimeElapsed(fixture);

    // not skip in pending stop
    g_assert(fixture->routine_manager->move(3));
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::PLAYING);
    fixture->routine_manager->setPendingStop(fixture->ndir_routine_stop);
    g_assert(!fixture->routine_manager->hasToSkipMedia("dialog_4"));
}

static void test_routine_manager_get_current_action_token(TestFixture* fixture, gconstpointer ignored)
{
    g_assert(fixture->routine_manager->getCurrentActionToken().empty());

    // start routine
    g_assert(fixture->routine_manager->start(fixture->routine_helper->getToken(), fixture->routine_helper->getActions()));
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 1);
    g_assert(fixture->routine_manager->getCurrentActionToken() == fixture->routine_helper->getActionToken(1));

    // run next action
    fixture->routine_manager->finish();
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 2);
    g_assert(fixture->routine_manager->getCurrentActionToken() == fixture->routine_helper->getActionToken(2));

    // stop routine
    fixture->routine_manager->stop();
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 0);
    g_assert(fixture->routine_manager->getCurrentActionToken().empty());
}

static void test_routine_manager_get_countable_action_info(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_routine_manager_start_routine(fixture, TestType::CountableAction);

    g_assert(fixture->routine_manager->getActionContainer().size() == 3);
    g_assert(fixture->routine_manager->getCountableActionSize() == 2); // BREAK type excluded
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 1);
    g_assert(fixture->routine_manager->getCountableActionIndex() == 1);

    fixture->routine_manager->finish();
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::SUSPENDED);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 2);
    g_assert(fixture->routine_manager->getCountableActionIndex() == 1);

    onTimeElapsed(fixture);
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::PLAYING);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 3);
    g_assert(fixture->routine_manager->getCountableActionIndex() == 2);

    fixture->routine_manager->finish();
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::FINISHED);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 0);
    g_assert(fixture->routine_manager->getCountableActionIndex() == 0);
}

static void test_routine_manager_reset(TestFixture* fixture, gconstpointer ignored)
{
    const auto& action_container = fixture->routine_manager->getActionContainer();
    const auto& action_dialogs = fixture->routine_manager->getActionDialogs();

    sub_test_routine_manager_start_routine(fixture, TestType::PostDelay);
    fixture->routine_manager->setPendingStop(fixture->ndir_routine_stop);

    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::PLAYING);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 1);
    g_assert(fixture->routine_manager->getCountableActionIndex() == 1);
    g_assert(!fixture->routine_manager->getToken().empty());
    g_assert(fixture->routine_manager->hasPostDelayed());
    g_assert(fixture->routine_manager->hasPendingStop());
    g_assert(!action_container.empty());
    g_assert(!action_dialogs.empty());

    fixture->routine_manager->interrupt();
    g_assert(fixture->routine_manager->isInterrupted());

    fixture->routine_manager->reset();
    g_assert(fixture->routine_manager->getListenerCount() == 1);
    g_assert(!fixture->routine_manager->getTextRequester());
    g_assert(!fixture->routine_manager->getDataRequester());
    g_assert(fixture->routine_manager->getActivity() == RoutineActivity::IDLE);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 0);
    g_assert(fixture->routine_manager->getCountableActionIndex() == 0);
    g_assert(fixture->routine_manager->getToken().empty());
    g_assert(!fixture->routine_manager->isInterrupted());
    g_assert(!fixture->routine_manager->hasPostDelayed());
    g_assert(!fixture->routine_manager->hasPendingStop());
    g_assert(action_container.empty());
    g_assert(action_dialogs.empty());

    // check last [action processed, mute delay, action timeout] flags reset
    auto checkFlagReset = [&](TestType test_type, std::function<bool()>&& check_func) {
        fixture->routine_manager->setTextRequester(
            [](const std::string& token, const std::string& text, const std::string& ps_id) {
                return "dialog_1";
            });
        fixture->routine_manager->setDataRequester(
            [](const std::string& ps_id, const NJson::Value& data) {
                return "dialog_3";
            });

        sub_test_routine_manager_start_routine(fixture, test_type);
        fixture->routine_manager->finish();

        g_assert(check_func());
        fixture->routine_manager->reset();
        g_assert(!check_func());
    };

    checkFlagReset(TestType::LastActionProcessed, [&] {
        return fixture->routine_manager->isLastActionProcessed();
    });

    checkFlagReset(TestType::BreakAction, [&] {
        return fixture->routine_manager->isMuteDelayed();
    });

    checkFlagReset(TestType::ActionTimeout, [&] {
        return fixture->routine_manager->isActionTimeout();
    });
}

int main(int argc, char* argv[])
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
    g_type_init();
#endif

    g_test_init(&argc, &argv, (void*)NULL);
    g_log_set_always_fatal((GLogLevelFlags)G_LOG_FATAL_MASK);

    G_TEST_ADD_FUNC("/core/RoutineManager/listener", test_routine_manager_listener);
    G_TEST_ADD_FUNC("/core/RoutineManager/validation", test_routine_manager_validation);
    G_TEST_ADD_FUNC("/core/RoutineManager/checkActionValid", test_routine_manager_check_action_valid);
    G_TEST_ADD_FUNC("/core/RoutineManager/composeActionQueue", test_routine_manager_compose_action_queue);
    G_TEST_ADD_FUNC("/core/RoutineManager/normal", test_routine_manager_normal);
    G_TEST_ADD_FUNC("/core/RoutineManager/interrupt", test_routine_manager_interrupt);
    G_TEST_ADD_FUNC("/core/RoutineManager/move", test_routine_manager_move);
    G_TEST_ADD_FUNC("/core/RoutineManager/stop", test_routine_manager_stop);
    G_TEST_ADD_FUNC("/core/RoutineManager/stopInPostDelayed", test_routine_manager_stop_in_post_delayed);
    G_TEST_ADD_FUNC("/core/RoutineManager/setPendingStop", test_routine_manager_set_pending_stop);
    G_TEST_ADD_FUNC("/core/RoutineManager/continue", test_routine_manager_continue);
    G_TEST_ADD_FUNC("/core/RoutineManager/continueInPostDelayed", test_routine_manager_continue_in_post_delayed);
    G_TEST_ADD_FUNC("/core/RoutineManager/postDelayAmongActions", test_routine_manager_post_delay_among_actions);
    G_TEST_ADD_FUNC("/core/RoutineManager/handleActionTimeout", test_routine_manager_handle_action_timeout);
    G_TEST_ADD_FUNC("/core/RoutineManager/usePresetActionTimeout", test_routine_manager_use_preset_action_timeout);
    G_TEST_ADD_FUNC("/core/RoutineManager/handleBreakAction", test_routine_manager_handle_break_action);
    G_TEST_ADD_FUNC("/core/RoutineManager/checkHasRoutineDirective", test_routine_manager_check_has_routine_directive);
    G_TEST_ADD_FUNC("/core/RoutineManager/checkRoutineAlive", test_routine_manager_check_routine_alive);
    G_TEST_ADD_FUNC("/core/RoutineManager/checkConditionToStop", test_routine_manager_check_condition_to_stop);
    G_TEST_ADD_FUNC("/core/RoutineManager/checkConditionToFinishAction", test_routine_manager_check_condition_to_finish_action);
    G_TEST_ADD_FUNC("/core/RoutineManager/checkConditionToCancel", test_routine_manager_check_condition_to_cancel);
    G_TEST_ADD_FUNC("/core/RoutineManager/checkHasToSkipMedia", test_routine_manager_check_has_to_skip_media);
    G_TEST_ADD_FUNC("/core/RoutineManager/getCurrentActionToken", test_routine_manager_get_current_action_token);
    G_TEST_ADD_FUNC("/core/RoutineManager/getCountableActionInfo", test_routine_manager_get_countable_action_info);
    G_TEST_ADD_FUNC("/core/RoutineManager/reset", test_routine_manager_reset);

    return g_test_run();
}
