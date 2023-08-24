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

#include "clientkit/capability.hh"
#include "clientkit/nugu_client.hh"

using namespace NuguClientKit;

static const char* NAMESPACE = "Fake";

class FakeAgent final : public Capability,
                        public IDirectiveSequencerListener {
public:
    FakeAgent()
        : Capability(NAMESPACE, "1.0")
    {
    }

    void updateInfoForContext(NJson::Value& ctx) override { }

    void updateCompactContext(NJson::Value& ctx) override { }

    void onCancelDirective(NuguDirective* ndir) override { }

    bool onPreHandleDirective(NuguDirective* ndir) override
    {
        return false;
    }

    bool onHandleDirective(NuguDirective* ndir) override
    {
        processDirective(ndir);

        return true;
    }

    void parsingDirective(const char* dname, const char* message) override
    {
        destroy_directive_by_agent = is_destroy_directive_by_agent;
    }

    std::string getContextInfo() override
    {
        return "";
    }

    void setDestroyDirectiveByAgent(bool by_agent)
    {
        is_destroy_directive_by_agent = by_agent;
    }

private:
    bool is_destroy_directive_by_agent = true;
};

using TestFixture = struct _TestFixture {
    std::unique_ptr<NuguClient> nugu_client;
    std::unique_ptr<FakeAgent> agent;
    IDirectiveSequencer* dir_seq;
    NuguDirective* ndir_first;
    NuguDirective* ndir_second;
};

static NuguDirective* createDirective(const std::string& name_space, const std::string& name, const std::string& msg_id, const std::string& dialog_id)
{
    return nugu_directive_new(name_space.c_str(), name.c_str(), "1.0", msg_id.c_str(), dialog_id.c_str(), "ref_1", "{}", "[]");
}

static void setup(TestFixture* fixture, gconstpointer user_data)
{
    fixture->nugu_client = std::unique_ptr<NuguClient>(new NuguClient());
    auto nugu_core_container(fixture->nugu_client->getNuguCoreContainer());

    fixture->agent = std::unique_ptr<FakeAgent>(new FakeAgent());
    fixture->agent->setNuguCoreContainer(nugu_core_container);

    fixture->dir_seq = nugu_core_container->getCapabilityHelper()->getDirectiveSequencer();
    fixture->dir_seq->addListener(NAMESPACE, fixture->agent.get());
}

static void teardown(TestFixture* fixture, gconstpointer user_data)
{
    fixture->agent.reset();
    fixture->nugu_client.reset();
}

static void sub_test_compose_directive(TestFixture* fixture)
{
    fixture->ndir_first = createDirective(NAMESPACE, "dir_1", "msg_1", "dlg_1");
    fixture->ndir_second = createDirective(NAMESPACE, "dir_2", "msg_2", "dlg_2");
}

static void test_capability_handle_single_dialog(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_compose_directive(fixture);

    // handle directive by capability (complete directive by self)
    fixture->agent->setDestroyDirectiveByAgent(false);
    g_assert(fixture->dir_seq->add(fixture->ndir_first));
    g_assert(!fixture->agent->getNuguDirective());

    // handle directive by agent
    fixture->agent->setDestroyDirectiveByAgent(true);
    g_assert(fixture->dir_seq->add(fixture->ndir_second));
    g_assert(fixture->agent->getNuguDirective());
    g_assert(fixture->dir_seq->getCanceledDialogId().empty());
    fixture->agent->destroyDirective(fixture->ndir_second);
}

static void test_capability_handle_multiple_dialogs_as_cancel(TestFixture* fixture, gconstpointer ignored)
{
    sub_test_compose_directive(fixture);

    // handle first directive
    g_assert(fixture->dir_seq->add(fixture->ndir_first));
    g_assert(fixture->agent->getNuguDirective());

    // handle second directive
    g_assert(fixture->dir_seq->add(fixture->ndir_second));
    g_assert(fixture->agent->getNuguDirective());
    g_assert(!fixture->dir_seq->getCanceledDialogId().empty());

    fixture->agent->destroyDirective(fixture->ndir_second);
}

static void test_capability_handle_multiple_dialogs_as_hold(TestFixture* fixture, gconstpointer ignored)
{
    // set directive cancel policy not to cancel previous dialog
    fixture->agent->setCancelPolicy(false);

    sub_test_compose_directive(fixture);

    // handle first directive
    g_assert(fixture->dir_seq->add(fixture->ndir_first));
    g_assert(fixture->agent->getNuguDirective());

    // handle second directive
    g_assert(fixture->dir_seq->add(fixture->ndir_second));
    g_assert(fixture->agent->getNuguDirective());
    g_assert(fixture->dir_seq->getCanceledDialogId().empty());

    fixture->agent->destroyDirective(fixture->ndir_first);
    fixture->agent->destroyDirective(fixture->ndir_second);
}

#define G_TEST_ADD_FUNC(name, func) \
    g_test_add(name, TestFixture, nullptr, setup, func, teardown);

int main(int argc, char* argv[])
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
    g_type_init();
#endif

    g_test_init(&argc, &argv, (void*)NULL);
    g_log_set_always_fatal((GLogLevelFlags)G_LOG_FATAL_MASK);

    G_TEST_ADD_FUNC("/clientkit/Capability/handleSingleDialog", test_capability_handle_single_dialog);
    G_TEST_ADD_FUNC("/clientkit/Capability/handleMultipleDialogsAsCancel", test_capability_handle_multiple_dialogs_as_cancel);
    G_TEST_ADD_FUNC("/clientkit/Capability/handleMultipleDialogsAsHold", test_capability_handle_multiple_dialogs_as_hold);

    return g_test_run();
}
