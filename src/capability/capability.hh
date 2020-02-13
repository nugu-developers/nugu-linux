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
#include <string>
#include <vector>

#include "base/nugu_event.h"
#include "base/nugu_network_manager.h"
#include "clientkit/capability_interface.hh"

namespace NuguCapability {

using namespace NuguClientKit;

class Capability;
class CapabilityEvent {
public:
    CapabilityEvent(const std::string& name, Capability* cap);
    virtual ~CapabilityEvent();

    bool isUserAction(const std::string& name);

    std::string getDialogMessageId();
    void setDialogMessageId(const std::string& id);
    void setType(enum nugu_event_type type);
    void forceClose();
    void sendEvent(const std::string& context, const std::string& payload);
    void sendAttachmentEvent(bool is_end, size_t size, unsigned char* data);

private:
    Capability* capability;
    NuguEvent* event;
    std::string dialog_id;
};

class Capability : virtual public ICapabilityInterface {
public:
    Capability(const std::string& name, const std::string& ver = "1.0");
    virtual ~Capability();

    void setNuguCoreContainer(INuguCoreContainer* core_container) override;
    void initialize() override;
    void deInitialize() override;

    std::string getReferrerDialogRequestId();
    void setReferrerDialogRequestId(const std::string& id);
    void setName(const std::string& name);
    std::string getName() override;
    void setVersion(const std::string& ver);
    std::string getVersion() override;
    std::string getPlayServiceIdInStackControl(const Json::Value& playstack_control);

    void processDirective(NuguDirective* ndir) override;
    void destoryDirective(NuguDirective* ndir);

    NuguDirective* getNuguDirective();

    void sendEvent(const std::string& name, const std::string& context, const std::string& payload);
    void sendEvent(CapabilityEvent* event, const std::string& context, const std::string& payload);
    void sendAttachmentEvent(CapabilityEvent* event, bool is_end, size_t size, unsigned char* data);

    void getProperty(const std::string& property, std::string& value) override;
    void getProperties(const std::string& property, std::list<std::string>& values) override;
    void setCapabilityListener(ICapabilityListener* clistener) override;
    void receiveCommand(const std::string& from, const std::string& command, const std::string& param) override;
    void receiveCommandAll(const std::string& command, const std::string& param) override;
    virtual void parsingDirective(const char* dname, const char* message);
    virtual std::string getContextInfo();
    ICapabilityHelper* getCapabilityHelper();

protected:
    bool initialized = false;
    bool has_attachment = false;

    INuguCoreContainer* core_container = nullptr;
    ICapabilityHelper* capa_helper = nullptr;
    IPlaySyncManager* playsync_manager = nullptr;

private:
    std::string cname;
    std::string version;
    std::string ref_dialog_id;
    NuguDirective* cur_ndir;
};

} // NuguCapability

#endif
