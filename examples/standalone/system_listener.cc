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

#include "system_listener.hh"

void SystemListener::onSystemMessageReport(SystemMessage message)
{
    std::cout << "[SYSTEM] ";

    switch (message) {
    case SystemMessage::ROUTE_ERROR_NOT_FOUND_PLAY:
        std::cout << "ROUTE_ERROR_NOT_FOUND_PLAY" << std::endl;
        break;
    default:
        std::cout << "invalid message" << std::endl;
        break;
    }
}

void SystemListener::onTurnOff()
{
    std::cout << "[SYSTEM] Device turn off" << std::endl;
}

void SystemListener::onRevoke(RevokeReason reason)
{
    std::cout << "[SYSTEM] ";

    switch (reason) {
    case RevokeReason::REVOKED_DEVICE:
        std::cout << "REVOKED_DEVICE" << std::endl;
        break;
    default:
        std::cout << "invalid reason" << std::endl;
        break;
    }
}
