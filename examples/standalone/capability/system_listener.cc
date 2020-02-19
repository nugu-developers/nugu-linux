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

void SystemListener::onException(SystemException exception)
{
    std::cout << "[SYSTEM] ";

    switch (exception) {
    case SystemException::UNAUTHORIZED_REQUEST_EXCEPTION:
        std::cout << "UNAUTHORIZED_REQUEST_EXCEPTION" << std::endl;
        break;
    case SystemException::PLAY_ROUTER_PROCESSING_EXCEPTION:
        std::cout << "PLAY_ROUTER_PROCESSING_EXCEPTION" << std::endl;
        break;
    case SystemException::ASR_RECOGNIZING_EXCEPTION:
        std::cout << "ASR_RECOGNIZING_EXCEPTION" << std::endl;
        break;
    case SystemException::TTS_SPEAKING_EXCEPTION:
        std::cout << "TTS_SPEAKING_EXCEPTION" << std::endl;
        break;
    case SystemException::INTERNAL_SERVICE_EXCEPTION:
        std::cout << "INTERNAL_SERVICE_EXCEPTION" << std::endl;
        break;
    default:
        std::cout << "UNKNOWN_EXCEPTION" << std::endl;
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

