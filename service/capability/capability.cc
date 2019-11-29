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

#include <glib.h>
#include <stdlib.h>
#include <string.h>

#include "capability.hh"
#include "capability_manager.hh"
#include "nugu_directive_sequencer.h"
#include "nugu_log.h"

namespace NuguCore {

CapabilityEvent::CapabilityEvent(const std::string& name, Capability* cap)
{
    capability = cap;

    event = nugu_event_new(cap->getName().c_str(), name.c_str(), cap->getVersion().c_str());
    dialog_id = nugu_event_peek_dialog_id(event);

    if (isUserAction(name))
        cap->setReferrerDialoRequestId(dialog_id);
}

CapabilityEvent::~CapabilityEvent()
{
    if (event) {
        nugu_event_free(event);
        event = nullptr;
    }
}

bool CapabilityEvent::isUserAction(const std::string& name)
{
    CapabilityType type = capability->getType();

    if ((type == CapabilityType::ASR && name == "Recognize")
        || (type == CapabilityType::TTS && name == "SpeechPlay")
        || (type == CapabilityType::AudioPlayer
            && (name.find("CommandIssued") != std::string::npos))
        || (type == CapabilityType::Text && name == "TextInput")
        || (type == CapabilityType::Display && name == "ElementSelected")
        || (type == CapabilityType::Permission && name == "RequestAccessCompleted"))
        return true;
    else
        return false;
}

std::string CapabilityEvent::getDialogMessageId()
{
    return dialog_id;
}

void CapabilityEvent::setDialogMessageId(const std::string& id)
{
    dialog_id = id;
    nugu_event_set_dialog_id(event, dialog_id.c_str());
}

void CapabilityEvent::sendEvent(const std::string& context, const std::string& payload)
{
    g_return_if_fail(event != NULL);

    if (context.size())
        nugu_event_set_context(event, context.c_str());

    if (payload.size())
        nugu_event_set_json(event, payload.c_str());

    std::string ref_dialog_id = capability->getReferrerDialoRequestId();
    if (ref_dialog_id.size())
        nugu_event_set_referrer_id(event, ref_dialog_id.c_str());

    CapabilityManager* cap_manager = CapabilityManager::getInstance();
    cap_manager->sendCommand(capability->getType(), CapabilityType::System, "activity", "");

    nugu_network_manager_send_event(event);
    // dialog_request_id is made every time to send event
    capability->notifyObservers(DIALOG_REQUEST_ID, (void*)dialog_id.c_str());
}

void CapabilityEvent::sendAttachmentEvent(bool is_end, size_t size, unsigned char* data)
{
    g_return_if_fail(event != NULL);

    if ((size && data) || is_end)
        nugu_network_manager_send_event_data(event, is_end, size, data);
}

Capability::Capability(CapabilityType type, const std::string& ver)
    : ctype(type)
    , version(ver)
    , ref_dialog_id("")
    , cur_ndir(NULL)
{
    cname = getTypeName(type);
    playsync_manager = CapabilityManager::getInstance()->getPlaySyncManager();
}

Capability::~Capability()
{
    if (!observers.empty())
        observers.clear();
}

void Capability::initialize()
{
}

std::string Capability::getReferrerDialoRequestId()
{
    return ref_dialog_id;
}

void Capability::setReferrerDialoRequestId(const std::string& id)
{
    if (id.size())
        ref_dialog_id = id;
}

void Capability::setName(CapabilityType type)
{
    cname = getTypeName(type);
}

std::string Capability::getName()
{
    return cname;
}

CapabilityType Capability::getType()
{
    return ctype;
}

std::string Capability::getTypeName(CapabilityType type)
{
    return CAPABILITY_TYPE_MAP.at(type);
}

void Capability::setVersion(const std::string& ver)
{
    version = ver;
}

std::string Capability::getVersion()
{
    return version;
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

        message = nugu_directive_peek_json(ndir);
        dname = nugu_directive_peek_name(ndir);

        cur_ndir = ndir;

        if (!message || !dname) {
            nugu_error("directive message is not correct");
            destoryDirective(ndir);
            return;
        }

        setReferrerDialoRequestId(nugu_directive_peek_referrer_id(ndir));

        parsingDirective(dname, message);

        // the directive with attachment should destroy by agent
        if (!hasDirectiveAttachment(dname))
            destoryDirective(ndir);
    }
}

void Capability::destoryDirective(NuguDirective* ndir)
{
    if (ndir == cur_ndir)
        cur_ndir = NULL;

    nugu_dirseq_complete(ndir);
}

NuguDirective* Capability::getNuguDirective()
{
    return cur_ndir;
}

bool Capability::hasDirectiveAttachment(const char* dname)
{
    if (getType() == CapabilityType::TTS && !strcmp(dname, "Speak"))
        return true;
    else
        return false;
}

void Capability::parsingDirective(const char* dname, const char* message)
{
    nugu_warn("%s[%s] is not support %s directive", getName().c_str(), getVersion().c_str(), dname);
    destoryDirective(cur_ndir);
}

void Capability::getProperty(const std::string& property, std::string& value)
{
    value = "";
}

void Capability::getProperties(const std::string& property, std::list<std::string>& values)
{
    values.clear();
}

void Capability::sendEvent(const std::string& name, const std::string& context, const std::string& payload)
{
    CapabilityEvent event(name, this);

    sendEvent(&event, context, payload);
}

void Capability::sendEvent(CapabilityEvent* event, const std::string& context, const std::string& payload)
{
    if (!event)
        return;

    event->sendEvent(context, payload);
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

void Capability::receiveCommand(CapabilityType from, std::string command, const std::string& param)
{
}

void Capability::receiveCommandAll(std::string command, const std::string& param)
{
}

std::string Capability::getContextInfo()
{
    Json::Value ctx;
    updateInfoForContext(ctx);

    return CapabilityManager::getInstance()->makeContextInfo(ctx);
}

/********************************************************************
 * implements ICapabilityObservable
 ********************************************************************/
void Capability::registerObserver(ICapabilityObserver* observer)
{
    auto iterator = std::find(observers.begin(), observers.end(), observer);

    if (iterator == observers.end())
        observers.push_back(observer);
}

void Capability::removeObserver(ICapabilityObserver* observer)
{
    auto iterator = std::find(observers.begin(), observers.end(), observer);

    if (iterator != observers.end())
        observers.erase(iterator);
}

void Capability::notifyObservers(CapabilitySignal signal, void* data)
{
    for (auto observer : observers)
        observer->notify(getName(), signal, data);
}

} // NuguCore
