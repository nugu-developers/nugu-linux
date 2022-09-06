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

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <glib.h>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>

#include "interaction_control_manager.hh"
#include "playsync_manager.hh"

using namespace NuguCore;

const unsigned int TIMER_DOWNSCALE_FACTOR = 50;
std::mutex mutex;
std::condition_variable cv;

class FakeStackTimer final : public IStackTimer {
public:
    FakeStackTimer(std::mutex& mutex, std::condition_variable& cv)
        : _mutex(mutex)
        , _cv(cv)
    {
    }

    virtual ~FakeStackTimer()
    {
        stop();
    }

    bool isStarted() override
    {
        return is_started;
    }

    void start(unsigned int sec = 0) override
    {
        if (is_started)
            return;

        setStarted(true);

        worker = std::thread { [&]() {
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

        if (worker.joinable())
            worker.join();
    }

    unsigned int getPlayStackHoldTime()
    {
        return getInterval() / 1000;
    }

    bool isRestarted()
    {
        return started_count > 1;
    }

    void reset()
    {
        started_count = 0;

        if (worker.joinable())
            worker.join();
    }

private:
    void setStarted(bool is_started)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        this->is_started = is_started;

        if (is_started)
            started_count++;
    }

    std::thread worker;
    bool is_started = false;
    std::mutex& _mutex;
    std::condition_variable& _cv;
    int started_count = 0;
};

struct DummyExtraInfo {
    std::string id;
};

class PlaySyncManagerListener : public IPlaySyncManagerListener {
public:
    void onSyncState(const std::string& ps_id, PlaySyncState state, void* extra_data) override
    {
        if (this->state != state) {
            this->state = state;
            this->extra_data = extra_data;
            same_state_call_count = 0;
        } else {
            same_state_call_count++;
        }

        sync_state_map[ps_id] = state;

        if (state == PlaySyncState::Released && inter_hook_func)
            inter_hook_func();
    }

    void onDataChanged(const std::string& ps_id, std::pair<void*, void*> extra_datas) override
    {
        this->extra_data = extra_datas.second;
    }

    void onStackChanged(const std::pair<std::string, std::string>& ps_ids) override
    {
        if (!ps_ids.first.empty())
            playstacks.emplace(ps_ids.first);

        if (!ps_ids.second.empty())
            playstacks.erase(ps_ids.second);
    }

    PlaySyncState getSyncState(const std::string& ps_id)
    {
        try {
            return sync_state_map.at(ps_id);
        } catch (std::out_of_range& exception) {
            return PlaySyncState::None;
        }
    }

    bool hasPlayStack(const std::string& ps_id)
    {
        return sync_state_map.find(ps_id) != sync_state_map.cend();
    }

    int getSameStateCallCount()
    {
        return same_state_call_count;
    }

    void* getExtraData()
    {
        return extra_data;
    }

    void setHookInonSyncState(std::function<void()>&& hook_func)
    {
        inter_hook_func = hook_func;
    }

    const std::set<std::string>& getPlaystacks()
    {
        return playstacks;
    }

private:
    std::map<std::string, PlaySyncState> sync_state_map;
    std::set<std::string> playstacks;
    PlaySyncState state = PlaySyncState::None;
    void* extra_data = nullptr;
    int same_state_call_count = 0;
    std::function<void()> inter_hook_func = nullptr;
};

class InteractionControlManagerListener : public IInteractionControlManagerListener {
public:
    void onHasMultiTurn() override
    {
        has_multi_turn_notified_count++;
    }

    void onModeChanged(bool is_multi_turn) override
    {
        if (!is_multi_turn)
            has_multi_turn_notified_count = 0;
    }

    int getHasMultiTurnNotifiedCount()
    {
        return has_multi_turn_notified_count;
    }

private:
    int has_multi_turn_notified_count = 0;
};

static NuguDirective* createDirective(const std::string& name_space, const std::string& name,
    const std::string& groups, const std::string& dialog_id)
{
    return nugu_directive_new(name_space.c_str(), name.c_str(), "1.0", "msg_1", dialog_id.c_str(),
        "ref_1", "{}", groups.c_str());
}

typedef struct {
    std::shared_ptr<PlaySyncManager> playsync_manager;
    std::shared_ptr<PlaySyncManagerListener> playsync_manager_listener;
    std::shared_ptr<PlaySyncManagerListener> playsync_manager_listener_snd;
    std::shared_ptr<InteractionControlManager> ic_manager;
    std::shared_ptr<InteractionControlManagerListener> ic_manager_listener;
    FakeStackTimer* fake_timer;
    DummyExtraInfo* dummy_extra_data;
    DummyExtraInfo* dummy_extra_data_snd;

    NuguDirective* ndir_info_disp;
    NuguDirective* ndir_media;
    NuguDirective* ndir_expect_speech;
    NuguDirective* ndir_expect_speech_second;
    NuguDirective* ndir_expect_speech_third;
    NuguDirective* ndir_nudge;
    NuguDirective* ndir_alerts;
    NuguDirective* ndir_disp_expect_speech;

} TestFixture;

static void setup(TestFixture* fixture, gconstpointer user_data)
{
    fixture->playsync_manager = std::make_shared<PlaySyncManager>();
    fixture->playsync_manager_listener = std::make_shared<PlaySyncManagerListener>();
    fixture->playsync_manager_listener_snd = std::make_shared<PlaySyncManagerListener>();
    fixture->ic_manager = std::make_shared<InteractionControlManager>();
    fixture->ic_manager_listener = std::make_shared<InteractionControlManagerListener>();
    fixture->fake_timer = new FakeStackTimer(mutex, cv);

    PlayStackManager* playstack_manager = new PlayStackManager();
    playstack_manager->setTimer(fixture->fake_timer);
    playstack_manager->setPlayStackHoldTime({ 7, 60 });
    fixture->ic_manager->addListener(fixture->ic_manager_listener.get());

    fixture->playsync_manager->setPlayStackManager(playstack_manager);
    fixture->playsync_manager->setInteractionControlManager(fixture->ic_manager.get());
    fixture->playsync_manager->addListener("TTS", fixture->playsync_manager_listener.get());

    fixture->dummy_extra_data = new DummyExtraInfo();
    fixture->dummy_extra_data_snd = new DummyExtraInfo();

    fixture->ndir_info_disp = createDirective("TTS", "Speak",
        "{ \"directives\": [\"TTS.Speak\", \"Display.FullText1\"] }", "dlg_1");
    fixture->ndir_media = createDirective("AudioPlayer", "Play",
        "{ \"directives\": [\"TTS.Speak\", \"AudioPlayer.Play\"] }", "dlg_1");
    fixture->ndir_expect_speech = createDirective("ASR", "test",
        "{ \"directives\": [\"TTS.Speak\", \"ASR.ExpectSpeech\", \"Session.Set\"] }", "dlg_1");
    fixture->ndir_expect_speech_second = createDirective("ASR", "test",
        "{ \"directives\": [\"TTS.Speak\", \"ASR.ExpectSpeech\", \"Session.Set\"] }", "dlg_2");
    fixture->ndir_expect_speech_third = createDirective("ASR", "test",
        "{ \"directives\": [\"TTS.Speak\", \"ASR.ExpectSpeech\", \"Session.Set\"] }", "dlg_3");
    fixture->ndir_nudge = createDirective("ASR", "test",
        "{ \"directives\": [\"TTS.Speak\", \"ASR.ExpectSpeech\", \"Session.Set\", \"Nudge.Append\"] }", "dlg_1");
    fixture->ndir_alerts = createDirective("Alerts", "SetAlert",
        "{ \"directives\": [\"Alerts.SetAlert\"] }", "dlg_1");
    fixture->ndir_disp_expect_speech = createDirective("Display", "test",
        "{ \"directives\": [\"Display.FullText1\", \"TTS.Speak\", \"ASR.ExpectSpeech\"] }", "dlg_1");
}

static void teardown(TestFixture* fixture, gconstpointer user_data)
{
    nugu_directive_unref(fixture->ndir_info_disp);
    nugu_directive_unref(fixture->ndir_media);
    nugu_directive_unref(fixture->ndir_expect_speech);
    nugu_directive_unref(fixture->ndir_expect_speech_second);
    nugu_directive_unref(fixture->ndir_expect_speech_third);
    nugu_directive_unref(fixture->ndir_nudge);
    nugu_directive_unref(fixture->ndir_alerts);
    nugu_directive_unref(fixture->ndir_disp_expect_speech);

    fixture->playsync_manager_listener.reset();
    fixture->playsync_manager_listener_snd.reset();
    fixture->playsync_manager.reset();
    fixture->ic_manager_listener.reset();
    fixture->ic_manager.reset();

    delete fixture->dummy_extra_data;
    delete fixture->dummy_extra_data_snd;

    fixture->dummy_extra_data = nullptr;
    fixture->dummy_extra_data_snd = nullptr;
}

static void onTimeElapsed(TestFixture* fixture)
{
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&] { return !fixture->fake_timer->isStarted(); });
    fixture->fake_timer->reset();
}

#define G_TEST_ADD_FUNC(name, func) \
    g_test_add(name, TestFixture, nullptr, setup, func, teardown);

static void test_playsync_manager_listener(TestFixture* fixture, gconstpointer ignored)
{
    // It already has one listener which is added in setup.

    // check invalid parameter
    fixture->playsync_manager->addListener("", fixture->playsync_manager_listener.get());
    g_assert(fixture->playsync_manager->getListenerCount() == 1);

    // check whether same listener is added duplicately
    fixture->playsync_manager->addListener("TTS", fixture->playsync_manager_listener.get());
    g_assert(fixture->playsync_manager->getListenerCount() == 1);

    // check invalid parameter
    fixture->playsync_manager->addListener("Display", nullptr);
    g_assert(fixture->playsync_manager->getListenerCount() == 1);

    fixture->playsync_manager->addListener("Display", fixture->playsync_manager_listener_snd.get());
    g_assert(fixture->playsync_manager->getListenerCount() == 2);

    // check invalid parameter
    fixture->playsync_manager->removeListener("");
    g_assert(fixture->playsync_manager->getListenerCount() == 2);

    fixture->playsync_manager->removeListener("TTS");
    g_assert(fixture->playsync_manager->getListenerCount() == 1);

    // check whether same listener is removed duplicately
    fixture->playsync_manager->removeListener("TTS");
    g_assert(fixture->playsync_manager->getListenerCount() == 1);

    fixture->playsync_manager->removeListener("Display");
    g_assert(fixture->playsync_manager->getListenerCount() == 0);
}

static void test_playsync_manager_prepare_sync(TestFixture* fixture, gconstpointer ignored)
{
    const auto& playstacks = fixture->playsync_manager->getPlayStacks();

    // check invalid parameter
    fixture->playsync_manager->prepareSync("", fixture->ndir_info_disp);
    g_assert(playstacks.empty());

    // check invalid parameter
    fixture->playsync_manager->prepareSync("ps_id_1", nullptr);
    g_assert(playstacks.empty());

    fixture->playsync_manager->prepareSync("ps_id_1", fixture->ndir_info_disp);
    const auto& playsync_container = playstacks.at("ps_id_1");
    g_assert(playsync_container.at("TTS").first == PlaySyncState::Prepared);
    g_assert(playsync_container.at("Display").first == PlaySyncState::Prepared);
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Prepared);
}

static void test_playsync_manager_start_sync(TestFixture* fixture, gconstpointer ignored)
{
    const auto& playstacks = fixture->playsync_manager->getPlayStacks();

    // try to start sync before prepare sync
    fixture->playsync_manager->startSync("ps_id_1", "TTS");
    g_assert(playstacks.find("ps_id_1") == playstacks.cend());
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::None);
    g_assert(!fixture->playsync_manager_listener->hasPlayStack("ps_id_1"));

    fixture->playsync_manager->prepareSync("ps_id_1", fixture->ndir_info_disp);
    const auto& playsync_container = playstacks.at("ps_id_1");
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Prepared);

    // check invalid parameter
    fixture->playsync_manager->startSync("", "TTS");
    g_assert(playsync_container.at("TTS").first == PlaySyncState::Prepared);

    // check invalid parameter
    fixture->playsync_manager->startSync("ps_id_1", "");
    g_assert(playsync_container.at("TTS").first == PlaySyncState::Prepared);

    // try to start sync not prepared play service id
    fixture->playsync_manager->startSync("ps_id_2", "TTS");
    g_assert(playsync_container.at("TTS").first == PlaySyncState::Prepared);

    fixture->playsync_manager->startSync("ps_id_1", "TTS");
    g_assert(playsync_container.at("TTS").first == PlaySyncState::Synced);
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Prepared);
    g_assert(fixture->playsync_manager_listener->hasPlayStack("ps_id_1"));

    // try to start sync not prepared play service id
    fixture->playsync_manager->startSync("ps_id_2", "Display");
    g_assert(playsync_container.at("Display").first == PlaySyncState::Prepared);

    fixture->playsync_manager->startSync("ps_id_1", "Display");
    g_assert(playsync_container.at("Display").first == PlaySyncState::Synced);
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Synced);
    g_assert(fixture->playsync_manager_listener->hasPlayStack("ps_id_1"));
    g_assert(fixture->playsync_manager_listener->getSameStateCallCount() == 0);

    // check whether onSyncState is called just one time when all items are synced
    fixture->playsync_manager->startSync("ps_id_1", "Display");
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Synced);
    g_assert(fixture->playsync_manager_listener->getSameStateCallCount() == 0);
}

static void test_playsync_manager_append_sync(TestFixture* fixture, gconstpointer ignored)
{
    const auto& playstacks = fixture->playsync_manager->getPlayStacks();
    fixture->playsync_manager->addListener("Display", fixture->playsync_manager_listener_snd.get());

    /***************************************************************************
     * 1) W / "call me"
     * 2) receive [TTS.Speak, ASR.ExpectSpeech, Session.Set] directives
     **************************************************************************/
    fixture->playsync_manager->prepareSync("ps_id_1", fixture->ndir_expect_speech);
    const auto& playsync_container = playstacks.at("ps_id_1");
    g_assert(playsync_container.at("TTS").first == PlaySyncState::Prepared);
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Prepared);

    fixture->playsync_manager->startSync("ps_id_1", "TTS");
    g_assert(playsync_container.at("TTS").first == PlaySyncState::Synced);
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Synced);

    /***************************************************************************
     * 3) DM / "contact 1"
     * 4) receive [PhoneCall.SendCandidates] directive
     * 5) send [PhoneCall.CandidatesListed] event
     * 6) receive [Display.FullText1, TTS.Speak, ASR.ExpectSpeech] directives
     **************************************************************************/
    auto handle_display([&](PlaySyncState state, DummyExtraInfo&& extra_info) {
        fixture->playsync_manager->prepareSync("ps_id_1", fixture->ndir_disp_expect_speech);
        g_assert(playsync_container.at("Display").first == state);

        fixture->playsync_manager->startSync("ps_id_1", "Display", &extra_info);
        g_assert(playsync_container.at("Display").first == PlaySyncState::Synced);
        g_assert(fixture->playsync_manager_listener_snd->getSyncState("ps_id_1") == PlaySyncState::Synced);
        g_assert(fixture->playsync_manager_listener->getSameStateCallCount() == 0);

        auto listener_extra_info(fixture->playsync_manager_listener_snd->getExtraData());
        g_assert(reinterpret_cast<DummyExtraInfo*>(listener_extra_info)->id == extra_info.id);
    });

    handle_display(PlaySyncState::Appending, { "100" });
    handle_display(PlaySyncState::Synced, { "200" }); // display already synced

    // release sync
    fixture->playsync_manager->releaseSyncImmediately("ps_id_1", "TTS");
    g_assert(playstacks.find("ps_id_1") == playstacks.cend());
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Released);
    g_assert(fixture->playsync_manager_listener_snd->getSyncState("ps_id_1") == PlaySyncState::Released);
}

static void sub_test_playsync_manager_preset_sync(TestFixture* fixture, std::string&& ps_id)
{
    const auto& playstacks = fixture->playsync_manager->getPlayStacks();

    /***************************************************************************
     * [1] handle in DisplayAgent
     **************************************************************************/
    fixture->playsync_manager->prepareSync(ps_id, fixture->ndir_info_disp);
    const auto& playsync_container = playstacks.at(ps_id);
    g_assert(playsync_container.at("TTS").first == PlaySyncState::Prepared);
    g_assert(playsync_container.at("Display").first == PlaySyncState::Prepared);
    g_assert(fixture->playsync_manager_listener->getSyncState(ps_id) == PlaySyncState::Prepared);

    fixture->playsync_manager->startSync(ps_id, "Display");
    g_assert(playsync_container.at("Display").first == PlaySyncState::Synced);
    g_assert(fixture->playsync_manager_listener->getSyncState(ps_id) == PlaySyncState::Prepared);

    /***************************************************************************
     * [2] handle in TTSAgent
     **************************************************************************/
    fixture->playsync_manager->prepareSync(ps_id, fixture->ndir_info_disp);
    g_assert(playsync_container.at("TTS").first == PlaySyncState::Prepared);
    g_assert(playsync_container.at("Display").first == PlaySyncState::Synced);
    g_assert(fixture->playsync_manager_listener->getSyncState(ps_id) == PlaySyncState::Prepared);

    fixture->playsync_manager->startSync(ps_id, "TTS");
    g_assert(playsync_container.at("TTS").first == PlaySyncState::Synced);
    g_assert(fixture->playsync_manager_listener->getSyncState(ps_id) == PlaySyncState::Synced);
}

static void sub_test_playsync_manager_preset_media_stacked(TestFixture* fixture)
{
    fixture->playsync_manager->prepareSync("ps_id_1", fixture->ndir_media);
    fixture->playsync_manager->startSync("ps_id_1", "TTS");
    fixture->playsync_manager->startSync("ps_id_1", "AudioPlayer");
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Synced);

    fixture->playsync_manager->prepareSync("ps_id_2", fixture->ndir_info_disp);
    fixture->playsync_manager->startSync("ps_id_2", "TTS");
    fixture->playsync_manager->startSync("ps_id_2", "Display");
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_2") == PlaySyncState::Synced);
}

static void test_playsync_manager_cancel_sync(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_playsync_manager_preset_sync(fixture, "ps_id_1");

    const auto& playsync_container = fixture->playsync_manager->getPlayStacks().at("ps_id_1");

    // check invalid parameter
    fixture->playsync_manager->cancelSync("", "Display");
    g_assert(playsync_container.at("Display").first == PlaySyncState::Synced);

    // check invalid parameter
    fixture->playsync_manager->cancelSync("ps_id_1", "");
    g_assert(playsync_container.at("TTS").first == PlaySyncState::Synced);
    g_assert(playsync_container.at("Display").first == PlaySyncState::Synced);

    // try to cancel sync not prepared play service id
    fixture->playsync_manager->cancelSync("ps_id_2", "Display");
    g_assert(playsync_container.at("Display").first == PlaySyncState::Synced);

    // try to cancel sync not prepared requester
    fixture->playsync_manager->cancelSync("ps_id_1", "AudioPlayer");
    g_assert(playsync_container.find("AudioPlayer") == playsync_container.cend());

    fixture->playsync_manager->cancelSync("ps_id_1", "Display");
    g_assert(playsync_container.at("TTS").first == PlaySyncState::Synced);
    g_assert(playsync_container.find("Display") == playsync_container.cend());
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Synced);
}

static void test_playsync_manager_release_sync_immediately(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_playsync_manager_preset_sync(fixture, "ps_id_2");

    const auto& playstacks = fixture->playsync_manager->getPlayStacks();
    const auto& playsync_container = playstacks.at("ps_id_2");

    // check invalid parameter
    fixture->playsync_manager->releaseSyncImmediately("", "TTS");
    g_assert(playsync_container.at("TTS").first == PlaySyncState::Synced);

    // check invalid parameter
    fixture->playsync_manager->releaseSyncImmediately("ps_id_2", "");
    g_assert(playsync_container.at("TTS").first == PlaySyncState::Synced);

    // try to release sync not prepared play service id
    fixture->playsync_manager->releaseSyncImmediately("ps_id_1", "TTS");
    g_assert(playsync_container.at("TTS").first == PlaySyncState::Synced);

    fixture->playsync_manager->releaseSyncImmediately("ps_id_2", "TTS");
    g_assert(playstacks.find("ps_id_2") == playstacks.cend());
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_2") == PlaySyncState::Released);
}

static void test_playsync_manager_release_sync(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_playsync_manager_preset_sync(fixture, "ps_id_1");

    const auto& playstacks = fixture->playsync_manager->getPlayStacks();
    const auto& playsync_container = playstacks.at("ps_id_1");

    // check invalid parameter
    fixture->playsync_manager->releaseSync("", "TTS");
    g_assert(playsync_container.at("TTS").first == PlaySyncState::Synced);

    // check invalid parameter
    fixture->playsync_manager->releaseSync("ps_id_1", "");
    g_assert(playsync_container.at("TTS").first == PlaySyncState::Synced);

    // try to release sync not prepared requester
    fixture->playsync_manager->releaseSync("ps_id_1", "AudioPlayer");
    g_assert(playsync_container.find("AudioPlayer") == playsync_container.cend());

    fixture->playsync_manager->releaseSync("ps_id_1", "TTS");
    g_assert(playsync_container.at("TTS").first == PlaySyncState::Synced);
    g_assert(playsync_container.at("Display").first == PlaySyncState::Synced);

    onTimeElapsed(fixture);
    g_assert(playstacks.find("ps_id_1") == playstacks.cend());
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Released);

    // check whether onSyncState is called just one time when all items are released
    fixture->playsync_manager->releaseSync("ps_id_1", "Display");
    onTimeElapsed(fixture);
    g_assert(fixture->playsync_manager_listener->getSameStateCallCount() == 0);
}

static void test_playsync_manager_release_sync_later(TestFixture* fixture, gconstpointer ignored)
{
    auto execMediaSync = [&](std::function<void()>&& case_checker) {
        fixture->playsync_manager->prepareSync("ps_id_1", fixture->ndir_media);
        fixture->playsync_manager->startSync("ps_id_1", "TTS");
        fixture->playsync_manager->startSync("ps_id_1", "AudioPlayer");
        g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Synced);
        fixture->playsync_manager->releaseSyncLater("ps_id_1", "AudioPlayer");

        if (case_checker)
            case_checker();

        g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Released);
    };

    // release by sdk
    execMediaSync([&] {
        onTimeElapsed(fixture);
    });

    // release by app
    execMediaSync([&] {
        fixture->playsync_manager->releaseSyncImmediately("ps_id_1", "AudioPlayer");
    });
}

static void test_playsync_manager_implicit_release_sync_later(TestFixture* fixture, gconstpointer ignored)
{
    fixture->playsync_manager->prepareSync("ps_id_1", fixture->ndir_media);
    fixture->playsync_manager->startSync("ps_id_1", "TTS");
    fixture->playsync_manager->startSync("ps_id_1", "AudioPlayer");
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Synced);

    fixture->playsync_manager->postPoneRelease();
    fixture->playsync_manager->continueRelease();
    fixture->playsync_manager->releaseSyncLater("ps_id_1", "AudioPlayer");
    fixture->playsync_manager->clearHolding();
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Synced);

    fixture->playsync_manager->postPoneRelease();
    fixture->playsync_manager->continueRelease();
    fixture->playsync_manager->releaseSyncLater("ps_id_1", "AudioPlayer");
    onTimeElapsed(fixture);
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Released);
}

static void test_playsync_manager_release_sync_later_in_stack_condition(TestFixture* fixture, gconstpointer ignored)
{
    auto execMediaStackedSync = [&](std::function<void()>&& case_checker) {
        fixture->playsync_manager->prepareSync("ps_id_1", fixture->ndir_media);
        fixture->playsync_manager->startSync("ps_id_1", "TTS");
        fixture->playsync_manager->startSync("ps_id_1", "AudioPlayer");
        g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Synced);
        fixture->playsync_manager->releaseSyncLater("ps_id_1", "AudioPlayer");

        fixture->playsync_manager->prepareSync("ps_id_2", fixture->ndir_info_disp);
        fixture->playsync_manager->startSync("ps_id_2", "TTS");
        fixture->playsync_manager->startSync("ps_id_2", "Display");
        g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_2") == PlaySyncState::Synced);

        if (case_checker)
            case_checker();

        g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Synced);
        onTimeElapsed(fixture);
        g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Released);
    };

    // close by sdk
    execMediaStackedSync([&] {
        fixture->playsync_manager->releaseSync("ps_id_2", "TTS");
        g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_2") == PlaySyncState::Released);
    });

    // close by app
    execMediaStackedSync([&] {
        fixture->playsync_manager->releaseSyncImmediately("ps_id_2", "TTS");
        g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_2") == PlaySyncState::Released);
    });

    // stop rendering timer before release
    execMediaStackedSync([&] {
        fixture->playsync_manager->postPoneRelease();
        fixture->playsync_manager->stopHolding();
        fixture->playsync_manager->releaseSync("ps_id_2", "TTS");
        g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_2") == PlaySyncState::Synced);
    });
}

static void test_playsync_manager_release_sync_unconditionally(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_playsync_manager_preset_media_stacked(fixture);

    const auto& playstacks = fixture->playsync_manager->getPlayStacks();

    // release all playstacks
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Synced);
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_2") == PlaySyncState::Synced);
    g_assert(!playstacks.empty());

    fixture->playsync_manager->releaseSyncUnconditionally();
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Released);
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_2") == PlaySyncState::Released);
    g_assert(playstacks.empty());

    // release the specific playstack (ps_id_1: Media, ps_id_2: TTS)
    sub_test_playsync_manager_preset_media_stacked(fixture);
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Synced);
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_2") == PlaySyncState::Synced);
    g_assert(playstacks.size() == 2);

    fixture->playsync_manager->releaseSyncUnconditionally("ps_id_1"); // release under stacked playstack first
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Released);
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_2") == PlaySyncState::Synced);
    g_assert(playstacks.size() == 1);
}

static void test_playsync_manager_normal_case(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_playsync_manager_preset_sync(fixture, "ps_id_1");

    fixture->playsync_manager->releaseSync("ps_id_1", "TTS");
    onTimeElapsed(fixture);
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Released);
}

static void test_playsync_manager_stop_case(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_playsync_manager_preset_sync(fixture, "ps_id_1");

    fixture->playsync_manager->releaseSyncImmediately("ps_id_1", "TTS");
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Released);
}

static void test_playsync_manager_ignore_render_case(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_playsync_manager_preset_sync(fixture, "ps_id_1");

    const auto& playstacks = fixture->playsync_manager->getPlayStacks();
    const auto& playsync_container = playstacks.at("ps_id_1");

    fixture->playsync_manager->cancelSync("ps_id_1", "Display");
    g_assert(playsync_container.at("TTS").first == PlaySyncState::Synced);
    g_assert(playsync_container.find("Display") == playsync_container.cend());
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Synced);

    fixture->playsync_manager->releaseSync("ps_id_1", "TTS");
    onTimeElapsed(fixture);
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Released);
}

static void test_playsync_manager_handle_info_layer(TestFixture* fixture, gconstpointer ignored)
{
    fixture->playsync_manager->addListener("Display", fixture->playsync_manager_listener_snd.get());

    /***************************************************************************
     * [1] handle in DisplayAgent
     **************************************************************************/
    fixture->playsync_manager->prepareSync("ps_id_1", fixture->ndir_info_disp);
    fixture->dummy_extra_data->id = "100";
    fixture->playsync_manager->startSync("ps_id_1", "Display", fixture->dummy_extra_data);
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Prepared);

    /***************************************************************************
     * [2] handle in TTSAgent
     **************************************************************************/
    fixture->playsync_manager->prepareSync("ps_id_1", fixture->ndir_info_disp);
    fixture->playsync_manager->startSync("ps_id_1", "TTS");
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Synced);

    g_assert(!fixture->playsync_manager_listener->getExtraData());

    auto extra_data = fixture->playsync_manager_listener_snd->getExtraData();
    g_assert(extra_data);
    g_assert(reinterpret_cast<DummyExtraInfo*>(extra_data)->id == "100");

    fixture->playsync_manager->releaseSync("ps_id_1", "TTS");
    onTimeElapsed(fixture);
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Released);
}

static void test_playsync_manager_playstack_holding(TestFixture* fixture, gconstpointer ignored)
{
    fixture->playsync_manager->prepareSync("ps_id_1", fixture->ndir_info_disp);
    fixture->playsync_manager->startSync("ps_id_1", "Display");
    fixture->playsync_manager->startSync("ps_id_1", "TTS");
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Synced);

    fixture->playsync_manager->releaseSync("ps_id_1", "TTS");
    fixture->playsync_manager->stopHolding();
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Synced);

    fixture->playsync_manager->resetHolding();
    onTimeElapsed(fixture);
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Released);
}

static void test_playsync_manager_adjust_playstack_hold_time(TestFixture* fixture, gconstpointer ignored)
{
    const unsigned int DEFAULT_HOLD_TIME_SEC = 7;
    const unsigned int ADJUST_HOLD_TIME_SEC = 15;

    // check validation
    fixture->playsync_manager->adjustPlayStackHoldTime(-1);
    g_assert(fixture->playsync_manager->getPlayStackHoldTime() == DEFAULT_HOLD_TIME_SEC);
    fixture->playsync_manager->adjustPlayStackHoldTime(0);
    g_assert(fixture->playsync_manager->getPlayStackHoldTime() == DEFAULT_HOLD_TIME_SEC);

    auto execSyncProcess([&](std::string&& ps_id, unsigned int hold_time) {
        const auto& playstacks = fixture->playsync_manager->getPlayStacks();

        // prepare
        fixture->playsync_manager->prepareSync(ps_id, fixture->ndir_info_disp);
        g_assert(fixture->playsync_manager_listener->getSyncState(ps_id) == PlaySyncState::Prepared);

        // adjust hold time
        if (hold_time != DEFAULT_HOLD_TIME_SEC) {
            fixture->playsync_manager->adjustPlayStackHoldTime(hold_time);
            g_assert(fixture->playsync_manager->getPlayStackHoldTime() == hold_time);
        }

        // start
        fixture->playsync_manager->startSync(ps_id, "Display");
        fixture->playsync_manager->startSync(ps_id, "TTS");
        g_assert(fixture->playsync_manager_listener->getSyncState(ps_id) == PlaySyncState::Synced);

        // release
        fixture->playsync_manager->releaseSync(ps_id, "Display");
        g_assert(fixture->fake_timer->getPlayStackHoldTime() == hold_time);
        onTimeElapsed(fixture);
        g_assert(playstacks.find(ps_id) == playstacks.cend());
        g_assert(fixture->playsync_manager_listener->getSyncState(ps_id) == PlaySyncState::Released);
    });

    execSyncProcess("ps_id_1", ADJUST_HOLD_TIME_SEC);
    execSyncProcess("ps_id_2", DEFAULT_HOLD_TIME_SEC);
}

static void test_playsync_manager_set_default_playstack_hold_time(TestFixture* fixture, gconstpointer ignored)
{
    const unsigned int DEFAULT_HOLD_TIME_SEC = 7;
    const unsigned int NEW_DEFAULT_HOLD_TIME_SEC = 3;
    const unsigned int ADJUST_HOLD_TIME_SEC = 15;

    // check validation
    g_assert(fixture->playsync_manager->getPlayStackHoldTime() == DEFAULT_HOLD_TIME_SEC);
    fixture->playsync_manager->setDefaultPlayStackHoldTime(-1);
    g_assert(fixture->playsync_manager->getPlayStackHoldTime() == DEFAULT_HOLD_TIME_SEC);
    fixture->playsync_manager->setDefaultPlayStackHoldTime(0);
    g_assert(fixture->playsync_manager->getPlayStackHoldTime() == DEFAULT_HOLD_TIME_SEC);

    // set new default playstack hold time
    fixture->playsync_manager->setDefaultPlayStackHoldTime(NEW_DEFAULT_HOLD_TIME_SEC);
    g_assert(fixture->playsync_manager->getPlayStackHoldTime() == NEW_DEFAULT_HOLD_TIME_SEC);

    // prepare
    fixture->playsync_manager->prepareSync("ps_id_1", fixture->ndir_info_disp);
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Prepared);

    // adjust hold time
    fixture->playsync_manager->adjustPlayStackHoldTime(ADJUST_HOLD_TIME_SEC);
    g_assert(fixture->playsync_manager->getPlayStackHoldTime() == ADJUST_HOLD_TIME_SEC);

    // start
    fixture->playsync_manager->startSync("ps_id_1", "Display");
    fixture->playsync_manager->startSync("ps_id_1", "TTS");
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Synced);

    // release
    fixture->playsync_manager->releaseSync("ps_id_1", "Display");
    g_assert(fixture->fake_timer->getPlayStackHoldTime() == ADJUST_HOLD_TIME_SEC);
    onTimeElapsed(fixture);
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Released);

    // reset to default hold time
    g_assert(fixture->playsync_manager->getPlayStackHoldTime() == NEW_DEFAULT_HOLD_TIME_SEC);
}

static void test_playsync_manager_restart_playstack_holding(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_playsync_manager_preset_sync(fixture, "ps_id_1");

    const auto& playstacks = fixture->playsync_manager->getPlayStacks();

    auto checkSynced([&] {
        const auto& playsync_container = playstacks.at("ps_id_1");
        g_assert(playsync_container.at("Display").first == PlaySyncState::Synced);
        g_assert(playsync_container.at("TTS").first == PlaySyncState::Synced);
        g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Synced);
    });

    // try to release
    fixture->playsync_manager->releaseSync("ps_id_1", "Display");
    checkSynced();

    // reset holding
    g_assert(!fixture->fake_timer->isRestarted());
    fixture->playsync_manager->restartHolding();
    g_assert(fixture->fake_timer->isRestarted());

    checkSynced();

    // release finally
    onTimeElapsed(fixture);
    g_assert(playstacks.find("ps_id_1") == playstacks.cend());
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Released);
}

static void test_playsync_manager_stop_playstack_holding_after_timer(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_playsync_manager_preset_sync(fixture, "ps_id_1");

    const auto& playstacks = fixture->playsync_manager->getPlayStacks();

    // try to release
    fixture->playsync_manager->releaseSync("ps_id_1", "Display");
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Synced);

    // stop playstack holding
    fixture->playsync_manager->clearHolding();

    onTimeElapsed(fixture);
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Synced);

    // release by another dialog
    sub_test_playsync_manager_preset_sync(fixture, "ps_id_2");
    g_assert(playstacks.find("ps_id_1") == playstacks.cend());
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Released);
}

static void test_playsync_manager_stop_playstack_holding_before_timer(TestFixture* fixture, gconstpointer ignored)
{
    const auto& playstacks = fixture->playsync_manager->getPlayStacks();

    auto setupStopHolding([&](bool is_media_stacked) {
        is_media_stacked ? sub_test_playsync_manager_preset_media_stacked(fixture)
                         : sub_test_playsync_manager_preset_sync(fixture, "ps_id_2");

        // stop playstack holding
        fixture->playsync_manager->postPoneRelease();
        fixture->playsync_manager->releaseSync("ps_id_2", "Display");

        onTimeElapsed(fixture);
        g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_2") == PlaySyncState::Synced);
    });

    for (const auto& type : { false, true }) { // false: info only, true: media stack
        // release by user
        setupStopHolding(type);
        fixture->playsync_manager->continueRelease();
        fixture->playsync_manager->releaseSyncImmediately("ps_id_2", "Display");
        g_assert(playstacks.find("ps_id_2") == playstacks.cend());
        g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_2") == PlaySyncState::Released);

        // release by another dialog
        setupStopHolding(type);
        sub_test_playsync_manager_preset_sync(fixture, "ps_id_3");
        g_assert(playstacks.find("ps_id_2") == playstacks.cend());
        g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_2") == PlaySyncState::Released);
    }
}

static void test_playsync_manager_check_playstack_layer(TestFixture* fixture, gconstpointer ignored)
{
    fixture->playsync_manager->prepareSync("ps_id_1", fixture->ndir_media);
    fixture->playsync_manager->startSync("ps_id_1", "TTS");
    g_assert(!fixture->playsync_manager->hasActivity("", PlayStackActivity::Media));
    g_assert(fixture->playsync_manager->hasActivity("ps_id_1", PlayStackActivity::Media));
    g_assert(!fixture->playsync_manager->hasActivity("ps_id_1", PlayStackActivity::TTS));
}

static void test_playsync_manager_replace_playstack(TestFixture* fixture, gconstpointer ignored)
{
    fixture->playsync_manager->addListener("Display", fixture->playsync_manager_listener_snd.get());
    const auto& notified_playstacks(fixture->playsync_manager_listener_snd->getPlaystacks());
    const auto& playstacks = fixture->playsync_manager->getPlayStacks();

    auto checkPlayStack = [&](std::string&& ps_id) {
        auto playsync_container = playstacks.at(ps_id);
        g_assert(playstacks.size() == 1);
        g_assert(playsync_container.size() == 2);
        g_assert(playsync_container.at("TTS").first == PlaySyncState::Synced);
        g_assert(playsync_container.at("Display").first == PlaySyncState::Synced);

        auto playstack_items = fixture->playsync_manager->getAllPlayStackItems();
        g_assert(playstack_items.size() == 1);
        g_assert(std::find(playstack_items.cbegin(), playstack_items.cend(), ps_id) != playstack_items.cend());
    };

    sub_test_playsync_manager_preset_sync(fixture, "ps_id_1");
    g_assert(notified_playstacks.find("ps_id_1") != notified_playstacks.end());
    checkPlayStack("ps_id_1");

    // replace playstack
    fixture->dummy_extra_data->id = "100";
    fixture->playsync_manager->replacePlayStack("ps_id_1", "ps_id_2", std::make_pair("Display", fixture->dummy_extra_data));
    g_assert(reinterpret_cast<DummyExtraInfo*>(fixture->playsync_manager_listener_snd->getExtraData())->id == "100");
    g_assert(notified_playstacks.find("ps_id_2") != notified_playstacks.end());
    checkPlayStack("ps_id_2");

    // check validation
    fixture->playsync_manager->replacePlayStack("ps_id_1", "ps_id_3"); // from not exist
    checkPlayStack("ps_id_2");
    fixture->playsync_manager->replacePlayStack("ps_id_2", ""); // to empty
    checkPlayStack("ps_id_2");
    fixture->playsync_manager->replacePlayStack("", "ps_id_3"); // from empty
    checkPlayStack("ps_id_2");
    fixture->playsync_manager->replacePlayStack("ps_id_2", "ps_id_2"); // same playstack
    checkPlayStack("ps_id_2");

    // update extra data
    fixture->dummy_extra_data_snd->id = "200";
    fixture->playsync_manager->replacePlayStack("ps_id_2", "ps_id_2", std::make_pair("Display", fixture->dummy_extra_data_snd));
    g_assert(reinterpret_cast<DummyExtraInfo*>(fixture->playsync_manager_listener_snd->getExtraData())->id == "200");

    // release sync
    fixture->playsync_manager->releaseSyncImmediately("ps_id_2", "Display");
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_2") == PlaySyncState::Released);
}

static void test_playsync_manager_recv_callback_only_participants(TestFixture* fixture, gconstpointer ignored)
{
    fixture->playsync_manager->addListener("AudioPlayer", fixture->playsync_manager_listener_snd.get());
    g_assert(fixture->playsync_manager->getListenerCount() == 2);

    fixture->playsync_manager->prepareSync("ps_id_1", fixture->ndir_info_disp);
    fixture->playsync_manager->startSync("ps_id_1", "TTS");
    fixture->playsync_manager->startSync("ps_id_1", "Display");
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Synced);
    g_assert(fixture->playsync_manager_listener_snd->getSyncState("ps_id_1") == PlaySyncState::None);
}

static void test_playsync_manager_media_stacked_case(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_playsync_manager_preset_media_stacked(fixture);

    // It released TTS activity immediately if the media is stacked
    fixture->playsync_manager->releaseSync("ps_id_2", "TTS");
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Synced);
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_2") == PlaySyncState::Released);

    fixture->playsync_manager->releaseSync("ps_id_1", "AudioPlayer");
    onTimeElapsed(fixture);
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Released);
}

static void test_playsync_manager_postpone_release(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_playsync_manager_preset_media_stacked(fixture);

    fixture->playsync_manager->postPoneRelease();
    fixture->playsync_manager->releaseSync("ps_id_2", "TTS");
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_2") == PlaySyncState::Synced);

    fixture->playsync_manager->continueRelease();
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_2") == PlaySyncState::Released);
}

static void test_playsync_manager_check_to_process_previous_dialog(TestFixture* fixture, gconstpointer ignored)
{
    fixture->playsync_manager->prepareSync("ps_id_1", fixture->ndir_media);
    fixture->playsync_manager->startSync("ps_id_1", "TTS");
    fixture->playsync_manager->startSync("ps_id_1", "AudioPlayer");
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Synced);

    g_assert(fixture->playsync_manager->isConditionToHandlePrevDialog(fixture->ndir_media, fixture->ndir_info_disp));
    g_assert(!fixture->playsync_manager->isConditionToHandlePrevDialog(fixture->ndir_expect_speech, fixture->ndir_info_disp));
}

static void test_playsync_manager_refresh_extra_data(TestFixture* fixture, gconstpointer ignored)
{
    fixture->playsync_manager->addListener("AudioPlayer", fixture->playsync_manager_listener_snd.get());

    fixture->dummy_extra_data->id = "100";
    fixture->playsync_manager->prepareSync("ps_id_1", fixture->ndir_media);
    fixture->playsync_manager->startSync("ps_id_1", "TTS");
    fixture->playsync_manager->startSync("ps_id_1", "AudioPlayer", fixture->dummy_extra_data);
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Synced);
    g_assert(reinterpret_cast<DummyExtraInfo*>(fixture->playsync_manager_listener_snd->getExtraData())->id == "100");
    g_assert(!fixture->playsync_manager_listener->getExtraData());

    fixture->dummy_extra_data_snd->id = "200";
    fixture->playsync_manager->prepareSync("ps_id_1", fixture->ndir_media);
    fixture->playsync_manager->startSync("ps_id_1", "AudioPlayer", fixture->dummy_extra_data_snd);
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Synced);
    g_assert(reinterpret_cast<DummyExtraInfo*>(fixture->playsync_manager_listener_snd->getExtraData())->id == "200");
    g_assert(!fixture->playsync_manager_listener->getExtraData());
}

static void test_playsync_manager_check_expect_speech(TestFixture* fixture, gconstpointer ignored)
{
    auto checkHasMultiTurnNotifiedRepeatedly = [&](NuguDirective* dir) {
        fixture->playsync_manager->prepareSync("ps_id_2", dir);
        g_assert(fixture->ic_manager_listener->getHasMultiTurnNotifiedCount() == 1);
    };

    fixture->playsync_manager->prepareSync("ps_id_1", fixture->ndir_info_disp);
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Prepared);
    g_assert(fixture->ic_manager_listener->getHasMultiTurnNotifiedCount() == 0);

    g_assert(nugu_directive_peek_dialog_id(fixture->ndir_expect_speech) != nugu_directive_peek_dialog_id(fixture->ndir_expect_speech_second));

    // [1] prepare sync playstack which has expect speech
    fixture->playsync_manager->prepareSync("ps_id_2", fixture->ndir_expect_speech);
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_2") == PlaySyncState::Prepared);
    g_assert(fixture->ic_manager_listener->getHasMultiTurnNotifiedCount() == 1);

    checkHasMultiTurnNotifiedRepeatedly(fixture->ndir_expect_speech);

    // [2] start sync
    fixture->playsync_manager->startSync("ps_id_2", "TTS");
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_2") == PlaySyncState::Synced);

    // [3] process multi-turn by InteractionControlManager
    fixture->ic_manager->start(InteractionMode::MULTI_TURN, "ASR");
    // assume do something...
    fixture->ic_manager->finish(InteractionMode::MULTI_TURN, "ASR");
    g_assert(fixture->ic_manager_listener->getHasMultiTurnNotifiedCount() == 0);

    // [4] emulate start recognition in TTS playing
    fixture->playsync_manager->postPoneRelease();
    fixture->playsync_manager->releaseSync("ps_id_2", "TTS");
    fixture->playsync_manager->stopHolding();

    // [6] try to prepare sync again about same playstack in releasing sync (notify onHasMultiTurn again)
    fixture->playsync_manager->prepareSync("ps_id_2", fixture->ndir_expect_speech_second);
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_2") == PlaySyncState::Synced);
    g_assert(fixture->ic_manager_listener->getHasMultiTurnNotifiedCount() == 1);

    checkHasMultiTurnNotifiedRepeatedly(fixture->ndir_expect_speech_second);

    // [extra] prepare sync another playstack
    fixture->playsync_manager->prepareSync("ps_id_3", fixture->ndir_expect_speech_third);
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_3") == PlaySyncState::Prepared);
    g_assert(fixture->ic_manager_listener->getHasMultiTurnNotifiedCount() == 2);
}

static void test_playsync_manager_check_expect_speech_with_nudge(TestFixture* fixture, gconstpointer ignored)
{
    // [1] handle expect speech : notify hasMultiTurn
    fixture->playsync_manager->prepareSync("ps_id_1", fixture->ndir_expect_speech_second);
    g_assert(fixture->ic_manager_listener->getHasMultiTurnNotifiedCount() == 1);

    // [2] process multi-turn by InteractionControlManager
    fixture->playsync_manager->startSync("ps_id_1", "TTS");
    fixture->ic_manager->start(InteractionMode::MULTI_TURN, "ASR");
    // assume do something...
    fixture->ic_manager->finish(InteractionMode::MULTI_TURN, "ASR");
    g_assert(fixture->ic_manager_listener->getHasMultiTurnNotifiedCount() == 0);

    // [3] handle nudge : not notify hasMultiTurn
    fixture->playsync_manager->prepareSync("ps_id_2", fixture->ndir_nudge);
    g_assert(fixture->ic_manager_listener->getHasMultiTurnNotifiedCount() == 0);
}

static void test_playsync_manager_reset(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_playsync_manager_preset_sync(fixture, "ps_id_1");

    const auto& playstacks = fixture->playsync_manager->getPlayStacks();

    g_assert(!playstacks.empty());
    g_assert(!fixture->playsync_manager->getAllPlayStackItems().empty());
    g_assert(!fixture->playsync_manager->hasPostPoneRelease());

    fixture->playsync_manager->postPoneRelease();
    fixture->playsync_manager->releaseSync("ps_id_1", "TTS");
    g_assert(fixture->playsync_manager->hasPostPoneRelease());

    fixture->playsync_manager->reset();

    g_assert(playstacks.empty());
    g_assert(fixture->playsync_manager->getAllPlayStackItems().empty());
    g_assert(!fixture->playsync_manager->hasPostPoneRelease());
    g_assert(fixture->playsync_manager->getListenerCount() == 1);
}

static void test_playsync_manager_register_capability_for_sync(TestFixture* fixture, gconstpointer ignored)
{
    fixture->playsync_manager->addListener("Alerts", fixture->playsync_manager_listener_snd.get());
    fixture->playsync_manager->registerCapabilityForSync("Alerts");

    fixture->playsync_manager->prepareSync("ps_id_1", fixture->ndir_alerts);
    g_assert(fixture->playsync_manager_listener_snd->getSyncState("ps_id_1") == PlaySyncState::Prepared);

    fixture->playsync_manager->startSync("ps_id_1", "Alerts");
    g_assert(fixture->playsync_manager_listener_snd->getSyncState("ps_id_1") == PlaySyncState::Synced);

    fixture->playsync_manager->releaseSyncImmediately("ps_id_1", "Alerts");
    g_assert(fixture->playsync_manager_listener_snd->getSyncState("ps_id_1") == PlaySyncState::Released);
}

static void test_playsync_manager_check_next_playstack(TestFixture* fixture, gconstpointer ignored)
{
    fixture->playsync_manager_listener->setHookInonSyncState([&]() {
        g_assert(fixture->playsync_manager->hasNextPlayStack());
    });

    fixture->playsync_manager->prepareSync("ps_id_1", fixture->ndir_info_disp);
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_1") == PlaySyncState::Prepared);

    fixture->playsync_manager->prepareSync("ps_id_2", fixture->ndir_info_disp);
    g_assert(fixture->playsync_manager_listener->getSyncState("ps_id_2") == PlaySyncState::Prepared);
    g_assert(!fixture->playsync_manager->hasNextPlayStack());
}

static void test_playsync_manager_check_on_stack_changed_callback(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_playsync_manager_preset_sync(fixture, "ps_id_1");

    const auto& playstacks(fixture->playsync_manager_listener->getPlaystacks());
    g_assert(playstacks.find("ps_id_1") != playstacks.end());

    fixture->playsync_manager->releaseSyncImmediately("ps_id_1", "TTS");
    g_assert(playstacks.find("ps_id_1") == playstacks.end());
}

static void test_playsync_manager_clear(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_playsync_manager_preset_media_stacked(fixture);

    const auto& playstacks = fixture->playsync_manager->getPlayStacks();

    g_assert(!playstacks.empty());
    g_assert(!fixture->playsync_manager->getAllPlayStackItems().empty());
    g_assert(!fixture->playsync_manager->hasPostPoneRelease());

    fixture->playsync_manager->postPoneRelease();
    g_assert(fixture->playsync_manager->hasPostPoneRelease());

    fixture->playsync_manager->clear();

    g_assert(playstacks.empty());
    g_assert(fixture->playsync_manager->getAllPlayStackItems().empty());
    g_assert(!fixture->playsync_manager->hasPostPoneRelease());
}

int main(int argc, char* argv[])
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
    g_type_init();
#endif

    g_test_init(&argc, &argv, (void*)NULL);
    g_log_set_always_fatal((GLogLevelFlags)G_LOG_FATAL_MASK);

    G_TEST_ADD_FUNC("/core/PlaySyncManager/listener", test_playsync_manager_listener);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/prepareSync", test_playsync_manager_prepare_sync);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/startSync", test_playsync_manager_start_sync);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/appendSync", test_playsync_manager_append_sync);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/cancelSync", test_playsync_manager_cancel_sync);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/releaseSyncImmediately", test_playsync_manager_release_sync_immediately);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/releaseSync", test_playsync_manager_release_sync);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/releaseSyncLater", test_playsync_manager_release_sync_later);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/implicitReleaseSyncLater", test_playsync_manager_implicit_release_sync_later);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/releaseSyncLaterInStackCondition", test_playsync_manager_release_sync_later_in_stack_condition);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/releaseSyncUnconditionally", test_playsync_manager_release_sync_unconditionally);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/normalCase", test_playsync_manager_normal_case);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/stopCase", test_playsync_manager_stop_case);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/ignoreRenderCase", test_playsync_manager_ignore_render_case);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/handleInfoLayer", test_playsync_manager_handle_info_layer);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/playstackHolding", test_playsync_manager_playstack_holding);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/adjustPlaystackHoldTime", test_playsync_manager_adjust_playstack_hold_time);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/setDefaultPlaystackHoldTime", test_playsync_manager_set_default_playstack_hold_time);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/restartPlaystackHolding", test_playsync_manager_restart_playstack_holding);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/stopPlaystackHoldingAfterTimer", test_playsync_manager_stop_playstack_holding_after_timer);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/stopPlaystackHoldingBeforeTimer", test_playsync_manager_stop_playstack_holding_before_timer);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/checkPlayStackActivity", test_playsync_manager_check_playstack_layer);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/replacePlayStack", test_playsync_manager_replace_playstack);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/recvCallbackOnlyParticipants", test_playsync_manager_recv_callback_only_participants);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/mediaStackedCase", test_playsync_manager_media_stacked_case);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/postPoneRelease", test_playsync_manager_postpone_release);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/checkToProcessPreviousDialog", test_playsync_manager_check_to_process_previous_dialog);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/refreshExtraData", test_playsync_manager_refresh_extra_data);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/checkExpectSpeech", test_playsync_manager_check_expect_speech);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/checkExpectSpeechWithNudge", test_playsync_manager_check_expect_speech_with_nudge);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/reset", test_playsync_manager_reset);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/registerCapabilityForSync", test_playsync_manager_register_capability_for_sync);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/checkNextPlayStack", test_playsync_manager_check_next_playstack);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/checkOnStackChangedCallback", test_playsync_manager_check_on_stack_changed_callback);
    G_TEST_ADD_FUNC("/core/PlaySyncManager/clear", test_playsync_manager_clear);

    return g_test_run();
}
