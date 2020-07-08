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
#include <string>

#include "session_manager.hh"

using namespace NuguCore;

typedef struct {
    std::shared_ptr<SessionManager> session_manager;
} TestFixture;

static void setup(TestFixture* fixture, gconstpointer user_data)
{
    fixture->session_manager = std::make_shared<SessionManager>();
}

static void teardown(TestFixture* fixture, gconstpointer user_data)
{
    fixture->session_manager.reset();
}

#define G_TEST_ADD_FUNC(name, func) \
    g_test_add(name, TestFixture, nullptr, setup, func, teardown);

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
    g_assert(session_container.at("dialog_id_1").session_id == "session_id_2");

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

int main(int argc, char* argv[])
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
    g_type_init();
#endif

    g_test_init(&argc, &argv, (void*)NULL);
    g_log_set_always_fatal((GLogLevelFlags)G_LOG_FATAL_MASK);

    G_TEST_ADD_FUNC("/core/SessionManager/set", test_session_manager_set);
    G_TEST_ADD_FUNC("/core/SessionManager/get", test_session_manager_get);
    G_TEST_ADD_FUNC("/core/SessionManager/singleActive", test_session_manager_single_active);
    G_TEST_ADD_FUNC("/core/SessionManager/multiActive", test_session_manager_multi_active);
    G_TEST_ADD_FUNC("/core/SessionManager/multiAgentParticipation", test_session_manager_multi_agent_participation);

    return g_test_run();
}
