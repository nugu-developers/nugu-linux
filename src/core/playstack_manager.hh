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

#ifndef __NUGU_PLAYSTACK_MANAGER_H__
#define __NUGU_PLAYSTACK_MANAGER_H__

#include <memory>

#include "clientkit/playstack_manager_interface.hh"
#include "nugu_timer.hh"

namespace NuguCore {

using namespace NuguClientKit;

class IStackTimer : public NUGUTimer {
public:
    virtual ~IStackTimer() = default;
    virtual bool isStarted() = 0;
};

class PlayStackManager : public IPlayStackManager {
public:
    PlayStackManager();
    virtual ~PlayStackManager();

    void addListener(IPlayStackManagerListener* listener) override;
    void removeListener(IPlayStackManagerListener* listener) override;
    int getListenerCount();
    void setTimer(IStackTimer* timer);

    bool add(const std::string& ps_id, NuguDirective* ndir) override;
    void remove(const std::string& ps_id, PlayStackRemoveMode mode = PlayStackRemoveMode::Normal) override;
    bool isStackedCondition(NuguDirective* ndir) override;
    bool hasExpectSpeech(NuguDirective* ndir) override;
    void stopHolding() override;
    void resetHolding() override;
    bool isActiveHolding();

    PlayStackLayer getPlayStackLayer(const std::string& ps_id) override;
    std::vector<std::string> getAllPlayStackItems() override;
    const PlayStack& getPlayStackContainer();

private:
    class StackTimer final : public IStackTimer {
    public:
        bool isStarted();
        void start(unsigned int sec = 0) override;
        void stop() override;

    private:
        bool is_started = false;
    };

private:
    PlayStackLayer extractPlayStackLayer(NuguDirective* ndir);
    std::string getSameLayerStack(PlayStackLayer layer);
    void handlePreviousStack(PlayStackLayer layer, bool is_stacked);
    bool addToContainer(const std::string& ps_id, PlayStackLayer layer);
    void removeFromContainer(const std::string& ps_id);
    void notifyStackRemoved(const std::string& ps_id);
    void clearContainer();
    bool isStackedCondition(PlayStackLayer layer);
    bool isStackedCondition(const std::string& ps_id);

    template <typename T>
    void removeItemInList(std::vector<T>& list, const T& item);

    const unsigned int DEFAULT_HOLD_TIME = 7 * NUGU_TIMER_UNIT_SEC;
    const unsigned int LONG_HOLD_TIME = 600 * NUGU_TIMER_UNIT_SEC;

    PlayStack playstack_container;
    std::vector<IPlayStackManagerListener*> listeners;
    std::unique_ptr<IStackTimer> timer = nullptr;

    bool has_long_timer = false;
    bool has_holding = false;
    bool is_expect_speech = false;
    bool is_stacked = false;
};

} // NuguCore

#endif /* __NUGU_PLAYSTACK_MANAGER_H__ */
