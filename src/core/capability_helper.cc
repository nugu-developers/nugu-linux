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

#include "capability_helper.hh"
#include "audio_recorder_manager.hh"
#include "capability_manager.hh"

namespace NuguCore {

CapabilityHelper* CapabilityHelper::instance = nullptr;

CapabilityHelper::CapabilityHelper()
{
}

CapabilityHelper::~CapabilityHelper()
{
}

CapabilityHelper* CapabilityHelper::getInstance()
{
    if (!instance) {
        instance = new CapabilityHelper();
    }

    return instance;
}

void CapabilityHelper::destroyInstance()
{
    if (instance) {
        delete instance;
        instance = nullptr;
    }
}

IPlaySyncManager* CapabilityHelper::getPlaySyncManager()
{
    return CapabilityManager::getInstance()->getPlaySyncManager();
}

IFocusManager* CapabilityHelper::getFocusManager()
{
    return CapabilityManager::getInstance()->getFocusManager();
}

ISessionManager* CapabilityHelper::getSessionManager()
{
    return CapabilityManager::getInstance()->getSessionManager();
}

IInteractionControlManager* CapabilityHelper::getInteractionControlManager()
{
    return CapabilityManager::getInstance()->getInteractionControlManager();
}

IDirectiveSequencer* CapabilityHelper::getDirectiveSequencer()
{
    return CapabilityManager::getInstance()->getDirectiveSequencer();
}

bool CapabilityHelper::setMute(bool mute)
{
    return AudioRecorderManager::getInstance()->setMute(mute);
}

bool CapabilityHelper::sendCommand(const std::string& from, const std::string& to, const std::string& command, const std::string& param)
{
    return CapabilityManager::getInstance()->sendCommand(from, to, command, param);
}

void CapabilityHelper::requestEventResult(NuguEvent* event)
{
    CapabilityManager::getInstance()->requestEventResult(event);
}

void CapabilityHelper::suspendAll()
{
    CapabilityManager::getInstance()->suspendAll();
}

void CapabilityHelper::restoreAll()
{
    CapabilityManager::getInstance()->restoreAll();
}

std::string CapabilityHelper::getWakeupWord()
{
    return CapabilityManager::getInstance()->getWakeupWord();
}

bool CapabilityHelper::getCapabilityProperty(const std::string& cap, const std::string& property, std::string& value)
{
    return CapabilityManager::getInstance()->getCapabilityProperty(cap, property, value);
}

bool CapabilityHelper::getCapabilityProperties(const std::string& cap, const std::string& property, std::list<std::string>& values)
{
    return CapabilityManager::getInstance()->getCapabilityProperties(cap, property, values);
}

std::string CapabilityHelper::makeContextInfo(const std::string& cname, Json::Value& ctx)
{
    return CapabilityManager::getInstance()->makeContextInfo(cname, ctx);
}

std::string CapabilityHelper::makeAllContextInfo()
{
    return CapabilityManager::getInstance()->makeAllContextInfo();
}

} // NuguCore
