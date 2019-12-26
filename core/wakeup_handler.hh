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

#ifndef __NUGU_WAKEUP_HANDLER_H__
#define __NUGU_WAKEUP_HANDLER_H__

#include <memory>

#include "interface/wakeup_interface.hh"

#include "capability_manager.hh"
#include "wakeup_detector.hh"

namespace NuguCore {

using namespace NuguInterface;

class WakeupHandler : public IWakeupHandler,
                      public IWakeupDetectorListener {
public:
    WakeupHandler();
    ~WakeupHandler();

    void setListener(IWakeupListener* listener) override;
    void startWakeup(void) override;
    void stopWakeup(void) override;
    void onWakeupState(WakeupState state) override;

private:
    IWakeupListener* listener = nullptr;
    std::unique_ptr<WakeupDetector> wakeup_detector;
};
} // NuguCore

#endif /* __NUGU_WAKEUP_HANDLER_H__ */
