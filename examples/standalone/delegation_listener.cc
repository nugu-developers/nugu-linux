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

#include <iostream>

#include "delegation_listener.hh"

void DelegationListener::delegate(const std::string& app_id, const std::string& ps_id, const std::string& data)
{
    // TODO : need to handle argument
    std::cout << "=======================================================\n"
              << "Delegation data\n"
              << "=======================================================\n"
              << "app_id : " << app_id
              << "ps_id : " << ps_id
              << "data : " << data
              << std::endl;
}

bool DelegationListener::requestContext(std::string& ps_id, std::string& data)
{
    // TODO: If it need to send context, set ps_id and data value and return true

    return false;
}
