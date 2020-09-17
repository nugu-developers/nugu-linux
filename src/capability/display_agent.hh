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

#include "capability/display_interface.hh"
#include "clientkit/capability.hh"

namespace NuguCapability {

struct DisplayRenderInfo {
    std::string id;
    std::string type;
    std::string payload;
    std::string dialog_id;
    std::string ps_id;
    std::string token;
    bool close = false;
};

class DisplayAgent : public Capability,
                     public IDisplayHandler,
                     public IPlaySyncManagerListener {
public:
    DisplayAgent();
    virtual ~DisplayAgent();

    void initialize() override;
    void deInitialize() override;

    void preprocessDirective(NuguDirective* ndir) override;
    void parsingDirective(const char* dname, const char* message) override;
    void updateInfoForContext(Json::Value& ctx) override;
    void setCapabilityListener(ICapabilityListener* clistener) override;

    void displayRendered(const std::string& id) override;
    void displayCleared(const std::string& id) override;
    void elementSelected(const std::string& id, const std::string& item_token) override;
    void informControlResult(const std::string& id, ControlType type, ControlDirection direction) override;
    void setListener(IDisplayListener* listener) override;
    void removeListener(IDisplayListener* listener) override;
    void stopRenderingTimer(const std::string& id) override;

    void sendEventElementSelected(const std::string& item_token);
    void sendEventCloseSucceeded(const std::string& ps_id);
    void sendEventCloseFailed(const std::string& ps_id);
    void sendEventControlFocusSucceeded(const std::string& ps_id, ControlDirection direction);
    void sendEventControlFocusFailed(const std::string& ps_id, ControlDirection direction);
    void sendEventControlScrollSucceeded(const std::string& ps_id, ControlDirection direction);
    void sendEventControlScrollFailed(const std::string& ps_id, ControlDirection direction);

    // implements IPlaySyncManagerListener
    void onSyncState(const std::string& ps_id, PlaySyncState state, void* extra_data) override;
    void onDataChanged(const std::string& ps_id, std::pair<void*, void*> extra_datas) override;

private:
    void sendEventClose(const std::string& ename, const std::string& ps_id);
    void sendEventControl(const std::string& ename, const std::string& ps_id, ControlDirection direction);
    void parsingClose(const char* message);
    void parsingControlFocus(const char* message);
    void parsingControlScroll(const char* message);
    void parsingUpdate(const char* message);
    void parsingTemplates(const char* message);

    std::string getTemplateId(const std::string& ps_id);
    std::string getDirectionString(ControlDirection direction);

    void renderDisplay(void* data);
    void clearDisplay(void* data);

    std::map<std::string, DisplayRenderInfo*> render_info;
    IDisplayListener* display_listener;
    std::string disp_cur_ps_id;
    std::string disp_cur_token;
};

} // NuguCapability

#endif
