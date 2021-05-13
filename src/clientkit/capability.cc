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

#include <cstring>
#include <string.h>

#include "base/nugu_log.h"

#include "clientkit/capability.hh"

namespace NuguClientKit {

struct Capability::Impl {
    std::string cname;
    std::string version;
    std::string ref_dialog_id;
    NuguDirective* cur_ndir = nullptr;
    NuguDirective* prev_ndir = nullptr;
    DirectiveCancelPolicy cancel_policy = { true };
    std::map<std::string, std::string> referrer_events;
    std::map<std::string, std::string> referrer_dirs;

    bool parseEventResultDesc(const std::string& desc, std::string& ename, std::string& msg_id, std::string& dialog_id, bool& success, int& code);
};

struct CapabilityEvent::Impl {
    Capability* capability = nullptr;
    NuguEvent* event = nullptr;
    std::string dialog_id;
};

CapabilityEvent::CapabilityEvent(const std::string& name, Capability* cap)
    : pimpl(std::unique_ptr<Impl>(new Impl()))
{
    pimpl->capability = cap;
    pimpl->event = nugu_event_new(cap->getName().c_str(), name.c_str(), cap->getVersion().c_str());
    pimpl->dialog_id = nugu_event_peek_dialog_id(pimpl->event);
}

CapabilityEvent::~CapabilityEvent()
{
    if (pimpl->event) {
        nugu_event_free(pimpl->event);
        pimpl->event = nullptr;
    }
}

std::string CapabilityEvent::getName()
{
    return nugu_event_peek_name(pimpl->event);
}

std::string CapabilityEvent::getDialogRequestId()
{
    return pimpl->dialog_id;
}

std::string CapabilityEvent::getMessageId()
{
    return nugu_event_peek_msg_id(pimpl->event);
}

void CapabilityEvent::setDialogRequestId(const std::string& id)
{
    pimpl->dialog_id = id;
    nugu_event_set_dialog_id(pimpl->event, pimpl->dialog_id.c_str());
}

void CapabilityEvent::setType(enum nugu_event_type type)
{
    nugu_event_set_type(pimpl->event, type);
}

void CapabilityEvent::forceClose()
{
    if (nugu_event_get_type(pimpl->event) == NUGU_EVENT_TYPE_DEFAULT)
        return;

    nugu_network_manager_force_close_event(pimpl->event);
}

void CapabilityEvent::sendEvent(const std::string& context, const std::string& payload)
{
    if (pimpl->event == NULL) {
        nugu_error("event is NULL");
        return;
    }

    if (context.size())
        nugu_event_set_context(pimpl->event, context.c_str());

    if (payload.size())
        nugu_event_set_json(pimpl->event, payload.c_str());

    std::string ref_dialog_id = pimpl->capability->getReferrerDialogRequestId(getName());
    if (ref_dialog_id.size())
        nugu_event_set_referrer_id(pimpl->event, ref_dialog_id.c_str());

    pimpl->capability->getCapabilityHelper()->sendCommand(pimpl->capability->getName(), "System", "activity", "");

    pimpl->capability->getCapabilityHelper()->requestEventResult(pimpl->event);
    nugu_network_manager_send_event(pimpl->event);
}

void CapabilityEvent::sendAttachmentEvent(bool is_end, size_t size, unsigned char* data)
{
    if (pimpl->event == NULL) {
        nugu_error("event is NULL");
        return;
    }

    if ((size && data) || is_end)
        nugu_network_manager_send_event_data(pimpl->event, is_end, size, data);
}

Capability::Capability(const std::string& name, const std::string& ver)
    : pimpl(std::unique_ptr<Impl>(new Impl()))
{
    pimpl->cname = name;
    pimpl->version = ver;
}

Capability::~Capability()
{
    pimpl->referrer_events.clear();
    pimpl->referrer_dirs.clear();
    event_result_cbs.clear();
}

void Capability::setNuguCoreContainer(INuguCoreContainer* core_container)
{
    this->core_container = core_container;
    capa_helper = core_container->getCapabilityHelper();

    playsync_manager = capa_helper->getPlaySyncManager();
    focus_manager = capa_helper->getFocusManager();
    session_manager = capa_helper->getSessionManager();
    directive_sequencer = capa_helper->getDirectiveSequencer();
    interaction_control_manager = capa_helper->getInteractionControlManager();
    routine_manager = capa_helper->getRoutineManager();
}

void Capability::initialize()
{
    destroy_directive_by_agent = false;
    suspended = false;
    suspend_policy = SuspendPolicy::STOP;

    pimpl->ref_dialog_id = "";
    pimpl->cur_ndir = nullptr;
    pimpl->prev_ndir = nullptr;
    pimpl->referrer_events.clear();
    pimpl->referrer_dirs.clear();
    event_result_cbs.clear();
}

void Capability::deInitialize()
{
    initialized = false;
}

void Capability::setSuspendPolicy(SuspendPolicy policy)
{
    suspend_policy = policy;
}

void Capability::suspend()
{
}

void Capability::restore()
{
}

void Capability::addEventResultCallback(const std::string& ename, EventResultCallback callback)
{
    if (ename.size() == 0 || callback == nullptr)
        return;

    event_result_cbs[ename] = std::move(callback);
}

void Capability::removeEventResultCallback(const std::string& ename)
{
    event_result_cbs.erase(ename);
}

void Capability::notifyEventResult(const std::string& event_desc)
{
    std::string ename;
    std::string msg_id;
    std::string dialog_id;
    bool success;
    int code;

    if (pimpl->parseEventResultDesc(event_desc, ename, msg_id, dialog_id, success, code)) {
        if (event_result_cbs.find(ename) == event_result_cbs.end())
            nugu_warn("%s is not match in event result callback", ename.c_str());
        else
            event_result_cbs[ename](ename, msg_id, dialog_id, success, code);
    }
}

void Capability::notifyEventResponse(const std::string& msg_id, const std::string& data, bool success)
{
}

void Capability::addReferrerEvents(const std::string& ename, const std::string& dname)
{
    pimpl->referrer_events[ename] = dname;
}

void Capability::addBlockingPolicy(const std::string& dname, BlockingPolicy policy)
{
    if (directive_sequencer == nullptr) {
        nugu_error("directive sequencer is nullptr");
        return;
    }

    directive_sequencer->addPolicy(pimpl->cname, dname, policy);
}

std::string Capability::getReferrerDialogRequestId(const std::string& ename)
{
    if (pimpl->referrer_events.find(ename) == pimpl->referrer_events.end())
        return "";

    return pimpl->referrer_dirs[pimpl->referrer_events[ename]];
}

void Capability::setReferrerDialogRequestId(const std::string& dname, const std::string& id)
{
    pimpl->referrer_dirs[dname] = id;
}

void Capability::setName(const std::string& name)
{
    pimpl->cname = name;
}

std::string Capability::getName()
{
    return pimpl->cname;
}

void Capability::setVersion(const std::string& ver)
{
    pimpl->version = ver;
}

std::string Capability::getVersion()
{
    return pimpl->version;
}

std::string Capability::getPlayServiceIdInStackControl(const Json::Value& playstack_control)
{
    if (!playstack_control.empty() && playstack_control["type"].asString() == "PUSH")
        return playstack_control["playServiceId"].asString();

    return "";
}

std::string Capability::getPlayServiceIdInStackControl(const char* payload)
{
    if (!payload) {
        nugu_error("The payload is not exist.");
        return "";
    }

    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(payload, root)) {
        nugu_error("parsing error");
        return "";
    }

    return getPlayServiceIdInStackControl(root["playStackControl"]);
}

InteractionMode Capability::getInteractionMode(const Json::Value& interaction_control)
{
    if (!interaction_control.empty() && interaction_control["mode"].asString() == "MULTI_TURN")
        return InteractionMode::MULTI_TURN;

    return InteractionMode::NONE;
}

void Capability::preprocessDirective(NuguDirective* ndir)
{
}

void Capability::cancelDirective(NuguDirective* ndir)
{
}

void Capability::processDirective(NuguDirective* ndir)
{
    if (!ndir) {
        nugu_error("The directive is not exist.");
        return;
    }

    const char* dname;
    const char* message;

    message = nugu_directive_peek_json(ndir);
    dname = nugu_directive_peek_name(ndir);

    // handle previous dialog if exist
    if (pimpl->cur_ndir) {
        nugu_directive_remove_data_callback(pimpl->cur_ndir);

        if (playsync_manager->isConditionToHandlePrevDialog(pimpl->cur_ndir, ndir)) {
            pimpl->prev_ndir = pimpl->cur_ndir;
        } else {
            nugu_dbg("cancel previous dialog");
            directive_sequencer->cancel(nugu_directive_peek_dialog_id(pimpl->cur_ndir));
        }
    }

    pimpl->cur_ndir = ndir;

    if (!message || !dname) {
        nugu_error("directive message is not correct");
        destroyDirective(ndir);
        return;
    }

    setReferrerDialogRequestId(nugu_directive_peek_name(ndir), nugu_directive_peek_dialog_id(ndir));

    destroy_directive_by_agent = false;
    parsingDirective(dname, message);

    // if 'destroy_directive_by_agent' is set to true, the related agent should destroy it by self.
    if (!destroy_directive_by_agent)
        destroyDirective(ndir);
}

void Capability::destroyDirective(NuguDirective* ndir, bool is_cancel)
{
    if (pimpl->prev_ndir) {
        directive_sequencer->complete(pimpl->prev_ndir);
        pimpl->prev_ndir = nullptr;
    }

    if (pimpl->cur_ndir && ndir == pimpl->cur_ndir) {
        nugu_directive_remove_data_callback(pimpl->cur_ndir);

        if (routine_manager->isActionProgress(nugu_directive_peek_dialog_id(pimpl->cur_ndir)))
            routine_manager->finish();

        if (is_cancel) {
            std::string dialog_id = nugu_directive_peek_dialog_id(pimpl->cur_ndir);

            if (pimpl->cancel_policy.cancel_all) {
                directive_sequencer->cancel(dialog_id);
            } else {
                directive_sequencer->complete(pimpl->cur_ndir);
                directive_sequencer->cancel(dialog_id, pimpl->cancel_policy.dir_groups);
            }

        } else {
            directive_sequencer->complete(pimpl->cur_ndir);
        }

        pimpl->cur_ndir = nullptr;
    }
}

NuguDirective* Capability::getNuguDirective()
{
    return pimpl->cur_ndir;
}

void Capability::parsingDirective(const char* dname, const char* message)
{
    nugu_warn("%s[%s] is not support %s directive", getName().c_str(), getVersion().c_str(), dname);
}

bool Capability::getProperty(const std::string& property, std::string& value)
{
    value = "";

    return true;
}

bool Capability::getProperties(const std::string& property, std::list<std::string>& values)
{
    values.clear();

    return true;
}

std::string Capability::sendEvent(const std::string& name, const std::string& context, const std::string& payload, EventResultCallback cb)
{
    CapabilityEvent event(name, this);

    sendEvent(&event, context, payload, std::move(cb));

    return event.getDialogRequestId();
}

void Capability::sendEvent(CapabilityEvent* event, const std::string& context, const std::string& payload, EventResultCallback cb)
{
    if (!event)
        return;

    std::string ename = event->getName();
    if (cb == nullptr)
        addEventResultCallback(ename,
            [&](const std::string& ename, const std::string& msg_id, const std::string& dialog_id, bool success, int code) {
                nugu_warn("receive %s.%s(%s) result %d(code:%d) - ignored", getName().c_str(), ename.c_str(), msg_id.c_str(), success, code);
            });
    else
        addEventResultCallback(ename, std::move(cb));

    event->sendEvent(context, payload);
}

void Capability::sendAttachmentEvent(CapabilityEvent* event, bool is_end, size_t size, unsigned char* data)
{
    if (!event)
        return;

    event->sendAttachmentEvent(is_end, size, data);
}

bool Capability::Impl::parseEventResultDesc(const std::string& desc, std::string& ename, std::string& msg_id, std::string& dialog_id, bool& success, int& code)
{
    char* temp = (char*)desc.c_str();
    char* _cname = std::strtok(temp, ".");
    char* _ename = std::strtok(NULL, ".");
    char* _msg_id = std::strtok(NULL, ".");
    char* _dialog_id = std::strtok(NULL, ".");
    char* _success = std::strtok(NULL, ".");
    char* _code = std::strtok(NULL, ".");

    if (!temp || !_cname || !_ename || !_msg_id || !_dialog_id || !_success || !_code) {
        nugu_error("event description is something wrong");
        return false;
    }

    ename = _ename;
    dialog_id = _dialog_id;
    msg_id = _msg_id;
    success = std::stoi(_success) == 1 ? true : false;
    code = std::stoi(_code);

    return true;
}

void Capability::setCapabilityListener(ICapabilityListener* clistener)
{
}

void Capability::setCancelPolicy(DirectiveCancelPolicy&& cancel_policy)
{
    pimpl->cancel_policy = cancel_policy;
}

bool Capability::receiveCommand(const std::string& from, const std::string& command, const std::string& param)
{
    return true;
}

void Capability::receiveCommandAll(const std::string& command, const std::string& param)
{
}

std::string Capability::getContextInfo()
{
    Json::Value ctx;
    updateInfoForContext(ctx);

    return capa_helper->makeContextInfo(getName(), ctx);
}

void Capability::updateCompactContext(Json::Value& ctx)
{
    ctx[getName()]["version"] = getVersion();
}

ICapabilityHelper* Capability::getCapabilityHelper()
{
    return capa_helper;
}

} // NuguClientKit
