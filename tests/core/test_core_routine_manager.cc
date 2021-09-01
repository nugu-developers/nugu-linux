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

using RoutineTestData = struct {
    Json::Value type;
    Json::Value play_service_id;
    Json::Value text;
    Json::Value data;
    Json::Value post_delay_msec;
};

using DirectiveElement = struct {
    std::string name_space;
    std::string name;
    std::string dialog_id;
    std::string groups;
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
    void composeActions(std::vector<RoutineTestData> test_datas)
    {
        actions.clear();

        for (auto& element : test_datas) {
            Json::Value action;
            action["type"] = element.type;
            action["playServiceId"] = element.play_service_id;

            if (!element.text.empty())
                action["text"] = element.text;

            if (!element.data.isNull())
                action["data"] = element.data;

            if (element.post_delay_msec > 0)
                action["postDelayInMilliseconds"] = element.post_delay_msec;

            actions.append(action);
        }
    }

    Json::Value getActions()
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

private:
    Json::Value actions;
    std::string token = "token";
};

class RoutineManagerListener : public IRoutineManagerListener {
public:
    void onActivity(RoutineActivity activity) override
    {
        this->activity = activity;
    }

    RoutineActivity getActivity()
    {
        return activity;
    }

private:
    RoutineActivity activity = RoutineActivity::IDLE;
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

typedef struct {
    std::shared_ptr<RoutineManager> routine_manager;
    std::shared_ptr<RoutineTestHelper> routine_helper;
    std::shared_ptr<RoutineManagerListener> routine_manager_listener;
    std::shared_ptr<RoutineManagerListener> routine_manager_listener_snd;
    FakeTimer* fake_timer;
    NuguDirective* ndir_info;
    NuguDirective* ndir_info_disp;
    NuguDirective* ndir_info_tts;
    NuguDirective* ndir_routine;
    NuguDirective* ndir_media;
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
    fixture->routine_manager->setTextRequester(
        [](const std::string& token, const std::string& text, const std::string& ps_id) {
            return ps_id == "ps_id_1" ? "dialog_1" : "dialog_2";
        });
    fixture->routine_manager->setDataRequester(
        [](const std::string& ps_id, const Json::Value& data) {
            return "dialog_3";
        });
    fixture->routine_helper->composeActions(
        {
            { "TEXT", "ps_id_1", "text_1", Json::nullValue },
            { "TEXT", "ps_id_2", "text_2", Json::nullValue },
            { "DATA", "ps_id_3", "", Json::Value("DATA_VALUE") },
        });

    fixture->ndir_info = createDirective({ "TTS", "Speak", "dialog_1",
        "{ \"directives\": [\"TTS.Speak\"] }" });
    fixture->ndir_info_disp = createDirective({ "Display", "FullText2", "dialog_2",
        "{ \"directives\": [\"Display.FullText2\", \"TTS.Speak\"] }" });
    fixture->ndir_info_tts = createDirective({ "TTS", "Speak", "dialog_2",
        "{ \"directives\": [\"Display.FullText2\", \"TTS.Speak\"] }" });
    fixture->ndir_routine = createDirective({ "Routine", "Continue", "dialog_1",
        "{ \"directives\": [\"TTS.Speak\", \"Routine.Continue\"] }" });
    fixture->ndir_media = createDirective({ "AudioPlayer", "Play", "dialog_1",
        "{ \"directives\": [\"TTS.Speak\", \"AudioPlayer.Play\"] }" });
    fixture->ndir_dm = createDirective({ "ASR", "ExpectSpeech", "dialog_1",
        "{ \"directives\": [\"TTS.Speak\", \"ASR.ExpectSpeech\"] }" });
}

static void teardown(TestFixture* fixture, gconstpointer user_data)
{
    nugu_directive_unref(fixture->ndir_info);
    nugu_directive_unref(fixture->ndir_info_disp);
    nugu_directive_unref(fixture->ndir_info_tts);
    nugu_directive_unref(fixture->ndir_routine);
    nugu_directive_unref(fixture->ndir_media);
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

    // check adding duplicately
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

    g_assert(!fixture->routine_manager->start(fixture->routine_helper->getToken(), Json::arrayValue));
    g_assert(action_container.empty() && fixture->routine_manager->getCurrentActionIndex() == 0);

    // remove previous actions when calling start
    g_assert(fixture->routine_manager->start(fixture->routine_helper->getToken(), fixture->routine_helper->getActions()));
    g_assert(fixture->routine_manager->start(fixture->routine_helper->getToken(), fixture->routine_helper->getActions()));
    g_assert(action_container.size() == 2);
}

static void test_routine_manager_compose_action_queue(TestFixture* fixture, gconstpointer ignored)
{
    fixture->routine_helper->composeActions(
        {
            { "", "ps_id_0", "text_0", Json::nullValue },
            { Json::nullValue, "ps_id_0", "", Json::nullValue },

            { "TEXT", "ps_id_1", "text_1", Json::nullValue },
            { "TEXT", "", "text_2", Json::nullValue },
            { "TEXT", Json::nullValue, "text_3", Json::nullValue },

            { "DATA", "ps_id_2", "", Json::Value("DATA_VALUE") },
            { "DATA", "ps_id_2", "", Json::nullValue },
            { "DATA", "", "", Json::Value("DATA_VALUE") },
            { "DATA", Json::nullValue, "", Json::Value("DATA_VALUE") },
        });

    g_assert(fixture->routine_manager->start(fixture->routine_helper->getToken(), fixture->routine_helper->getActions()));
    g_assert(fixture->routine_manager->getActionContainer().size() == 3);
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

static void sub_test_routine_manager_preset_start(TestFixture* fixture, gconstpointer ignored)
{
    const auto& action_container = fixture->routine_manager->getActionContainer();

    fixture->routine_manager->addListener(fixture->routine_manager_listener.get());
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::IDLE);

    g_assert(fixture->routine_manager->start(fixture->routine_helper->getToken(), fixture->routine_helper->getActions()));
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::PLAYING);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 1);
    g_assert(!action_container.empty());
}

static void test_routine_manager_interrupt(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_routine_manager_preset_start(fixture, ignored);

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

static void test_routine_manager_stop(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_routine_manager_preset_start(fixture, ignored);

    const auto& action_container = fixture->routine_manager->getActionContainer();

    fixture->routine_manager->stop();
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::STOPPED);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 0);
    g_assert(action_container.empty());
}

static void sub_test_routine_manager_preset_post_delay_actions(TestFixture* fixture, gconstpointer ignored)
{
    fixture->routine_helper->composeActions(
        {
            { "TEXT", "ps_id_1", "text_1", Json::nullValue, 2000 },
            { "TEXT", "ps_id_2", "text_2", Json::nullValue, 3000 },
            { "DATA", "ps_id_3", "", Json::Value("DATA_VALUE") },
            { "DATA", "ps_id_4", "", Json::Value("DATA_VALUE") },
        });

    g_assert(fixture->routine_manager->start(fixture->routine_helper->getToken(), fixture->routine_helper->getActions()));
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 1);
}

static void test_routine_manager_stop_in_post_delayed(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_routine_manager_preset_post_delay_actions(fixture, ignored);

    // hold by interrupt
    fixture->routine_manager->finish();
    fixture->routine_manager->interrupt();
    onTimeElapsed(fixture);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 1);

    // stop
    fixture->routine_manager->stop();
    g_assert(!fixture->routine_manager->hasPostDelayed());
}

static void test_routine_manager_continue(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_routine_manager_preset_start(fixture, ignored);

    // execute next second actionx
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
    sub_test_routine_manager_preset_post_delay_actions(fixture, ignored);

    // hold by interrupt
    fixture->routine_manager->finish();
    fixture->routine_manager->interrupt();
    onTimeElapsed(fixture);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 1);

    // resume
    fixture->routine_manager->resume();
    onTimeElapsed(fixture);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 2);
}

static void test_routine_manager_post_delay_among_actions(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_routine_manager_preset_post_delay_actions(fixture, ignored);

    // apply delay about 2000 msec
    fixture->routine_manager->finish();
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 1);
    onTimeElapsed(fixture);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 2);

    // apply delay about 3000 msec
    fixture->routine_manager->finish();
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 2);
    onTimeElapsed(fixture);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 3);

    // no delay
    fixture->routine_manager->finish();
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 4);
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

    sub_test_routine_manager_preset_start(fixture, ignored);
    g_assert(fixture->routine_manager->isRoutineAlive());

    fixture->routine_manager->interrupt();
    g_assert(fixture->routine_manager->isRoutineAlive());

    fixture->routine_manager->stop();
    g_assert(!fixture->routine_manager->isRoutineAlive());
}

static void test_routine_manager_check_condition_to_stop(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_routine_manager_preset_start(fixture, ignored);

    g_assert(!fixture->routine_manager->isConditionToStop(nullptr));

    // The received directive is result of routine action. (not stop)
    g_assert(fixture->routine_manager->isRoutineAlive());
    g_assert(fixture->routine_manager->isActionProgress("dialog_1"));
    g_assert(!fixture->routine_manager->isConditionToStop(fixture->ndir_info));
    g_assert(!fixture->routine_manager->isConditionToStop(fixture->ndir_routine));

    // The received directive has ASR.ExpectSpeech or AudioPlayer.Play in progressing middle action. (stop)
    g_assert(fixture->routine_manager->isConditionToStop(fixture->ndir_dm));
    g_assert(fixture->routine_manager->isConditionToStop(fixture->ndir_media));

    // The received directive is for another play, not routine action. (stop if routine directive not exist)
    fixture->routine_manager->finish();
    g_assert(fixture->routine_manager->isActionProgress("dialog_2"));
    g_assert(fixture->routine_manager->isConditionToStop(fixture->ndir_info));
    g_assert(!fixture->routine_manager->isConditionToStop(fixture->ndir_routine));

    // progress last action
    fixture->routine_manager->finish();
    g_assert(fixture->routine_manager->isRoutineAlive());
    g_assert(fixture->routine_manager->isActionProgress("dialog_3"));

    // The received directive has ASR.ExpectSpeech or AudioPlayer.Play in progressing last action. (not stop)
    changeDialogId(fixture->ndir_dm, "dialog_3");
    changeDialogId(fixture->ndir_media, "dialog_3");
    g_assert(!fixture->routine_manager->isConditionToStop(fixture->ndir_dm));
    g_assert(!fixture->routine_manager->isConditionToStop(fixture->ndir_media));

    // The routine is not activated. (not stop)
    fixture->routine_manager->stop();
    g_assert(!fixture->routine_manager->isRoutineAlive());
    g_assert(!fixture->routine_manager->isConditionToStop(fixture->ndir_info));
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

static void test_routine_manager_reset(TestFixture* fixture, gconstpointer ignored)
{
    const auto& action_container = fixture->routine_manager->getActionContainer();
    const auto& action_dialogs = fixture->routine_manager->getActionDialogs();
    fixture->routine_manager->addListener(fixture->routine_manager_listener.get());

    fixture->routine_helper->composeActions(
        {
            { "TEXT", "ps_id_1", "text_1", Json::nullValue, 2000 },
            { "DATA", "ps_id_2", "", Json::Value("DATA_VALUE") },
        });

    g_assert(fixture->routine_manager->start(fixture->routine_helper->getToken(), fixture->routine_helper->getActions()));
    g_assert(fixture->routine_manager_listener->getActivity() == RoutineActivity::PLAYING);
    g_assert(fixture->routine_manager->getCurrentActionIndex() == 1);
    g_assert(!fixture->routine_manager->getToken().empty());
    g_assert(fixture->routine_manager->hasPostDelayed());
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
    g_assert(fixture->routine_manager->getToken().empty());
    g_assert(!fixture->routine_manager->isInterrupted());
    g_assert(!fixture->routine_manager->hasPostDelayed());
    g_assert(action_container.empty());
    g_assert(action_dialogs.empty());
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
    G_TEST_ADD_FUNC("/core/RoutineManager/composeActionQueue", test_routine_manager_compose_action_queue);
    G_TEST_ADD_FUNC("/core/RoutineManager/normal", test_routine_manager_normal);
    G_TEST_ADD_FUNC("/core/RoutineManager/interrupt", test_routine_manager_interrupt);
    G_TEST_ADD_FUNC("/core/RoutineManager/stop", test_routine_manager_stop);
    G_TEST_ADD_FUNC("/core/RoutineManager/stopInPostDelayed", test_routine_manager_stop_in_post_delayed);
    G_TEST_ADD_FUNC("/core/RoutineManager/continue", test_routine_manager_continue);
    G_TEST_ADD_FUNC("/core/RoutineManager/continueInPostDelayed", test_routine_manager_continue_in_post_delayed);
    G_TEST_ADD_FUNC("/core/RoutineManager/postDelayAmongActions", test_routine_manager_post_delay_among_actions);
    G_TEST_ADD_FUNC("/core/RoutineManager/checkHasRoutineDirective", test_routine_manager_check_has_routine_directive);
    G_TEST_ADD_FUNC("/core/RoutineManager/checkRoutineAlive", test_routine_manager_check_routine_alive);
    G_TEST_ADD_FUNC("/core/RoutineManager/checkConditionToStop", test_routine_manager_check_condition_to_stop);
    G_TEST_ADD_FUNC("/core/RoutineManager/checkConditionToFinishAction", test_routine_manager_check_condition_to_finish_action);
    G_TEST_ADD_FUNC("/core/RoutineManager/reset", test_routine_manager_reset);

    return g_test_run();
}
