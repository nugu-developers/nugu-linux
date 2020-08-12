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

#ifndef __NUGU_INTERACTION_CONTROL_MANAGER_INTERFACE_H__
#define __NUGU_INTERACTION_CONTROL_MANAGER_INTERFACE_H__

#include <string>

namespace NuguClientKit {

/**
 * @file interaction_control_manager_interface.hh
 * @defgroup InteractionControlManagerInterface InteractionControlManagerInterface
 * @ingroup SDKNuguClientKit
 * @brief InteractionControlManager Interface
 *
 * Interface of InteractionControlManager which manages multi-turn maintaining situation.
 *
 * @{
 */

/**
 * @brief Interaction Mode
 */
enum class InteractionMode {
    NONE, /**< no action required */
    MULTI_TURN /**< maintain Multi-turn */
};

/**
 * @brief IInteractionControlManagerListener interface
 * @see InteractionControlManager
 */
class IInteractionControlManagerListener {
public:
    virtual ~IInteractionControlManagerListener() = default;

    /**
     * @brief Receive callback when the interaction mode is changed.
     * @param[in] is_multi_turn whether current mode is multi-turn or not
     */
    virtual void onModeChanged(bool is_multi_turn) = 0;
};

/**
 * @brief InteractionControlManager interface
 * @see IInteractionControlManagerListener
 */
class IInteractionControlManager {
public:
    virtual ~IInteractionControlManager() = default;

    /**
     * @brief Add IInteractionControlManagerListener.
     * @param[in] listener IInteractionControlManagerListener instance
     */
    virtual void addListener(IInteractionControlManagerListener* listener) = 0;

    /**
     * @brief Remove IInteractionControlManagerListener.
     * @param[in] listener IInteractionControlManagerListener instance
     */
    virtual void removeListener(IInteractionControlManagerListener* listener) = 0;

    /**
     * @brief Start specific interaction mode.
     * @param[in] mode a kind of interaction mode
     * @param[in] requester a object which request interaction mode
     */
    virtual void start(InteractionMode mode, const std::string& requester) = 0;

    /**
     * @brief Finish specific interaction mode.
     * @param[in] mode a kind of interaction mode
     * @param[in] requester a object which request interaction mode
     */
    virtual void finish(InteractionMode mode, const std::string& requester) = 0;

    /**
     * @brief Clear all about interaction mode
     */
    virtual void clear() = 0;
};

/**
 * @}
 */

} // NuguClientKit

#endif /* __NUGU_INTERACTION_CONTROL_MANAGER_INTERFACE_H__ */
