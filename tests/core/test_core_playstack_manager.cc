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

#include <iostream>

#include <algorithm>
#include <functional>
#include <glib.h>
#include <memory>

#include "playstack_manager.hh"

using namespace NuguCore;

static const unsigned int DEFAULT_NORMAL_HOLD_TIME_SEC = 7;
static const unsigned int DEFAULT_LONG_HOLD_TIME_SEC = 600;

class PlayStackManagerListener : public IPlayStackManagerListener {
public:
    void onStackAdded(const std::string& ps_id)
    {
        ps_ids.emplace_back(ps_id);
    }

    void onStackRemoved(const std::string& ps_id)
    {
        if (inter_hook_func)
            inter_hook_func();

        ps_ids.erase(
            std::remove_if(ps_ids.begin(), ps_ids.end(),
                [&](const std::string& element) {
                    return element == ps_id;
                }),
            ps_ids.end());
    }

    std::vector<std::string> getPlayServiceIds()
    {
        return ps_ids;
    }

    void setHookInOnStackRemoved(std::function<void()>&& hook_func)
    {
        inter_hook_func = hook_func;
    }

private:
    std::vector<std::string> ps_ids;
    std::function<void()> inter_hook_func = nullptr;
};

static NuguDirective* createDirective(const std::string& name_space, const std::string& name, const std::string& groups)
{
    return nugu_directive_new(name_space.c_str(), name.c_str(), "1.0", "msg_1", "dlg_1",
        "ref_1", "{}", groups.c_str());
}

typedef struct {
    std::shared_ptr<PlayStackManager> playstack_manager;
    std::shared_ptr<PlayStackManagerListener> playstack_manager_listener;
    std::shared_ptr<PlayStackManagerListener> playstack_manager_listener_snd;

    NuguDirective* ndir_info;
    NuguDirective* ndir_media;
    NuguDirective* ndir_disp;
    NuguDirective* ndir_expect_speech;
} TestFixture;

static void setup(TestFixture* fixture, gconstpointer user_data)
{
    fixture->playstack_manager = std::make_shared<PlayStackManager>();
    fixture->playstack_manager_listener = std::make_shared<PlayStackManagerListener>();
    fixture->playstack_manager_listener_snd = std::make_shared<PlayStackManagerListener>();

    fixture->ndir_info = createDirective("TTS", "test", "{ \"directives\": [\"TTS.Speak\"] }");
    fixture->ndir_media = createDirective("AudioPlayer", "test", "{ \"directives\": [\"AudioPlayer.Play\"] }");
    fixture->ndir_disp = createDirective("Display", "test", "{ \"directives\": [\"Display.FullText1\", \"TTS.Speak\"] }");
    fixture->ndir_expect_speech = createDirective("ASR", "test", "{ \"directives\": [\"TTS.Speak\", \"ASR.ExpectSpeech\", \"Session.Set\"] }");
}

static void teardown(TestFixture* fixture, gconstpointer user_data)
{
    nugu_directive_unref(fixture->ndir_info);
    nugu_directive_unref(fixture->ndir_media);
    nugu_directive_unref(fixture->ndir_disp);
    nugu_directive_unref(fixture->ndir_expect_speech);

    fixture->playstack_manager_listener.reset();
    fixture->playstack_manager_listener_snd.reset();
    fixture->playstack_manager.reset();
}

#define G_TEST_ADD_FUNC(name, func) \
    g_test_add(name, TestFixture, nullptr, setup, func, teardown);

static void test_playstack_manager_basic(TestFixture* fixture, gconstpointer ignored)
{
    const auto& playstack_container = fixture->playstack_manager->getPlayStackContainer();

    fixture->playstack_manager->add("", fixture->ndir_info);
    g_assert(playstack_container.first.empty());

    fixture->playstack_manager->add("ps_id_1", fixture->ndir_info);
    g_assert(!playstack_container.first.empty());

    // check adding duplicately
    fixture->playstack_manager->add("ps_id_1", fixture->ndir_info);
    g_assert(!playstack_container.first.empty());

    fixture->playstack_manager->remove("", PlayStackRemoveMode::Immediately);
    g_assert(!playstack_container.first.empty());

    fixture->playstack_manager->remove("ps_id_2", PlayStackRemoveMode::Immediately);
    g_assert(!playstack_container.first.empty());

    fixture->playstack_manager->remove("ps_id_1", PlayStackRemoveMode::Immediately);
    g_assert(playstack_container.first.empty());
}

static void test_playstack_manager_get_stack(TestFixture* fixture, gconstpointer ignored)
{
    g_assert(fixture->playstack_manager->getAllPlayStackItems().empty());

    fixture->playstack_manager->add("ps_id_1", fixture->ndir_info);
    g_assert(fixture->playstack_manager->getAllPlayStackItems().at(0) == "ps_id_1");
    g_assert(fixture->playstack_manager->getPlayStackActivity("") == PlayStackActivity::None);
    g_assert(fixture->playstack_manager->getPlayStackActivity("ps_id_1") == PlayStackActivity::TTS);

    fixture->playstack_manager->remove("ps_id_1", PlayStackRemoveMode::Immediately);
    fixture->playstack_manager->add("ps_id_2", fixture->ndir_media);
    fixture->playstack_manager->add("ps_id_3", fixture->ndir_info);
    auto play_stack_items = fixture->playstack_manager->getAllPlayStackItems();
    g_assert(play_stack_items.at(0) == "ps_id_3" && play_stack_items.at(1) == "ps_id_2");
}

static void test_playstack_manager_listener(TestFixture* fixture, gconstpointer ignored)
{
    fixture->playstack_manager->addListener(nullptr);
    g_assert(fixture->playstack_manager->getListenerCount() == 0);

    fixture->playstack_manager->addListener(fixture->playstack_manager_listener.get());
    g_assert(fixture->playstack_manager->getListenerCount() == 1);

    // check adding duplicately
    fixture->playstack_manager->addListener(fixture->playstack_manager_listener.get());
    g_assert(fixture->playstack_manager->getListenerCount() == 1);

    fixture->playstack_manager->addListener(fixture->playstack_manager_listener_snd.get());
    g_assert(fixture->playstack_manager->getListenerCount() == 2);

    fixture->playstack_manager->add("ps_id_1", fixture->ndir_info);
    g_assert(fixture->playstack_manager_listener->getPlayServiceIds().at(0) == "ps_id_1"
        && fixture->playstack_manager_listener_snd->getPlayServiceIds().at(0) == "ps_id_1");

    fixture->playstack_manager->remove("ps_id_1", PlayStackRemoveMode::Immediately);
    g_assert(fixture->playstack_manager_listener->getPlayServiceIds().empty()
        && fixture->playstack_manager_listener_snd->getPlayServiceIds().empty());

    fixture->playstack_manager->removeListener(nullptr);
    g_assert(fixture->playstack_manager->getListenerCount() == 2);

    fixture->playstack_manager->removeListener(fixture->playstack_manager_listener.get());
    g_assert(fixture->playstack_manager->getListenerCount() == 1);

    fixture->playstack_manager->removeListener(fixture->playstack_manager_listener_snd.get());
    g_assert(fixture->playstack_manager->getListenerCount() == 0);
}

static void test_playstack_manager_hold_stack(TestFixture* fixture, gconstpointer ignored)
{
    const auto& playstack_container = fixture->playstack_manager->getPlayStackContainer();

    // hold stack during 7s'
    fixture->playstack_manager->add("ps_id_1", fixture->ndir_info);
    fixture->playstack_manager->remove("ps_id_1");
    g_assert(!playstack_container.first.empty());

    // remove stack immediately
    fixture->playstack_manager->add("ps_id_2", fixture->ndir_info);
    fixture->playstack_manager->remove("ps_id_2", PlayStackRemoveMode::Immediately);
    g_assert(playstack_container.first.empty());

    // hold stack during 10m'
    fixture->playstack_manager->add("ps_id_3", fixture->ndir_info);
    fixture->playstack_manager->remove("ps_id_3", PlayStackRemoveMode::Later);
    g_assert(!playstack_container.first.empty());
}

static void test_playstack_manager_layer_policy(TestFixture* fixture, gconstpointer ignored)
{
    const auto& playstack_container = fixture->playstack_manager->getPlayStackContainer();

    // Info to Info -> replace
    fixture->playstack_manager->add("ps_id_1", fixture->ndir_info);
    fixture->playstack_manager->add("ps_id_2", fixture->ndir_info);
    g_assert(playstack_container.first.size() == 1
        && playstack_container.first.cbegin()->first == "ps_id_2"
        && playstack_container.first.cbegin()->second == PlayStackActivity::TTS);

    // Info to Media -> replace
    fixture->playstack_manager->add("ps_id_3", fixture->ndir_media);
    g_assert(playstack_container.first.size() == 1
        && playstack_container.first.cbegin()->first == "ps_id_3"
        && playstack_container.first.cbegin()->second == PlayStackActivity::Media);

    // Media to Info -> stacked
    fixture->playstack_manager->add("ps_id_4", fixture->ndir_info);
    g_assert(playstack_container.first.size() == 2);

    // Media to Media -> replace
    fixture->playstack_manager->remove("ps_id_4", PlayStackRemoveMode::Immediately);
    fixture->playstack_manager->add("ps_id_5", fixture->ndir_media);
    g_assert(playstack_container.first.size() == 1
        && playstack_container.first.cbegin()->first == "ps_id_5"
        && playstack_container.first.cbegin()->second == PlayStackActivity::Media);
}

static void test_playstack_manager_control_holding(TestFixture* fixture, gconstpointer ignored)
{
    const auto& playstack_container = fixture->playstack_manager->getPlayStackContainer();

    // stop holding
    fixture->playstack_manager->add("ps_id_1", fixture->ndir_info);
    fixture->playstack_manager->remove("ps_id_1");
    bool has_stack_before = !playstack_container.first.empty();
    fixture->playstack_manager->stopHolding();
    g_assert(has_stack_before && !playstack_container.first.empty());

    // reset holding
    fixture->playstack_manager->resetHolding();
    g_assert(fixture->playstack_manager->isActiveHolding());

    // clear holding
    fixture->playstack_manager->clearHolding();
    g_assert(!fixture->playstack_manager->isActiveHolding());
}

static void test_playstack_manager_check_stack(TestFixture* fixture, gconstpointer ignored)
{
    fixture->playstack_manager->add("ps_id_1", fixture->ndir_info);
    g_assert(!fixture->playstack_manager->isStackedCondition(fixture->ndir_info));

    fixture->playstack_manager->remove("ps_id_1", PlayStackRemoveMode::Immediately);
    fixture->playstack_manager->add("ps_id_2", fixture->ndir_media);
    g_assert(fixture->playstack_manager->isStackedCondition(fixture->ndir_info));
    g_assert(!fixture->playstack_manager->isStackedCondition(fixture->ndir_media));
}

static void test_playstack_manager_check_expect_speech(TestFixture* fixture, gconstpointer ignored)
{
    g_assert(!fixture->playstack_manager->hasExpectSpeech(nullptr));
    g_assert(!fixture->playstack_manager->hasExpectSpeech(fixture->ndir_info));
    g_assert(fixture->playstack_manager->hasExpectSpeech(fixture->ndir_expect_speech));
}

static void test_playstack_manager_check_adding_playstack(TestFixture* fixture, gconstpointer ignored)
{
    fixture->playstack_manager->addListener(fixture->playstack_manager_listener.get());

    fixture->playstack_manager->add("ps_id_1", fixture->ndir_info);
    g_assert(fixture->playstack_manager_listener->getPlayServiceIds().at(0) == "ps_id_1");

    // has not display rendering info (TTS only)
    fixture->playstack_manager_listener->setHookInOnStackRemoved([&]() {
        g_assert(!fixture->playstack_manager->hasAddingPlayStack());
    });

    fixture->playstack_manager->add("ps_id_2", fixture->ndir_info);
    g_assert(fixture->playstack_manager_listener->getPlayServiceIds().at(0) == "ps_id_2");
    g_assert(!fixture->playstack_manager->hasAddingPlayStack());

    // has display rendering info (Display, AudioPlayer)
    fixture->playstack_manager_listener->setHookInOnStackRemoved([&]() {
        g_assert(fixture->playstack_manager->hasAddingPlayStack());
    });

    fixture->playstack_manager->add("ps_id_3", fixture->ndir_disp);
    g_assert(fixture->playstack_manager_listener->getPlayServiceIds().at(0) == "ps_id_3");
    g_assert(!fixture->playstack_manager->hasAddingPlayStack());

    fixture->playstack_manager->add("ps_id_4", fixture->ndir_media);
    g_assert(fixture->playstack_manager_listener->getPlayServiceIds().at(0) == "ps_id_4");
    g_assert(!fixture->playstack_manager->hasAddingPlayStack());
}

static void test_playstack_manager_adjust_playstack_hold_time(TestFixture* fixture, gconstpointer ignored)
{
    // check default hold times
    const auto& hold_times(fixture->playstack_manager->getPlayStackHoldTime());
    g_assert(hold_times.normal_time == DEFAULT_NORMAL_HOLD_TIME_SEC);
    g_assert(hold_times.long_time == DEFAULT_LONG_HOLD_TIME_SEC);

    // check custom hold times
    fixture->playstack_manager->setPlayStackHoldTime({ 5, 10 });
    g_assert(hold_times.normal_time == 5);
    g_assert(hold_times.long_time == 10);

    // check whether setting long time by default value if not pass 2nd argument
    fixture->playstack_manager->setPlayStackHoldTime({ 15 });
    g_assert(hold_times.normal_time == 15);
    g_assert(hold_times.long_time == DEFAULT_LONG_HOLD_TIME_SEC);
}

static void test_playstack_manager_reset_playstack_hold_time(TestFixture* fixture, gconstpointer ignored)
{
    const auto& hold_times(fixture->playstack_manager->getPlayStackHoldTime());
    fixture->playstack_manager->setPlayStackHoldTime({ 5, 10 });
    g_assert(hold_times.normal_time == 5);
    g_assert(hold_times.long_time == 10);

    // reset hold time
    fixture->playstack_manager->resetPlayStackHoldTime();
    g_assert(hold_times.normal_time == DEFAULT_NORMAL_HOLD_TIME_SEC);
    g_assert(hold_times.long_time == DEFAULT_LONG_HOLD_TIME_SEC);
}

static void test_playstack_manager_reset(TestFixture* fixture, gconstpointer ignored)
{
    const auto& playstack_container = fixture->playstack_manager->getPlayStackContainer();

    fixture->playstack_manager->add("ps_id_1", fixture->ndir_expect_speech);
    auto flag_set_before = fixture->playstack_manager->getFlagSet();
    g_assert(flag_set_before.find(true) != flag_set_before.cend());
    g_assert(!playstack_container.first.empty());

    fixture->playstack_manager->add("ps_id_2", fixture->ndir_info);
    fixture->playstack_manager->remove("ps_id_2");
    g_assert(fixture->playstack_manager->isActiveHolding());

    fixture->playstack_manager->reset();
    auto flag_set_after = fixture->playstack_manager->getFlagSet();
    g_assert(flag_set_after.find(true) == flag_set_after.cend());
    g_assert(playstack_container.first.empty());
    g_assert(!fixture->playstack_manager->isActiveHolding());
    g_assert(!fixture->playstack_manager->hasAddingPlayStack());
}

int main(int argc, char* argv[])
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
    g_type_init();
#endif

    g_test_init(&argc, &argv, (void*)NULL);
    g_log_set_always_fatal((GLogLevelFlags)G_LOG_FATAL_MASK);

    G_TEST_ADD_FUNC("/core/PlayStackManager/basic", test_playstack_manager_basic);
    G_TEST_ADD_FUNC("/core/PlayStackManager/getStack", test_playstack_manager_get_stack);
    G_TEST_ADD_FUNC("/core/PlayStackManager/listener", test_playstack_manager_listener);
    G_TEST_ADD_FUNC("/core/PlayStackManager/holdStack", test_playstack_manager_hold_stack);
    G_TEST_ADD_FUNC("/core/PlayStackManager/layerPolicy", test_playstack_manager_layer_policy);
    G_TEST_ADD_FUNC("/core/PlayStackManager/controlHolding", test_playstack_manager_control_holding);
    G_TEST_ADD_FUNC("/core/PlayStackManager/checkStack", test_playstack_manager_check_stack);
    G_TEST_ADD_FUNC("/core/PlayStackManager/checkExpectSpeech", test_playstack_manager_check_expect_speech);
    G_TEST_ADD_FUNC("/core/PlayStackManager/checkAddingPlayStack", test_playstack_manager_check_adding_playstack);
    G_TEST_ADD_FUNC("/core/PlayStackManager/adjustPlayStackHoldTime", test_playstack_manager_adjust_playstack_hold_time);
    G_TEST_ADD_FUNC("/core/PlayStackManager/resetPlayStackHoldTime", test_playstack_manager_reset_playstack_hold_time);
    G_TEST_ADD_FUNC("/core/PlayStackManager/reset", test_playstack_manager_reset);

    return g_test_run();
}
