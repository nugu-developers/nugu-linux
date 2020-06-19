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
#include <glib.h>
#include <memory>

#include "playstack_manager.hh"

using namespace NuguCore;

class PlayStackManagerListener : public IPlayStackManagerListener {
public:
    void onStackAdded(const std::string& ps_id)
    {
        ps_ids.emplace_back(ps_id);
    }

    void onStackRemoved(const std::string& ps_id)
    {
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

private:
    std::vector<std::string> ps_ids;
};

static NuguDirective* createDirective(const std::string& name_space, const std::string& name, const std::string& groups)
{
    return nugu_directive_new(name_space.c_str(), name.c_str(), "1.0", "msg_1", "dlg_1",
        "ref_1", "{}", groups.c_str());
}

static void test_playstack_manager_basic(void)
{
    auto playstack_manager(std::make_shared<PlayStackManager>());
    NuguDirective* ndir = createDirective("TTS", "test", "{ \"directives\": [\"TTS.Speak\"] }");

    playstack_manager->add("", ndir);
    g_assert(playstack_manager->getPlayStackContainer().first.empty());

    playstack_manager->add("ps_id_1", ndir);
    g_assert(!playstack_manager->getPlayStackContainer().first.empty());

    // check adding duplicately
    playstack_manager->add("ps_id_1", ndir);
    g_assert(!playstack_manager->getPlayStackContainer().first.empty());

    playstack_manager->remove("", PlayStackRemoveMode::Immediately);
    g_assert(!playstack_manager->getPlayStackContainer().first.empty());

    playstack_manager->remove("ps_id_2", PlayStackRemoveMode::Immediately);
    g_assert(!playstack_manager->getPlayStackContainer().first.empty());

    playstack_manager->remove("ps_id_1", PlayStackRemoveMode::Immediately);
    g_assert(playstack_manager->getPlayStackContainer().first.empty());

    nugu_directive_unref(ndir);
}

static void test_playstack_manager_get_stack(void)
{
    auto playstack_manager(std::make_shared<PlayStackManager>());
    NuguDirective* ndir_info = createDirective("TTS", "test", "{ \"directives\": [\"TTS.Speak\"] }");
    NuguDirective* ndir_media = createDirective("AudioPlayer", "test", "{ \"directives\": [\"AudioPlayer.Play\"] }");

    g_assert(playstack_manager->getAllPlayStackItems().empty());

    playstack_manager->add("ps_id_1", ndir_info);
    g_assert(playstack_manager->getAllPlayStackItems().at(0) == "ps_id_1");
    g_assert(playstack_manager->getPlayStackLayer("") == PlayStackLayer::None);
    g_assert(playstack_manager->getPlayStackLayer("ps_id_1") == PlayStackLayer::Info);

    playstack_manager->remove("ps_id_1", PlayStackRemoveMode::Immediately);
    playstack_manager->add("ps_id_2", ndir_media);
    playstack_manager->add("ps_id_3", ndir_info);
    auto play_stack_items = playstack_manager->getAllPlayStackItems();
    g_assert(play_stack_items.at(0) == "ps_id_2" && play_stack_items.at(1) == "ps_id_3");

    nugu_directive_unref(ndir_info);
    nugu_directive_unref(ndir_media);
}

static void test_playstack_manager_listener(void)
{
    auto playstack_manager(std::make_shared<PlayStackManager>());
    auto playstack_manager_listener(std::make_shared<PlayStackManagerListener>());
    auto playstack_manager_listener_snd(std::make_shared<PlayStackManagerListener>());
    NuguDirective* ndir = createDirective("TTS", "test", "{ \"directives\": [\"TTS.Speak\"] }");

    playstack_manager->addListener(nullptr);
    g_assert(playstack_manager->getListenerCount() == 0);

    playstack_manager->addListener(playstack_manager_listener.get());
    g_assert(playstack_manager->getListenerCount() == 1);

    // check adding duplicately
    playstack_manager->addListener(playstack_manager_listener.get());
    g_assert(playstack_manager->getListenerCount() == 1);

    playstack_manager->addListener(playstack_manager_listener_snd.get());
    g_assert(playstack_manager->getListenerCount() == 2);

    playstack_manager->add("ps_id_1", ndir);
    g_assert(playstack_manager_listener->getPlayServiceIds().at(0) == "ps_id_1"
        && playstack_manager_listener_snd->getPlayServiceIds().at(0) == "ps_id_1");

    playstack_manager->remove("ps_id_1", PlayStackRemoveMode::Immediately);
    g_assert(playstack_manager_listener->getPlayServiceIds().empty()
        && playstack_manager_listener_snd->getPlayServiceIds().empty());

    playstack_manager->removeListener(nullptr);
    g_assert(playstack_manager->getListenerCount() == 2);

    playstack_manager->removeListener(playstack_manager_listener.get());
    g_assert(playstack_manager->getListenerCount() == 1);

    playstack_manager->removeListener(playstack_manager_listener_snd.get());
    g_assert(playstack_manager->getListenerCount() == 0);

    nugu_directive_unref(ndir);
}

static void test_playstack_manager_holdStack(void)
{
    auto playstack_manager(std::make_shared<PlayStackManager>());
    NuguDirective* ndir = createDirective("TTS", "test", "{ \"directives\": [\"TTS.Speak\"] }");

    // hold stack during 7s'
    playstack_manager->add("ps_id_1", ndir);
    playstack_manager->remove("ps_id_1");
    g_assert(!playstack_manager->getPlayStackContainer().first.empty());

    // remove stack immediately
    playstack_manager->add("ps_id_2", ndir);
    playstack_manager->remove("ps_id_2", PlayStackRemoveMode::Immediately);
    g_assert(playstack_manager->getPlayStackContainer().first.empty());

    // hold stack during 10m'
    playstack_manager->add("ps_id_3", ndir);
    playstack_manager->remove("ps_id_3", PlayStackRemoveMode::Later);
    g_assert(!playstack_manager->getPlayStackContainer().first.empty());

    nugu_directive_unref(ndir);
}

static void test_playstack_manager_layerPolicy(void)
{
    auto playstack_manager(std::make_shared<PlayStackManager>());
    NuguDirective* ndir_info = createDirective("TTS", "test", "{ \"directives\": [\"TTS.Speak\"] }");
    NuguDirective* ndir_media = createDirective("AudioPlayer", "test", "{ \"directives\": [\"AudioPlayer.Play\"] }");

    // Info to Info -> replace
    playstack_manager->add("ps_id_1", ndir_info);
    playstack_manager->add("ps_id_2", ndir_info);
    g_assert(playstack_manager->getPlayStackContainer().first.size() == 1
        && playstack_manager->getPlayStackContainer().first.cbegin()->first == "ps_id_2"
        && playstack_manager->getPlayStackContainer().first.cbegin()->second == PlayStackLayer::Info);

    // Info to Media -> replace
    playstack_manager->add("ps_id_3", ndir_media);
    g_assert(playstack_manager->getPlayStackContainer().first.size() == 1
        && playstack_manager->getPlayStackContainer().first.cbegin()->first == "ps_id_3"
        && playstack_manager->getPlayStackContainer().first.cbegin()->second == PlayStackLayer::Media);

    // Media to Info -> stacked
    playstack_manager->add("ps_id_4", ndir_info);
    g_assert(playstack_manager->getPlayStackContainer().first.size() == 2);

    // Media to Media -> replace
    playstack_manager->remove("ps_id_4", PlayStackRemoveMode::Immediately);
    playstack_manager->add("ps_id_5", ndir_media);
    g_assert(playstack_manager->getPlayStackContainer().first.size() == 1
        && playstack_manager->getPlayStackContainer().first.cbegin()->first == "ps_id_5"
        && playstack_manager->getPlayStackContainer().first.cbegin()->second == PlayStackLayer::Media);

    nugu_directive_unref(ndir_info);
    nugu_directive_unref(ndir_media);
}

static void test_playstack_manager_controlHolding(void)
{
    auto playstack_manager(std::make_shared<PlayStackManager>());
    NuguDirective* ndir = createDirective("TTS", "test", "{ \"directives\": [\"TTS.Speak\"] }");

    // stop holding
    playstack_manager->add("ps_id_1", ndir);
    playstack_manager->remove("ps_id_1");
    bool has_stack_before = !playstack_manager->getPlayStackContainer().first.empty();
    playstack_manager->stopHolding();
    g_assert(has_stack_before && !playstack_manager->getPlayStackContainer().first.empty());

    // reset holding
    playstack_manager->resetHolding();
    g_assert(playstack_manager->isActiveHolding());

    nugu_directive_unref(ndir);
}

int main(int argc, char* argv[])
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
    g_type_init();
#endif

    g_test_init(&argc, &argv, (void*)NULL);
    g_log_set_always_fatal((GLogLevelFlags)G_LOG_FATAL_MASK);

    g_test_add_func("/core/PlayStackManager/basic", test_playstack_manager_basic);
    g_test_add_func("/core/PlayStackManager/getStack", test_playstack_manager_get_stack);
    g_test_add_func("/core/PlayStackManager/listener", test_playstack_manager_listener);
    g_test_add_func("/core/PlayStackManager/holdStack", test_playstack_manager_holdStack);
    g_test_add_func("/core/PlayStackManager/layerPolicy", test_playstack_manager_layerPolicy);
    g_test_add_func("/core/PlayStackManager/controlHolding", test_playstack_manager_controlHolding);

    return g_test_run();
}
