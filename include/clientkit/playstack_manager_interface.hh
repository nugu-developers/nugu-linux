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

#ifndef __NUGU_PLAYSTACK_MANAGER_INTERFACE_H__
#define __NUGU_PLAYSTACK_MANAGER_INTERFACE_H__

#include <map>
#include <string>
#include <vector>

#include <base/nugu_directive.h>

namespace NuguClientKit {

/**
 * @file playstack_manager_interface.hh
 * @defgroup PlayStackManagerInterface PlayStackManagerInterface
 * @ingroup SDKNuguClientKit
 * @brief PlayStackManager Interface
 *
 * Interface of PlayStackManager which manage play service id in playStackControl.
 *
 * @{
 */

/**
 * @brief PlayStack Layer Type
 */
enum class PlayStackLayer {
    None, /**< No Layer */
    Alert, /**< Alert Layer */
    Call, /**< Call Layer */
    Info, /**< Info Layer */
    Media /**< Media Layer */
};

/**
 * @brief PlayStack Remove Mode
 */
enum class PlayStackRemoveMode {
    Normal, /**< remove stack after 7s' */
    Immediately, /**< remove stack immediately */
    Later /**< remove stack after 10m' */
};

/**
 * @brief IPlayStackManagerListener interface
 * @see IPlayStackManager
 */
class IPlayStackManagerListener {
public:
    virtual ~IPlayStackManagerListener() = default;

    /**
     * @brief Receive callback when play service id is added to playstack.
     * @param[in] ps_is play service id
     */
    virtual void onStackAdded(const std::string& ps_id) = 0;

    /**
     * @brief Receive callback when play service id is removed from playstack.
     * @param[in] ps_is play service id
     */
    virtual void onStackRemoved(const std::string& ps_id) = 0;
};

/**
 * @brief PlayStackManager interface
 * @see IPlayStackManagerListener
 */
class IPlayStackManager {
public:
    using PlayStack = std::pair<std::map<std::string, PlayStackLayer>, std::vector<std::string>>;

public:
    virtual ~IPlayStackManager() = default;

    /**
     * @brief Add IPlayStackManagerListener.
     * @param[in] listener IPlayStackManagerListener instance
     */
    virtual void addListener(IPlayStackManagerListener* listener) = 0;

    /**
     * @brief Remove IPlayStackManagerListener.
     * @param[in] listener IPlayStackManagerListener instance
     */
    virtual void removeListener(IPlayStackManagerListener* listener) = 0;

    /**
     * @brief Add play service id to playstack.
     * @param[in] ps_id play service id
     * @param[in] ndir directive
     */
    virtual void add(const std::string& ps_id, NuguDirective* ndir) = 0;

    /**
     * @brief Remove play service id from playstack.
     * @param[in] ps_id play service id
     * @param[in] mode playstack remove mode (defalut : Normal)
     */
    virtual void remove(const std::string& ps_id, PlayStackRemoveMode mode = PlayStackRemoveMode::Normal) = 0;

    /**
     * @brief Check whether to be stacked condition if the current layer is added.
     * @param[in] ndir directive
     * @return true if stacked condition, otherwise false
     */
    virtual bool isStackedCondition(NuguDirective* ndir) = 0;

    /**
     * @brief Stop timer for removing playstack.
     */
    virtual void stopHolding() = 0;

    /**
     * @brief Reset timer for removing playstack.
     */
    virtual void resetHolding() = 0;

    /**
     * @brief Get playstack layer type which is mapped to play service id.
     * @param[in] ps_id play service id
     * @return PlayStackLayer
     */
    virtual PlayStackLayer getPlayStackLayer(const std::string& ps_id) = 0;

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

#endif /* __NUGU_PLAYSTACK_MANAGER_INTERFACE_H__ */
