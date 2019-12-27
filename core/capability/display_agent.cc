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

#include "base/nugu_log.h"

#include "capability_manager.hh"
#include "display_agent.hh"

namespace NuguCore {

static const std::string capability_version = "1.0";

DisplayAgent::DisplayAgent()
    : Capability(CapabilityType::Display, capability_version)
{
}

DisplayAgent::~DisplayAgent()
{
}

void DisplayAgent::parsingDirective(const char* dname, const char* message)
{
    Json::Value root;
    Json::Reader reader;
    const char* dnamespace;
    std::string type;

    dnamespace = nugu_directive_peek_namespace(getNuguDirective());
    type = std::string(dnamespace) + "." + std::string(dname);

    nugu_dbg("message: %s", message);

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    std::string playstackctl_ps_id = getPlayServiceIdInStackControl(root["playStackControl"]);

    if (!playstackctl_ps_id.empty()) {
        std::string id = composeRenderInfo(std::make_tuple(
            root["playServiceId"].asString(),
            type,
            message,
            nugu_directive_peek_dialog_id(getNuguDirective()),
            root["token"].asString()));

        // sync display rendering with context
        PlaySyncManager::DisplayRenderer renderer;
        renderer.cap_type = getType();
        renderer.listener = this;
        renderer.only_rendering = true;
        renderer.duration = root["duration"].asString();
        renderer.display_id = id;

        playsync_manager->addContext(playstackctl_ps_id, getType(), std::move(renderer));
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

void DisplayAgent::sendEventElementSelected(const std::string& item_token)
{
    std::string ename = "ElementSelected";
    std::string payload = "";
    Json::StyledWriter writer;
    Json::Value root;

    root["playServiceId"] = disp_cur_ps_id;
    root["token"] = item_token;
    payload = writer.write(root);

    sendEvent(ename, getContextInfo(), payload);
}

void DisplayAgent::onElementSelected(const std::string& item_token)
{
    sendEventElementSelected(item_token);
}

} // NuguCore
