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

#include <string>
#include <vector>

namespace NuguClientKit {

/**
 * @file playsync_manager_interface.hh
 * @defgroup PlaySyncManagerInterface PlaySyncManagerInterface
 * @ingroup SDKNuguClientKit
 * @brief PlaySyncManager Interface
 *
 * Interface of PlaySyncManager which manage play service context stack.
 *
 * @{
 */

/**
 * @brief PlayerSyncManagerListener interface
 * @see IPlaySyncManager
 */
class IPlaySyncManagerListener {
public:
    virtual ~IPlaySyncManagerListener() = default;

    /**
     * @brief Receive callback when display context is synced and need to render screen.
     * @param[in] id rendering display id
     */
    virtual void onSyncDisplayContext(const std::string& id) = 0;

    /**
     * @brief Receive callback when the current display has to be cleared.
     * @param[in] id rendering display id
     * @param[in] unconditionally whether clear display unconditionally or not
     */
    virtual bool onReleaseDisplayContext(const std::string& id, bool unconditionally) = 0;
};

/**
 * @brief PlayerSyncManager interface
 * @see IPlaySyncManagerListener
 */
class IPlaySyncManager {
public:
    using DisplayRenderInfo = struct {
        std::string id; /**< Unique id for identifying render info */
        std::string type; /**< Template Type */
        std::string payload; /**< Info for rendering to display */
        std::string dialog_id; /**< Dialog request id */
        std::string ps_id; /**< Play service id */
        std::string token; /**< Unique identifier of template */
    };
    using DisplayRenderer = struct {
        IPlaySyncManagerListener* listener = nullptr; /**< IPlaySyncManagerListener instance */
        std::string cap_name; /**< Capability name of handling render info */
        std::string duration; /**< Duration about showing display */
        std::string display_id; /**< id for identifying render info */
        bool only_rendering = false; /**< Whether handling only render info */
    };

public:
    virtual ~IPlaySyncManager() = default;

    /**
     * @brief Add current play service id to context stack.
     * @param[in] ps_id play service id
     * @param[in] cap_name CapabilityAgent name
     */
    virtual void addContext(const std::string& ps_id, const std::string& cap_name) = 0;

    /**
     * @brief Add current play service id to context stack. It used when display rendering info exist.
     * @param[in] ps_id play service id
     * @param[in] cap_name CapabilityAgent name
     * @param[in] renderer DisplayRenderer object
     */
    virtual void addContext(const std::string& ps_id, const std::string& cap_name, DisplayRenderer&& renderer) = 0;

    /**
     * @brief Remove play service id from context stack.
     * @param[in] ps_id play service id
     * @param[in] cap_name CapabilityAgent name
     * @param[in] immediately whether remove context immediately or not
     */
    virtual void removeContext(const std::string& ps_id, const std::string& cap_name, bool immediately = true) = 0;

    /**
     * @brief Clear pendind play service id from context stack.
     * @param[in] ps_id play service id
     */
    virtual void clearPendingContext(const std::string& ps_id) = 0;

    /**
     * @brief Get all play service id list which exist in context stack.
     */
    virtual std::vector<std::string> getAllPlayStackItems() = 0;

    /**
     * @brief Get play service id of specific CapabilityAgent.
     * @param[in] cap_name CapabilityAgent name
     */
    virtual std::string getPlayStackItem(const std::string& cap_name) = 0;

    /**
     * @brief Set whether current ASR is expect speech situation.
     * @param[in] expect_speech whether expect speech situation or not
     */
    virtual void setExpectSpeech(bool expect_speech) = 0;

    /**
     * @brief Notify mic on as starting listening speech.
     */
    virtual void onMicOn() = 0;

    /**
     * @brief Clear holding context.
     */
    virtual void clearContextHold() = 0;

    /**
     * @brief Notify error occurred when ASR.
     */
    virtual void onASRError() = 0;
};

/**
 * @}
 */

} // NuguClientKit

#endif /* __NUGU_PLAYSYNC_MANAGER_INTERFACE_H__ */
