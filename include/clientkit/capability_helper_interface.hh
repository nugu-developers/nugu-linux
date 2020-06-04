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
#include <base/nugu_focus.h>
#include <clientkit/focus_manager_interface.hh>
#include <clientkit/playsync_manager_interface.hh>

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
 * @brief FocusListener interface
 */
class IFocusListener {
public:
    virtual ~IFocusListener() = default;

    /**
     * @brief Receive callback when request focus is acquired.
     * @param[in] event focus event
     */
    virtual void onFocus(void* event) = 0;

    /**
     * @brief Receive callback when request focus is released.
     * @param[in] event focus event
     * @param[in] mode focus mode
     * @return focus result
     * @retval NUGU_FOCUS_REMOVE lost and remove focus
     * @retval NUGU_FOCUS_PAUSE lost but add focus to queue
     */
    virtual NuguFocusResult onUnfocus(void* event, NuguUnFocusMode mode) = 0;

    /**
     * @brief Receive callback when another focus try to steal.
     * @param[in] event focus event
     * @param[in] target_type focus which request to steal
     * @return focus steal result
     * @retval NUGU_FOCUS_STEAL_ALLOW allow the caller's request to steal the focus
     * @retval NUGU_FOCUS_STEAL_REJECT reject the caller's request to steal the focus
     */
    virtual NuguFocusStealResult onStealRequest(void* event, NuguFocusType target_type) = 0;
};

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
     * @brief Get IFocusManager instance
     * @return IFocusManager instance
     */
    virtual IFocusManager* getFocusManager() = 0;

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
     */
    virtual void sendCommand(const std::string& from, const std::string& to, const std::string& command, const std::string& param) = 0;

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
     * @brief Check the response directive group on the server to see if asr focus should be released
     * @param[in] groups directive groups
     * @param[in] dialog_id directive's dialog request id
     */
    virtual void checkAndReleaseASRFocus(const std::string& groups, const std::string& dialog_id) = 0;

    /**
     * @brief Get property from CapabilityAgent.
     * @param[in] cap CapabilityAgent
     * @param[in] property property key
     * @param[in] value property value
     */
    virtual void getCapabilityProperty(const std::string& cap, const std::string& property, std::string& value) = 0;

    /**
     * @brief Get properties from CapabilityAgent.
     * @param[in] cap CapabilityAgent
     * @param[in] property property key
     * @param[in] values property values
     */
    virtual void getCapabilityProperties(const std::string& cap, const std::string& property, std::list<std::string>& values) = 0;

    /**
     * @brief Add focus about some focus type.
     * @param[in] fname focus name
     * @param[in] type focus type
     * @param[in] listener focus listener instance
     */
    virtual int addFocus(const std::string& fname, NuguFocusType type, IFocusListener* listener) = 0;

    /**
     * @brief Remove focus.
     * @param[in] fname focus name
     */
    virtual int removeFocus(const std::string& fname) = 0;

    /**
     * @brief Release focus.
     * @param[in] fname focus name
     */
    virtual int releaseFocus(const std::string& fname) = 0;

    /**
     * @brief Request focus.
     * @param[in] fname focus name
     * @param[in] event focus event
     */
    virtual int requestFocus(const std::string& fname, void* event) = 0;

    /**
     * @brief Check whether param's focus type is focused.
     * @param[in] type focus type
     */
    virtual bool isFocusOn(NuguFocusType type) = 0;

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
