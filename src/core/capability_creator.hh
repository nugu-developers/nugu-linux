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

#ifndef __CAPABILITY_CREATOR_H__
#define __CAPABILITY_CREATOR_H__

#include <functional>
#include <list>

#include "interface/capability/capability_interface.hh"
#include "interface/network_manager_interface.hh"
#include "interface/wakeup_interface.hh"

namespace NuguCore {

using namespace NuguInterface;

class CapabilityCreator {
public:
    CapabilityCreator() = delete;

    static IWakeupHandler* createWakeupHandler();
    static INetworkManager* createNetworkManager();

    struct Element {
        std::string name;
        bool is_default;
        std::function<ICapabilityInterface*()> creator;
    };

    static const std::list<Element>& getCapabilityList();
};

} // NuguCore

#endif /* __CAPABILITY_CREATOR_H__ */
