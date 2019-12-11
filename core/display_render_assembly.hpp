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

#include "capability_manager.hh"

namespace NuguCore {

template <typename T>
DisplayRenderAssembly<T>::DisplayRenderAssembly()
{
}

template <typename T>
DisplayRenderAssembly<T>::~DisplayRenderAssembly()
{
    disp_cur_token = disp_cur_ps_id = "";
    display_listener = nullptr;

    for (auto info : render_info) {
        delete info.second;
    }

    render_info.clear();
}

template <typename T>
std::string DisplayRenderAssembly<T>::composeRenderInfo(const RenderInfoParam& param)
{
    PlaySyncManager::DisplayRenderInfo* info = new PlaySyncManager::DisplayRenderInfo;
    struct timeval tv;

    gettimeofday(&tv, NULL);
    info->id = std::to_string(tv.tv_sec) + std::to_string(tv.tv_usec);

    std::tie(info->ps_id, info->type, info->payload, info->dialog_id, info->token) = param;
    render_info[info->id] = info;

    return info->id;
}

template <typename T>
void DisplayRenderAssembly<T>::displayRendered(const std::string& id)
{
    if (static_cast<T*>(this)->getType() == CapabilityType::Display) {
        if (render_info.find(id) != render_info.end()) {
            disp_cur_token = render_info[id]->token;
            disp_cur_ps_id = render_info[id]->ps_id;
        }
    }
}

template <typename T>
void DisplayRenderAssembly<T>::displayCleared(const std::string& id)
{
    std::string ps_id = "";

    if (render_info.find(id) != render_info.end()) {
        auto info = render_info[id];
        ps_id = info->ps_id;
        render_info.erase(id);
        delete info;

        disp_cur_token = disp_cur_ps_id = "";
    }

    CapabilityManager::getInstance()->getPlaySyncManager()->clearPendingContext(ps_id);
}

template <typename T>
void DisplayRenderAssembly<T>::elementSelected(const std::string& id, const std::string& item_token)
{
    auto derived = static_cast<T*>(this);

    if (derived->getType() == CapabilityType::Display) {
        if (render_info.find(id) == render_info.end()) {
            nugu_warn("SDK doesn't know or manage the template(%s)", id.c_str());
            return;
        }

        disp_cur_token = render_info[id]->token;
        disp_cur_ps_id = render_info[id]->ps_id;

        derived->onElementSelected(item_token);
    }
}

template <typename T>
void DisplayRenderAssembly<T>::setListener(IDisplayListener* listener)
{
    if (!listener)
        return;

    display_listener = listener;
}

template <typename T>
void DisplayRenderAssembly<T>::removeListener(IDisplayListener* listener)
{
    if (!display_listener && (display_listener == listener))
        display_listener = nullptr;
}

template <typename T>
void DisplayRenderAssembly<T>::stopRenderingTimer(const std::string& id)
{
    CapabilityManager::getInstance()->getPlaySyncManager()->clearContextHold();
}

template <typename T>
void DisplayRenderAssembly<T>::onSyncDisplayContext(const std::string& id)
{
    nugu_dbg("%s sync context", static_cast<T*>(this)->getName().c_str());

    if (render_info.find(id) == render_info.end())
        return;

    if (display_listener)
        display_listener->renderDisplay(id, render_info[id]->type, render_info[id]->payload, render_info[id]->dialog_id);
}

template <typename T>
bool DisplayRenderAssembly<T>::onReleaseDisplayContext(const std::string& id, bool unconditionally)
{
    nugu_dbg("%s release context", static_cast<T*>(this)->getName().c_str());

    if (render_info.find(id) == render_info.end())
        return true;

    bool ret = false;

    if (display_listener)
        ret = display_listener->clearDisplay(id, unconditionally);

    if (unconditionally || ret) {
        auto info = render_info[id];
        render_info.erase(id);
        delete info;
    }

    if (unconditionally && !ret)
        nugu_warn("should clear display if unconditionally is true!!");

    return ret;
}

}
