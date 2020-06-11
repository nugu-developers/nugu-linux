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

#ifndef __NUGU_SESSION_MANAGER_H__
#define __NUGU_SESSION_MANAGER_H__

#include <map>

#include "clientkit/session_manager_interface.hh"

namespace NuguCore {

using namespace NuguClientKit;

class SessionManager : public ISessionManager {
public:
    SessionManager();
    virtual ~SessionManager();

    void set(const std::string& dialog_id, Session&& session) override;
    void activate(const std::string& dialog_id) override;
    void deactivate(const std::string& dialog_id) override;
    Json::Value getActiveSessionInfo() override;

private:
    std::map<std::string, Session> session_map;
};

} // NuguCore

#endif /* __NUGU_SESSION_MANAGER_H__ */
