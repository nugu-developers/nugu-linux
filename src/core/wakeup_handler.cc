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
#include "capability_manager.hh"

#include "wakeup_handler.hh"

namespace NuguCore {

WakeupHandler::WakeupHandler(const std::string& model_path)
    : wakeup_detector(std::unique_ptr<WakeupDetector>(new WakeupDetector(WakeupDetector::Attribute { "", "", "", model_path })))
{
    wakeup_detector->setListener(this);
}

WakeupHandler::~WakeupHandler()
{
}

void WakeupHandler::setListener(IWakeupListener* listener)
{
    this->listener = listener;
}

void WakeupHandler::startWakeup(void)
{
    wakeup_detector->startWakeup();
}

void WakeupHandler::stopWakeup(void)
{
    wakeup_detector->stopWakeup();
}

void WakeupHandler::onWakeupState(WakeupState state)
{
    switch (state) {
    case WakeupState::FAIL:
        nugu_dbg("WakeupState::FAIL");

        if (listener)
            listener->onWakeupState(WakeupDetectState::WAKEUP_FAIL);

        break;
    case WakeupState::DETECTING:
        nugu_dbg("WakeupState::DETECTING");

        if (listener)
            listener->onWakeupState(WakeupDetectState::WAKEUP_DETECTING);
        break;
    case WakeupState::DETECTED:
        nugu_dbg("WakeupState::DETECTED");

        CapabilityManager::getInstance()->sendCommand("ASR", "ASR", "wakeup_detected", "");

        if (listener)
            listener->onWakeupState(WakeupDetectState::WAKEUP_DETECTED);
        break;
    case WakeupState::DONE:
        nugu_dbg("WakeupState::DONE");
        break;
    }
}

} // NuguCore
