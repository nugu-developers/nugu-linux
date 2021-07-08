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
static const char* CAPABILITY_VERSION = "1.8";

DisplayAgent::DisplayAgent()
    : Capability(CAPABILITY_NAME, CAPABILITY_VERSION)
    , render_helper(std::unique_ptr<DisplayRenderHelper>(new DisplayRenderHelper()))
    , display_listener(nullptr)
{
    template_names = {
        "FullText1",
        "FullText2",
        "FullText3",
        "ImageText1",
        "ImageText2",
        "ImageText3",
        "ImageText4",
        "TextList1",
        "TextList2",
        "TextList3",
        "TextList4",
        "ImageList1",
        "ImageList2",
        "ImageList3",
        "Weather1",
        "Weather2",
        "Weather3",
        "Weather4",
        "Weather5",
        "FullImage",
        "Score1",
        "Score2",
        "SearchList1",
        "SearchList2",
        "UnifiedSearch1",
        "CommerceList",
        "CommerceOption",
        "CommercePrice",
        "CommerceInfo",
        "Call1",
        "Call2",
        "Call3",
        "Timer",
        "Dummy",
        "CustomTemplate",
        "TabExtension"
    };
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

    for (const auto& template_name : template_names)
        addBlockingPolicy(template_name, { BlockingMedium::AUDIO, true });

    addReferrerEvents("CloseSucceeded", "Close");
    addReferrerEvents("CloseFailed", "Close");
    addReferrerEvents("ControlFocusSucceeded", "ControlFocus");
    addReferrerEvents("ControlFocusFailed", "ControlFocus");
    addReferrerEvents("ControlScrollSucceeded", "ControlScroll");
    addReferrerEvents("ControlScrollFailed", "ControlScroll");

    initialized = true;
}

void DisplayAgent::deInitialize()
{
    playsync_manager->removeListener(getName());
    render_helper->clear();
    session_dialog_ids.clear();

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
    auto render_info = render_helper->getRenderInfo(id);

    if (!render_info) {
        nugu_warn("There is no render info : %s", id.c_str());
        return;
    }

    disp_cur_token = render_info->token;
    disp_cur_ps_id = render_info->ps_id;
}

void DisplayAgent::displayCleared(const std::string& id)
{
    auto render_info = render_helper->getRenderInfo(id);

    if (!render_info) {
        nugu_warn("There is no render info : %s", id.c_str());
        return;
    }

    if (render_info->close)
        sendEventCloseSucceeded(render_info->ps_id);

    capa_helper->sendCommand("Display", "Nudge", "clearNudge", render_info->dialog_id);

    render_helper->removedRenderInfo(id);
    deactiveSession();

    disp_cur_token = disp_cur_ps_id = "";
}

void DisplayAgent::elementSelected(const std::string& id, const std::string& item_token, const std::string& postback)
{
    auto render_info = render_helper->getRenderInfo(id);

    if (!render_info) {
        nugu_warn("There is no render info : %s", id.c_str());
        return;
    }

    disp_cur_token = render_info->token;
    disp_cur_ps_id = render_info->ps_id;

    sendEventElementSelected(item_token, postback);
}

void DisplayAgent::informControlResult(const std::string& id, ControlType type, ControlDirection direction)
{
    auto render_info = render_helper->getRenderInfo(id);

    if (!render_info) {
        nugu_warn("There is no render info : %s", id.c_str());
        return;
    }

    (type == ControlType::Scroll)
        ? sendEventControlScrollSucceeded(render_info->ps_id, direction)
        : sendEventControlFocusSucceeded(render_info->ps_id, direction);
}

void DisplayAgent::setListener(IDisplayListener* listener)
{
    if (!listener)
        return;

    display_listener = listener;
    render_helper->setDisplayListener(display_listener);
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

void DisplayAgent::onSyncState(const std::string& ps_id, PlaySyncState state, void* extra_data)
{
    if (state == PlaySyncState::Synced)
        render_helper->renderDisplay(extra_data);
    else if (state == PlaySyncState::Released)
        render_helper->clearDisplay(extra_data, playsync_manager->hasNextPlayStack());
}

void DisplayAgent::onDataChanged(const std::string& ps_id, std::pair<void*, void*> extra_datas)
{
    render_helper->updateDisplay(extra_datas, playsync_manager->hasNextPlayStack());
}

void DisplayAgent::sendEventElementSelected(const std::string& item_token, const std::string& postback)
{
    std::string ename = "ElementSelected";
    std::string payload = "";
    Json::FastWriter writer;
    Json::Reader reader;
    Json::Value root;
    Json::Value temp;

    root["playServiceId"] = disp_cur_ps_id;
    root["token"] = item_token;

    if (reader.parse(postback, temp))
        root["postback"] = temp;

    payload = writer.write(root);

    sendEvent(ename, capa_helper->makeAllContextInfo(), payload);
}

void DisplayAgent::sendEventCloseSucceeded(const std::string& ps_id)
{
    sendEventClose("CloseSucceeded", ps_id);
}

void DisplayAgent::sendEventCloseFailed(const std::string& ps_id)
{
    sendEventClose("CloseFailed", ps_id);
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

void DisplayAgent::sendEventClose(const std::string& ename, const std::string& ps_id)
{
    std::string payload = "";
    Json::FastWriter writer;
    Json::Value root;

    root["playServiceId"] = ps_id;
    payload = writer.write(root);

    sendEvent(ename, getContextInfo(), payload);
}

void DisplayAgent::sendEventControl(const std::string& ename, const std::string& ps_id, ControlDirection direction)
{
    std::string payload = "";
    Json::FastWriter writer;
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

    activateSession(getNuguDirective());

    if (display_listener)
        display_listener->updateDisplay(template_id, message);
}

void DisplayAgent::parsingTemplates(const char* message)
{
    Json::Value root;
    Json::Reader reader;
    NuguDirective* ndir = getNuguDirective();

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    if (root["playServiceId"].empty()) {
        nugu_error("The required parameters are not set");
        return;
    }

    if (root["token"].empty()) {
        if (strcmp(nugu_directive_peek_name(ndir), "Custom")) {
            nugu_warn("support to allow empty token for legacy CustomTemplate");
        } else {
            nugu_error("The required parameters are not set");
            return;
        }
    }

    activateSession(ndir);

    auto render_info = composeRenderInfo(ndir, root["playServiceId"].asString(), root["token"].asString());
    playsync_manager->startSync(getPlayServiceIdInStackControl(root["playStackControl"]), getName(), render_info);
}

void DisplayAgent::activateSession(NuguDirective* ndir)
{
    std::string dialog_id = nugu_directive_peek_dialog_id(ndir);
    session_dialog_ids.emplace(dialog_id);
    session_manager->activate(dialog_id);
}

void DisplayAgent::deactiveSession()
{
    for (const auto& session_dialog_id : session_dialog_ids)
        session_manager->deactivate(session_dialog_id);

    session_dialog_ids.clear();
}

DisplayRenderInfo* DisplayAgent::composeRenderInfo(NuguDirective* ndir, const std::string& ps_id, const std::string& token)
{
    return render_helper->getRenderInfoBuilder()
        ->setType(std::string(nugu_directive_peek_namespace(ndir)) + "." + std::string(nugu_directive_peek_name(ndir)))
        ->setView(nugu_directive_peek_json(ndir))
        ->setDialogId(nugu_directive_peek_dialog_id(ndir))
        ->setPlayServiceId(ps_id)
        ->setToken(token)
        ->setPostPoneRemove(true)
        ->build();
}

std::string DisplayAgent::getTemplateId(const std::string& ps_id)
{
    return render_helper->getTemplateId(ps_id);
}

std::string DisplayAgent::getDirectionString(ControlDirection direction)
{
    if (direction == ControlDirection::PREVIOUS)
        return "PREVIOUS";
    else
        return "NEXT";
}

} // NuguCapability
