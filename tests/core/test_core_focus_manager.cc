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

#define ASR_USER_FOCUS_TYPE "ASRUser"
#define ASR_USER_FOCUS_PRIORITY 100

#define INFO_FOCUS_TYPE "Info"
#define INFO_FOCUS_PRIORITY 200

#define ALERTS_FOCUS_TYPE "Alerts"
#define ALERTS_FOCUS_PRIORITY 300

#define MEDIA_FOCUS_TYPE "Media"
#define MEDIA_FOCUS_PRIORITY 400

#define INVALID_FOCUS_NAME "Invalid"

#define ASR_USER_NAME "asr_user"

#define ANOTHER_ASR_USER_NAME "another_asr_user"

#define INFO_NAME "information"

#define ALERTS_NAME "alerts"

#define MEDIA_NAME "media"

#define REQUEST_FOCUS(resource, type, name) \
    resource->requestFocus(type, name)

#define RELEASE_FOCUS(resource, type) \
    resource->releaseFocus(type)

#define HOLD_FOCUS(resource, type) \
    resource->holdFocus(type)

#define UNHOLD_FOCUS(resource, type) \
    resource->unholdFocus(type)

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

    bool holdFocus(const std::string& type)
    {
        return focus_manager_interface->holdFocus(type);
    }

    bool unholdFocus(const std::string& type)
    {
        return focus_manager_interface->unholdFocus(type);
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
    TestFocusResorce* asr_resource;
    TestFocusResorce* info_resource;
    TestFocusResorce* alert_resource;
    TestFocusResorce* media_resource;
    TestFocusResorce* another_asr_resource;
} nFocusFixture;

static void setup(nFocusFixture* fixture, gconstpointer user_data)
{
    fixture->focus_manager = new FocusManager();
    fixture->focus_observer = new FocusManagerObserver(fixture->focus_manager);
    fixture->focus_manager->addObserver(fixture->focus_observer);

    fixture->asr_resource = new TestFocusResorce(fixture->focus_manager);
    fixture->info_resource = new TestFocusResorce(fixture->focus_manager);
    fixture->alert_resource = new TestFocusResorce(fixture->focus_manager);
    fixture->media_resource = new TestFocusResorce(fixture->focus_manager);
    fixture->another_asr_resource = new TestFocusResorce(fixture->focus_manager);

    std::vector<FocusConfiguration> focus_configuration;

    focus_configuration.push_back({ ASR_USER_FOCUS_TYPE, ASR_USER_FOCUS_PRIORITY });
    focus_configuration.push_back({ INFO_FOCUS_TYPE, INFO_FOCUS_PRIORITY });
    focus_configuration.push_back({ ALERTS_FOCUS_TYPE, ALERTS_FOCUS_PRIORITY });
    focus_configuration.push_back({ MEDIA_FOCUS_TYPE, MEDIA_FOCUS_PRIORITY });
    fixture->focus_manager->setConfigurations(focus_configuration, focus_configuration);
}

static void teardown(nFocusFixture* fixture, gconstpointer user_data)
{
    delete fixture->focus_manager;
    delete fixture->focus_observer;
    delete fixture->asr_resource;
    delete fixture->info_resource;
    delete fixture->alert_resource;
    delete fixture->media_resource;
    delete fixture->another_asr_resource;
}

static void _setup_nugu_focus_policy(nFocusFixture* fixture)
{
    std::vector<FocusConfiguration> request_configuration {
        { CALL_FOCUS_TYPE, CALL_FOCUS_REQUEST_PRIORITY },
        { ASR_USER_FOCUS_TYPE, ASR_USER_FOCUS_REQUEST_PRIORITY },
        { INFO_FOCUS_TYPE, INFO_FOCUS_REQUEST_PRIORITY },
        { ASR_DM_FOCUS_TYPE, ASR_DM_FOCUS_REQUEST_PRIORITY },
        { ALERTS_FOCUS_TYPE, ALERTS_FOCUS_REQUEST_PRIORITY },
        { ASR_BEEP_FOCUS_TYPE, ASR_BEEP_FOCUS_REQUEST_PRIORITY },
        { MEDIA_FOCUS_TYPE, MEDIA_FOCUS_REQUEST_PRIORITY },
        { SOUND_FOCUS_TYPE, SOUND_FOCUS_REQUEST_PRIORITY },
        { DUMMY_FOCUS_TYPE, DUMMY_FOCUS_REQUEST_PRIORITY }
    };

    std::vector<FocusConfiguration> release_configuration {
        { CALL_FOCUS_TYPE, CALL_FOCUS_RELEASE_PRIORITY },
        { ASR_USER_FOCUS_TYPE, ASR_USER_FOCUS_RELEASE_PRIORITY },
        { ASR_DM_FOCUS_TYPE, ASR_DM_FOCUS_RELEASE_PRIORITY },
        { INFO_FOCUS_TYPE, INFO_FOCUS_RELEASE_PRIORITY },
        { ALERTS_FOCUS_TYPE, ALERTS_FOCUS_RELEASE_PRIORITY },
        { ASR_BEEP_FOCUS_TYPE, ASR_BEEP_FOCUS_RELEASE_PRIORITY },
        { MEDIA_FOCUS_TYPE, MEDIA_FOCUS_RELEASE_PRIORITY },
        { SOUND_FOCUS_TYPE, SOUND_FOCUS_RELEASE_PRIORITY },
        { DUMMY_FOCUS_TYPE, DUMMY_FOCUS_RELEASE_PRIORITY }
    };

    fixture->focus_manager->setConfigurations(request_configuration, release_configuration);
}

#define G_TEST_ADD_FUNC(name, func) \
    g_test_add(name, nFocusFixture, NULL, setup, func, teardown);

static void test_focusmanager_configurations(nFocusFixture* fixture, gconstpointer ignored)
{
    FocusManager* focus_manager = fixture->focus_manager;
    std::vector<FocusConfiguration> focus_configuration;

    // Add asr user Resource to FocusManager
    focus_configuration.push_back({ ASR_USER_FOCUS_TYPE, ASR_USER_FOCUS_PRIORITY });
    focus_manager->setConfigurations(focus_configuration, focus_configuration);

    g_assert(focus_manager->getFocusResourcePriority(ASR_USER_FOCUS_TYPE) == ASR_USER_FOCUS_PRIORITY);
    g_assert(focus_manager->getFocusResourcePriority(INFO_FOCUS_TYPE) == -1);

    // Add asr user Resource with higher priority and Information Resource to FocusManager
    focus_configuration.clear();
    focus_configuration.push_back({ ASR_USER_FOCUS_TYPE, ASR_USER_FOCUS_PRIORITY + 100 });
    focus_configuration.push_back({ INFO_FOCUS_TYPE, INFO_FOCUS_PRIORITY });
    focus_manager->setConfigurations(focus_configuration, focus_configuration);

    g_assert(focus_manager->getFocusResourcePriority(ASR_USER_FOCUS_TYPE) == (ASR_USER_FOCUS_PRIORITY + 100));
    g_assert(focus_manager->getFocusResourcePriority(INFO_FOCUS_TYPE) == INFO_FOCUS_PRIORITY);
}

static void test_focusmanager_request_resource_with_invalid_name(nFocusFixture* fixture, gconstpointer ignored)
{
    g_assert(!REQUEST_FOCUS(fixture->asr_resource, INVALID_FOCUS_NAME, ASR_USER_NAME));
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::NONE);
}

static void test_focusmanager_request_resource_with_no_activate_resource(nFocusFixture* fixture, gconstpointer igresource)
{
    g_assert(REQUEST_FOCUS(fixture->asr_resource, ASR_USER_FOCUS_TYPE, ASR_USER_NAME));
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::FOREGROUND);
}

static void test_focusmanager_request_resource_with_higher_priority_resource(nFocusFixture* fixture, gconstpointer ignored)
{
    g_assert(REQUEST_FOCUS(fixture->asr_resource, ASR_USER_FOCUS_TYPE, ASR_USER_NAME));
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::FOREGROUND);

    g_assert(REQUEST_FOCUS(fixture->media_resource, MEDIA_FOCUS_TYPE, MEDIA_NAME));
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::BACKGROUND);
}

static void test_focusmanager_request_resource_with_lower_priority_resource(nFocusFixture* fixture, gconstpointer ignored)
{
    g_assert(REQUEST_FOCUS(fixture->media_resource, MEDIA_FOCUS_TYPE, MEDIA_NAME));
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::FOREGROUND);

    g_assert(REQUEST_FOCUS(fixture->asr_resource, ASR_USER_FOCUS_TYPE, ASR_USER_NAME));
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::BACKGROUND);
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::FOREGROUND);
}

static void test_focusmanager_request_two_resources_with_highest_priority_resource(nFocusFixture* fixture, gconstpointer ignored)
{
    g_assert(REQUEST_FOCUS(fixture->asr_resource, ASR_USER_FOCUS_TYPE, ASR_USER_NAME));
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::FOREGROUND);

    g_assert(REQUEST_FOCUS(fixture->alert_resource, ALERTS_FOCUS_TYPE, ALERTS_NAME));
    ASSERT_EXPECTED_STATE(fixture->alert_resource, FocusState::BACKGROUND);

    g_assert(REQUEST_FOCUS(fixture->media_resource, MEDIA_FOCUS_TYPE, MEDIA_NAME));
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::BACKGROUND);
}

static void test_focusmanager_request_two_resources_with_medium_priority_resource(nFocusFixture* fixture, gconstpointer ignored)
{
    g_assert(REQUEST_FOCUS(fixture->alert_resource, ALERTS_FOCUS_TYPE, ALERTS_NAME));
    ASSERT_EXPECTED_STATE(fixture->alert_resource, FocusState::FOREGROUND);

    g_assert(REQUEST_FOCUS(fixture->asr_resource, ASR_USER_FOCUS_TYPE, ASR_USER_NAME));
    ASSERT_EXPECTED_STATE(fixture->alert_resource, FocusState::BACKGROUND);
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::FOREGROUND);

    g_assert(REQUEST_FOCUS(fixture->media_resource, MEDIA_FOCUS_TYPE, MEDIA_NAME));
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::BACKGROUND);
    ASSERT_EXPECTED_STATE(fixture->alert_resource, FocusState::BACKGROUND);
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::FOREGROUND);
}

static void test_focusmanager_request_two_resources_with_lowest_priority_resource(nFocusFixture* fixture, gconstpointer ignored)
{
    g_assert(REQUEST_FOCUS(fixture->media_resource, MEDIA_FOCUS_TYPE, MEDIA_NAME));
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::FOREGROUND);

    g_assert(REQUEST_FOCUS(fixture->alert_resource, ALERTS_FOCUS_TYPE, ALERTS_NAME));
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::BACKGROUND);
    ASSERT_EXPECTED_STATE(fixture->alert_resource, FocusState::FOREGROUND);

    g_assert(REQUEST_FOCUS(fixture->asr_resource, ASR_USER_FOCUS_TYPE, ASR_USER_NAME));
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::FOREGROUND);
    ASSERT_EXPECTED_STATE(fixture->alert_resource, FocusState::BACKGROUND);
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::BACKGROUND);
}

static void test_focusmanager_request_same_resource_on_foreground(nFocusFixture* fixture, gconstpointer ignored)
{
    g_assert(REQUEST_FOCUS(fixture->media_resource, MEDIA_FOCUS_TYPE, MEDIA_NAME));
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::FOREGROUND);

    g_assert(REQUEST_FOCUS(fixture->media_resource, MEDIA_FOCUS_TYPE, MEDIA_NAME));
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::FOREGROUND);
}

static void test_focusmanager_request_same_resource_on_background(nFocusFixture* fixture, gconstpointer ignored)
{
    g_assert(REQUEST_FOCUS(fixture->asr_resource, ASR_USER_FOCUS_TYPE, ASR_USER_NAME));
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::FOREGROUND);

    g_assert(REQUEST_FOCUS(fixture->media_resource, MEDIA_FOCUS_TYPE, MEDIA_NAME));
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::BACKGROUND);

    g_assert(REQUEST_FOCUS(fixture->media_resource, MEDIA_FOCUS_TYPE, MEDIA_NAME));
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::BACKGROUND);
}

static void test_focusmanager_request_same_resource_with_other_name(nFocusFixture* fixture, gconstpointer ignored)
{
    g_assert(REQUEST_FOCUS(fixture->asr_resource, ASR_USER_FOCUS_TYPE, ASR_USER_NAME));
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::FOREGROUND);

    g_assert(REQUEST_FOCUS(fixture->another_asr_resource, ASR_USER_FOCUS_TYPE, ANOTHER_ASR_USER_NAME));
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::NONE);
    ASSERT_EXPECTED_STATE(fixture->another_asr_resource, FocusState::FOREGROUND);
}

static void test_focusmanager_release_no_activity_resource(nFocusFixture* fixture, gconstpointer ignored)
{
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::NONE);
    g_assert(RELEASE_FOCUS(fixture->asr_resource, ASR_USER_FOCUS_TYPE));
}

static void test_focusmanager_release_resource_with_other_name(nFocusFixture* fixture, gconstpointer ignored)
{
    g_assert(REQUEST_FOCUS(fixture->asr_resource, ASR_USER_FOCUS_TYPE, ASR_USER_NAME));
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::FOREGROUND);

    g_assert(REQUEST_FOCUS(fixture->another_asr_resource, ASR_USER_FOCUS_TYPE, ANOTHER_ASR_USER_NAME));
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::NONE);
    ASSERT_EXPECTED_STATE(fixture->another_asr_resource, FocusState::FOREGROUND);

    g_assert(RELEASE_FOCUS(fixture->asr_resource, ASR_USER_FOCUS_TYPE));
    ASSERT_EXPECTED_STATE(fixture->another_asr_resource, FocusState::FOREGROUND);
}

static void test_focusmanager_release_foreground_resource(nFocusFixture* fixture, gconstpointer ignored)
{
    g_assert(REQUEST_FOCUS(fixture->asr_resource, ASR_USER_FOCUS_TYPE, ASR_USER_NAME));
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::FOREGROUND);

    g_assert(RELEASE_FOCUS(fixture->asr_resource, ASR_USER_FOCUS_TYPE));
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::NONE);
}

static void test_focusmanager_release_foreground_resource_with_background_resource(nFocusFixture* fixture, gconstpointer ignored)
{
    g_assert(REQUEST_FOCUS(fixture->asr_resource, ASR_USER_FOCUS_TYPE, ASR_USER_NAME));
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::FOREGROUND);

    g_assert(REQUEST_FOCUS(fixture->media_resource, MEDIA_FOCUS_TYPE, MEDIA_NAME));
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::BACKGROUND);

    g_assert(RELEASE_FOCUS(fixture->asr_resource, ASR_USER_FOCUS_TYPE));
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::NONE);
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::FOREGROUND);
}

static void test_focusmanager_release_background_resource(nFocusFixture* fixture, gconstpointer ignored)
{
    g_assert(REQUEST_FOCUS(fixture->asr_resource, ASR_USER_FOCUS_TYPE, ASR_USER_NAME));
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::FOREGROUND);

    g_assert(REQUEST_FOCUS(fixture->media_resource, MEDIA_FOCUS_TYPE, MEDIA_NAME));
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::BACKGROUND);

    g_assert(RELEASE_FOCUS(fixture->media_resource, MEDIA_FOCUS_TYPE));
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::FOREGROUND);
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::NONE);
}

static void test_focusmanager_stop_foreground_focus_with_one_activity(nFocusFixture* fixture, gconstpointer ignored)
{
    g_assert(REQUEST_FOCUS(fixture->asr_resource, ASR_USER_FOCUS_TYPE, ASR_USER_NAME));
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::FOREGROUND);

    fixture->asr_resource->stopForegroundFocus();
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::NONE);
}

static void test_focusmanager_stop_foreground_focus_with_one_activity_one_pending(nFocusFixture* fixture, gconstpointer ignored)
{
    g_assert(REQUEST_FOCUS(fixture->asr_resource, ASR_USER_FOCUS_TYPE, ASR_USER_NAME));
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::FOREGROUND);

    g_assert(REQUEST_FOCUS(fixture->media_resource, MEDIA_FOCUS_TYPE, MEDIA_NAME));
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::BACKGROUND);

    fixture->asr_resource->stopForegroundFocus();
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::NONE);
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::FOREGROUND);
}

static void test_focusmanager_stop_all_focus_with_no_resource(nFocusFixture* fixture, gconstpointer ignored)
{
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::NONE);
    fixture->asr_resource->stopAllFocus();
}

static void test_focusmanager_stop_all_focus_with_one_resource(nFocusFixture* fixture, gconstpointer ignored)
{
    g_assert(REQUEST_FOCUS(fixture->asr_resource, ASR_USER_FOCUS_TYPE, ASR_USER_NAME));
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::FOREGROUND);

    fixture->asr_resource->stopAllFocus();
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::NONE);
}

static void test_focusmanager_stop_all_focus_with_two_resources(nFocusFixture* fixture, gconstpointer ignored)
{
    g_assert(REQUEST_FOCUS(fixture->asr_resource, ASR_USER_FOCUS_TYPE, ASR_USER_NAME));
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::FOREGROUND);

    g_assert(REQUEST_FOCUS(fixture->media_resource, MEDIA_FOCUS_TYPE, MEDIA_NAME));
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::BACKGROUND);

    fixture->asr_resource->stopAllFocus();
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::NONE);
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::NONE);
}

static void test_focusmanager_hold_and_unhold_basic_function(nFocusFixture* fixture, gconstpointer ignored)
{
    _setup_nugu_focus_policy(fixture);

    // request priority: ASR USER > Media
    g_assert(REQUEST_FOCUS(fixture->asr_resource, ASR_USER_FOCUS_TYPE, ASR_USER_NAME));
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::FOREGROUND);

    g_assert(REQUEST_FOCUS(fixture->media_resource, MEDIA_FOCUS_TYPE, MEDIA_NAME));
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::BACKGROUND);

    // hold focus: ASR USER
    g_assert(HOLD_FOCUS(fixture->asr_resource, ASR_USER_FOCUS_TYPE));

    g_assert(RELEASE_FOCUS(fixture->asr_resource, ASR_USER_FOCUS_TYPE));
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::NONE);
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::BACKGROUND);

    g_assert(UNHOLD_FOCUS(fixture->asr_resource, ASR_USER_FOCUS_TYPE));
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::FOREGROUND);
}

static void test_focusmanager_request_higher_priority_resource_than_hold_resource(nFocusFixture* fixture, gconstpointer ignored)
{
    _setup_nugu_focus_policy(fixture);

    g_assert(REQUEST_FOCUS(fixture->alert_resource, ALERTS_FOCUS_TYPE, ALERTS_NAME));
    ASSERT_EXPECTED_STATE(fixture->alert_resource, FocusState::FOREGROUND);

    g_assert(HOLD_FOCUS(fixture->alert_resource, ALERTS_FOCUS_TYPE));

    g_assert(REQUEST_FOCUS(fixture->asr_resource, ASR_USER_FOCUS_TYPE, ASR_USER_NAME));
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::FOREGROUND);
    ASSERT_EXPECTED_STATE(fixture->alert_resource, FocusState::BACKGROUND);
}

static void test_focusmanager_request_lower_priority_resource_than_hold_resource(nFocusFixture* fixture, gconstpointer ignored)
{
    _setup_nugu_focus_policy(fixture);

    g_assert(REQUEST_FOCUS(fixture->alert_resource, ALERTS_FOCUS_TYPE, ALERTS_NAME));
    ASSERT_EXPECTED_STATE(fixture->alert_resource, FocusState::FOREGROUND);

    g_assert(HOLD_FOCUS(fixture->alert_resource, ALERTS_FOCUS_TYPE));

    g_assert(REQUEST_FOCUS(fixture->media_resource, MEDIA_FOCUS_TYPE, MEDIA_NAME));
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::BACKGROUND);
    ASSERT_EXPECTED_STATE(fixture->alert_resource, FocusState::FOREGROUND);
}

static void test_focusmanager_request_same_priority_resource_and_auto_unhold(nFocusFixture* fixture, gconstpointer ignored)
{
    _setup_nugu_focus_policy(fixture);

    g_assert(HOLD_FOCUS(fixture->alert_resource, ALERTS_FOCUS_TYPE));

    g_assert(REQUEST_FOCUS(fixture->alert_resource, ALERTS_FOCUS_TYPE, ALERTS_NAME));
    g_assert(RELEASE_FOCUS(fixture->alert_resource, ALERTS_FOCUS_TYPE));
    ASSERT_EXPECTED_STATE(fixture->alert_resource, FocusState::NONE);

    g_assert(REQUEST_FOCUS(fixture->media_resource, MEDIA_FOCUS_TYPE, MEDIA_NAME));
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::FOREGROUND);
}

static void test_focusmanager_release_active_resource_and_hold_lower_priority_resource(nFocusFixture* fixture, gconstpointer ignored)
{
    _setup_nugu_focus_policy(fixture);

    g_assert(REQUEST_FOCUS(fixture->alert_resource, ALERTS_FOCUS_TYPE, ALERTS_NAME));
    ASSERT_EXPECTED_STATE(fixture->alert_resource, FocusState::FOREGROUND);

    g_assert(HOLD_FOCUS(fixture->alert_resource, ALERTS_FOCUS_TYPE));

    g_assert(REQUEST_FOCUS(fixture->media_resource, MEDIA_FOCUS_TYPE, MEDIA_NAME));
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::BACKGROUND);
    ASSERT_EXPECTED_STATE(fixture->alert_resource, FocusState::FOREGROUND);

    // media focus is still background because info is holded with higher priority.
    g_assert(RELEASE_FOCUS(fixture->alert_resource, ALERTS_FOCUS_TYPE));
    ASSERT_EXPECTED_STATE(fixture->alert_resource, FocusState::NONE);
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::BACKGROUND);
}

static void test_focusmanager_unhold_resource_and_activate_lower_priority_resource(nFocusFixture* fixture, gconstpointer ignored)
{
    _setup_nugu_focus_policy(fixture);

    g_assert(HOLD_FOCUS(fixture->alert_resource, ALERTS_FOCUS_TYPE));

    g_assert(REQUEST_FOCUS(fixture->media_resource, MEDIA_FOCUS_TYPE, MEDIA_NAME));
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::BACKGROUND);

    g_assert(UNHOLD_FOCUS(fixture->alert_resource, ALERTS_FOCUS_TYPE));
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::FOREGROUND);
}

static void test_focusmanager_request_and_release_configurations(nFocusFixture* fixture, gconstpointer ignored)
{
    FocusManager* focus_manager = fixture->focus_manager;
    std::vector<FocusConfiguration> focus_request_configuration;
    std::vector<FocusConfiguration> focus_release_configuration;

    // Add asr user Resource to FocusManager
    focus_request_configuration.push_back({ ASR_USER_FOCUS_TYPE, INFO_FOCUS_PRIORITY });
    focus_release_configuration.push_back({ ASR_USER_FOCUS_TYPE, ASR_USER_FOCUS_PRIORITY });
    focus_manager->setConfigurations(focus_request_configuration, focus_release_configuration);

    // Focus resource's priority is followed by focus release configuration.
    g_assert(focus_manager->getFocusResourcePriority(ASR_USER_FOCUS_TYPE) == ASR_USER_FOCUS_PRIORITY);
    g_assert(focus_manager->getFocusResourcePriority(INFO_FOCUS_TYPE) == -1);
}

static void test_focusmanager_release_resource_with_different_request_and_same_release_priority_resources(nFocusFixture* fixture, gconstpointer ignored)
{
    FocusManager* focus_manager = fixture->focus_manager;
    std::vector<FocusConfiguration> focus_request_configuration;
    std::vector<FocusConfiguration> focus_release_configuration;

    // Add asr user Resource to FocusManager
    focus_request_configuration.push_back({ ASR_USER_FOCUS_TYPE, ASR_USER_FOCUS_PRIORITY });
    focus_request_configuration.push_back({ INFO_FOCUS_TYPE, INFO_FOCUS_PRIORITY });
    focus_request_configuration.push_back({ ALERTS_FOCUS_TYPE, ALERTS_FOCUS_PRIORITY });

    focus_release_configuration.push_back({ ASR_USER_FOCUS_TYPE, ASR_USER_FOCUS_PRIORITY });
    focus_release_configuration.push_back({ INFO_FOCUS_TYPE, INFO_FOCUS_PRIORITY });
    focus_release_configuration.push_back({ ALERTS_FOCUS_TYPE, INFO_FOCUS_PRIORITY });
    focus_manager->setConfigurations(focus_request_configuration, focus_release_configuration);

    g_assert(REQUEST_FOCUS(fixture->asr_resource, ASR_USER_FOCUS_TYPE, ASR_USER_NAME));
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::FOREGROUND);
    // stacked resource: asr user > info
    g_assert(REQUEST_FOCUS(fixture->info_resource, INFO_FOCUS_TYPE, INFO_NAME));
    ASSERT_EXPECTED_STATE(fixture->info_resource, FocusState::BACKGROUND);
    // stacked resource: asr user > info = alerts
    g_assert(REQUEST_FOCUS(fixture->alert_resource, ALERTS_FOCUS_TYPE, ALERTS_NAME));
    ASSERT_EXPECTED_STATE(fixture->alert_resource, FocusState::BACKGROUND);

    // stacked resource: info = alerts
    g_assert(RELEASE_FOCUS(fixture->asr_resource, ASR_USER_FOCUS_TYPE));
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::NONE);

    // pop fifo resource
    ASSERT_EXPECTED_STATE(fixture->info_resource, FocusState::FOREGROUND);

    g_assert(RELEASE_FOCUS(fixture->info_resource, INFO_FOCUS_TYPE));
    ASSERT_EXPECTED_STATE(fixture->info_resource, FocusState::NONE);

    // pop fifo resource
    ASSERT_EXPECTED_STATE(fixture->alert_resource, FocusState::FOREGROUND);
}

static void test_focusmanager_steal_resource_with_higher_resource_by_request_priority(nFocusFixture* fixture, gconstpointer igresource)
{
    FocusManager* focus_manager = fixture->focus_manager;
    std::vector<FocusConfiguration> focus_request_configuration;
    std::vector<FocusConfiguration> focus_release_configuration;

    // Request Priority: asr user = media
    focus_request_configuration.push_back({ ASR_USER_FOCUS_TYPE, ASR_USER_FOCUS_PRIORITY });
    focus_request_configuration.push_back({ MEDIA_FOCUS_TYPE, ASR_USER_FOCUS_PRIORITY });
    // Release Priority: asr user > media
    focus_release_configuration.push_back({ ASR_USER_FOCUS_TYPE, ASR_USER_FOCUS_PRIORITY });
    focus_release_configuration.push_back({ MEDIA_FOCUS_TYPE, MEDIA_FOCUS_PRIORITY });
    focus_manager->setConfigurations(focus_request_configuration, focus_release_configuration);

    g_assert(REQUEST_FOCUS(fixture->asr_resource, ASR_USER_FOCUS_TYPE, ASR_USER_NAME));
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::FOREGROUND);

    // asr user focus is stealed by media focus caused by equal to request priority
    g_assert(REQUEST_FOCUS(fixture->media_resource, MEDIA_FOCUS_TYPE, MEDIA_NAME));
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::FOREGROUND);
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::BACKGROUND);
}

static void test_focusmanager_no_steal_resource_with_higher_resource_request_priority(nFocusFixture* fixture, gconstpointer igresource)
{
    FocusManager* focus_manager = fixture->focus_manager;
    std::vector<FocusConfiguration> focus_request_configuration;
    std::vector<FocusConfiguration> focus_release_configuration;

    // Request Priority: asr user > media
    focus_request_configuration.push_back({ ASR_USER_FOCUS_TYPE, ASR_USER_FOCUS_PRIORITY });
    focus_request_configuration.push_back({ MEDIA_FOCUS_TYPE, MEDIA_FOCUS_PRIORITY });
    // Release Priority: asr user > media
    focus_release_configuration.push_back({ ASR_USER_FOCUS_TYPE, ASR_USER_FOCUS_PRIORITY });
    focus_release_configuration.push_back({ MEDIA_FOCUS_TYPE, MEDIA_FOCUS_PRIORITY });
    focus_manager->setConfigurations(focus_request_configuration, focus_release_configuration);

    g_assert(REQUEST_FOCUS(fixture->asr_resource, ASR_USER_FOCUS_TYPE, ASR_USER_NAME));
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::FOREGROUND);

    // asr user focus can't be stolen because media focus has a low request priority
    g_assert(REQUEST_FOCUS(fixture->media_resource, MEDIA_FOCUS_TYPE, MEDIA_NAME));
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::BACKGROUND);
}

static void test_focusmanager_release_resources_by_release_priority(nFocusFixture* fixture, gconstpointer igresource)
{
    FocusManager* focus_manager = fixture->focus_manager;
    std::vector<FocusConfiguration> focus_request_configuration;
    std::vector<FocusConfiguration> focus_release_configuration;

    // Request Priority: asr user = media > alerts
    focus_request_configuration.push_back({ ASR_USER_FOCUS_TYPE, ASR_USER_FOCUS_PRIORITY });
    focus_request_configuration.push_back({ ALERTS_FOCUS_TYPE, ALERTS_FOCUS_PRIORITY });
    focus_request_configuration.push_back({ MEDIA_FOCUS_TYPE, ASR_USER_FOCUS_PRIORITY });
    // Release Priority: asr user > alerts > media
    focus_release_configuration.push_back({ ASR_USER_FOCUS_TYPE, ASR_USER_FOCUS_PRIORITY });
    focus_release_configuration.push_back({ ALERTS_FOCUS_TYPE, ALERTS_FOCUS_PRIORITY });
    focus_release_configuration.push_back({ MEDIA_FOCUS_TYPE, MEDIA_FOCUS_PRIORITY });
    focus_manager->setConfigurations(focus_request_configuration, focus_release_configuration);

    g_assert(REQUEST_FOCUS(fixture->media_resource, MEDIA_FOCUS_TYPE, MEDIA_NAME));
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::FOREGROUND);

    g_assert(REQUEST_FOCUS(fixture->alert_resource, ALERTS_FOCUS_TYPE, ALERTS_NAME));
    ASSERT_EXPECTED_STATE(fixture->alert_resource, FocusState::FOREGROUND);
    ASSERT_EXPECTED_STATE(fixture->media_resource, FocusState::BACKGROUND);

    g_assert(REQUEST_FOCUS(fixture->asr_resource, ASR_USER_FOCUS_TYPE, ASR_USER_NAME));
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::FOREGROUND);
    ASSERT_EXPECTED_STATE(fixture->alert_resource, FocusState::BACKGROUND);

    g_assert(RELEASE_FOCUS(fixture->asr_resource, ASR_USER_FOCUS_TYPE));
    ASSERT_EXPECTED_STATE(fixture->asr_resource, FocusState::NONE);
    ASSERT_EXPECTED_STATE(fixture->alert_resource, FocusState::FOREGROUND);
}

static void test_focusmanager_convert_state_text_to_focusstate_enum(nFocusFixture* fixture, gconstpointer ignored)
{
    FocusManager* focus_manager = fixture->focus_manager;
    const std::string FOREGROUND_STATE = focus_manager->getStateString(FocusState::FOREGROUND);
    const std::string BACKGROUND_STATE = focus_manager->getStateString(FocusState::BACKGROUND);
    const std::string NONE_STATE = focus_manager->getStateString(FocusState::NONE);

    // check normal case
    g_assert(focus_manager->convertToFocusState(FOREGROUND_STATE) == FocusState::FOREGROUND);
    g_assert(focus_manager->convertToFocusState(BACKGROUND_STATE) == FocusState::BACKGROUND);
    g_assert(focus_manager->convertToFocusState(NONE_STATE) == FocusState::NONE);

    // check abnormal case (incorrect text)
    try {
        focus_manager->convertToFocusState("abcd");
        g_test_fail();
    } catch (std::out_of_range& exception) {
        g_assert_nonnull(exception.what());
    }
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
    G_TEST_ADD_FUNC("/core/FocusManager/HoldAndUnHoldBasicFunction", test_focusmanager_hold_and_unhold_basic_function);
    G_TEST_ADD_FUNC("/core/FocusManager/RequestHigherPriorityResourcethanHoldResource", test_focusmanager_request_higher_priority_resource_than_hold_resource);
    G_TEST_ADD_FUNC("/core/FocusManager/RequestLowerPriorityResourcethanHoldResource", test_focusmanager_request_lower_priority_resource_than_hold_resource);
    G_TEST_ADD_FUNC("/core/FocusManager/RequestSamePriorityResourceAndAutoUnHold", test_focusmanager_request_same_priority_resource_and_auto_unhold);
    G_TEST_ADD_FUNC("/core/FocusManager/ReleaseActiveResourceAndHoldLowerPriorityResource", test_focusmanager_release_active_resource_and_hold_lower_priority_resource);
    G_TEST_ADD_FUNC("/core/FocusManager/UnHoldResourceAndActivateLowerPriorityResource", test_focusmanager_unhold_resource_and_activate_lower_priority_resource);
    G_TEST_ADD_FUNC("/core/FocusManager/FocusRequestAndReleaseConfigurations", test_focusmanager_request_and_release_configurations);
    G_TEST_ADD_FUNC("/core/FocusManager/FocusReleaseResourceWithDifferentRequestAndSameReleasePriorityResources", test_focusmanager_release_resource_with_different_request_and_same_release_priority_resources);
    G_TEST_ADD_FUNC("/core/FocusManager/StealResourceWithHigherResourceByRequestPriority", test_focusmanager_steal_resource_with_higher_resource_by_request_priority);
    G_TEST_ADD_FUNC("/core/FocusManager/NoStealResourceWithHigherResourceByRequestPriority", test_focusmanager_no_steal_resource_with_higher_resource_request_priority);
    G_TEST_ADD_FUNC("/core/FocusManager/ReleaseResourcesByReleasePriority", test_focusmanager_release_resources_by_release_priority);
    G_TEST_ADD_FUNC("/core/FocusManager/ConvertStateTextToFocusStateEnum", test_focusmanager_convert_state_text_to_focusstate_enum);

    return g_test_run();
}
