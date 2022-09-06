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

#include "interaction_control_manager.hh"

using namespace NuguCore;

class InteractionControlManagerListener : public IInteractionControlManagerListener {
public:
    // param-1 (bool) : whether multi-turn
    // param-2 (int) : onModeChanged method call count
    using InteractionModeChecker = std::pair<bool, int>;

public:
    void onHasMultiTurn() override
    {
        has_multi_turn = true;
    }

    void onModeChanged(bool is_multi_turn) override
    {
        if (mode_checker.first == is_multi_turn)
            mode_checker.second++;
        else
            mode_checker = { is_multi_turn, 1 };

        if (!is_multi_turn)
            has_multi_turn = false;
    };

    InteractionModeChecker getModeChecker()
    {
        return mode_checker;
    }

    bool hasMultiTurn()
    {
        return has_multi_turn;
    }

private:
    InteractionModeChecker mode_checker = { false, 0 };
    bool has_multi_turn = false;
};

using TestModeChecker = InteractionControlManagerListener::InteractionModeChecker;

typedef struct _TestFixture {
    std::shared_ptr<InteractionControlManager> ic_manager;
    std::shared_ptr<InteractionControlManagerListener> ic_manager_listener;
    std::shared_ptr<InteractionControlManagerListener> ic_manager_listener_snd;
} TestFixture;

static void setup(TestFixture* fixture, gconstpointer user_data)
{
    fixture->ic_manager = std::make_shared<InteractionControlManager>();
    fixture->ic_manager_listener = std::make_shared<InteractionControlManagerListener>();
    fixture->ic_manager_listener_snd = std::make_shared<InteractionControlManagerListener>();
}

static void teardown(TestFixture* fixture, gconstpointer user_data)
{
    fixture->ic_manager_listener.reset();
    fixture->ic_manager_listener_snd.reset();
    fixture->ic_manager.reset();
}

#define G_TEST_ADD_FUNC(name, func) \
    g_test_add(name, TestFixture, nullptr, setup, func, teardown);

static void test_interaction_control_manager_listener(TestFixture* fixture, gconstpointer ignored)
{
    fixture->ic_manager->addListener(nullptr);
    g_assert(fixture->ic_manager->getListenerCount() == 0);

    fixture->ic_manager->addListener(fixture->ic_manager_listener.get());
    g_assert(fixture->ic_manager->getListenerCount() == 1);

    // check whether same listener is added duplicately
    fixture->ic_manager->addListener(fixture->ic_manager_listener.get());
    g_assert(fixture->ic_manager->getListenerCount() == 1);

    fixture->ic_manager->addListener(fixture->ic_manager_listener_snd.get());
    g_assert(fixture->ic_manager->getListenerCount() == 2);

    fixture->ic_manager->removeListener(nullptr);
    g_assert(fixture->ic_manager->getListenerCount() == 2);

    fixture->ic_manager->removeListener(fixture->ic_manager_listener.get());
    g_assert(fixture->ic_manager->getListenerCount() == 1);

    // check whether same listener is removed duplicately
    fixture->ic_manager->removeListener(fixture->ic_manager_listener.get());
    g_assert(fixture->ic_manager->getListenerCount() == 1);

    fixture->ic_manager->removeListener(fixture->ic_manager_listener_snd.get());
    g_assert(fixture->ic_manager->getListenerCount() == 0);
}

static void test_interaction_control_manager_single_requester(TestFixture* fixture, gconstpointer ignored)
{
    fixture->ic_manager->addListener(fixture->ic_manager_listener.get());
    g_assert(fixture->ic_manager_listener->getModeChecker() == (TestModeChecker { false, 0 }));

    fixture->ic_manager->start(InteractionMode::MULTI_TURN, "");
    g_assert(fixture->ic_manager_listener->getModeChecker() == (TestModeChecker { false, 0 }));

    fixture->ic_manager->start(InteractionMode::NONE, "requester_1");
    g_assert(fixture->ic_manager_listener->getModeChecker() == (TestModeChecker { false, 0 }));

    fixture->ic_manager->start(InteractionMode::MULTI_TURN, "requester_1");
    g_assert(fixture->ic_manager_listener->getModeChecker() == (TestModeChecker { true, 1 }));

    // check whether callback is called duplicately when multi-turn is already set
    fixture->ic_manager->start(InteractionMode::MULTI_TURN, "requester_1");
    g_assert(fixture->ic_manager_listener->getModeChecker() == (TestModeChecker { true, 1 }));

    fixture->ic_manager->finish(InteractionMode::MULTI_TURN, "");
    g_assert(fixture->ic_manager_listener->getModeChecker() == (TestModeChecker { true, 1 }));

    fixture->ic_manager->finish(InteractionMode::MULTI_TURN, "requester_not_start");
    g_assert(fixture->ic_manager_listener->getModeChecker() == (TestModeChecker { true, 1 }));

    fixture->ic_manager->finish(InteractionMode::MULTI_TURN, "requester_1");
    g_assert(fixture->ic_manager_listener->getModeChecker() == (TestModeChecker { false, 1 }));

    // check whether callback is called duplicately when multi-turn is already unset
    fixture->ic_manager->finish(InteractionMode::MULTI_TURN, "requester_1");
    g_assert(fixture->ic_manager_listener->getModeChecker() == (TestModeChecker { false, 1 }));
}

static void test_interaction_control_manager_multi_requester(TestFixture* fixture, gconstpointer ignored)
{
    fixture->ic_manager->addListener(fixture->ic_manager_listener.get());
    g_assert(fixture->ic_manager_listener->getModeChecker() == (TestModeChecker { false, 0 }));

    fixture->ic_manager->start(InteractionMode::MULTI_TURN, "requester_1");
    g_assert(fixture->ic_manager_listener->getModeChecker() == (TestModeChecker { true, 1 }));

    fixture->ic_manager->start(InteractionMode::MULTI_TURN, "requester_2");
    g_assert(fixture->ic_manager_listener->getModeChecker() == (TestModeChecker { true, 1 }));

    fixture->ic_manager->finish(InteractionMode::MULTI_TURN, "requester_1");
    g_assert(fixture->ic_manager_listener->getModeChecker() == (TestModeChecker { true, 1 }));

    fixture->ic_manager->finish(InteractionMode::MULTI_TURN, "requester_2");
    g_assert(fixture->ic_manager_listener->getModeChecker() == (TestModeChecker { false, 1 }));
}

static void test_interaction_control_manager_mixed_mode_multi_requester(TestFixture* fixture, gconstpointer ignored)
{
    fixture->ic_manager->addListener(fixture->ic_manager_listener.get());
    g_assert(fixture->ic_manager_listener->getModeChecker() == (TestModeChecker { false, 0 }));

    // case-1 : first request is MULTI_TURN mode
    fixture->ic_manager->start(InteractionMode::MULTI_TURN, "requester_1");
    g_assert(fixture->ic_manager_listener->getModeChecker() == (TestModeChecker { true, 1 }));

    fixture->ic_manager->start(InteractionMode::NONE, "requester_2");
    g_assert(fixture->ic_manager_listener->getModeChecker() == (TestModeChecker { true, 1 }));

    fixture->ic_manager->finish(InteractionMode::MULTI_TURN, "requester_1");
    g_assert(fixture->ic_manager_listener->getModeChecker() == (TestModeChecker { false, 1 }));

    fixture->ic_manager->finish(InteractionMode::NONE, "requester_2");
    g_assert(fixture->ic_manager_listener->getModeChecker() == (TestModeChecker { false, 1 }));

    // case-2 : first request is NONE mode
    fixture->ic_manager->start(InteractionMode::NONE, "requester_1");
    g_assert(fixture->ic_manager_listener->getModeChecker() == (TestModeChecker { false, 1 }));

    fixture->ic_manager->start(InteractionMode::MULTI_TURN, "requester_2");
    g_assert(fixture->ic_manager_listener->getModeChecker() == (TestModeChecker { true, 1 }));

    fixture->ic_manager->finish(InteractionMode::NONE, "requester_1");
    g_assert(fixture->ic_manager_listener->getModeChecker() == (TestModeChecker { true, 1 }));

    fixture->ic_manager->finish(InteractionMode::MULTI_TURN, "requester_2");
    g_assert(fixture->ic_manager_listener->getModeChecker() == (TestModeChecker { false, 1 }));

    // case-3 : try to finish not started requester
    fixture->ic_manager->start(InteractionMode::MULTI_TURN, "requester_1");
    g_assert(fixture->ic_manager_listener->getModeChecker() == (TestModeChecker { true, 1 }));

    fixture->ic_manager->finish(InteractionMode::NONE, "requester_2");
    g_assert(fixture->ic_manager_listener->getModeChecker() == (TestModeChecker { true, 1 }));

    fixture->ic_manager->finish(InteractionMode::MULTI_TURN, "requester_2");
    g_assert(fixture->ic_manager_listener->getModeChecker() == (TestModeChecker { true, 1 }));

    fixture->ic_manager->finish(InteractionMode::MULTI_TURN, "requester_1");
    g_assert(fixture->ic_manager_listener->getModeChecker() == (TestModeChecker { false, 1 }));
}

static void test_interaction_control_manager_has_multi_turn(TestFixture* fixture, gconstpointer ignored)
{
    fixture->ic_manager->addListener(fixture->ic_manager_listener.get());
    g_assert(!fixture->ic_manager_listener->hasMultiTurn());

    fixture->ic_manager->notifyHasMultiTurn();
    g_assert(fixture->ic_manager_listener->hasMultiTurn());
}

static void test_interaction_control_manager_check_multi_turn_active(TestFixture* fixture, gconstpointer ignored)
{
    fixture->ic_manager->addListener(fixture->ic_manager_listener.get());
    g_assert(!fixture->ic_manager->isMultiTurnActive());

    fixture->ic_manager->start(InteractionMode::MULTI_TURN, "requester_1");
    g_assert(fixture->ic_manager->isMultiTurnActive());

    fixture->ic_manager->finish(InteractionMode::MULTI_TURN, "requester_1");
    g_assert(!fixture->ic_manager->isMultiTurnActive());
}

static void test_interaction_control_manager_clear(TestFixture* fixture, gconstpointer ignored)
{
    fixture->ic_manager->addListener(fixture->ic_manager_listener.get());
    g_assert(fixture->ic_manager_listener->getModeChecker() == (TestModeChecker { false, 0 }));

    fixture->ic_manager->start(InteractionMode::MULTI_TURN, "requester_1");
    g_assert(fixture->ic_manager_listener->getModeChecker() == (TestModeChecker { true, 1 }));

    fixture->ic_manager->start(InteractionMode::MULTI_TURN, "requester_2");
    g_assert(fixture->ic_manager_listener->getModeChecker() == (TestModeChecker { true, 1 }));

    fixture->ic_manager->clear();
    g_assert(fixture->ic_manager_listener->getModeChecker() == (TestModeChecker { false, 1 }));
}

static void test_interaction_control_manager_reset(TestFixture* fixture, gconstpointer ignored)
{
    const auto& requesters = fixture->ic_manager->getAllRequesters();
    fixture->ic_manager->addListener(fixture->ic_manager_listener.get());

    fixture->ic_manager->start(InteractionMode::MULTI_TURN, "requester_1");
    fixture->ic_manager->start(InteractionMode::MULTI_TURN, "requester_2");
    g_assert(requesters.size() == 2);

    fixture->ic_manager->reset();
    g_assert(fixture->ic_manager->getListenerCount() == 1);
    g_assert(requesters.empty());
}

int main(int argc, char* argv[])
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
    g_type_init();
#endif

    g_test_init(&argc, &argv, (void*)NULL);
    g_log_set_always_fatal((GLogLevelFlags)G_LOG_FATAL_MASK);

    G_TEST_ADD_FUNC("/core/InteractionControlManager/listener", test_interaction_control_manager_listener);
    G_TEST_ADD_FUNC("/core/InteractionControlManager/singleRequester", test_interaction_control_manager_single_requester);
    G_TEST_ADD_FUNC("/core/InteractionControlManager/multiRequester", test_interaction_control_manager_multi_requester);
    G_TEST_ADD_FUNC("/core/InteractionControlManager/mixedModeMultiRequester", test_interaction_control_manager_mixed_mode_multi_requester);
    G_TEST_ADD_FUNC("/core/InteractionControlManager/hasMultiTurn", test_interaction_control_manager_has_multi_turn);
    G_TEST_ADD_FUNC("/core/InteractionControlManager/checkMultiTurnActive", test_interaction_control_manager_check_multi_turn_active);
    G_TEST_ADD_FUNC("/core/InteractionControlManager/clear", test_interaction_control_manager_clear);
    G_TEST_ADD_FUNC("/core/InteractionControlManager/reset", test_interaction_control_manager_reset);

    return g_test_run();
}
