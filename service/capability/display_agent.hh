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

#ifndef __NUGU_DISPLAY_AGENT_H__
#define __NUGU_DISPLAY_AGENT_H__

#include <interface/capability/display_interface.hh>

#include "capability.hh"

namespace NuguCore {

using namespace NuguInterface;

class DisplayAgent : public Capability, public IDisplayHandler, public IPlaySyncManagerListener {
public:
    DisplayAgent();
    virtual ~DisplayAgent();
    void initialize() override;

    void processDirective(NuguDirective* ndir) override;
    void updateInfoForContext(Json::Value& ctx) override;
    std::string getContextInfo();
    void setCapabilityListener(ICapabilityListener* clistener) override;

    // implement handler
    void displayRendered(const std::string& token) override;
    void displayCleared() override;
    void elementSelected(const std::string& item_token) override;

    // implement IContextManagerListener
    void onSyncContext(const std::string& ps_id, std::pair<std::string, std::string> render_info) override;
    void onReleaseContext(const std::string& ps_id) override;

private:
    void sendEventElementSelected(const std::string& item_token);

    IDisplayListener* display_listener;
    std::string ps_id;
    std::string cur_token;
};

} // NuguCore

#endif
