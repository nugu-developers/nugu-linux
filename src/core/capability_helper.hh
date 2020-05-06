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

#ifndef __NUGU_CAPABILITY_HELPER_H__
#define __NUGU_CAPABILITY_HELPER_H__

#include "clientkit/capability_helper_interface.hh"

namespace NuguCore {

using namespace NuguClientKit;

class CapabilityHelper : public ICapabilityHelper {
public:
    static CapabilityHelper* getInstance();
    static void destroyInstance();

    // implements ICapabilityHelper
    IPlaySyncManager* getPlaySyncManager() override;

    bool setMute(bool mute) override;
    void sendCommand(const std::string& from, const std::string& to, const std::string& command, const std::string& param) override;
    void requestEventResult(NuguEvent* event) override;
    void notifyEventResult(const char* msg_id, bool success, int code) override;
    void notifyEventResponse(const char* msg_id, const char* json, bool success) override;
    void suspendAll() override;
    void restoreAll() override;
    std::string getWakeupWord() override;
    void checkAndReleaseASRFocus(const std::string& groups, const std::string& dialog_id) override;
    void getCapabilityProperty(const std::string& cap, const std::string& property, std::string& value) override;
    void getCapabilityProperties(const std::string& cap, const std::string& property, std::list<std::string>& values) override;

    // about focus
    int addFocus(const std::string& fname, NuguFocusType type, IFocusListener* listener) override;
    int removeFocus(const std::string& fname) override;
    int releaseFocus(const std::string& fname) override;
    int requestFocus(const std::string& fname, void* event) override;
    bool isFocusOn(NuguFocusType type) override;

    // about context
    std::string makeContextInfo(const std::string& cname, Json::Value& ctx) override;
    std::string makeAllContextInfo() override;
    std::string makeAllContextInfoStack() override;

private:
    CapabilityHelper();
    virtual ~CapabilityHelper();

    static CapabilityHelper* instance;
};

} // NuguCore

#endif /* __NUGU_CAPABILITY_HELPER_H__ */
