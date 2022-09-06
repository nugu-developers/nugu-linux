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

#ifndef __NUGU_FOCUS_MANAGER_INTERFACE_H__
#define __NUGU_FOCUS_MANAGER_INTERFACE_H__

#include <iostream>
#include <vector>

namespace NuguClientKit {

/**
 * @file focus_manager_interface.hh
 * @defgroup FocusManagerInterface FocusManagerInterface
 * @ingroup SDKNuguClientKit
 * @brief Focus Manager Interface
 *
 * FocusManager manage focus configuration and focus can be requested or released.
 *
 * @{
 */

#define CALL_FOCUS_TYPE "Call" /** @def Default Call Focus Type */
#define CALL_FOCUS_REQUEST_PRIORITY 100 /** @def Default Call Focus Request Priority */
#define CALL_FOCUS_RELEASE_PRIORITY 100 /** @def Default Call Focus Release Priority */

#define ASR_USER_FOCUS_TYPE "ASRUser" /** @def Default ASR User Focus Type */
#define ASR_USER_FOCUS_REQUEST_PRIORITY 100 /** @def Default ASR User Focus Request Priority */
#define ASR_USER_FOCUS_RELEASE_PRIORITY 200 /** @def Default ASR User Focus Release Priority */

#define ASR_DM_FOCUS_TYPE "ASRDM" /** @def Default ASR DM Focus Type */
#define ASR_DM_FOCUS_REQUEST_PRIORITY 400 /** @def Default ASR DM Focus Request Priority */
#define ASR_DM_FOCUS_RELEASE_PRIORITY 300 /** @def Default ASR DM Focus Release Priority */

#define INFO_FOCUS_TYPE "Info" /** @def Default Information Focus Type */
#define INFO_FOCUS_REQUEST_PRIORITY 200 /** @def Default Information Focus Request Priority */
#define INFO_FOCUS_RELEASE_PRIORITY 300 /** @def Default Information Focus Release Priority */

#define ALERTS_FOCUS_TYPE "Alerts" /** @def Default Alerts Focus Type */
#define ALERTS_FOCUS_REQUEST_PRIORITY 200 /** @def Default Alerts Focus Request Priority */
#define ALERTS_FOCUS_RELEASE_PRIORITY 300 /** @def Default Alerts Focus Release Priority */

#define ASR_BEEP_FOCUS_TYPE "ASRBeep" /** @def Default ASR Beep Focus Type */
#define ASR_BEEP_FOCUS_REQUEST_PRIORITY 500 /** @def Default ASR Beep Focus Request Priority */
#define ASR_BEEP_FOCUS_RELEASE_PRIORITY 400 /** @def Default ASR Beep Focus Release Priority */

#define MEDIA_FOCUS_TYPE "Media" /** @def Default Media Focus Type */
#define MEDIA_FOCUS_REQUEST_PRIORITY 300 /** @def Default Media Focus Request Priority */
#define MEDIA_FOCUS_RELEASE_PRIORITY 500 /** @def Default Media Focus Release Priority */

#define SOUND_FOCUS_TYPE "Sound" /** @def Default Sound Focus Type */
#define SOUND_FOCUS_REQUEST_PRIORITY 500 /** @def Default Sound Focus Request Priority */
#define SOUND_FOCUS_RELEASE_PRIORITY 500 /** @def Default Sound Focus Release Priority */

#define DUMMY_FOCUS_TYPE "Dummy" /** @def Default Dummy Focus Type */
#define DUMMY_FOCUS_REQUEST_PRIORITY 600 /** @def Default Dummy Focus Request Priority */
#define DUMMY_FOCUS_RELEASE_PRIORITY 500 /** @def Default Dummy Focus Release Priority */

/**
 * @brief FocusState
 */
enum class FocusState {
    FOREGROUND, /**< The focus is activated */
    BACKGROUND, /**< The focus is pending and waiting for release the foreground resource. */
    NONE /**< The focus is released */
};

/**
 * @brief FocusConfiguration
 */
typedef struct _FocusConfiguration {
    std::string type; /**< focus type */
    int priority; /**< focus priority */
} FocusConfiguration;

/**
 * @brief IFocusResourceListener
 * @see IFocusManager
 */
class IFocusResourceListener {
public:
    virtual ~IFocusResourceListener() = default;

    /**
     * @brief Notify the resource of focus state change
     * @param[in] state resource's new state
     */
    virtual void onFocusChanged(FocusState state) = 0;
};

/**
 * @brief IFocusManagerObserver
 * @see IFocusManager
 */
class IFocusManagerObserver {
public:
    virtual ~IFocusManagerObserver() = default;

    /**
     * @brief Support to monitor the change of focus state for all resources
     * @param[in] configuration focus configuration
     * @param[in] state resource's new state
     * @param[in] name focus name
     */
    virtual void onFocusChanged(const FocusConfiguration& configuration, FocusState state, const std::string& name) = 0;
};

/**
 * @brief IFocusManager
 * @see IFocusResourceListener
 * @see IFocusManagerObserver
 */
class IFocusManager {
public:
    /**
     * @brief Request Focus
     * @param[in] type focus type
     * @param[in] name focus name
     * @param[in] listener listener object
     * @return if the request is success, then true otherwise false
     */
    virtual bool requestFocus(const std::string& type, const std::string& name, IFocusResourceListener* listener) = 0;

    /**
     * @brief Release focus
     * @param[in] type focus type
     * @param[in] name focus name
     * @return if the release is success, then true otherwise false
     */
    virtual bool releaseFocus(const std::string& type, const std::string& name) = 0;

    /**
     * @brief Hold focus. If requested to hold focus, the low priority focus is not preempted.
     * @param[in] type focus type
     * @return if the hold is success, then true otherwise false
     */
    virtual bool holdFocus(const std::string& type) = 0;

    /**
     * @brief Unhold focus
     * @param[in] type focus type
     * @return if the unhold is success, then true otherwise false
     */
    virtual bool unholdFocus(const std::string& type) = 0;

    /**
     * @brief Set focus configurations.
     * @param[in] request configurations for focus request
     * @param[in] release configurations for focus release
     */
    virtual void setConfigurations(std::vector<FocusConfiguration>& request, std::vector<FocusConfiguration>& release) = 0;

    /**
     * @brief Stop all focus.
     */
    virtual void stopAllFocus() = 0;

    /**
     * @brief Stop highest priority of focus that is foreground state.
     */
    virtual void stopForegroundFocus() = 0;

    /**
     * @brief Get state string
     * @param[in] state focus state
     * @return state string
     */
    virtual std::string getStateString(FocusState state) = 0;

    /**
     * @brief Convert state text to matched FocusState enum.
     * @param[in] state_text state text
     * @throw std::out_of_range exception thrown if matched FocusState not exist
     * @return FocusState enum
     */
    virtual FocusState convertToFocusState(const std::string& state_text) = 0;

    /**
     * @brief Add the Observer object
     * @param[in] observer observer object
     */
    virtual void addObserver(IFocusManagerObserver* observer) = 0;

    /**
     * @brief Remove the Observer object
     * @param[in] observer observer object
     */
    virtual void removeObserver(IFocusManagerObserver* observer) = 0;
};

} // NuguClientKit

#endif /* __NUGU_FOCUS_MANAGER_INTERFACE_H__ */
