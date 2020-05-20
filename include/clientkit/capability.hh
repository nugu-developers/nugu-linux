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

#ifndef __NUGU_CAPABILITY_H__
#define __NUGU_CAPABILITY_H__

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <base/nugu_event.h>
#include <base/nugu_network_manager.h>
#include <clientkit/capability_interface.hh>

namespace NuguClientKit {

/**
 * @file capability.hh
 * @defgroup Capability Capability
 * @ingroup SDKNuguClientKit
 * @brief base capability class
 *
 * A base class of all capability agents which inherit ICapabilityInterface
 * and implements required functions basically.
 *
 * @{
 */

/**
 * @brief Capability
 */
class Capability;

/**
 * @brief CapabilityEvent
 */
class CapabilityEvent {
public:
    CapabilityEvent(const std::string& name, Capability* cap);
    virtual ~CapabilityEvent();

    /**
     * @brief Get event name
     * @return event name
     */
    std::string getName();

    /**
     * @brief Get dialog request id
     * @return dialog request id
     */
    std::string getDialogRequestId();

    /**
     * @brief Get message id
     * @return message id
     */
    std::string getMessageId();

    /**
     * @brief Set dialog request id.
     * @param[in] id dialog request id
     */
    void setDialogRequestId(const std::string& id);

    /**
     * @brief Set event type.
     * @param[in] type event type
     */
    void setType(enum nugu_event_type type);

    /**
     * @brief Close event forcibly.
     */
    void forceClose();

    /**
     * @brief Send event to server.
     * @param[in] context context info
     * @param[in] payload payload info
     * @param[in] is_sync whether synchronized blocking event
     */
    void sendEvent(const std::string& context, const std::string& payload, bool is_sync = false);

    /**
     * @brief Send attachment event to server.
     * @param[in] is_end whether final attachment event
     * @param[in] size attachment data size
     * @param[in] data attachment data
     */
    void sendAttachmentEvent(bool is_end, size_t size, unsigned char* data);

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl;
};

/**
 * @brief Capability
 */
class Capability : virtual public ICapabilityInterface {
public:
    Capability(const std::string& name, const std::string& ver = "1.0");
    virtual ~Capability();

    /**
     * @brief Set INuguCoreContainer for using functions in NuguCore.
     * @param[in] core_container NuguCoreContainer instance
     */
    void setNuguCoreContainer(INuguCoreContainer* core_container) override;

    /**
     * @brief Initialize the current object.
     */
    void initialize() override;

    /**
     * @brief Deinitialize the current object.
     */
    void deInitialize() override;

    /**
     * @brief Set capability suspend policy
     * @param[in] policy suspend policy
     */
    void setSuspendPolicy(SuspendPolicy policy = SuspendPolicy::STOP) override;

    /**
     * @brief Suspend current action
     */
    void suspend() override;

    /**
     * @brief Restore previous suspended action
     */
    void restore() override;

    /**
     * @brief Add event result callback for error handling
     * @param[in] ename event name
     * @param[in] callback event result callback
     */
    void addEventResultCallback(const std::string& ename, EventResultCallback callback) override;

    /**
     * @brief Remove event result callback
     * @param[in] ename event name
     */
    void removeEventResultCallback(const std::string& ename) override;

    /**
     * @brief Notify event result
     * @param[in] event_desc event result description (format: 'cname.ename.msgid.dialogid.success.code')
     */
    void notifyEventResult(const std::string& event_desc) override;

    /**
     * @brief Notify event response info.
     * @param[in] msg_id message id which is sent with event
     * @param[in] json raw data which is received from server about event
     * @param[in] success whether receive event response
     */
    void notifyEventResponse(const char* msg_id, const char* json, bool success);

    /**
     * @brief Add event name and directive name for referred dialog request id.
     * @param[in] ename event name
     * @param[in] dname directive name
     * @return referred dialog request id
     */
    void addReferrerEvents(const std::string& ename, const std::string& dname);

    /**
     * @brief Get referred dialog request id.
     * @param[in] ename event name
     * @return referred dialog request id
     */
    std::string getReferrerDialogRequestId(const std::string& ename);

    /**
     * @brief Set referred dialog request id.
     * @param[in] dname directive name
     * @param[in] id referred dialog request id
     */
    void setReferrerDialogRequestId(const std::string& dname, const std::string& id);

    /**
     * @brief Set the capability name of the current object.
     * @param[in] name capability name
     */
    void setName(const std::string& name);

    /**
     * @brief Get the capability name of the current object.
     * @return capability name of the object
     */
    std::string getName() override;

    /**
     * @brief Set the capability version of the current object.
     * @param[in] ver capability version
     */
    void setVersion(const std::string& ver);

    /**
     * @brief Get the capability version of the current object.
     * @return capability version of the object
     */
    std::string getVersion() override;

    /**
     * @brief Get play service id which is managed by play stack control.
     * @return current play service id
     */
    std::string getPlayServiceIdInStackControl(const Json::Value& playstack_control);

    /**
     * @brief Process directive received from Directive Sequencer.
     * @param[in] ndir directive
     */
    void processDirective(NuguDirective* ndir) override;

    /**
     * @brief Destroy directive received from Directive Sequencer.
     * @param[in] ndir directive
     */
    void destroyDirective(NuguDirective* ndir);

    /**
     * @brief Get directive received from Directive Sequencer.
     * @return received directive
     */
    NuguDirective* getNuguDirective();

    /**
     * @brief Send event to server.
     * @param[in] name event name
     * @param[in] context context info
     * @param[in] payload payload info
     * @param[in] is_sync whether synchronized blocking event
     * @return event's request dialog id
     */
    std::string sendEvent(const std::string& name, const std::string& context, const std::string& payload, EventResultCallback cb = nullptr, bool is_sync = false);

    /**
     * @brief Send event to server.
     * @param[in] event CapabilityEvent instance
     * @param[in] context context info
     * @param[in] payload payload info
     * @param[in] is_sync whether synchronized blocking event
     */
    void sendEvent(CapabilityEvent* event, const std::string& context, const std::string& payload, EventResultCallback cb = nullptr, bool is_sync = false);

    /**
     * @brief Send attachment event to server.
     * @param[in] event CapabilityEvent instance
     * @param[in] is_end whether final attachment event
     * @param[in] size attachment data size
     * @param[in] data attachment data
     */
    void sendAttachmentEvent(CapabilityEvent* event, bool is_end, size_t size, unsigned char* data);

    /**
     * @brief It is possible to share own property value among objects.
     * @param[in] property capability property
     * @param[in] values capability property value
     */
    void getProperty(const std::string& property, std::string& value) override;

    /**
     * @brief It is possible to share own property values among objects.
     * @param[in] property capability property
     * @param[in] values capability property values
     */
    void getProperties(const std::string& property, std::list<std::string>& values) override;

    /**
     * @brief Set the listener object.
     * @param[in] clistener listener
     */
    void setCapabilityListener(ICapabilityListener* clistener) override;

    /**
     * @brief Process command from other objects.
     * @param[in] from capability who send the command
     * @param[in] command command
     * @param[in] param command parameter
     */
    void receiveCommand(const std::string& from, const std::string& command, const std::string& param) override;

    /**
     * @brief Process command received from capability manager.
     * @param[in] command command
     * @param[in] param command parameter
     */
    void receiveCommandAll(const std::string& command, const std::string& param) override;

    /**
     * @brief Parsing directive and do the required action.
     * @param[in] dname directive name
     * @param[in] message directive data
     */
    virtual void parsingDirective(const char* dname, const char* message);

    /**
     * @brief Get current context info.
     * @return context info
     */
    virtual std::string getContextInfo();

    /**
     * @brief Get ICapabilityHelper instance for using NuguCore functions.
     * @return ICapabilityHelper instance
     */
    ICapabilityHelper* getCapabilityHelper();

protected:
    /** @brief whether capability initialized */
    bool initialized = false;

    /** @brief whether has attachment */
    bool has_attachment = false;

    /** @brief whether capability suspend */
    bool suspended = false;

    /** @brief INuguCoreContainer instance for using NuguCore functions */
    INuguCoreContainer* core_container = nullptr;

    /** @brief ICapabilityHelper instance for using NuguCore functions */
    ICapabilityHelper* capa_helper = nullptr;

    /** @brief IPlaySyncManager instance for using play context sync */
    IPlaySyncManager* playsync_manager = nullptr;

    /** @brief SuspendPolicy variable for deciding suspend action (default:STOP) */
    SuspendPolicy suspend_policy = SuspendPolicy::STOP;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl;
    std::map<std::string, EventResultCallback> event_result_cbs;
};

/**
 * @}
 */

} // NuguClientKit

#endif
