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

static void test_session_manager_set(void)
{
    auto session_manager(std::make_shared<SessionManager>());
    const auto& session_container = session_manager->getAllSessions();

    session_manager->set("", {});
    g_assert(session_container.empty());

    session_manager->set("dialog_id_1", {});
    g_assert(session_container.empty());

    session_manager->set("dialog_id_1", { "", "ps_id_1" });
    g_assert(session_container.empty());

    session_manager->set("dialog_id_1", { "session_id_1", "" });
    g_assert(session_container.empty());

    session_manager->set("dialog_id_1", { "session_id_1", "ps_id_1" });
    g_assert(session_container.size() == 1);

    session_manager->set("dialog_id_1", { "session_id_2", "ps_id_2" });
    g_assert(session_container.at("dialog_id_1").session_id == "session_id_2");

    session_manager->set("dialog_id_2", { "session_id_3", "ps_id_3" });
    g_assert(session_container.size() == 2);
}

static void test_session_manager_get(void)
{
    auto session_manager(std::make_shared<SessionManager>());

    session_manager->set("dialog_id_1", { "session_id_1", "ps_id_1" });
    session_manager->set("dialog_id_2", { "session_id_2", "ps_id_2" });
    session_manager->activate("dialog_id_1");
    session_manager->activate("dialog_id_2");
    auto active_session_info = session_manager->getActiveSessionInfo();

    g_assert(active_session_info.size() == 2
        && (active_session_info[0]["sessionId"].asString() == "session_id_1")
        && (active_session_info[1]["sessionId"].asString() == "session_id_2"));
}

static void test_session_manager_single_active(void)
{
    auto session_manager(std::make_shared<SessionManager>());
    const auto& session_container = session_manager->getAllSessions();
    const auto& active_list = session_manager->getActiveList();

    session_manager->activate("dialog_id_1");
    g_assert(active_list.size() == 1
        && session_manager->getActiveSessionInfo().isNull());

    session_manager->set("dialog_id_1", { "session_id_1", "ps_id_1" });
    auto active_session_info = session_manager->getActiveSessionInfo();
    g_assert(active_session_info.size() == 1
        && active_session_info[0]["sessionId"].asString() == "session_id_1");

    session_manager->deactivate("");
    g_assert(!session_manager->getActiveSessionInfo().isNull());

    session_manager->deactivate("dialog_id_1");
    g_assert(session_manager->getActiveSessionInfo().isNull()
        && session_container.empty()
        && active_list.empty());
}

static void test_session_manager_multi_active(void)
{
    auto session_manager(std::make_shared<SessionManager>());
    const auto& session_container = session_manager->getAllSessions();
    const auto& active_list = session_manager->getActiveList();

    session_manager->activate("dialog_id_3");
    session_manager->activate("dialog_id_4");
    session_manager->set("dialog_id_3", { "session_id_3", "ps_id_3" });
    session_manager->set("dialog_id_5", { "session_id_5", "ps_id_5" });
    auto active_session_info = session_manager->getActiveSessionInfo();

    g_assert(active_list.size() == 2 && session_container.size() == 2
        && active_session_info.size() == 1
        && active_session_info[0]["sessionId"].asString() == "session_id_3");

    session_manager->activate("dialog_id_5");
    active_session_info = session_manager->getActiveSessionInfo();
    g_assert(active_list.size() == 3 && session_container.size() == 2
        && active_session_info.size() == 2
        && active_session_info[0]["sessionId"].asString() == "session_id_3"
        && active_session_info[1]["sessionId"].asString() == "session_id_5");

    session_manager->deactivate("dialog_id_3");
    session_manager->deactivate("dialog_id_5");
    g_assert(session_manager->getActiveSessionInfo().isNull());

    session_manager->deactivate("dialog_id_4");
    g_assert(session_container.empty() && active_list.empty());
}

int main(int argc, char* argv[])
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
    g_type_init();
#endif

    g_test_init(&argc, &argv, (void*)NULL);
    g_log_set_always_fatal((GLogLevelFlags)G_LOG_FATAL_MASK);

    g_test_add_func("/core/SessionManager/set", test_session_manager_set);
    g_test_add_func("/core/SessionManager/get", test_session_manager_get);
    g_test_add_func("/core/SessionManager/singleActive", test_session_manager_single_active);
    g_test_add_func("/core/SessionManager/multiActive", test_session_manager_multi_active);

    return g_test_run();
}
