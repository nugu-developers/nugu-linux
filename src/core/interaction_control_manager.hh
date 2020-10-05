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

#ifndef __NUGU_INTERACTION_CONTROL_MANAGER_H__
#define __NUGU_INTERACTION_CONTROL_MANAGER_H__

#include <set>
#include <vector>

#include "clientkit/interaction_control_manager_interface.hh"

namespace NuguCore {

using namespace NuguClientKit;

class InteractionControlManager : public IInteractionControlManager {
public:
    InteractionControlManager() = default;
    virtual ~InteractionControlManager();

    void reset();
    void addListener(IInteractionControlManagerListener* listener) override;
    void removeListener(IInteractionControlManagerListener* listener) override;
    int getListenerCount();

    void notifyHasMultiTurn() override;
    void start(InteractionMode mode, const std::string& requester) override;
    void finish(InteractionMode mode, const std::string& requester) override;
    void clear() override;

private:
    void notifyModeChange(bool is_multi_turn);
    void clearContainer();

    std::vector<IInteractionControlManagerListener*> listeners;
    std::set<std::string> requester_set;
};

} // NuguCore

#endif /* __NUGU_INTERACTION_CONTROL_MANAGER_H__ */
