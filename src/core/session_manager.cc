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

#include <algorithm>

#include "base/nugu_log.h"
#include "session_manager.hh"

namespace NuguCore {

SessionManager::~SessionManager()
{
    clear();
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

void SessionManager::activate(const std::string& dialog_id)
{
    if (dialog_id.empty()) {
        nugu_error("The dialog request ID is empty.");
        return;
    }

    auto result = std::find_if(active_list.begin(), active_list.end(),
        [&](std::pair<std::string, int>& item) {
            if (item.first == dialog_id) {
                item.second++;
                return true;
            }

            return false;
        });

    if (result == active_list.end())
        active_list.emplace_back(std::make_pair(dialog_id, 1));
}

void SessionManager::deactivate(const std::string& dialog_id)
{
    if (dialog_id.empty()) {
        nugu_error("The dialog request ID is empty.");
        return;
    }

    active_list.erase(
        std::remove_if(active_list.begin(), active_list.end(),
            [&](std::pair<std::string, int>& item) {
                if (item.first == dialog_id && (--item.second) < 1) {
                    session_map.erase(dialog_id);
                    return true;
                }

                return false;
            }),
        active_list.end());
}

Json::Value SessionManager::getActiveSessionInfo()
{
    Json::Value session_info_list;

    for (const auto& item : active_list) {
        try {
            auto session = session_map.at(item.first);

            Json::Value session_info;
            session_info["sessionId"] = session.session_id;
            session_info["playServiceId"] = session.ps_id;
            session_info_list.append(session_info);
        } catch (std::out_of_range& exception) {
            nugu_warn("The such session is not exist.");
        }
    }

    return session_info_list;
}

void SessionManager::clear()
{
    session_map.clear();
    active_list.clear();
}

const SessionManager::ActiveDialogs& SessionManager::getActiveList()
{
    return active_list;
}

const SessionManager::Sessions& SessionManager::getAllSessions()
{
    return session_map;
}

} // NuguCore
