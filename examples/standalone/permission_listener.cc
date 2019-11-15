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

#include "permission_listener.hh"

void PermissionListener::requestContext(std::list<PermissionState>& permission_list)
{
    permission_list.push_back(location_permission);
}

void PermissionListener::requestPermission(const std::set<Permission::Type>& permission_set)
{
    // TODO: It has to do permission acquirement process in application.

    for (const auto& permission_type : permission_set) {
        switch (permission_type) {
        case Permission::Type::LOCATION:
            location_permission.state = Permission::State::GRANTED;
            break;
        }
    }
}
