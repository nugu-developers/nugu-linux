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

#ifndef __NUGU_CAPABILITY_HELPER_INTERFACE_H__
#define __NUGU_CAPABILITY_HELPER_INTERFACE_H__

#include <json/json.h>
#include <list>

#include <base/nugu_event.h>
#include <clientkit/directive_sequencer_interface.hh>
#include <clientkit/focus_manager_interface.hh>
#include <clientkit/playstack_manager_interface.hh>
#include <clientkit/playsync_manager_interface.hh>
#include <clientkit/session_manager_interface.hh>

namespace NuguClientKit {
/**
 * @file capability_helper_interface.hh
 * @defgroup CapabilityHelperInterface CapabilityHelperInterface
 * @ingroup SDKNuguClientKit
 * @brief CapabilityHelper Interface
 *
 * Helper for using required functions in CapabilityManager or NuguCore Components
 *
 * @{
 */

/**
 * @brief CapabilityHelper interface
 */
class ICapabilityHelper {
public:
    virtual ~ICapabilityHelper() = default;

    /**
     * @brief Get IPlaySyncManager instance
     * @return IPlaySyncManager instance
     */
    virtual IPlaySyncManager* getPlaySyncManager() = 0;

    /**
     * @brief Get IPlayStackManager instance
     * @return IPlayStackManager instance
     */
    virtual IPlayStackManager* getPlayStackManager() = 0;

    /**
     * @brief Get IFocusManager instance
     * @return IFocusManager instance
     */
    virtual IFocusManager* getFocusManager() = 0;

    /**
     * @brief Get ISessionManager instance
     * @return ISessionManager instance
     */
    virtual ISessionManager* getSessionManager() = 0;

    /**
     * @brief Get IDirectiveSequencer instance
     * @return IDirectiveSequencer instance
     */
    virtual IDirectiveSequencer* getDirectiveSequencer() = 0;

    /**
     * @brief Set Audio Recorder mute/unmute
     * @param[in] mute mute/unmute
     */
    virtual bool setMute(bool mute) = 0;

    /**
     * @brief Send command between CapabilityAgents.
     * @param[in] from source CapabilityAgent
     * @param[in] to target CapabilityAgent
     * @param[in] command command
     * @param[in] param parameter
     * @return send command result
     * @retval true CapabilityAgent and command are valid
     * @retval false CapabilityAgent or command is invalid
     */
    virtual bool sendCommand(const std::string& from, const std::string& to, const std::string& command, const std::string& param) = 0;

    /**
     * @brief Request to send event result via CapabilityManager.
     * @param[in] event event for monitoring result
     */
    virtual void requestEventResult(NuguEvent* event) = 0;

    /**
     * @brief Suspend all current capability action
     */
    virtual void suspendAll() = 0;

    /**
     * @brief Restore previous suspended capability action
     */
    virtual void restoreAll() = 0;

    /**
     * @brief Get wakeup keyword for detection
     */
    virtual std::string getWakeupWord() = 0;

    /**
     * @brief Get property from CapabilityAgent.
     * @param[in] cap CapabilityAgent
     * @param[in] property property key
     * @param[in] value property value
     * @return property get result
     * @retval true CapabilityAgent and property are valid
     * @retval false CapabilityAgent or property is invalid
     */
    virtual bool getCapabilityProperty(const std::string& cap, const std::string& property, std::string& value) = 0;

    /**
     * @brief Get properties from CapabilityAgent.
     * @param[in] cap CapabilityAgent
     * @param[in] property property key
     * @param[in] values property values
     * @return property get result
     * @retval true CapabilityAgent and property are valid
     * @retval false CapabilityAgent or property is invalid
     */
    virtual bool getCapabilityProperties(const std::string& cap, const std::string& property, std::list<std::string>& values) = 0;

    /**
     * @brief Get context info.
     * @param[in] cname The name of the capability requesting the context
     * @param[in] ctx reference object for storing context info
     */
    virtual std::string makeContextInfo(const std::string& cname, Json::Value& ctx) = 0;

    /**
     * @brief Get context info from All CapabilityAgents.
     */
    virtual std::string makeAllContextInfo() = 0;

    /**
     * @brief Get context info including play stack of All CapabilityAgents.
     */
    virtual std::string makeAllContextInfoStack() = 0;
};

/**
 * @}
 */

} // NuguClientKit

#endif /* __NUGU_CAPABILITY_HELPER_INTERFACE_H__ */
