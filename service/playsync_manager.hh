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

#ifndef __NUGU_PLAYSYNC_MANAGER_H__
#define __NUGU_PLAYSYNC_MANAGER_H__

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <core/nugu_timer.h>
#include <interface/capability/capability_interface.hh>

namespace NuguCore {

using namespace NuguInterface;

class IPlaySyncManagerListener {
public:
    virtual ~IPlaySyncManagerListener() = default;
    virtual void onSyncContext(const std::string& ps_id, std::pair<std::string, std::string> render_info) = 0;
    virtual void onReleaseContext(const std::string& ps_id) = 0;
};

class PlaySyncManager {
public:
    using DisplayRenderer = struct {
        IPlaySyncManagerListener* listener;
        CapabilityType cap_type;
        std::string duration;
        std::pair<std::string, std::string> render_info;
        bool only_rendering = false;
    };

public:
    PlaySyncManager();
    virtual ~PlaySyncManager();

    void addContext(const std::string& ps_id, CapabilityType cap_type);
    void addContext(const std::string& ps_id, CapabilityType cap_type, DisplayRenderer renderer);
    void removeContext(std::string ps_id, CapabilityType cap_type, bool immediately = false);
    std::vector<std::string> getAllPlayStackItems();
    std::string getPlayStackItem(CapabilityType cap_type);

    void setExpectSpeech(bool expect_speech);
    void onMicOn();
    void onASRError();

private:
    void addStackElement(std::string ps_id, CapabilityType cap_type);
    bool removeStackElement(std::string ps_id, CapabilityType cap_type);
    void addRenderer(std::string ps_id, DisplayRenderer& renderer);
    void removeRenderer(std::string ps_id);
    void setTimerInterval(const std::string& ps_id);

    static const std::map<std::string, long> DURATION_MAP;

    template <typename T, typename V>
    std::vector<std::string> getKeyOfMap(std::map<T, V>& map);

    using TimerCallbackParam = struct {
        PlaySyncManager* instance;
        std::string ps_id;
        std::function<void()> callback; // temp : for holding function for stack log
    };

    std::vector<std::string> context_stack;
    std::map<std::string, std::vector<CapabilityType>> context_map;
    std::map<std::string, DisplayRenderer> renderer_map;

    NuguTimer* timer = nullptr;
    TimerCallbackParam timer_cb_param = {};
    bool is_expect_speech = false;
};
} // NuguCore

#endif /* __NUGU_PLAYSYNC_MANAGER_H__ */
