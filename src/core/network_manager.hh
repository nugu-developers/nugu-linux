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

#ifndef __NUGU_NETWORK_MANAGER_WRAPPER_H__
#define __NUGU_NETWORK_MANAGER_WRAPPER_H__

#include <vector>

#include "clientkit/network_manager_interface.hh"

namespace NuguCore {

using namespace NuguClientKit;

class NetworkManager : public INetworkManager {
public:
    NetworkManager();
    ~NetworkManager();

    void addListener(INetworkManagerListener* listener) override;
    void removeListener(INetworkManagerListener* listener) override;
    std::vector<INetworkManagerListener*> getListener();
    bool connect() override;
    bool disconnect() override;
    bool setToken(std::string token) override;
    bool setRegistryUrl(std::string url) override;
    bool setUserAgent(std::string app_version, std::string additional_info) override;

private:
    std::vector<INetworkManagerListener*> listeners;
};

} // NuguCore

#endif /* __NUGU_NETWORK_MANAGER_WRAPPER_H__ */
