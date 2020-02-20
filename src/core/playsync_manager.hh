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

#include "clientkit/playsync_manager_interface.hh"
#include "nugu_timer.hh"

namespace NuguCore {

using namespace NuguClientKit;

class PlaySyncManager : public IPlaySyncManager {
public:
    PlaySyncManager();
    virtual ~PlaySyncManager();

    void addContext(const std::string& ps_id, const std::string& cap_name) override;
    void addContext(const std::string& ps_id, const std::string& cap_name, DisplayRenderer&& renderer) override;
    void removeContext(const std::string& ps_id, const std::string& cap_name, bool immediately = true) override;
    void removeContextLater(const std::string& ps_id, const std::string& cap_name, unsigned int sec) override;
    void clearPendingContext(const std::string& ps_id) override;
    std::vector<std::string> getAllPlayStackItems() override;
    std::string getPlayStackItem(const std::string& cap_name) override;

    void setExpectSpeech(bool expect_speech) override;
    void onMicOn() override;
    void clearContextHold() override;
    void onASRError() override;

    using ContextMap = std::map<std::string, std::vector<std::string>>;

private:
    void addStackElement(const std::string& ps_id, const std::string& cap_name);
    bool removeStackElement(const std::string& ps_id, const std::string& cap_name);
    void addRenderer(const std::string& ps_id, DisplayRenderer& renderer);
    bool removeRenderer(const std::string& ps_id, bool unconditionally = true);
    int getRendererInterval(const std::string& ps_id);
    void removeContextAction(const std::string& ps_id, bool immediately);

    const std::map<std::string, int> DURATION_MAP;

    template <typename T, typename V>
    std::vector<std::string> getKeyOfMap(const std::map<T, V>& map);

    ContextMap context_map;
    std::vector<std::string> context_stack;
    std::map<std::string, DisplayRenderer> renderer_map;

    NUGUTimer* timer = nullptr;
    bool is_expect_speech = false;
};
} // NuguCore

#endif /* __NUGU_PLAYSYNC_MANAGER_H__ */
