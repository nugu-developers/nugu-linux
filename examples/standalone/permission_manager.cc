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

#include "permission_manager.hh"

void PermissionManager::requestContext(std::list<PermissionState>& permission_list)
{
    permission_list.push_back(location_permission);
}

void PermissionManager::requestContext(LocationInfo& location_info)
{
    // TODO: It has to extract location state and current from application.

    location_info.state = location_state;
    location_info.permission_granted = (location_permission.state == Permission::State::GRANTED);
    location_info.latitude = "37.565715"; // dummy data
    location_info.longitude = "126.988675"; // dummy data
}

void PermissionManager::requestPermission(const std::set<Permission::Type>& permission_set)
{
    // TODO: It has to do permission acquirement process in application.

    for (const auto& permission_type : permission_set) {
        switch (permission_type) {
        case Permission::Type::LOCATION:
            location_permission.state = Permission::State::GRANTED;
            location_state = Location::State::AVAILABLE;
            break;
        }
    }
}

IPermissionListener* PermissionManager::getPermissionListener()
{
    return this;
}

ILocationListener* PermissionManager::getLocationListener()
{
    return this;
}
