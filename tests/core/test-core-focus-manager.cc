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

#include "focus_manager.hh"

using namespace NuguCore;

#define INVALID_FOCUS_NAME "Invalid"

#define DIALOG_NAME "dialog"

#define ANOTHER_DIALOG_NAME "another_dialog"

#define COMMUNICATIONS_NAME "communications"

#define ALERTS_NAME "alerts"

#define CONTENT_NAME "content"

#define REQUEST_FOCUS(resource, type, name) \
    resource->requestFocus(type, name)

#define RELEASE_FOCUS(resource, type) \
    resource->releaseFocus(type)

#define ASSERT_EXPECTED_STATE(resource, state) \
    g_assert(resource->getState() == state);

class FocusManagerObserver : public IFocusManagerObserver {
public:
    explicit FocusManagerObserver(IFocusManager* focus_manager)
        : focus_manager_interface(focus_manager)
    {
    }
    virtual ~FocusManagerObserver() = default;

    void onFocusChanged(const FocusConfiguration& configuration, FocusState state, const std::string& name) override
    {
        std::string msg;

        msg.append("==================================================\n[")
            .append(configuration.type)
            .append(" - ")
            .append(name)
            .append("] ")
            .append(focus_manager_interface->getStateString(state))
            .append(" (priority: ")
            .append(std::to_string(configuration.priority))
            .append(")\n==================================================");

        std::cout << msg << std::endl;
    }

private:
    IFocusManager* focus_manager_interface;
};

class TestFocusResorce : public IFocusResourceListener {
public:
    explicit TestFocusResorce(IFocusManager* focus_manager)
        : focus_manager_interface(focus_manager)
        , cur_state(FocusState::NONE)
    {
    }
    virtual ~TestFocusResorce() = default;

    bool requestFocus(const std::string& type, const std::string& name)
    {
        this->name = name;
        return focus_manager_interface->requestFocus(type, name, this);
    }

    bool releaseFocus(const std::string& type)
    {
        return focus_manager_interface->releaseFocus(type, name);
    }

    void stopAllFocus()
    {
        focus_manager_interface->stopAllFocus();
    }

    void stopForegroundFocus()
    {
        focus_manager_interface->stopForegroundFocus();
    }

    void onFocusChanged(FocusState state) override
    {
        cur_state = state;
    }

    FocusState getState()
    {
        return cur_state;
    }

private:
    IFocusManager* focus_manager_interface;
    FocusState cur_state;
    std::string name;
};

typedef struct {
    FocusManager* focus_manager;
    FocusManagerObserver* focus_observer;
    TestFocusResorce* dialog_resource;
    TestFocusResorce* comm_resource;
    TestFocusResorce* alert_resource;
    TestFocusResorce* content_resource;
    TestFocusResorce* another_dialog_resource;
} ntimerFixture;

static void setup(ntimerFixture* fixture, gconstpointer user_data)
{
    fixture->focus_manager = new FocusManager();
    fixture->focus_observer = new FocusManagerObserver(fixture->focus_manager);
    fixture->focus_manager->addObserver(fixture->focus_observer);

    fixture->dialog_resource = new TestFocusResorce(fixture->focus_manager);
    fixture->comm_resource = new TestFocusResorce(fixture->focus_manager);
    fixture->alert_resource = new TestFocusResorce(fixture->focus_manager);
    fixture->content_resource = new TestFocusResorce(fixture->focus_manager);
    fixture->another_dialog_resource = new TestFocusResorce(fixture->focus_manager);

    std::vector<FocusConfiguration> focus_configuration;

    focus_configuration.push_back({ DIALOG_FOCUS_TYPE, DIALOG_FOCUS_PRIORITY });
    focus_configuration.push_back({ COMMUNICATIONS_FOCUS_TYPE, COMMUNICATIONS_FOCUS_PRIORITY });
    focus_configuration.push_back({ ALERTS_FOCUS_TYPE, ALERTS_FOCUS_PRIORITY });
    focus_configuration.push_back({ CONTENT_FOCUS_TYPE, CONTENT_FOCUS_PRIORITY });
    fixture->focus_manager->setConfigurations(focus_configuration);
}

static void teardown(ntimerFixture* fixture, gconstpointer user_data)
{
    delete fixture->focus_manager;
    delete fixture->focus_observer;
    delete fixture->dialog_resource;
    delete fixture->comm_resource;
    delete fixture->alert_resource;
    delete fixture->content_resource;
    delete fixture->another_dialog_resource;
}

#define G_TEST_ADD_FUNC(name, func) \
    g_test_add(name, ntimerFixture, NULL, setup, func, teardown);

static void test_focusmanager_configurations(ntimerFixture* fixture, gconstpointer ignored)
{
    FocusManager* focus_manager = fixture->focus_manager;
    std::vector<FocusConfiguration> focus_configuration;

    // Add Dialog Resource to FocusManager
    focus_configuration.push_back({ DIALOG_FOCUS_TYPE, DIALOG_FOCUS_PRIORITY });
    focus_manager->setConfigurations(focus_configuration);

    g_assert(focus_manager->getFocusResourcePriority(DIALOG_FOCUS_TYPE) == DIALOG_FOCUS_PRIORITY);
    g_assert(focus_manager->getFocusResourcePriority(COMMUNICATIONS_FOCUS_TYPE) == -1);

    // Add Dialog Resource with higher priority and Communication Resource to FocusManager
    focus_configuration.clear();
    focus_configuration.push_back({ DIALOG_FOCUS_TYPE, DIALOG_FOCUS_PRIORITY + 100 });
    focus_configuration.push_back({ COMMUNICATIONS_FOCUS_TYPE, COMMUNICATIONS_FOCUS_PRIORITY });
    focus_manager->setConfigurations(focus_configuration);

    g_assert(focus_manager->getFocusResourcePriority(DIALOG_FOCUS_TYPE) == (DIALOG_FOCUS_PRIORITY + 100));
    g_assert(focus_manager->getFocusResourcePriority(COMMUNICATIONS_FOCUS_TYPE) == COMMUNICATIONS_FOCUS_PRIORITY);
}

static void test_focusmanager_request_resource_with_invalid_name(ntimerFixture* fixture, gconstpointer ignored)
{
    g_assert(!REQUEST_FOCUS(fixture->dialog_resource, INVALID_FOCUS_NAME, DIALOG_NAME));
    ASSERT_EXPECTED_STATE(fixture->dialog_resource, FocusState::NONE);
}

static void test_focusmanager_request_resource_with_no_activate_resource(ntimerFixture* fixture, gconstpointer igresource)
{
    g_assert(REQUEST_FOCUS(fixture->dialog_resource, DIALOG_FOCUS_TYPE, DIALOG_NAME));
    ASSERT_EXPECTED_STATE(fixture->dialog_resource, FocusState::FOREGROUND);
}

static void test_focusmanager_request_resource_with_higher_priority_resource(ntimerFixture* fixture, gconstpointer ignored)
{
    g_assert(REQUEST_FOCUS(fixture->dialog_resource, DIALOG_FOCUS_TYPE, DIALOG_NAME));
    ASSERT_EXPECTED_STATE(fixture->dialog_resource, FocusState::FOREGROUND);

    g_assert(REQUEST_FOCUS(fixture->content_resource, CONTENT_FOCUS_TYPE, CONTENT_NAME));
    ASSERT_EXPECTED_STATE(fixture->content_resource, FocusState::BACKGROUND);
}

static void test_focusmanager_request_resource_with_lower_priority_resource(ntimerFixture* fixture, gconstpointer ignored)
{
    g_assert(REQUEST_FOCUS(fixture->content_resource, CONTENT_FOCUS_TYPE, CONTENT_NAME));
    ASSERT_EXPECTED_STATE(fixture->content_resource, FocusState::FOREGROUND);

    g_assert(REQUEST_FOCUS(fixture->dialog_resource, DIALOG_FOCUS_TYPE, DIALOG_NAME));
    ASSERT_EXPECTED_STATE(fixture->content_resource, FocusState::BACKGROUND);
    ASSERT_EXPECTED_STATE(fixture->dialog_resource, FocusState::FOREGROUND);
}

static void test_focusmanager_request_two_resources_with_highest_priority_resource(ntimerFixture* fixture, gconstpointer ignored)
{
    g_assert(REQUEST_FOCUS(fixture->dialog_resource, DIALOG_FOCUS_TYPE, DIALOG_NAME));
    ASSERT_EXPECTED_STATE(fixture->dialog_resource, FocusState::FOREGROUND);

    g_assert(REQUEST_FOCUS(fixture->alert_resource, ALERTS_FOCUS_TYPE, ALERTS_NAME));
    ASSERT_EXPECTED_STATE(fixture->alert_resource, FocusState::BACKGROUND);

    g_assert(REQUEST_FOCUS(fixture->content_resource, CONTENT_FOCUS_TYPE, CONTENT_NAME));
    ASSERT_EXPECTED_STATE(fixture->content_resource, FocusState::BACKGROUND);
}

static void test_focusmanager_request_two_resources_with_medium_priority_resource(ntimerFixture* fixture, gconstpointer ignored)
{
    g_assert(REQUEST_FOCUS(fixture->alert_resource, ALERTS_FOCUS_TYPE, ALERTS_NAME));
    ASSERT_EXPECTED_STATE(fixture->alert_resource, FocusState::FOREGROUND);

    g_assert(REQUEST_FOCUS(fixture->dialog_resource, DIALOG_FOCUS_TYPE, DIALOG_NAME));
    ASSERT_EXPECTED_STATE(fixture->alert_resource, FocusState::BACKGROUND);
    ASSERT_EXPECTED_STATE(fixture->dialog_resource, FocusState::FOREGROUND);

    g_assert(REQUEST_FOCUS(fixture->content_resource, CONTENT_FOCUS_TYPE, CONTENT_NAME));
    ASSERT_EXPECTED_STATE(fixture->content_resource, FocusState::BACKGROUND);
    ASSERT_EXPECTED_STATE(fixture->alert_resource, FocusState::BACKGROUND);
    ASSERT_EXPECTED_STATE(fixture->dialog_resource, FocusState::FOREGROUND);
}

static void test_focusmanager_request_two_resources_with_lowest_priority_resource(ntimerFixture* fixture, gconstpointer ignored)
{
    g_assert(REQUEST_FOCUS(fixture->content_resource, CONTENT_FOCUS_TYPE, CONTENT_NAME));
    ASSERT_EXPECTED_STATE(fixture->content_resource, FocusState::FOREGROUND);

    g_assert(REQUEST_FOCUS(fixture->alert_resource, ALERTS_FOCUS_TYPE, ALERTS_NAME));
    ASSERT_EXPECTED_STATE(fixture->content_resource, FocusState::BACKGROUND);
    ASSERT_EXPECTED_STATE(fixture->alert_resource, FocusState::FOREGROUND);

    g_assert(REQUEST_FOCUS(fixture->dialog_resource, DIALOG_FOCUS_TYPE, DIALOG_NAME));
    ASSERT_EXPECTED_STATE(fixture->dialog_resource, FocusState::FOREGROUND);
    ASSERT_EXPECTED_STATE(fixture->alert_resource, FocusState::BACKGROUND);
    ASSERT_EXPECTED_STATE(fixture->content_resource, FocusState::BACKGROUND);
}

static void test_focusmanager_request_same_resource_on_foreground(ntimerFixture* fixture, gconstpointer ignored)
{
    g_assert(REQUEST_FOCUS(fixture->content_resource, CONTENT_FOCUS_TYPE, CONTENT_NAME));
    ASSERT_EXPECTED_STATE(fixture->content_resource, FocusState::FOREGROUND);

    g_assert(REQUEST_FOCUS(fixture->content_resource, CONTENT_FOCUS_TYPE, CONTENT_NAME));
    ASSERT_EXPECTED_STATE(fixture->content_resource, FocusState::FOREGROUND);
}

static void test_focusmanager_request_same_resource_on_background(ntimerFixture* fixture, gconstpointer ignored)
{
    g_assert(REQUEST_FOCUS(fixture->dialog_resource, DIALOG_FOCUS_TYPE, DIALOG_NAME));
    ASSERT_EXPECTED_STATE(fixture->dialog_resource, FocusState::FOREGROUND);

    g_assert(REQUEST_FOCUS(fixture->content_resource, CONTENT_FOCUS_TYPE, CONTENT_NAME));
    ASSERT_EXPECTED_STATE(fixture->content_resource, FocusState::BACKGROUND);

    g_assert(REQUEST_FOCUS(fixture->content_resource, CONTENT_FOCUS_TYPE, CONTENT_NAME));
    ASSERT_EXPECTED_STATE(fixture->content_resource, FocusState::BACKGROUND);
}

static void test_focusmanager_request_same_resource_with_other_name(ntimerFixture* fixture, gconstpointer ignored)
{
    g_assert(REQUEST_FOCUS(fixture->dialog_resource, DIALOG_FOCUS_TYPE, DIALOG_NAME));
    ASSERT_EXPECTED_STATE(fixture->dialog_resource, FocusState::FOREGROUND);

    g_assert(REQUEST_FOCUS(fixture->another_dialog_resource, DIALOG_FOCUS_TYPE, ANOTHER_DIALOG_NAME));
    ASSERT_EXPECTED_STATE(fixture->dialog_resource, FocusState::NONE);
    ASSERT_EXPECTED_STATE(fixture->another_dialog_resource, FocusState::FOREGROUND);
}

static void test_focusmanager_release_no_activity_resource(ntimerFixture* fixture, gconstpointer ignored)
{
    ASSERT_EXPECTED_STATE(fixture->dialog_resource, FocusState::NONE);
    g_assert(RELEASE_FOCUS(fixture->dialog_resource, DIALOG_FOCUS_TYPE));
}

static void test_focusmanager_release_resource_with_other_name(ntimerFixture* fixture, gconstpointer ignored)
{
    g_assert(REQUEST_FOCUS(fixture->dialog_resource, DIALOG_FOCUS_TYPE, DIALOG_NAME));
    ASSERT_EXPECTED_STATE(fixture->dialog_resource, FocusState::FOREGROUND);

    g_assert(REQUEST_FOCUS(fixture->another_dialog_resource, DIALOG_FOCUS_TYPE, ANOTHER_DIALOG_NAME));
    ASSERT_EXPECTED_STATE(fixture->dialog_resource, FocusState::NONE);
    ASSERT_EXPECTED_STATE(fixture->another_dialog_resource, FocusState::FOREGROUND);

    g_assert(RELEASE_FOCUS(fixture->dialog_resource, DIALOG_FOCUS_TYPE));
    ASSERT_EXPECTED_STATE(fixture->another_dialog_resource, FocusState::FOREGROUND);
}

static void test_focusmanager_release_foreground_resource(ntimerFixture* fixture, gconstpointer ignored)
{
    g_assert(REQUEST_FOCUS(fixture->dialog_resource, DIALOG_FOCUS_TYPE, DIALOG_NAME));
    ASSERT_EXPECTED_STATE(fixture->dialog_resource, FocusState::FOREGROUND);

    g_assert(RELEASE_FOCUS(fixture->dialog_resource, DIALOG_FOCUS_TYPE));
    ASSERT_EXPECTED_STATE(fixture->dialog_resource, FocusState::NONE);
}

static void test_focusmanager_release_foreground_resource_with_background_resource(ntimerFixture* fixture, gconstpointer ignored)
{
    g_assert(REQUEST_FOCUS(fixture->dialog_resource, DIALOG_FOCUS_TYPE, DIALOG_NAME));
    ASSERT_EXPECTED_STATE(fixture->dialog_resource, FocusState::FOREGROUND);

    g_assert(REQUEST_FOCUS(fixture->content_resource, CONTENT_FOCUS_TYPE, CONTENT_NAME));
    ASSERT_EXPECTED_STATE(fixture->content_resource, FocusState::BACKGROUND);

    g_assert(RELEASE_FOCUS(fixture->dialog_resource, DIALOG_FOCUS_TYPE));
    ASSERT_EXPECTED_STATE(fixture->dialog_resource, FocusState::NONE);
    ASSERT_EXPECTED_STATE(fixture->content_resource, FocusState::FOREGROUND);
}

static void test_focusmanager_release_background_resource(ntimerFixture* fixture, gconstpointer ignored)
{
    g_assert(REQUEST_FOCUS(fixture->dialog_resource, DIALOG_FOCUS_TYPE, DIALOG_NAME));
    ASSERT_EXPECTED_STATE(fixture->dialog_resource, FocusState::FOREGROUND);

    g_assert(REQUEST_FOCUS(fixture->content_resource, CONTENT_FOCUS_TYPE, CONTENT_NAME));
    ASSERT_EXPECTED_STATE(fixture->content_resource, FocusState::BACKGROUND);

    g_assert(RELEASE_FOCUS(fixture->content_resource, CONTENT_FOCUS_TYPE));
    ASSERT_EXPECTED_STATE(fixture->dialog_resource, FocusState::FOREGROUND);
    ASSERT_EXPECTED_STATE(fixture->content_resource, FocusState::NONE);
}

static void test_focusmanager_stop_foreground_focus_with_one_activity(ntimerFixture* fixture, gconstpointer ignored)
{
    g_assert(REQUEST_FOCUS(fixture->dialog_resource, DIALOG_FOCUS_TYPE, DIALOG_NAME));
    ASSERT_EXPECTED_STATE(fixture->dialog_resource, FocusState::FOREGROUND);

    fixture->dialog_resource->stopForegroundFocus();
    ASSERT_EXPECTED_STATE(fixture->dialog_resource, FocusState::NONE);
}

static void test_focusmanager_stop_foreground_focus_with_one_activity_one_pending(ntimerFixture* fixture, gconstpointer ignored)
{
    g_assert(REQUEST_FOCUS(fixture->dialog_resource, DIALOG_FOCUS_TYPE, DIALOG_NAME));
    ASSERT_EXPECTED_STATE(fixture->dialog_resource, FocusState::FOREGROUND);

    g_assert(REQUEST_FOCUS(fixture->content_resource, CONTENT_FOCUS_TYPE, CONTENT_NAME));
    ASSERT_EXPECTED_STATE(fixture->content_resource, FocusState::BACKGROUND);

    fixture->dialog_resource->stopForegroundFocus();
    ASSERT_EXPECTED_STATE(fixture->dialog_resource, FocusState::NONE);
    ASSERT_EXPECTED_STATE(fixture->content_resource, FocusState::FOREGROUND);
}

static void test_focusmanager_stop_all_focus_with_no_resource(ntimerFixture* fixture, gconstpointer ignored)
{
    ASSERT_EXPECTED_STATE(fixture->dialog_resource, FocusState::NONE);
    fixture->dialog_resource->stopAllFocus();
}

static void test_focusmanager_stop_all_focus_with_one_resource(ntimerFixture* fixture, gconstpointer ignored)
{
    g_assert(REQUEST_FOCUS(fixture->dialog_resource, DIALOG_FOCUS_TYPE, DIALOG_NAME));
    ASSERT_EXPECTED_STATE(fixture->dialog_resource, FocusState::FOREGROUND);

    fixture->dialog_resource->stopAllFocus();
    ASSERT_EXPECTED_STATE(fixture->dialog_resource, FocusState::NONE);
}

static void test_focusmanager_stop_all_focus_with_two_resources(ntimerFixture* fixture, gconstpointer ignored)
{
    g_assert(REQUEST_FOCUS(fixture->dialog_resource, DIALOG_FOCUS_TYPE, DIALOG_NAME));
    ASSERT_EXPECTED_STATE(fixture->dialog_resource, FocusState::FOREGROUND);

    g_assert(REQUEST_FOCUS(fixture->content_resource, CONTENT_FOCUS_TYPE, CONTENT_NAME));
    ASSERT_EXPECTED_STATE(fixture->content_resource, FocusState::BACKGROUND);

    fixture->dialog_resource->stopAllFocus();
    ASSERT_EXPECTED_STATE(fixture->dialog_resource, FocusState::NONE);
    ASSERT_EXPECTED_STATE(fixture->content_resource, FocusState::NONE);
}

int main(int argc, char* argv[])
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
    g_type_init();
#endif

    g_test_init(&argc, &argv, (void*)NULL);
    g_log_set_always_fatal((GLogLevelFlags)G_LOG_FATAL_MASK);

    G_TEST_ADD_FUNC("/core/FocusManager/FocusConfigurations", test_focusmanager_configurations);
    G_TEST_ADD_FUNC("/core/FocusManager/RequestResourceInvalidName", test_focusmanager_request_resource_with_invalid_name);
    G_TEST_ADD_FUNC("/core/FocusManager/RequestResourceWithNoActivateResource", test_focusmanager_request_resource_with_no_activate_resource);
    G_TEST_ADD_FUNC("/core/FocusManager/RequestResourceWithHigherPriorityResource", test_focusmanager_request_resource_with_higher_priority_resource);
    G_TEST_ADD_FUNC("/core/FocusManager/RequestResourceWithLowerPriorityResource", test_focusmanager_request_resource_with_lower_priority_resource);
    G_TEST_ADD_FUNC("/core/FocusManager/RequestTowResourcesWithHighestPriorityResource", test_focusmanager_request_two_resources_with_highest_priority_resource);
    G_TEST_ADD_FUNC("/core/FocusManager/RequestTowResourcesWithMediumPriorityResource", test_focusmanager_request_two_resources_with_medium_priority_resource);
    G_TEST_ADD_FUNC("/core/FocusManager/RequestTowResourcesWithLowestPriorityResource", test_focusmanager_request_two_resources_with_lowest_priority_resource);
    G_TEST_ADD_FUNC("/core/FocusManager/RequestSameResourceOnForeground", test_focusmanager_request_same_resource_on_foreground);
    G_TEST_ADD_FUNC("/core/FocusManager/RequestSameResourceOnBackground", test_focusmanager_request_same_resource_on_background);
    G_TEST_ADD_FUNC("/core/FocusManager/RequestSameResourceWithOtherInterfaceName", test_focusmanager_request_same_resource_with_other_name);
    G_TEST_ADD_FUNC("/core/FocusManager/ReleaseNoActivityResource", test_focusmanager_release_no_activity_resource);
    G_TEST_ADD_FUNC("/core/FocusManager/ReleaseResourceWithOtherInterfaceName", test_focusmanager_release_resource_with_other_name);
    G_TEST_ADD_FUNC("/core/FocusManager/ReleaseForegroundResource", test_focusmanager_release_foreground_resource);
    G_TEST_ADD_FUNC("/core/FocusManager/ReleaseForegroundResourceWithBackgroundResource", test_focusmanager_release_foreground_resource_with_background_resource);
    G_TEST_ADD_FUNC("/core/FocusManager/ReleaseBackgroundResource", test_focusmanager_release_background_resource);
    G_TEST_ADD_FUNC("/core/FocusManager/StopForegroundFocusWithOneActivity", test_focusmanager_stop_foreground_focus_with_one_activity);
    G_TEST_ADD_FUNC("/core/FocusManager/StopForegroundFocusWithOneActivityOnePending", test_focusmanager_stop_foreground_focus_with_one_activity_one_pending);
    G_TEST_ADD_FUNC("/core/FocusManager/StopAllFocusWithNoResource", test_focusmanager_stop_all_focus_with_no_resource);
    G_TEST_ADD_FUNC("/core/FocusManager/StopAllFocusWithOneResource", test_focusmanager_stop_all_focus_with_one_resource);
    G_TEST_ADD_FUNC("/core/FocusManager/StopAllFocusWithTwoResources", test_focusmanager_stop_all_focus_with_two_resources);

    return g_test_run();
}
