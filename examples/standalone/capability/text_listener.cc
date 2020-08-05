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

#include "text_listener.hh"

void TextListener::onState(TextState state, const std::string& dialog_id)
{
    if (state == prev_state && dialog_id == prev_dialog_id)
        return;

    std::cout << "[Text][id:" << dialog_id << "] ";

    switch (state) {
    case TextState::BUSY:
        std::cout << "BUSY" << std::endl;
        break;
    case TextState::IDLE:
        std::cout << "IDLE" << std::endl;
        break;
    default:
        std::cout << "UNKNOWN STATE" << std::endl;
        break;
    }

    prev_state = state;
    prev_dialog_id = dialog_id;
}

void TextListener::onComplete(const std::string& dialog_id)
{
    std::cout << "[Text][id:" << dialog_id << "] onComplete" << std::endl;
}

void TextListener::onError(TextError error, const std::string& dialog_id)
{
    std::cout << "[Text][id:" << dialog_id << "] onError: ";

    switch (error) {
    case TextError::RESPONSE_TIMEOUT:
        std::cout << "Response Timeout" << std::endl;
        break;
    default:
        std::cout << "UNKNOWN ERROR" << std::endl;
        break;
    }
}

bool TextListener::handleTextCommand(const std::string& text, const std::string& token)
{
    std::cout << "[Text] text command = " << text << " (not consume)" << std::endl;

    // return true if it consume text command
    return false;
}
