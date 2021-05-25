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
    , uniq(0)
{
    wakeup_detector->setListener(this);
}

void WakeupHandler::setListener(IWakeupListener* listener)
{
    this->listener = listener;
}

bool WakeupHandler::startWakeup()
{
    std::string id = "id#" + std::to_string(uniq++);
    setWakeupId(id);
    return wakeup_detector->startWakeup(id);
}

void WakeupHandler::stopWakeup()
{
    wakeup_detector->stopWakeup();
}

void WakeupHandler::onWakeupState(WakeupState state, const std::string& id, float noise, float speech)
{
    if (request_wakeup_id != id) {
        nugu_warn("[id: %s] ignore [id: %s]'s state %d", request_wakeup_id.c_str(), id.c_str(), state);
        return;
    }

    switch (state) {
    case WakeupState::FAIL:
        nugu_dbg("[id: %s] WakeupState::FAIL", id.c_str());

        if (listener)
            listener->onWakeupState(WakeupDetectState::WAKEUP_FAIL, noise, speech);

        break;
    case WakeupState::DETECTING:
        nugu_dbg("[id: %s] WakeupState::DETECTING", id.c_str());

        if (listener)
            listener->onWakeupState(WakeupDetectState::WAKEUP_DETECTING, noise, speech);
        break;
    case WakeupState::DETECTED:
        nugu_dbg("[id: %s] WakeupState::DETECTED", id.c_str());

        if (listener)
            listener->onWakeupState(WakeupDetectState::WAKEUP_DETECTED, noise, speech);
        break;
    case WakeupState::DONE:
        nugu_dbg("[id: %s] WakeupState::DONE", id.c_str());
        break;
    }
}

void WakeupHandler::setWakeupId(const std::string& id)
{
    request_wakeup_id = id;
    nugu_dbg("startListening with new id(%s)", request_wakeup_id.c_str());
}

void WakeupHandler::changeModel(const std::string& model_path)
{
    wakeup_detector->changeModel(model_path);
}

} // NuguCore
