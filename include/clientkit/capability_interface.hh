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

#ifndef __NUGU_CAPABILITY_INTERFACE_H__
#define __NUGU_CAPABILITY_INTERFACE_H__

#include <functional>
#include <json/json.h>
#include <list>
#include <set>
#include <string>

#include <base/nugu_directive.h>
#include <clientkit/nugu_core_container_interface.hh>

namespace NuguClientKit {

/**
 * @file capability_interface.hh
 * @defgroup CapabilityInterface CapabilityInterface
 * @ingroup SDKNuguClientKit
 * @brief capability interface
 *
 * An abstract object that a capability agent must perform in common.
 *
 * @{
 */

/**
 * @brief Policy about canceling directives which are belong to the specific dialog id.
 */
typedef struct {
    bool cancel_all; /**< cancel all directives or selections */
    std::set<std::string> dir_groups; /**< directive groups for canceling */
} DirectiveCancelPolicy;

/**
 * @brief capability listener interface
 * @see ICapabilityHandler
 */
class ICapabilityListener {
public:
    virtual ~ICapabilityListener() = default;
};

/**
 * @brief capability interface
 * @see NuguDirective
 * @see ICapabilityListener
 */
class ICapabilityInterface {
public:
    /**
     * @brief Capability suspend policy
     */
    enum class SuspendPolicy {
        STOP, /**< Stop current action unconditionally */
        PAUSE /**< Pause current action. It could resume later */
    };

    /**
     * @brief Event result callback for error handling
     * @param[in] ename event name
     * @param[in] msg_id event message id
     * @param[in] dialog_id event request dialog id
     * @param[in] success event result
     * @param[in] code event result code (similar to http status code)
     */
    using EventResultCallback = std::function<void(const std::string&, const std::string&, const std::string&, int, int)>;

public:
    virtual ~ICapabilityInterface() = default;

    /**
     * @brief Set INuguCoreContainer for using functions in NuguCore.
     * @param[in] core_container NuguCoreContainer instance
     */
    virtual void setNuguCoreContainer(INuguCoreContainer* core_container) = 0;

    /**
     * @brief Initialize the current object.
     */
    virtual void initialize() = 0;

    /**
     * @brief Deinitialize the current object.
     */
    virtual void deInitialize() = 0;

    /**
     * @brief Set capability suspend policy
     * @param[in] policy suspend policy
     */
    virtual void setSuspendPolicy(SuspendPolicy policy = SuspendPolicy::STOP) = 0;

    /**
     * @brief Suspend current action
     */
    virtual void suspend() = 0;

    /**
     * @brief Restore previous suspended action
     */
    virtual void restore() = 0;

    /**
     * @brief Add event result callback for error handling
     * @param[in] ename event name
     * @param[in] callback event result callback
     */
    virtual void addEventResultCallback(const std::string& ename, EventResultCallback callback) = 0;

    /**
     * @brief Remove event result callback
     * @param[in] ename event name
     */
    virtual void removeEventResultCallback(const std::string& ename) = 0;

    /**
     * @brief Notify event result
     * @param[in] event_desc event result description (format: 'cname.ename.msgid.dialogid.success.code')
     */
    virtual void notifyEventResult(const std::string& event_desc) = 0;

    /**
     * @brief Notify event response info.
     * @param[in] msg_id message id which is sent with event
     * @param[in] data raw data which is received from server about event (json format)
     * @param[in] success whether receive event response
     */
    virtual void notifyEventResponse(const std::string& msg_id, const std::string& data, bool success) = 0;

    /**
     * @brief Get the capability name of the current object.
     * @return capability name of the object
     */
    virtual std::string getName() = 0;

    /**
     * @brief Get the capability version of the current object.
     * @return capability version of the object
     */
    virtual std::string getVersion() = 0;

    /**
     * @brief Receive a directive preprocessing request from Directive sequencer.
     * @param[in] ndir directive
     */
    virtual void preprocessDirective(NuguDirective* ndir) = 0;

    /**
     * @brief Receive a directive cancellation from the Directive sequencer.
     * @param[in] ndir directive
     */
    virtual void cancelDirective(NuguDirective* ndir) = 0;

    /**
     * @brief Receive a directive processing request from Directive sequencer.
     * @param[in] ndir directive
     */
    virtual void processDirective(NuguDirective* ndir) = 0;

    /**
     * @brief Update the current context information of the capability agent.
     * @param[in] ctx capability agent's context
     */
    virtual void updateInfoForContext(Json::Value& ctx) = 0;

    /**
     * @brief Update the compact context information of the capability agent.
     * @param[in] ctx capability agent's context
     */
    virtual void updateCompactContext(Json::Value& ctx) = 0;

    /**
     * @brief Process command from other objects.
     * @param[in] from capability who send the command
     * @param[in] command command
     * @param[in] param command parameter
     * @return command result
     * @retval true The command is valid
     * @retval false The command is invalid
     */
    virtual bool receiveCommand(const std::string& from, const std::string& command, const std::string& param) = 0;

    /**
     * @brief Process command received from capability manager.
     * @param[in] command command
     * @param[in] param command parameter
     */
    virtual void receiveCommandAll(const std::string& command, const std::string& param) = 0;

    /**
     * @brief It is possible to share own property value among objects.
     * @param[in] property capability property
     * @param[in] values capability property value
     * @return property get result
     * @retval true The property is valid
     * @retval false The property is invalid
     */
    virtual bool getProperty(const std::string& property, std::string& value) = 0;

    /**
     * @brief It is possible to share own property values among objects.
     * @param[in] property capability property
     * @param[in] values capability property values
     * @return property get result
     * @retval true The property is valid
     * @retval false The property is invalid
     */
    virtual bool getProperties(const std::string& property, std::list<std::string>& values) = 0;

    /**
     * @brief Set the listener object
     * @param[in] clistener listener
     */
    virtual void setCapabilityListener(ICapabilityListener* clistener) = 0;

    /**
     * @brief Set directive cancel policy
     * @param[in] cancel_previous_dialog whether canceling previous dialog or not
     * @param[in] cancel_policy policy object
     */
    virtual void setCancelPolicy(bool cancel_previous_dialog, DirectiveCancelPolicy&& cancel_policy = { true }) = 0;
};

/**
 * @}
 */

} // NuguClientKit

#endif /* __NUGU_CAPABILITY_INTERFACE_H__ */
