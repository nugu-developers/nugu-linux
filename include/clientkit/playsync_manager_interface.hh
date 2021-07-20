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

#ifndef __NUGU_PLAYSYNC_MANAGER_INTERFACE_H__
#define __NUGU_PLAYSYNC_MANAGER_INTERFACE_H__

#include <map>
#include <string>
#include <vector>

#include <base/nugu_directive.h>

namespace NuguClientKit {

/**
 * @file playsync_manager_interface.hh
 * @defgroup PlaySyncManagerInterface PlaySyncManagerInterface
 * @ingroup SDKNuguClientKit
 * @brief PlaySyncManager Interface
 *
 * Interface of PlaySyncManager which manage context and sync flow between related capability agents.
 *
 * @{
 */

/**
 * @brief PlaySync State
 */
enum class PlaySyncState {
    None, /**< No State */
    Prepared, /**< agents are prepared for sync */
    Synced, /**< agents are synced */
    Released, /**< agents are released */
    Appending /**< agents are appending to already sync */
};

/**
 * @brief PlayStack Activity Type
 */
enum class PlayStackActivity {
    None, /**< No Activity */
    Alert, /**< Alert Activity */
    Call, /**< Call Activity */
    TTS, /**< TTS Activity */
    Media /**< Media Activity */
};

/**
 * @brief IPlaySyncManagerListener interface
 * @see IPlaySyncManager
 */
class IPlaySyncManagerListener {
public:
    virtual ~IPlaySyncManagerListener() = default;

    /**
     * @brief Receive callback when sync state is changed.
     * @param[in] ps_is play service id
     * @param[in] state play sync state
     * @param[in] extra_data an extra_data which is sent at starting sync
     */
    virtual void onSyncState(const std::string& ps_id, PlaySyncState state, void* extra_data);

    /**
     * @brief Receive callback when the extra data is changed.
     * @param[in] ps_is play service id
     * @param[in] extra_datas the extra_datas which are composed by previous and new
     */
    virtual void onDataChanged(const std::string& ps_id, std::pair<void*, void*> extra_datas);

    /**
     * @brief Receive callback when the play stack is changed.
     * @param[in] ps_ids play service ids (first:added stack, second:removed stack)
     */
    virtual void onStackChanged(const std::pair<std::string, std::string>& ps_ids);
};

/**
 * @brief IPlaySyncManager interface
 * @see IPlaySyncManagerListener
 */
class IPlaySyncManager {
public:
    using PlaySyncContainer = std::map<std::string, std::pair<PlaySyncState, void*>>;
    using PlayStacks = std::map<std::string, PlaySyncContainer>;

public:
    virtual ~IPlaySyncManager() = default;

    /**
     * @brief Register capability agent for sync (default:TTS, AudioPlayer, Display)
     * @param[in] capability_name capability agent name
     */
    virtual void registerCapabilityForSync(const std::string& capability_name) = 0;

    /**
     * @brief Add IPlaySyncManagerListener.
     * @param[in] requester capability agent name
     * @param[in] listener IPlaySyncManagerListener instance
     */
    virtual void addListener(const std::string& requester, IPlaySyncManagerListener* listener) = 0;

    /**
     * @brief Remove IPlayStackManagerListener.
     * @param[in] requester capability agent name
     */
    virtual void removeListener(const std::string& requester) = 0;

    /**
     * @brief Prepare sync capability agents which are included in directive.
     * @param[in] ps_id play service id
     * @param[in] ndir directive
     */
    virtual void prepareSync(const std::string& ps_id, NuguDirective* ndir) = 0;

    /**
     * @brief Start sync specific capability agent which was prepared.
     * @param[in] ps_id play service id
     * @param[in] requester capability agent name
     * @param[in] extra_data an extra_data which is used after synced
     */
    virtual void startSync(const std::string& ps_id, const std::string& requester, void* extra_data = nullptr) = 0;

    /**
     * @brief Cancel sync specific capability agent which was synced.
     * @param[in] ps_id play service id
     * @param[in] requester capability agent name
     */
    virtual void cancelSync(const std::string& ps_id, const std::string& requester) = 0;

    /**
     * @brief Release sync all capability agents. (Actually, it release sync after context holding time.)
     * @param[in] ps_id play service id
     * @param[in] requester capability agent name
     */
    virtual void releaseSync(const std::string& ps_id, const std::string& requester) = 0;

    /**
     * @brief Release sync all capability agents after long time elapsed.
     * @param[in] ps_id play service id
     * @param[in] requester capability agent name
     */
    virtual void releaseSyncLater(const std::string& ps_id, const std::string& requester) = 0;

    /**
     * @brief Release sync all capability agents immediately.
     * @param[in] ps_id play service id
     * @param[in] requester capability agent name
     */
    virtual void releaseSyncImmediately(const std::string& ps_id, const std::string& requester) = 0;

    /**
     * @brief Release sync all capability agents unconditionally.
     */
    virtual void releaseSyncUnconditionally() = 0;

    /**
     * @brief Postpone already started release if exist.
     */
    virtual void postPoneRelease() = 0;

    /**
     * @brief Continue pending release if exist.
     */
    virtual void continueRelease() = 0;

    /**
     * @brief Stop timer for releasing sync.
     */
    virtual void stopHolding() = 0;

    /**
     * @brief Reset timer for releasing sync.
     */
    virtual void resetHolding() = 0;

    /**
     * @brief Clear timer for releasing sync.
     */
    virtual void clearHolding() = 0;

    /**
     * @brief Clear all playstack info.
     */
    virtual void clear() = 0;

    /**
     * @brief Check whether the previous dialog has to be handled or not.
     * @param[in] prev_ndir preivous directive
     * @param[in] cur_ndir current directive
     * @return true if has to be handled, otherwise false
     */
    virtual bool isConditionToHandlePrevDialog(NuguDirective* prev_ndir, NuguDirective* cur_ndir) = 0;

    /**
     * @brief Check whether the specific playstack activity exist.
     * @param[in] ps_id play service id
     * @param[in] activity playstack activity
     * @return true if has to be handled, otherwise false
     */
    virtual bool hasActivity(const std::string& ps_id, PlayStackActivity activity) = 0;

    /**
     * @brief Check whether the next playstack to handle exists.
     * @return true if the next playstack exists, otherwise false
     */
    virtual bool hasNextPlayStack() = 0;

    /**
     * @brief Get all items which are stored in playstack.
     * @return Playstack items (play service id list)
     */
    virtual std::vector<std::string> getAllPlayStackItems() = 0;
};

/**
 * @}
 */

} // NuguClientKit

#endif /* __NUGU_PLAYSYNC_MANAGER_INTERFACE_H__ */
