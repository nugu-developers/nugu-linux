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
#include <memory>
#include <vector>

#include <base/nugu_directive.h>

#include "clientkit/playsync_manager_interface.hh"
#include "playstack_manager.hh"

namespace NuguCore {

using namespace NuguClientKit;

class PlaySyncManager : public IPlaySyncManager,
                        public IPlayStackManagerListener {
public:
    PlaySyncManager();
    virtual ~PlaySyncManager();

    void setPlayStackManager(PlayStackManager* playstack_manager);
    void addListener(const std::string& requester, IPlaySyncManagerListener* listener) override;
    void removeListener(const std::string& requester) override;
    int getListenerCount();

    void prepareSync(const std::string& ps_id, NuguDirective* ndir) override;
    void startSync(const std::string& ps_id, const std::string& requester, void* extra_data = nullptr) override;
    void cancelSync(const std::string& ps_id, const std::string& requester) override;
    void releaseSync(const std::string& ps_id, const std::string& requester) override;
    void releaseSyncLater(const std::string& ps_id, const std::string& requester) override;
    void releaseSyncImmediately(const std::string& ps_id, const std::string& requester) override;

    void postPoneRelease() override;
    void continueRelease() override;
    void stopHolding() override;
    void resetHolding() override;

    bool isConditionToHandlePrevDialog(NuguDirective* prev_ndir, NuguDirective* cur_ndir) override;
    bool hasLayer(const std::string& ps_id, PlayStackLayer layer) override;
    std::vector<std::string> getAllPlayStackItems() override;
    const PlayStacks& getPlayStacks();

    // overriding IPlayStackManagerListener
    void onStackAdded(const std::string& ps_id) override;
    void onStackRemoved(const std::string& ps_id) override;

private:
    void updateExtraData(const std::string& ps_id, const std::string& requester, void* extra_data);
    void notifyStateChanged(const std::string& ps_id, PlaySyncState state);
    bool isConditionToSyncAction(const std::string& ps_id, const std::string& requester, PlaySyncState state);
    void rawReleaseSync(const std::string& ps_id, const std::string& requester, PlayStackRemoveMode stack_remove_mode);
    void clearContainer();
    void clearPostPonedRelease();

    const std::vector<std::string> SYNC_CAPABILITAY_LIST { "TTS", "AudioPlayer", "Display" };

    std::unique_ptr<PlayStackManager> playstack_manager = nullptr;
    std::function<void()> postponed_release_func = nullptr;
    std::map<std::string, IPlaySyncManagerListener*> listener_map;
    PlayStacks playstack_map;
    bool release_postponed = false;
};

} // NuguCore

#endif /* __NUGU_PLAYSYNC_MANAGER_H__ */
