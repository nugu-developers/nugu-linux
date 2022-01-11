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

#ifndef __NUGU_PERMISSION_AGENT_H__
#define __NUGU_PERMISSION_AGENT_H__

#include <clientkit/capability.hh>

using namespace NuguClientKit;

typedef struct {
    std::string name;
    std::string state;
} PermissionInfo;

class IPermissionListener : virtual public ICapabilityListener {
public:
    virtual ~IPermissionListener() = default;
    virtual void requestContext(std::vector<PermissionInfo>& permission_infos) = 0;
};

class PermissionAgent : public Capability {
public:
    PermissionAgent();
    virtual ~PermissionAgent() = default;

    void setCapabilityListener(ICapabilityListener* clistener) override;
    void updateInfoForContext(Json::Value& ctx) override;

private:
    IPermissionListener* permission_listener = nullptr;
};

#endif /* __NUGU_PERMISSION_AGENT_H__ */
