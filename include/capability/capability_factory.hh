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

#ifndef __CAPABILITY_FACTORY_H__
#define __CAPABILITY_FACTORY_H__

#include <functional>
#include <list>

#include <clientkit/capability_interface.hh>

namespace NuguCapability {

using namespace NuguClientKit;

class ASRAgent;
class TTSAgent;
class AudioPlayerAgent;
class SystemAgent;
class DisplayAgent;
class ExtensionAgent;
class TextAgent;
class DelegationAgent;
class LocationAgent;
class SpeakerAgent;
class MicAgent;

class CapabilityFactory {
public:
    CapabilityFactory() = delete;

    struct Element {
        std::string name;
        bool is_default;
        std::function<ICapabilityInterface*()> creator;
    };

    static const std::list<Element>& getCapabilityList();

    template <typename T, typename V, typename... Ts>
    static V* makeCapability(Ts&&... params);
};

}

#endif /* __CAPABILITY_FACTORY_H__ */
