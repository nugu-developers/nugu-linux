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
#include <set>

#include "clientkit/playsync_manager_interface.hh"
#include "nugu_timer.hh"

namespace NuguCore {

using namespace NuguClientKit;

enum class PlayStackRemoveMode {
    Normal,
    Immediately,
    Later
};

class IPlayStackManagerListener {
public:
    virtual ~IPlayStackManagerListener() = default;

    virtual void onStackAdded(const std::string& ps_id) = 0;
    virtual void onStackRemoved(const std::string& ps_id) = 0;
};

class IStackTimer : public NUGUTimer {
public:
    virtual ~IStackTimer() = default;
    virtual bool isStarted() = 0;
};

class PlayStackManager {
public:
    using PlayStack = std::pair<std::map<std::string, PlayStackActivity>, std::vector<std::string>>;
    using PlayStakcHoldTimes = struct {
        unsigned int normal_time;
        unsigned int long_time;
    };

public:
    PlayStackManager();
    virtual ~PlayStackManager();

    void reset();
    void addListener(IPlayStackManagerListener* listener);
    void removeListener(IPlayStackManagerListener* listener);
    int getListenerCount();
    void setTimer(IStackTimer* timer);

    bool add(const std::string& ps_id, NuguDirective* ndir);
    void remove(const std::string& ps_id, PlayStackRemoveMode mode = PlayStackRemoveMode::Normal);
    bool isStackedCondition(NuguDirective* ndir);
    bool hasExpectSpeech(NuguDirective* ndir);
    void stopHolding();
    void resetHolding();
    void clearHolding();
    bool isActiveHolding();
    bool hasAddingPlayStack();
    void setPlayStackHoldTime(PlayStakcHoldTimes&& hold_times_sec);
    PlayStakcHoldTimes getPlayStackHoldTime();

    PlayStackActivity getPlayStackActivity(const std::string& ps_id);
    std::vector<std::string> getAllPlayStackItems();
    const PlayStack& getPlayStackContainer();
    std::set<bool> getFlagSet();

private:
    class StackTimer final : public IStackTimer {
    public:
        bool isStarted();
        void start(unsigned int sec = 0) override;
        void stop() override;
        void notifyCallback() override;

    private:
        bool is_started = false;
    };

private:
    PlayStackActivity extractPlayStackActivity(NuguDirective* ndir);
    std::string getNoneMediaLayerStack();
    void handlePreviousStack(bool is_stacked);
    bool hasDisplayRenderingInfo(NuguDirective* ndir);
    bool hasKeyword(NuguDirective* ndir, std::vector<std::string>&& keywords);
    bool addToContainer(const std::string& ps_id, PlayStackActivity activity);
    void removeFromContainer(const std::string& ps_id);
    void notifyStackRemoved(const std::string& ps_id);
    void clearContainer();
    bool isStackedCondition(PlayStackActivity activity);
    bool isStackedCondition(const std::string& ps_id);

    template <typename T>
    void removeItemInList(std::vector<T>& list, const T& item);

    const unsigned int DEFAULT_NORMAL_HOLD_TIME_SEC = 7;
    const unsigned int DEFAULT_LONG_HOLD_TIME_SEC = 600;

    PlayStack playstack_container;
    std::vector<IPlayStackManagerListener*> listeners;
    std::unique_ptr<IStackTimer> timer = nullptr;
    PlayStakcHoldTimes hold_times_sec { DEFAULT_NORMAL_HOLD_TIME_SEC, DEFAULT_LONG_HOLD_TIME_SEC };

    bool has_long_timer = false;
    bool has_holding = false;
    bool is_expect_speech = false;
    bool is_stacked = false;
    bool has_adding_playstack = false;
};

} // NuguCore

#endif /* __NUGU_PLAYSTACK_MANAGER_H__ */
