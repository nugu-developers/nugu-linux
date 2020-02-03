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

#ifndef __NUGU_CORE_CONTAINER_H__
#define __NUGU_CORE_CONTAINER_H__

#include "clientkit/media_player_interface.hh"
#include "clientkit/network_manager_interface.hh"
#include "clientkit/wakeup_interface.hh"

namespace NuguCore {

using namespace NuguClientKit;

class NuguCoreContainer {
public:
    NuguCoreContainer() = delete;

    static IWakeupHandler* createWakeupHandler(const std::string& model_path = "");
    static INetworkManager* createNetworkManager();
    static IMediaPlayer* createMediaPlayer();
};

} // NuguCore

#endif /* __NUGU_CORE_CONTAINER_H__ */
