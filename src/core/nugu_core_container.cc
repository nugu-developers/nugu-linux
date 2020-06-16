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

#include "base/nugu_log.h"
#include "capability_helper.hh"
#include "capability_manager.hh"
#include "media_player.hh"
#include "network_manager.hh"
#include "nugu_timer.hh"
#include "speech_recognizer.hh"
#include "tts_player.hh"
#include "wakeup_handler.hh"

#include "nugu_core_container.hh"

namespace NuguCore {

NuguCoreContainer::~NuguCoreContainer()
{
    CapabilityHelper::destroyInstance();
}

INetworkManager* NuguCoreContainer::createNetworkManager()
{
    return new NetworkManager();
}

IWakeupHandler* NuguCoreContainer::createWakeupHandler(const std::string& model_path)
{
    return new WakeupHandler(model_path);
}

ISpeechRecognizer* NuguCoreContainer::createSpeechRecognizer(const std::string& model_path, const EpdAttribute& epd_attr)
{
    SpeechRecognizer::Attribute sr_attribute;

    sr_attribute.model_path = model_path;

    if (epd_attr.epd_timeout > 0)
        sr_attribute.epd_timeout = epd_attr.epd_timeout;

    if (epd_attr.epd_max_duration > 0)
        sr_attribute.epd_max_duration = epd_attr.epd_max_duration;

    return new SpeechRecognizer(std::move(sr_attribute));
}

IMediaPlayer* NuguCoreContainer::createMediaPlayer()
{
    return new MediaPlayer();
}

ITTSPlayer* NuguCoreContainer::createTTSPlayer()
{
    return new TTSPlayer();
}

INuguTimer* NuguCoreContainer::createNuguTimer()
{
    return new NUGUTimer();
}

ICapabilityHelper* NuguCoreContainer::getCapabilityHelper()
{
    return CapabilityHelper::getInstance();
}

void NuguCoreContainer::setWakeupWord(const std::string& wakeup_word)
{
    if (!wakeup_word.empty())
        CapabilityManager::getInstance()->setWakeupWord(wakeup_word);
}

void NuguCoreContainer::addCapability(const std::string& cname, ICapabilityInterface* cap)
{
    CapabilityManager::getInstance()->addCapability(cname, cap);
}

void NuguCoreContainer::removeCapability(const std::string& cname)
{
    CapabilityManager::getInstance()->removeCapability(cname);
}

void NuguCoreContainer::destroyInstance()
{
    CapabilityManager::destroyInstance();
}

INetworkManagerListener* NuguCoreContainer::getNetworkManagerListener()
{
    return dynamic_cast<INetworkManagerListener*>(CapabilityManager::getInstance());
}

void NuguCoreContainer::createAudioRecorderManager()
{
    AudioRecorderManager::getInstance();
}

void NuguCoreContainer::destoryAudioRecorderManager()
{
    AudioRecorderManager::destroyInstance();
}

} // NuguCore
