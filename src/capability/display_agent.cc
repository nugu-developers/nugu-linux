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

#include <string.h>
#include <sys/time.h>

#include "base/nugu_log.h"
#include "display_agent.hh"

namespace NuguCapability {

static const char* CAPABILITY_NAME = "Display";
static const char* CAPABILITY_VERSION = "1.6";

DisplayAgent::DisplayAgent()
    : Capability(CAPABILITY_NAME, CAPABILITY_VERSION)
    , display_listener(nullptr)
{
}

void DisplayAgent::initialize()
{
    if (initialized) {
        nugu_info("It's already initialized.");
        return;
    }

    Capability::initialize();

    disp_cur_ps_id = "";
    disp_cur_token = "";

    playsync_manager->addListener(getName(), this);

    initialized = true;
}

void DisplayAgent::deInitialize()
{
    playsync_manager->removeListener(getName());

    for (auto info : render_info)
        delete info.second;

    render_info.clear();

    initialized = false;
}

void DisplayAgent::preprocessDirective(NuguDirective* ndir)
{
    const char* dname;
    const char* message;

    if (!ndir
        || !(dname = nugu_directive_peek_name(ndir))
        || !(message = nugu_directive_peek_json(ndir))) {
        nugu_error("The directive info is not exist.");
        return;
    }

    if (strcmp(dname, "Action") && strcmp(dname, "ControlFocus") && strcmp(dname, "ControlScroll") && strcmp(dname, "Update"))
        playsync_manager->prepareSync(getPlayServiceIdInStackControl(message), ndir);
}

void DisplayAgent::parsingDirective(const char* dname, const char* message)
{
    Json::Value root;
    Json::Reader reader;

    nugu_dbg("message: %s", message);

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    if (!strcmp(dname, "Close")) {
        parsingClose(message);
    } else if (!strcmp(dname, "ControlFocus")) {
        parsingControlFocus(message);
    } else if (!strcmp(dname, "ControlScroll")) {
        parsingControlScroll(message);
    } else if (!strcmp(dname, "Update")) {
        parsingUpdate(message);
    } else {
        parsingTemplates(message);
    }
}

void DisplayAgent::updateInfoForContext(Json::Value& ctx)
{
    Json::Value display;

    display["version"] = getVersion();
    if (disp_cur_token.size()) {
        display["playServiceId"] = disp_cur_ps_id;
        display["token"] = disp_cur_token;
    }

    ctx[getName()] = display;
}

void DisplayAgent::setCapabilityListener(ICapabilityListener* listener)
{
    if (listener)
        setListener(dynamic_cast<IDisplayListener*>(listener));
}

void DisplayAgent::displayRendered(const std::string& id)
{
    if (render_info.find(id) == render_info.end()) {
        nugu_warn("There is no render infor : %s", id.c_str());
        return;
    }

    disp_cur_token = render_info[id]->token;
    disp_cur_ps_id = render_info[id]->ps_id;
}

void DisplayAgent::displayCleared(const std::string& id)
{
    if (render_info.find(id) == render_info.end()) {
        nugu_warn("There is no render infor : %s", id.c_str());
        return;
    }

    if (render_info[id]->close)
        sendEventCloseSucceeded(render_info[id]->ps_id);

    delete render_info[id];
    render_info.erase(id);

    disp_cur_token = disp_cur_ps_id = "";
}

void DisplayAgent::elementSelected(const std::string& id, const std::string& item_token)
{
    if (render_info.find(id) == render_info.end()) {
        nugu_warn("There is no render infor : %s", id.c_str());
        return;
    }

    disp_cur_token = render_info[id]->token;
    disp_cur_ps_id = render_info[id]->ps_id;

    sendEventElementSelected(item_token);
}

void DisplayAgent::informControlResult(const std::string& id, ControlType type, ControlDirection direction)
{
    std::string ps_id;

    if (render_info.find(id) == render_info.end()) {
        nugu_warn("the template(id: %s) is invalid", id.c_str());
        return;
    }

    ps_id = render_info[id]->ps_id;

    if (type == ControlType::Scroll)
        sendEventControlScrollSucceeded(ps_id, direction);
    else
        sendEventControlFocusSucceeded(ps_id, direction);
}

void DisplayAgent::setListener(IDisplayListener* listener)
{
    if (!listener)
        return;

    display_listener = listener;
}

void DisplayAgent::removeListener(IDisplayListener* listener)
{
    if (!display_listener && (display_listener == listener))
        display_listener = nullptr;
}

void DisplayAgent::stopRenderingTimer(const std::string& id)
{
    // TBD
}

void DisplayAgent::sendEventElementSelected(const std::string& item_token)
{
    std::string ename = "ElementSelected";
    std::string payload = "";
    Json::StyledWriter writer;
    Json::Value root;

    root["playServiceId"] = disp_cur_ps_id;
    root["token"] = item_token;
    // TBD: postback
    payload = writer.write(root);

    sendEvent(ename, getContextInfo(), payload);
}

void DisplayAgent::sendEventCloseSucceeded(const std::string& ps_id)
{
    sendEventClose("CloseSucceeded", ps_id);
}

void DisplayAgent::sendEventCloseFailed(const std::string& ps_id)
{
    sendEventClose("CloseFailed", ps_id);
}
void DisplayAgent::sendEventClose(const std::string& ename, const std::string& ps_id)
{
    std::string payload = "";
    Json::StyledWriter writer;
    Json::Value root;

    root["playServiceId"] = ps_id;
    payload = writer.write(root);

    sendEvent(ename, getContextInfo(), payload);
}

void DisplayAgent::sendEventControlFocusSucceeded(const std::string& ps_id, ControlDirection direction)
{
    sendEventControl("ControlFocusSucceeded", ps_id, direction);
}

void DisplayAgent::sendEventControlFocusFailed(const std::string& ps_id, ControlDirection direction)
{
    sendEventControl("ControlFocusFailed", ps_id, direction);
}

void DisplayAgent::sendEventControlScrollSucceeded(const std::string& ps_id, ControlDirection direction)
{
    sendEventControl("ControlScrollSucceeded", ps_id, direction);
}

void DisplayAgent::sendEventControlScrollFailed(const std::string& ps_id, ControlDirection direction)
{
    sendEventControl("ControlScrollFailed", ps_id, direction);
}

void DisplayAgent::sendEventControl(const std::string& ename, const std::string& ps_id, ControlDirection direction)
{
    std::string payload = "";
    Json::StyledWriter writer;
    Json::Value root;

    root["playServiceId"] = ps_id;
    root["direction"] = getDirectionString(direction);
    payload = writer.write(root);

    sendEvent(ename, getContextInfo(), payload);
}

void DisplayAgent::parsingClose(const char* message)
{
    Json::Value root;
    Json::Reader reader;
    std::string ps_id;
    std::string template_id;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    if (root["playServiceId"].empty()) {
        nugu_error("The required parameters are not set");
        return;
    }

    ps_id = root["playServiceId"].asString();
    template_id = getTemplateId(ps_id);
    if (template_id.empty()) {
        nugu_error("The template is invalid (ps_id: %s)", ps_id.c_str());
        sendEventCloseFailed(ps_id);
        return;
    }

    playsync_manager->releaseSyncImmediately(getPlayServiceIdInStackControl(root["playStackControl"]), getName());
}

void DisplayAgent::parsingControlFocus(const char* message)
{
    Json::Value root;
    Json::Reader reader;
    std::string ps_id;
    std::string template_id;
    ControlDirection direction;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    if (root["playServiceId"].empty() || root["direction"].empty()) {
        nugu_error("The required parameters are not set");
        return;
    }

    if (root["direction"].asString() == "PREVIOUS")
        direction = ControlDirection::PREVIOUS;
    else
        direction = ControlDirection::NEXT;

    ps_id = root["playServiceId"].asString();
    template_id = getTemplateId(ps_id);
    if (template_id.empty()) {
        nugu_error("The template is invalid (ps_id: %s)", ps_id.c_str());
        sendEventControlFocusFailed(ps_id, direction);
        return;
    }

    if (display_listener)
        display_listener->controlDisplay(template_id, ControlType::Focus, direction);
}

void DisplayAgent::parsingControlScroll(const char* message)
{
    Json::Value root;
    Json::Reader reader;
    std::string ps_id;
    std::string template_id;
    ControlDirection direction;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    if (root["playServiceId"].empty() || root["direction"].empty()) {
        nugu_error("The required parameters are not set");
        return;
    }

    if (root["direction"].asString() == "PREVIOUS")
        direction = ControlDirection::PREVIOUS;
    else
        direction = ControlDirection::NEXT;

    ps_id = root["playServiceId"].asString();
    template_id = getTemplateId(ps_id);
    if (template_id.empty()) {
        nugu_warn("The template is invalid (ps_id: %s)", ps_id.c_str());
        sendEventControlScrollFailed(ps_id, direction);
        return;
    }

    if (display_listener)
        display_listener->controlDisplay(template_id, ControlType::Scroll, direction);
}

void DisplayAgent::parsingUpdate(const char* message)
{
    Json::Value root;
    Json::Reader reader;
    std::string ps_id;
    std::string template_id;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    if (root["playServiceId"].empty() || root["token"].empty()) {
        nugu_error("The required parameters are not set");
        return;
    }

    if (disp_cur_token != root["token"].asString()) {
        nugu_warn("The token is invalid");
        return;
    }

    ps_id = root["playServiceId"].asString();
    template_id = getTemplateId(ps_id);
    if (template_id.empty()) {
        nugu_warn("The template is invalid (ps_id: %s)", ps_id.c_str());
        return;
    }

    if (display_listener)
        display_listener->updateDisplay(template_id, message);
}

void DisplayAgent::parsingTemplates(const char* message)
{
    Json::Value root;
    Json::Reader reader;
    const char* dname;
    const char* dnamespace;

    dname = nugu_directive_peek_name(getNuguDirective());
    dnamespace = nugu_directive_peek_namespace(getNuguDirective());

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    if (root["playServiceId"].empty()) {
        nugu_error("The required parameters are not set");
        return;
    }

    if (root["token"].empty()) {
        if (strcmp(dname, "Custom")) {
            nugu_warn("support to allow empty token for legacy CustomTemplate");
        } else {
            nugu_error("The required parameters are not set");
            return;
        }
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);

    DisplayRenderInfo* info = new DisplayRenderInfo;
    info->id = std::to_string(tv.tv_sec) + std::to_string(tv.tv_usec);
    info->type = std::string(dnamespace) + "." + std::string(dname);
    info->payload = message;
    info->dialog_id = nugu_directive_peek_dialog_id(getNuguDirective());
    info->ps_id = root["playServiceId"].asString();
    info->token = root["token"].asString();

    render_info[info->id] = info;

    playsync_manager->startSync(getPlayServiceIdInStackControl(root["playStackControl"]), getName(), info);
}

std::string DisplayAgent::getTemplateId(const std::string& ps_id)
{
    for (const auto& info : render_info) {
        if (info.second->ps_id == ps_id) {
            return info.second->id;
        }
    }
    return "";
}

std::string DisplayAgent::getDirectionString(ControlDirection direction)
{
    if (direction == ControlDirection::PREVIOUS)
        return "PREVIOUS";
    else
        return "NEXT";
}

void DisplayAgent::onSyncState(const std::string& ps_id, PlaySyncState state, void* extra_data)
{
    if (state == PlaySyncState::Synced)
        renderDisplay(extra_data);
    else if (state == PlaySyncState::Released)
        clearDisplay(extra_data);
}

void DisplayAgent::onDataChanged(const std::string& ps_id, std::pair<void*, void*> extra_datas)
{
    clearDisplay(extra_datas.first, (extra_datas.second ? true : false));
    renderDisplay(extra_datas.second);
}

void DisplayAgent::renderDisplay(void* data)
{
    if (!display_listener || !data) {
        nugu_warn("The DisplayListener or render data is not exist.");
        return;
    }

    auto render_info = reinterpret_cast<DisplayRenderInfo*>(data);
    display_listener->renderDisplay(render_info->id, render_info->type, render_info->payload, render_info->dialog_id);
}

void DisplayAgent::clearDisplay(void* data, bool has_next_render)
{
    if (!display_listener || !data) {
        nugu_warn("The DisplayListener or render data is not exist.");
        return;
    }

    auto render_info = reinterpret_cast<DisplayRenderInfo*>(data);
    this->render_info[render_info->id]->close = true;
    display_listener->clearDisplay(render_info->id, true, has_next_render || playsync_manager->hasNextPlayStack());
}

} // NuguCapability
