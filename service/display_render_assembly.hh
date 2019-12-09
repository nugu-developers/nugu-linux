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

#ifndef __NUGU_DISPLAY_RENDER_ASSEMBLY_H__
#define __NUGU_DISPLAY_RENDER_ASSEMBLY_H__

#include <interface/capability/display_interface.hh>

namespace NuguCore {

using namespace NuguInterface;

template <typename T>
class DisplayRenderAssembly : virtual public IDisplayHandler,
                              public IPlaySyncManagerListener {
public:
    using RenderInfoParam = std::tuple<std::string, std::string, std::string, std::string, std::string>;

    DisplayRenderAssembly();
    virtual ~DisplayRenderAssembly();

    std::string composeRenderInfo(const RenderInfoParam& param);

    // implement IDisplayHandler
    void displayRendered(const std::string& id);
    void displayCleared(const std::string& id);
    void elementSelected(const std::string& id, const std::string& item_token);
    void setListener(IDisplayListener* listener);
    void removeListener(IDisplayListener* listener);
    void stopRenderingTimer(const std::string& id);

    // implement IContextManagerListener
    void onSyncDisplayContext(const std::string& id) override;
    bool onReleaseDisplayContext(const std::string& id, bool unconditionally) override;

protected:
    virtual void onElementSelected(const std::string& item_token) = 0;

    IDisplayListener* display_listener = nullptr;
    std::string disp_cur_ps_id = "";
    std::string disp_cur_token = "";

private:
    std::map<std::string, PlaySyncManager::DisplayRenderInfo*> render_info;
};
}

#include "display_render_assembly.hpp"

#endif /* __NUGU_DISPLAY_RENDER_ASSEMBLY_H__ */
