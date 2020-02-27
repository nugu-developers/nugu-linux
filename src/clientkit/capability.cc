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

#include <string.h>

#include "base/nugu_directive_sequencer.h"
#include "base/nugu_log.h"

#include "clientkit/capability.hh"

namespace NuguClientKit {

struct Capability::Impl {
    std::string cname;
    std::string version;
    std::string ref_dialog_id;
    NuguDirective* cur_ndir = nullptr;
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

    if (isUserAction(name))
        cap->setReferrerDialogRequestId("");
}

CapabilityEvent::~CapabilityEvent()
{
    if (pimpl->event) {
        nugu_event_free(pimpl->event);
        pimpl->event = nullptr;
    }
}

bool CapabilityEvent::isUserAction(const std::string& name)
{
    const std::string& cname = pimpl->capability->getName();

    if ((cname == "ASR" && name == "Recognize")
        || (cname == "TTS" && name == "SpeechPlay")
        || (cname == "AudioPlayer" && (name.find("CommandIssued") != std::string::npos))
        || (cname == "Text" && name == "TextInput")
        || (cname == "Display" && name == "ElementSelected"))
        return true;
    else
        return false;
}

std::string CapabilityEvent::getDialogMessageId()
{
    return pimpl->dialog_id;
}

void CapabilityEvent::setDialogMessageId(const std::string& id)
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

void CapabilityEvent::sendEvent(const std::string& context, const std::string& payload, bool is_sync)
{
    if (pimpl->event == NULL) {
        nugu_error("event is NULL");
        return;
    }

    if (context.size())
        nugu_event_set_context(pimpl->event, context.c_str());

    if (payload.size())
        nugu_event_set_json(pimpl->event, payload.c_str());

    std::string ref_dialog_id = pimpl->capability->getReferrerDialogRequestId();
    if (ref_dialog_id.size())
        nugu_event_set_referrer_id(pimpl->event, ref_dialog_id.c_str());

    pimpl->capability->getCapabilityHelper()->sendCommand(pimpl->capability->getName(), "System", "activity", "");

    if (is_sync)
        nugu_network_manager_send_event(pimpl->event, 1);
    else
        nugu_network_manager_send_event(pimpl->event, 0);
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
}

void Capability::setNuguCoreContainer(INuguCoreContainer* core_container)
{
    this->core_container = core_container;
    capa_helper = core_container->getCapabilityHelper();
    playsync_manager = capa_helper->getPlaySyncManager();
}

void Capability::initialize()
{
}

void Capability::deInitialize()
{
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

std::string Capability::getReferrerDialogRequestId()
{
    return pimpl->ref_dialog_id;
}

void Capability::setReferrerDialogRequestId(const std::string& id)
{
    pimpl->ref_dialog_id = id;
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
    std::string playstack_ps_id;

    if (!playstack_control.isNull() && !playstack_control.empty()) {
        std::string playstack_type = playstack_control["type"].asString();

        if (playstack_type.size() > 0 && playstack_type == "PUSH") {
            playstack_ps_id = playstack_control["playServiceId"].asString();
        }
    }

    return playstack_ps_id;
}

void Capability::processDirective(NuguDirective* ndir)
{
    if (ndir) {
        const char* dname;
        const char* message;
        const char* dref_id;

        message = nugu_directive_peek_json(ndir);
        dname = nugu_directive_peek_name(ndir);

        pimpl->cur_ndir = ndir;

        if (!message || !dname) {
            nugu_error("directive message is not correct");
            destroyDirective(ndir);
            return;
        }

        dref_id = nugu_directive_peek_referrer_id(ndir);
        if (!dref_id || !strlen(dref_id))
            dref_id = nugu_directive_peek_dialog_id(ndir);

        setReferrerDialogRequestId(dref_id);

        has_attachment = false;
        parsingDirective(dname, message);

        // the directive with attachment should destroy by agent
        if (!has_attachment)
            destroyDirective(ndir);
    }
}

void Capability::destroyDirective(NuguDirective* ndir)
{
    if (ndir == pimpl->cur_ndir)
        pimpl->cur_ndir = NULL;

    nugu_dirseq_complete(ndir);
}

NuguDirective* Capability::getNuguDirective()
{
    return pimpl->cur_ndir;
}

void Capability::parsingDirective(const char* dname, const char* message)
{
    nugu_warn("%s[%s] is not support %s directive", getName().c_str(), getVersion().c_str(), dname);
    destroyDirective(pimpl->cur_ndir);
}

void Capability::getProperty(const std::string& property, std::string& value)
{
    value = "";
}

void Capability::getProperties(const std::string& property, std::list<std::string>& values)
{
    values.clear();
}

void Capability::sendEvent(const std::string& name, const std::string& context, const std::string& payload, bool is_sync)
{
    CapabilityEvent event(name, this);

    sendEvent(&event, context, payload, is_sync);
}

void Capability::sendEvent(CapabilityEvent* event, const std::string& context, const std::string& payload, bool is_sync)
{
    if (!event)
        return;

    event->sendEvent(context, payload, is_sync);
}

void Capability::sendAttachmentEvent(CapabilityEvent* event, bool is_end, size_t size, unsigned char* data)
{
    if (!event)
        return;

    event->sendAttachmentEvent(is_end, size, data);
}

void Capability::setCapabilityListener(ICapabilityListener* clistener)
{
}

void Capability::receiveCommand(const std::string& from, const std::string& command, const std::string& param)
{
}

void Capability::receiveCommandAll(const std::string& command, const std::string& param)
{
}

std::string Capability::getContextInfo()
{
    Json::Value ctx;
    updateInfoForContext(ctx);

    return capa_helper->makeContextInfo(ctx);
}

ICapabilityHelper* Capability::getCapabilityHelper()
{
    return capa_helper;
}

} // NuguClientKit
