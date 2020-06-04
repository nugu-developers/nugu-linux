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

#include "session_manager.hh"

namespace NuguCore {

SessionManager::SessionManager()
{
}

SessionManager::~SessionManager()
{
    session_map.clear();
}

void SessionManager::set(const std::string& dialog_id, Session&& session)
{
    if (dialog_id.empty() || session.session_id.empty() || session.ps_id.empty()) {
        nugu_error("The required session info is not sufficient.");
        return;
    }

    // It remove the previous session of same key, if exists.
    session_map.erase(dialog_id);
    session_map.emplace(dialog_id, session);
}

void SessionManager::sync(const std::string& dialog_id)
{
    if (dialog_id.empty()) {
        nugu_error("The dialog request ID is empty.");
        return;
    }

    try {
        session_map.at(dialog_id).is_synced = true;
    } catch (std::out_of_range& exception) {
        nugu_warn("The such session is not exist.");
    }
}

void SessionManager::release(const std::string& dialog_id)
{
    if (dialog_id.empty()) {
        nugu_error("The dialog request ID is empty.");
        return;
    }

    session_map.erase(dialog_id);
}

Json::Value SessionManager::getSyncedSessionInfo()
{
    Json::Value session_info_list;

    for (const auto& item : session_map) {
        if (item.second.is_synced) {
            Json::Value session_info;
            session_info["sessionId"] = item.second.session_id;
            session_info["playServiceId"] = item.second.ps_id;
            session_info_list.append(session_info);
        }
    }

    return session_info_list;
}

} // NuguCore
