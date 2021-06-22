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

#include "clientkit/playsync_manager_interface.hh"

namespace NuguClientKit {
void IPlaySyncManagerListener::onSyncState(const std::string& ps_id, PlaySyncState state, void* extra_data)
{
}

void IPlaySyncManagerListener::onDataChanged(const std::string& ps_id, std::pair<void*, void*> extra_datas)
{
}

void IPlaySyncManagerListener::onStackChanged(const std::pair<std::string, std::string>& ps_ids)
{
}

} // NuguClientKit
