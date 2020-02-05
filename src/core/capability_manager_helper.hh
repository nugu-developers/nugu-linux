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

#ifndef __NUGU_CAPABILITY_MANAGER_HELPER_H__
#define __NUGU_CAPABILITY_MANAGER_HELPER_H__

#include "clientkit/capability_interface.hh"

namespace NuguCore {

using namespace NuguClientKit;

class CapabilityManagerHelper {
public:
    CapabilityManagerHelper() = delete;

    static void setWakeupWord(const std::string& wakeup_word);
    static void addCapability(const std::string& cname, ICapabilityInterface* cap);
    static void removeCapability(const std::string& cname);
    static void destroyInstance();
};
} // NuguCore

#endif /* __NUGU_CAPABILITY_MANAGER_HELPER_H__ */
