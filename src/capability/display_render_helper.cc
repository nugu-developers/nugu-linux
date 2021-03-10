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

#include <sys/time.h>

#include "base/nugu_log.h"
#include "display_render_helper.hh"

namespace NuguCapability {

DisplayRenderHelper::Builder::Builder(DisplayRenderHelper* parent)
    : _parent(parent)
{
}

DisplayRenderHelper::Builder* DisplayRenderHelper::Builder::setId(const std::string& id)
{
    render_info.id = id;

    return this;
}

DisplayRenderHelper::Builder* DisplayRenderHelper::Builder::setType(const std::string& type)
{
    render_info.type = type;

    return this;
}

DisplayRenderHelper::Builder* DisplayRenderHelper::Builder::setView(const std::string& view)
{
    render_info.view = view;

    return this;
}

DisplayRenderHelper::Builder* DisplayRenderHelper::Builder::setDialogId(const std::string& dialog_id)
{
    render_info.dialog_id = dialog_id;

    return this;
}

DisplayRenderHelper::Builder* DisplayRenderHelper::Builder::setPlayServiceId(const std::string& ps_id)
{
    render_info.ps_id = ps_id;

    return this;
}

DisplayRenderHelper::Builder* DisplayRenderHelper::Builder::setToken(const std::string& token)
{
    render_info.token = token;

    return this;
}

DisplayRenderHelper::Builder* DisplayRenderHelper::Builder::setPostPoneRemove(bool postpone_remove)
{
    render_info.postpone_remove = postpone_remove;

    return this;
}

DisplayRenderInfo* DisplayRenderHelper::Builder::build()
{
    if (render_info.type.empty() || render_info.view.empty() || render_info.dialog_id.empty()) {
        nugu_error("Some required datas are not set.");
        return nullptr;
    }

    DisplayRenderInfo* info = new DisplayRenderInfo;
    info->id = !render_info.id.empty() ? render_info.id : makeId();
    info->type = render_info.type;
    info->view = render_info.view;
    info->dialog_id = render_info.dialog_id;
    info->postpone_remove = render_info.postpone_remove;
    info->ps_id = render_info.ps_id;
    info->token = render_info.token;

    _parent->setRenderInfo(info);
    render_info = {};

    return info;
}

std::string DisplayRenderHelper::Builder::makeId()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return std::to_string(tv.tv_sec) + std::to_string(tv.tv_usec);
}

DisplayRenderHelper::DisplayRenderHelper()
    : builder(std::unique_ptr<Builder>(new DisplayRenderHelper::Builder(this)))
{
}

DisplayRenderHelper::~DisplayRenderHelper()
{
    clear();
}

void DisplayRenderHelper::setDisplayListener(IDisplayListener* listener)
{
    if (listener)
        display_listener = listener;
}

DisplayRenderHelper::Builder* DisplayRenderHelper::getRenderInfoBuilder()
{
    return builder.get();
}

void DisplayRenderHelper::setRenderInfo(DisplayRenderInfo* render_info)
{
    if (!render_info || render_info->id.empty()) {
        nugu_error("The Render Info is not exist.");
        return;
    }

    render_infos.emplace(render_info->id, render_info);
}

DisplayRenderInfo* DisplayRenderHelper::getRenderInfo(const std::string& id) noexcept
{
    try {
        return render_infos.at(id);
    } catch (std::out_of_range& exception) {
        return nullptr;
    }
}

std::string DisplayRenderHelper::getTemplateId(const std::string& ps_id)
{
    for (const auto& info : render_infos)
        if (info.second->ps_id == ps_id)
            return info.second->id;

    return "";
}

std::string DisplayRenderHelper::renderDisplay(void* data)
{
    if (!display_listener || !data) {
        nugu_warn("The DisplayListener or render data is not exist.");
        return "";
    }

    auto render_info = reinterpret_cast<DisplayRenderInfo*>(data);
    display_listener->renderDisplay(render_info->id, render_info->type, render_info->view, render_info->dialog_id);

    return render_info->id;
}

std::string DisplayRenderHelper::updateDisplay(std::pair<void*, void*> datas, bool has_next_render)
{
    clearDisplay(datas.first, (datas.second || has_next_render));
    return renderDisplay(datas.second);
}

void DisplayRenderHelper::clearDisplay(void* data, bool has_next_render)
{
    if (!display_listener || !data) {
        nugu_warn("The DisplayListener or render data is not exist.");
        return;
    }

    auto render_info = reinterpret_cast<DisplayRenderInfo*>(data);
    std::string id = render_info->id;

    display_listener->clearDisplay(id, true, has_next_render);
    render_info->postpone_remove ? setRenderClose(id) : removedRenderInfo(id);
}

void DisplayRenderHelper::setRenderClose(const std::string& id) noexcept
{
    try {
        render_infos.at(id)->close = true;
    } catch (std::out_of_range& exception) {
        nugu_warn("No render info exist.");
    }
}

void DisplayRenderHelper::removedRenderInfo(const std::string& id) noexcept
{
    try {
        delete render_infos.at(id);
        render_infos.erase(id);
    } catch (std::out_of_range& exception) {
        nugu_warn("No render info exist.");
    }
}

void DisplayRenderHelper::clear()
{
    for (auto info : render_infos)
        delete info.second;

    render_infos.clear();
}

} // NuguCapability
