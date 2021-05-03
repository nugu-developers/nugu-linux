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

#ifndef __NUGU_ROUTINE_MANAGER_INTERFACE_H__
#define __NUGU_ROUTINE_MANAGER_INTERFACE_H__

#include <functional>

#include <base/nugu_directive.h>

namespace NuguClientKit {

/**
 * @file routine_manager_interface.hh
 * @defgroup RoutineManagerInterface RoutineManagerInterface
 * @ingroup SDKNuguClientKit
 * @brief RoutineManager Interface
 *
 * Interface of RoutineManager for control routine play.
 *
 * @{
 */

/**
 * @brief Routine Activity list
 */
enum class RoutineActivity {
    IDLE, /**< Routine is never executed. */
    PLAYING, /**< Routine is being executed. */
    INTERRUPTED, /**< Routine is interrupted. */
    FINISHED, /**< Routine execution is done. */
    STOPPED /**< Routine is stopped before finish. */
};

/**
 * @brief IRoutineManagerListener interface
 * @see IRoutineManager
 */
class IRoutineManagerListener {
public:
    virtual ~IRoutineManagerListener() = default;

    /**
     * @brief Receive callback when the routine activity state is changed.
     * @param[in] activity current routine activity state
     */
    virtual void onActivity(RoutineActivity activity) = 0;
};

/**
 * @brief IRoutineManager interface
 * @see IRoutineManagerListener
 */
class IRoutineManager {
public:
    using TextRequester = std::function<std::string(const std::string&, const std::string&, const std::string&)>;
    using DataRequester = std::function<std::string(const std::string&, const Json::Value&)>;

public:
    virtual ~IRoutineManager() = default;

    /**
     * @brief Add IRoutineManagerListener.
     * @param[in] listener IRoutineManagerListener instance
     */
    virtual void addListener(IRoutineManagerListener* listener) = 0;

    /**
     * @brief Remove IRoutineManagerListener.
     * @param[in] listener IRoutineManagerListener instance
     */
    virtual void removeListener(IRoutineManagerListener* listener) = 0;

    /**
     * @brief Set TextRequester delegate for handling text type action.
     * @param[in] requester TextRequester delegate
     */
    virtual void setTextRequester(TextRequester requester) = 0;

    /**
     * @brief Set DataRequester delegate for handling data type action.
     * @param[in] requester DataRequester delegate
     */
    virtual void setDataRequester(DataRequester requester) = 0;

    /**
     * @brief Start routine.
     * @param[in] token token which is delivered from routine play
     * @param[in] actions action list
     */
    virtual bool start(const std::string& token, const Json::Value& actions) = 0;

    /**
     * @brief Stop routine.
     */
    virtual void stop() = 0;

    /**
     * @brief Interrupt routine.
     */
    virtual void interrupt() = 0;

    /**
     * @brief Resume routine.
     */
    virtual void resume() = 0;

    /**
     * @brief Finish action.
     */
    virtual void finish() = 0;

    /**
     * @brief Get index of current active action.
     * @return index of current active action
     */
    virtual int getCurrentActionIndex() = 0;

    /**
     * @brief Check whether routine is in progress currently.
     * @return true if in progress, otherwise false
     */
    virtual bool isRoutineProgress() = 0;

    /**
     * @brief Check whether routine has remained action to execute.
     * @return true if has action, otherwise false
     */
    virtual bool isRoutineAlive() = 0;

    /**
     * @brief Check whether action is in progress currently.
     * @param[in] dialog_id dialog id of action
     * @return true if in progress, otherwise false
     */
    virtual bool isActionProgress(const std::string& dialog_id) = 0;

    /**
     * @brief Check whether routine directive exist in NuguDirective.
     * @param[in] ndir NuguDirective object
     * @return true if exist, otherwise false
     */
    virtual bool hasRoutineDirective(const NuguDirective* ndir) = 0;

    /**
     * @brief Check whether current condition is to stop routine.
     * @param[in] ndir NuguDirective object
     * @return true if satisfied, otherwise false
     */
    virtual bool isConditionToStop(const NuguDirective* ndir) = 0;
};

/**
 * @}
 */

} // NuguClientKit

#endif /* __NUGU_ROUTINE_MANAGER_INTERFACE_H__ */
