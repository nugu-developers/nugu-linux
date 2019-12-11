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

#include "capability.hh"
#include "display_render_assembly.hh"

namespace NuguCore {

using namespace NuguInterface;

class DisplayAgent : public Capability,
                     public DisplayRenderAssembly<DisplayAgent> {
public:
    DisplayAgent();
    virtual ~DisplayAgent();

    void parsingDirective(const char* dname, const char* message) override;
    void updateInfoForContext(Json::Value& ctx) override;
    void setCapabilityListener(ICapabilityListener* clistener) override;

    // implement DisplayRenderAssembly
    void onElementSelected(const std::string& item_token);

private:
    void sendEventElementSelected(const std::string& item_token);
};

} // NuguCore

#endif
