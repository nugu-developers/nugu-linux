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

#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "capability_manager.hh"
#include "display_agent.hh"
#include "nugu_log.h"

namespace NuguCore {

static const std::string capability_version = "1.0";

DisplayAgent::DisplayAgent()
    : Capability(CapabilityType::Display, capability_version)
    , display_listener(nullptr)
    , cur_ps_id("")
    , cur_token("")
{
}

DisplayAgent::~DisplayAgent()
{
    cur_token = cur_ps_id = "";
    display_listener = nullptr;

    for (auto info : render_info) {
        delete info.second;
    }
    render_info.clear();
}

void DisplayAgent::processDirective(NuguDirective* ndir)
{
    Json::Value root;
    Json::Reader reader;
    const char* dnamespace;
    const char* dname;
    const char* message;
    std::string type;

    dnamespace = nugu_directive_peek_namespace(ndir);
    message = nugu_directive_peek_json(ndir);
    dname = nugu_directive_peek_name(ndir);

    if (!message || !dname) {
        nugu_error("directive message is not correct");
        destoryDirective(ndir);
        return;
    }
    type = std::string(dnamespace) + "." + std::string(dname);

    nugu_dbg("message: %s", message);

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        destoryDirective(ndir);
        return;
    }

    std::string playstackctl_ps_id = getPlayServiceIdInStackControl(root["playStackControl"]);

    if (!playstackctl_ps_id.empty()) {
        struct timeval tv;

        gettimeofday(&tv, NULL);
        std::string id = std::to_string(tv.tv_sec) + std::to_string(tv.tv_usec);

        PlaySyncManager::DisplayRenderInfo* info = new PlaySyncManager::DisplayRenderInfo;
        info->id = id;
        info->ps_id = root["playServiceId"].asString();
        info->type = type;
        info->payload = message;
        info->dialog_id = nugu_directive_peek_dialog_id(ndir);
        info->token = root["token"].asString();
        render_info[id] = info;

        // sync display rendering with context
        PlaySyncManager::DisplayRenderer renderer;
        renderer.cap_type = getType();
        renderer.listener = this;
        renderer.only_rendering = true;
        renderer.duration = root["duration"].asString();
        renderer.display_id = id;

        playsync_manager->addContext(playstackctl_ps_id, getType(), renderer);
    }

    destoryDirective(ndir);
}

void DisplayAgent::updateInfoForContext(Json::Value& ctx)
{
    Json::Value display;

    display["version"] = getVersion();
    if (cur_token.size()) {
        display["playServiceId"] = cur_ps_id;
        display["token"] = cur_token;
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
    if (render_info.find(id) != render_info.end()) {
        cur_token = render_info[id]->token;
        cur_ps_id = render_info[id]->ps_id;
    }
}

void DisplayAgent::displayCleared(const std::string& id)
{
    std::string ps_id = "";

    if (render_info.find(id) != render_info.end()) {
        auto info = render_info[id];
        cur_token = cur_ps_id = "";
        ps_id = info->ps_id;
        render_info.erase(id);
        delete info;
    }

    playsync_manager->clearPendingContext(ps_id);
}

void DisplayAgent::elementSelected(const std::string& id, const std::string& item_token)
{
    if (render_info.find(id) == render_info.end()) {
        nugu_warn("SDK doesn't know or manage the template(%s)", id.c_str());
        return;
    }

    cur_token = render_info[id]->token;
    cur_ps_id = render_info[id]->ps_id;
    sendEventElementSelected(item_token);
}

void DisplayAgent::setListener(IDisplayListener* listener)
{
    if (!listener)
        return;

    display_listener = listener;
}

void DisplayAgent::removeListener(IDisplayListener* listener)
{
    if (display_listener == listener)
        display_listener = nullptr;
}

void DisplayAgent::stopRenderingTimer(const std::string& id)
{
    playsync_manager->clearContextHold();
}

void DisplayAgent::sendEventElementSelected(const std::string& item_token)
{
    std::string ename = "ElementSelected";
    std::string payload = "";
    Json::StyledWriter writer;
    Json::Value root;

    root["playServiceId"] = cur_ps_id;
    root["token"] = item_token;
    payload = writer.write(root);

    sendEvent(ename, getContextInfo(), payload);
}

void DisplayAgent::onSyncDisplayContext(const std::string& id)
{
    nugu_dbg("Display sync context");

    if (render_info.find(id) == render_info.end())
        return;

    display_listener->renderDisplay(id, render_info[id]->type, render_info[id]->payload, render_info[id]->dialog_id);
}

bool DisplayAgent::onReleaseDisplayContext(const std::string& id, bool unconditionally)
{
    nugu_dbg("Display release context");

    if (render_info.find(id) == render_info.end())
        return true;

    bool ret = display_listener->clearDisplay(id, unconditionally);
    if (unconditionally || ret) {
        auto info = render_info[id];
        render_info.erase(id);
        delete info;
    }

    if (unconditionally && !ret)
        nugu_warn("should clear display if unconditionally is true!!");

    return ret;
}

} // NuguCore
