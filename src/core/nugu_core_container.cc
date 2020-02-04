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
#include "media_player.hh"
#include "network_manager.hh"
#include "speech_recognizer.hh"
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
    // TODO : It needs guarantee related plugin is loaded.

    return new WakeupHandler(model_path);
}

ISpeechRecognizer* NuguCoreContainer::createSpeechRecognizer(const std::string& model_path)
{
    // TODO : It needs guarantee related plugin is loaded.

    SpeechRecognizer::Attribute sr_attribute;
    sr_attribute.model_path = model_path;

    return new SpeechRecognizer(std::move(sr_attribute));
}

IMediaPlayer* NuguCoreContainer::createMediaPlayer()
{
    // TODO : It needs guarantee related plugin is loaded.

    return new MediaPlayer();
}

ICapabilityHelper* NuguCoreContainer::getCapabilityHelper()
{
    return CapabilityHelper::getInstance();
}

} // NuguCore
