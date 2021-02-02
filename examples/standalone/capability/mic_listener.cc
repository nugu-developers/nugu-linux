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

#include "mic_listener.hh"
#include "speaker_status.hh"

void MicListener::micStatusChanged(MicStatus& status)
{
    switch (status) {
    case MicStatus::ON:
        std::cout << "[MIC] ON..." << std::endl;
        SpeakerStatus::getInstance()->setMicMute(false);
        break;

    case MicStatus::OFF:
        std::cout << "[MIC] OFF..." << std::endl;
        SpeakerStatus::getInstance()->setMicMute(true);
        break;
    }
}
