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
#include <memory>
#include <set>
#include <string>

#include "session_manager.hh"

using namespace NuguCore;

class SessionManagerListener : public ISessionManagerListener {
public:
    void activated(const std::string& dialog_id, Session session)
    {
        if (session_active_map.find(dialog_id) != session_active_map.cend())
            session_active_map[dialog_id]++;
        else
            session_active_map.emplace(dialog_id, 1);

        call_count++;
    }

    void deactivated(const std::string& dialog_id)
    {
        session_active_map.erase(dialog_id);

        call_count++;
    }

    int getSessionActive(const std::string& dialog_id)
    {
        return session_active_map[dialog_id];
    }

    bool isSessionActive(const std::string& dialog_id)
    {
        return session_active_map[dialog_id] > 0;
    }

    int getCallCount()
    {
        return call_count;
    }

private:
    std::map<std::string, int> session_active_map;
    int call_count = 0;
};

typedef struct {
    std::shared_ptr<SessionManager> session_manager;
    std::shared_ptr<SessionManagerListener> session_manager_listener;
    std::shared_ptr<SessionManagerListener> session_manager_listener_snd;
} TestFixture;

static void setup(TestFixture* fixture, gconstpointer user_data)
{
    fixture->session_manager = std::make_shared<SessionManager>();
    fixture->session_manager_listener = std::make_shared<SessionManagerListener>();
    fixture->session_manager_listener_snd = std::make_shared<SessionManagerListener>();
}

static void teardown(TestFixture* fixture, gconstpointer user_data)
{
    fixture->session_manager_listener.reset();
    fixture->session_manager_listener_snd.reset();
    fixture->session_manager.reset();
}

#define G_TEST_ADD_FUNC(name, func) \
    g_test_add(name, TestFixture, nullptr, setup, func, teardown);

static void test_session_manager_listener(TestFixture* fixture, gconstpointer ignored)
{
    fixture->session_manager->addListener(nullptr);
    g_assert(fixture->session_manager->getListenerCount() == 0);

    fixture->session_manager->addListener(fixture->session_manager_listener.get());
    g_assert(fixture->session_manager->getListenerCount() == 1);

    // check adding duplicately
    fixture->session_manager->addListener(fixture->session_manager_listener.get());
    g_assert(fixture->session_manager->getListenerCount() == 1);

    fixture->session_manager->addListener(fixture->session_manager_listener_snd.get());
    g_assert(fixture->session_manager->getListenerCount() == 2);

    fixture->session_manager->removeListener(nullptr);
    g_assert(fixture->session_manager->getListenerCount() == 2);

    fixture->session_manager->removeListener(fixture->session_manager_listener.get());
    g_assert(fixture->session_manager->getListenerCount() == 1);

    // check removing again
    fixture->session_manager->removeListener(fixture->session_manager_listener.get());
    g_assert(fixture->session_manager->getListenerCount() == 1);

    fixture->session_manager->removeListener(fixture->session_manager_listener_snd.get());
    g_assert(fixture->session_manager->getListenerCount() == 0);
}

static void test_session_manager_set(TestFixture* fixture, gconstpointer ignored)
{
    const auto& session_container = fixture->session_manager->getAllSessions();

    fixture->session_manager->set("", {});
    g_assert(session_container.empty());

    fixture->session_manager->set("dialog_id_1", {});
    g_assert(session_container.empty());

    fixture->session_manager->set("dialog_id_1", { "", "ps_id_1" });
    g_assert(session_container.empty());

    fixture->session_manager->set("dialog_id_1", { "session_id_1", "" });
    g_assert(session_container.empty());

    fixture->session_manager->set("dialog_id_1", { "session_id_1", "ps_id_1" });
    g_assert(session_container.size() == 1);

    fixture->session_manager->set("dialog_id_1", { "session_id_2", "ps_id_2" });
    g_assert(session_container.at("dialog_id_1").session_id == "session_id_1");

    fixture->session_manager->set("dialog_id_2", { "session_id_3", "ps_id_3" });
    g_assert(session_container.size() == 2);
}

static void test_session_manager_get(TestFixture* fixture, gconstpointer ignored)
{
    fixture->session_manager->set("dialog_id_1", { "session_id_1", "ps_id_1" });
    fixture->session_manager->set("dialog_id_2", { "session_id_2", "ps_id_2" });
    fixture->session_manager->activate("dialog_id_1");
    fixture->session_manager->activate("dialog_id_2");
    auto active_session_info = fixture->session_manager->getActiveSessionInfo();

    g_assert(active_session_info.size() == 2
        && (active_session_info[0]["sessionId"].asString() == "session_id_1")
        && (active_session_info[1]["sessionId"].asString() == "session_id_2"));
}

static void test_session_manager_single_active(TestFixture* fixture, gconstpointer ignored)
{
    const auto& session_container = fixture->session_manager->getAllSessions();
    const auto& active_list = fixture->session_manager->getActiveList();

    fixture->session_manager->activate("dialog_id_1");
    g_assert(active_list.size() == 1
        && fixture->session_manager->getActiveSessionInfo().isNull());

    fixture->session_manager->set("dialog_id_1", { "session_id_1", "ps_id_1" });
    auto active_session_info = fixture->session_manager->getActiveSessionInfo();
    g_assert(active_session_info.size() == 1
        && active_session_info[0]["sessionId"].asString() == "session_id_1");

    fixture->session_manager->deactivate("");
    g_assert(!fixture->session_manager->getActiveSessionInfo().isNull());

    fixture->session_manager->deactivate("dialog_id_1");
    g_assert(fixture->session_manager->getActiveSessionInfo().isNull()
        && session_container.empty()
        && active_list.empty());
}

static void test_session_manager_multi_active(TestFixture* fixture, gconstpointer ignored)
{
    const auto& session_container = fixture->session_manager->getAllSessions();
    const auto& active_list = fixture->session_manager->getActiveList();

    fixture->session_manager->activate("dialog_id_3");
    fixture->session_manager->activate("dialog_id_4");
    fixture->session_manager->set("dialog_id_3", { "session_id_3", "ps_id_3" });
    fixture->session_manager->set("dialog_id_5", { "session_id_5", "ps_id_5" });
    auto active_session_info = fixture->session_manager->getActiveSessionInfo();

    g_assert(active_list.size() == 2 && session_container.size() == 2
        && active_session_info.size() == 1
        && active_session_info[0]["sessionId"].asString() == "session_id_3");

    fixture->session_manager->activate("dialog_id_5");
    active_session_info = fixture->session_manager->getActiveSessionInfo();
    g_assert(active_list.size() == 3 && session_container.size() == 2
        && active_session_info.size() == 2
        && active_session_info[0]["sessionId"].asString() == "session_id_3"
        && active_session_info[1]["sessionId"].asString() == "session_id_5");

    fixture->session_manager->deactivate("dialog_id_3");
    fixture->session_manager->deactivate("dialog_id_5");
    g_assert(fixture->session_manager->getActiveSessionInfo().isNull());

    fixture->session_manager->deactivate("dialog_id_4");
    g_assert(session_container.empty() && active_list.empty());
}

static void test_session_manager_multi_agent_participation(TestFixture* fixture, gconstpointer ignored)
{
    fixture->session_manager->activate("dialog_id_1"); // by ASR
    fixture->session_manager->set("dialog_id_1", { "session_id_1", "ps_id_1" }); // by Session
    fixture->session_manager->activate("dialog_id_1"); // by Display
    auto active_session_info = fixture->session_manager->getActiveSessionInfo();
    g_assert(active_session_info.size() == 1 && active_session_info[0]["sessionId"].asString() == "session_id_1");

    fixture->session_manager->deactivate("dialog_id_1"); // by ASR
    active_session_info = fixture->session_manager->getActiveSessionInfo();
    g_assert(active_session_info.size() == 1 && active_session_info[0]["sessionId"].asString() == "session_id_1");

    fixture->session_manager->deactivate("dialog_id_1"); // by Display
    g_assert(fixture->session_manager->getActiveSessionInfo().empty());
}

static void test_session_manager_maintain_multi_session(TestFixture* fixture, gconstpointer ignored)
{
    std::set<std::string> holding_dialogIds;
    fixture->session_manager->addListener(fixture->session_manager_listener.get());

    // [1] receive dialog_1 [Display.Templates, Session.Set, ASR.ExpectSpeech]
    fixture->session_manager->activate("dialog_id_1"); // Display.Templates
    fixture->session_manager->set("dialog_id_1", { "session_id_1", "ps_id_1" }); // Session.Set
    fixture->session_manager->activate("dialog_id_1"); // ASR.ExpectSpeech

    // [2] ASR.ExpectSpeech is ended, but Display is not closed
    fixture->session_manager->deactivate("dialog_id_1"); // ASR.ExpectSpeech
    holding_dialogIds.emplace("dialog_id_1");

    g_assert(fixture->session_manager_listener->isSessionActive("dialog_id_1"));
    g_assert(fixture->session_manager->getActiveSessionInfo().size() == 1);

    // [3] receive dialog_2 [Display.Update, Session.Set, ASR.ExpectSpeech]
    fixture->session_manager->activate("dialog_id_2"); // Display.Update
    fixture->session_manager->set("dialog_id_2", { "session_id_2", "ps_id_2" }); // Session.Set
    fixture->session_manager->activate("dialog_id_2"); // ASR.ExpectSpeech

    // [4] ASR.ExpectSpeech is ended, but Display is not closed.
    fixture->session_manager->deactivate("dialog_id_2"); // ASR.ExpectSpeech
    holding_dialogIds.emplace("dialog_id_2");

    g_assert(fixture->session_manager_listener->isSessionActive("dialog_id_1"));
    g_assert(fixture->session_manager_listener->isSessionActive("dialog_id_2"));
    g_assert(fixture->session_manager->getActiveSessionInfo().size() == 2);

    // [5] Display cleared
    for (const auto& holding_dialogId : holding_dialogIds)
        fixture->session_manager->deactivate(holding_dialogId);

    g_assert(!fixture->session_manager_listener->isSessionActive("dialog_id_1"));
    g_assert(!fixture->session_manager_listener->isSessionActive("dialog_id_2"));
    g_assert(fixture->session_manager->getActiveSessionInfo().empty());

    holding_dialogIds.clear();
}

static void test_session_manager_notify_single_active_state(TestFixture* fixture, gconstpointer ignored)
{
    fixture->session_manager->addListener(fixture->session_manager_listener.get());

    fixture->session_manager->activate("dialog_id_1");
    g_assert(fixture->session_manager_listener->getSessionActive("dialog_id_1") == 0);

    fixture->session_manager->activate("dialog_id_1");
    g_assert(fixture->session_manager_listener->getSessionActive("dialog_id_1") == 0);

    fixture->session_manager->set("dialog_id_1", { "session_id_1", "ps_id_1" });
    g_assert(fixture->session_manager_listener->getSessionActive("dialog_id_1") == 1);

    fixture->session_manager->deactivate("dialog_id_1");
    g_assert(fixture->session_manager_listener->getSessionActive("dialog_id_1") == 1);

    fixture->session_manager->deactivate("dialog_id_1");
    g_assert(fixture->session_manager_listener->getSessionActive("dialog_id_1") == 0);

    fixture->session_manager->set("dialog_id_1", { "session_id_1", "ps_id_1" });
    g_assert(fixture->session_manager_listener->getSessionActive("dialog_id_1") == 0);

    fixture->session_manager->activate("dialog_id_1");
    g_assert(fixture->session_manager_listener->getSessionActive("dialog_id_1") == 1);

    fixture->session_manager->activate("dialog_id_1");
    g_assert(fixture->session_manager_listener->getSessionActive("dialog_id_1") == 1);

    fixture->session_manager->set("dialog_id_1", { "session_id_1", "ps_id_1" });
    g_assert(fixture->session_manager_listener->getSessionActive("dialog_id_1") == 1);

    fixture->session_manager->deactivate("dialog_id_1");
    g_assert(fixture->session_manager_listener->getSessionActive("dialog_id_1") == 1);

    fixture->session_manager->deactivate("dialog_id_1");
    g_assert(fixture->session_manager_listener->getSessionActive("dialog_id_1") == 0);
}

static void test_session_manager_notify_multi_active_state(TestFixture* fixture, gconstpointer ignored)
{
    fixture->session_manager->addListener(fixture->session_manager_listener.get());

    fixture->session_manager->activate("dialog_id_1");
    g_assert(fixture->session_manager_listener->getSessionActive("dialog_id_1") == 0);

    fixture->session_manager->set("dialog_id_1", { "session_id_1", "ps_id_1" });
    g_assert(fixture->session_manager_listener->getSessionActive("dialog_id_1") == 1);

    fixture->session_manager->activate("dialog_id_2");
    g_assert(fixture->session_manager_listener->getSessionActive("dialog_id_2") == 0);

    fixture->session_manager->set("dialog_id_2", { "session_id_2", "ps_id_2" });
    g_assert(fixture->session_manager_listener->getSessionActive("dialog_id_2") == 1);

    fixture->session_manager->deactivate("dialog_id_1");
    g_assert(fixture->session_manager_listener->getSessionActive("dialog_id_1") == 0);

    fixture->session_manager->deactivate("dialog_id_2");
    g_assert(fixture->session_manager_listener->getSessionActive("dialog_id_2") == 0);
}

static void test_session_manager_maintain_recent_session_id(TestFixture* fixture, gconstpointer ignored)
{
    const auto& session_container = fixture->session_manager->getAllSessions();

    fixture->session_manager->activate("dialog_id_1");
    fixture->session_manager->set("dialog_id_1", { "session_id_1", "ps_id_1" });
    auto active_session_info = fixture->session_manager->getActiveSessionInfo();
    g_assert(session_container.size() == 1);
    g_assert(active_session_info.size() == 1);
    g_assert(active_session_info[0]["sessionId"].asString() == "session_id_1");
    g_assert(active_session_info[0]["playServiceId"].asString() == "ps_id_1");

    // extract recent sessionId only if playServiceId is same
    fixture->session_manager->activate("dialog_id_2");
    fixture->session_manager->set("dialog_id_2", { "session_id_2", "ps_id_1" });
    active_session_info = fixture->session_manager->getActiveSessionInfo();
    g_assert(session_container.size() == 2);
    g_assert(active_session_info.size() == 1);
    g_assert(active_session_info[0]["sessionId"].asString() == "session_id_2");
    g_assert(active_session_info[0]["playServiceId"].asString() == "ps_id_1");

    // separate sessionId if playServiceId is different
    fixture->session_manager->activate("dialog_id_3");
    fixture->session_manager->set("dialog_id_3", { "session_id_3", "ps_id_2" });
    active_session_info = fixture->session_manager->getActiveSessionInfo();
    g_assert(session_container.size() == 3);
    g_assert(active_session_info.size() == 2);
    g_assert(active_session_info[0]["sessionId"].asString() == "session_id_2");
    g_assert(active_session_info[0]["playServiceId"].asString() == "ps_id_1");
    g_assert(active_session_info[1]["sessionId"].asString() == "session_id_3");
    g_assert(active_session_info[1]["playServiceId"].asString() == "ps_id_2");
}

static void test_session_manager_activate_with_no_set(TestFixture* fixture, gconstpointer ignored)
{
    fixture->session_manager->addListener(fixture->session_manager_listener.get());

    fixture->session_manager->activate("dialog_id_1");
    g_assert(!fixture->session_manager_listener->isSessionActive("dialog_id_1"));
    g_assert(fixture->session_manager_listener->getCallCount() == 0);
    g_assert(fixture->session_manager->getActiveSessionInfo().empty());

    fixture->session_manager->deactivate("dialog_id_1");
    g_assert(!fixture->session_manager_listener->isSessionActive("dialog_id_1"));
    g_assert(fixture->session_manager_listener->getCallCount() == 0);
}

static void test_session_manager_clear(TestFixture* fixture, gconstpointer ignored)
{
    fixture->session_manager->addListener(fixture->session_manager_listener.get());

    fixture->session_manager->activate("dialog_id_1");
    fixture->session_manager->set("dialog_id_1", { "session_id_1", "ps_id_1" });
    fixture->session_manager->set("dialog_id_2", { "session_id_2", "ps_id_2" });
    g_assert(!fixture->session_manager->getActiveSessionInfo().empty());
    g_assert(fixture->session_manager_listener->isSessionActive("dialog_id_1"));

    fixture->session_manager->activate("dialog_id_2");
    fixture->session_manager->activate("dialog_id_2");
    g_assert(fixture->session_manager_listener->isSessionActive("dialog_id_2"));

    fixture->session_manager->clear();
    g_assert(fixture->session_manager->getActiveSessionInfo().empty());
    g_assert(!fixture->session_manager_listener->isSessionActive("dialog_id_1"));
    g_assert(!fixture->session_manager_listener->isSessionActive("dialog_id_2"));
}

static void test_session_manager_reset(TestFixture* fixture, gconstpointer ignored)
{
    const auto& session_container = fixture->session_manager->getAllSessions();
    const auto& active_list = fixture->session_manager->getActiveList();
    fixture->session_manager->addListener(fixture->session_manager_listener.get());

    fixture->session_manager->activate("dialog_id_1");
    fixture->session_manager->set("dialog_id_1", { "session_id_1", "ps_id_1" });
    g_assert(session_container.size() == 1);
    g_assert(active_list.size() == 1);

    fixture->session_manager->reset();
    g_assert(fixture->session_manager->getListenerCount() == 1);
    g_assert(session_container.empty());
    g_assert(active_list.empty());
}

int main(int argc, char* argv[])
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
    g_type_init();
#endif

    g_test_init(&argc, &argv, (void*)NULL);
    g_log_set_always_fatal((GLogLevelFlags)G_LOG_FATAL_MASK);

    G_TEST_ADD_FUNC("/core/SessionManager/listener", test_session_manager_listener);
    G_TEST_ADD_FUNC("/core/SessionManager/set", test_session_manager_set);
    G_TEST_ADD_FUNC("/core/SessionManager/get", test_session_manager_get);
    G_TEST_ADD_FUNC("/core/SessionManager/singleActive", test_session_manager_single_active);
    G_TEST_ADD_FUNC("/core/SessionManager/multiActive", test_session_manager_multi_active);
    G_TEST_ADD_FUNC("/core/SessionManager/multiAgentParticipation", test_session_manager_multi_agent_participation);
    G_TEST_ADD_FUNC("/core/SessionManager/maintainMultiSession", test_session_manager_maintain_multi_session);
    G_TEST_ADD_FUNC("/core/SessionManager/notifySingleActiveState", test_session_manager_notify_single_active_state);
    G_TEST_ADD_FUNC("/core/SessionManager/notifyMultiActiveState", test_session_manager_notify_multi_active_state);
    G_TEST_ADD_FUNC("/core/SessionManager/maintainRecentSessionId", test_session_manager_maintain_recent_session_id);
    G_TEST_ADD_FUNC("/core/SessionManager/activateWithNoSet", test_session_manager_activate_with_no_set);
    G_TEST_ADD_FUNC("/core/SessionManager/clear", test_session_manager_clear);
    G_TEST_ADD_FUNC("/core/SessionManager/reset", test_session_manager_reset);

    return g_test_run();
}
