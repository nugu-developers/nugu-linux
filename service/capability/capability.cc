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
Capability::Capability(CapabilityType type, const std::string& ver)
    : ctype(type)
    , version(ver)
{
    cname = getTypeName(type);
    playsync_manager = CapabilityManager::getInstance()->getPlaySyncManager();
}

Capability::~Capability()
{
    if (!observers.empty())
        observers.clear();
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
    std::string _name;

    switch (type) {
    case CapabilityType::AudioPlayer:
        _name = "AudioPlayer";
        break;
    case CapabilityType::Display:
        _name = "Display";
        break;
    case CapabilityType::System:
        _name = "System";
        break;
    case CapabilityType::TTS:
        _name = "TTS";
        break;
    case CapabilityType::ASR:
        _name = "ASR";
        break;
    case CapabilityType::Text:
        _name = "Text";
        break;
    case CapabilityType::Extension:
        _name = "Extension";
        break;
    }
    return _name;
}

void Capability::setVersion(const std::string& ver)
{
    version = ver;
}

std::string Capability::getVersion()
{
    return version;
}

void Capability::destoryDirective(NuguDirective* ndir)
{
    nugu_dirseq_complete(ndir);
}

void Capability::getProperty(const std::string& property, std::string& value)
{
    value = "";
}

void Capability::getProperties(const std::string& property, std::list<std::string>& values)
{
    values.clear();
}

void Capability::sendEvent(NuguEvent* event, bool is_end, size_t size,
    unsigned char* data)
{
    g_return_if_fail(event != NULL);

    if ((size && data) || is_end) {
        nugu_network_manager_send_event_data(event, is_end, size, data);
    } else {
        nugu_network_manager_send_event(event);
        // dialog_request_id is made every time to send event
        notifyObservers(DIALOG_REQUEST_ID, (void*)nugu_event_peek_dialog_id(event));
    }
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
