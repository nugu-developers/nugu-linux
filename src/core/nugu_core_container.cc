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

#include "media_player.hh"
#include "network_manager.hh"
#include "speech_recognizer.hh"
#include "wakeup_handler.hh"

#include "nugu_core_container.hh"

namespace NuguCore {

IWakeupHandler* NuguCoreContainer::createWakeupHandler(const std::string& model_path)
{
    return new WakeupHandler(model_path);
}

INetworkManager* NuguCoreContainer::createNetworkManager()
{
    return new NetworkManager();
}

IMediaPlayer* NuguCoreContainer::createMediaPlayer()
{
    return new MediaPlayer();
}

} // NuguCore
