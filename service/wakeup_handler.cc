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

#include "wakeup_handler.hh"
#include "capability_manager.hh"
#include "nugu_log.h"

namespace NuguCore {

WakeupHandler::WakeupHandler()
    : wakeup_detector(std::unique_ptr<WakeupDetector>(new WakeupDetector()))
{
    wakeup_detector->setListener(this);

    CapabilityManager::getInstance()->addFocus("kwd", NUGU_FOCUS_TYPE_WAKEWORD, this);
}

WakeupHandler::~WakeupHandler()
{
    CapabilityManager::getInstance()->removeFocus("kwd");
}

void WakeupHandler::setListener(IWakeupListener* listener)
{
    this->listener = listener;
}

void WakeupHandler::startWakeup(void)
{
    CapabilityManager::getInstance()->requestFocus("kwd", NUGU_FOCUS_RESOURCE_MIC, NULL);
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

        CapabilityManager::getInstance()->sendCommand(CapabilityType::ASR, CapabilityType::ASR, "wakeup_detected", "");
        CapabilityManager::getInstance()->releaseFocus("kwd", NUGU_FOCUS_RESOURCE_MIC);

        if (listener)
            listener->onWakeupState(WakeupDetectState::WAKEUP_DETECTED);
        break;
    case WakeupState::DONE:
        nugu_dbg("WakeupState::DONE");
        break;
    }
}

NuguFocusResult WakeupHandler::onFocus(NuguFocusResource rsrc, void* event)
{
    wakeup_detector->startWakeup();

    return NUGU_FOCUS_OK;
}

NuguFocusResult WakeupHandler::onUnfocus(NuguFocusResource rsrc, void* event)
{
    wakeup_detector->stopWakeup();

    return NUGU_FOCUS_REMOVE;
}

NuguFocusStealResult WakeupHandler::onStealRequest(NuguFocusResource rsrc, void* event, NuguFocusType target_type)
{
    /* Reject the focus request with same type (reject self steal) */
    if (target_type == NUGU_FOCUS_TYPE_WAKEWORD)
        return NUGU_FOCUS_STEAL_REJECT;

    return NUGU_FOCUS_STEAL_ALLOW;
}

} // NuguCore
