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
#include <vector>

#include "clientkit/session_manager_interface.hh"
#include "nugu.h"

namespace NuguCore {

using namespace NuguClientKit;

class NUGU_API SessionManager : public ISessionManager {
public:
    using ActiveDialogs = std::vector<std::pair<std::string, int>>;
    using Sessions = std::map<std::string, Session>;

public:
    SessionManager() = default;
    virtual ~SessionManager();

    void reset();
    void addListener(ISessionManagerListener* listener) override;
    void removeListener(ISessionManagerListener* listener) override;
    int getListenerCount();

    void set(const std::string& dialog_id, Session&& session) override;
    void activate(const std::string& dialog_id) override;
    void deactivate(const std::string& dialog_id) override;
    NJson::Value getActiveSessionInfo() override;
    void clear() override;

    const ActiveDialogs& getActiveList();
    const Sessions& getAllSessions();

private:
    void notifyActiveState(const std::string& dialog_id, bool is_active = true);
    void clearContainer();

    std::vector<ISessionManagerListener*> listeners;
    ActiveDialogs active_list;
    Sessions session_map;
};

} // NuguCore

#endif /* __NUGU_SESSION_MANAGER_H__ */
