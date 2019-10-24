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

#ifndef __NUGU_CAPABILITY_H__
#define __NUGU_CAPABILITY_H__

#include <algorithm>
#include <deque>
#include <list>
#include <map>
#include <string>
#include <vector>

#include <core/nugu_event.h>
#include <core/nugu_network_manager.h>
#include <interface/capability/capability_interface.hh>

#include "playsync_manager.hh"

namespace NuguCore {

using namespace NuguInterface;

class Capability : public ICapabilityInterface {
public:
    Capability(CapabilityType type, const std::string& ver = "1.0");
    virtual ~Capability();
    virtual void initialize() = 0;

    std::string getTypeName(CapabilityType type) override;
    CapabilityType getType() override;
    void setName(CapabilityType type);
    std::string getName();
    void setVersion(const std::string& ver);
    std::string getVersion();

    void destoryDirective(NuguDirective* ndir);
    void sendEvent(NuguEvent* event, bool is_end = false, size_t size = 0, unsigned char* data = NULL);

    virtual void getProperty(const std::string& property, std::string& value) override;
    virtual void getProperties(const std::string& property, std::list<std::string>& values) override;
    virtual void setCapabilityListener(ICapabilityListener* clistener) override;
    virtual void receiveCommand(CapabilityType from, std::string command, const std::string& param) override;
    virtual void receiveCommandAll(std::string command, const std::string& param) override;

    // implements ICapabilityObservable
    void registerObserver(ICapabilityObserver* observer) override;
    void removeObserver(ICapabilityObserver* observer) override;
    void notifyObservers(CapabilitySignal signal, void* data) override;

protected:
    bool initialized = false;
    PlaySyncManager* playsync_manager = nullptr;

private:
    CapabilityType ctype;
    std::string cname;
    std::string version;
    std::vector<ICapabilityObserver*> observers;
};

} // NuguCore

#endif
