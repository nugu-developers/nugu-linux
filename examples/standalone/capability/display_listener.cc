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

#include "display_listener.hh"

void DisplayListener::renderDisplay(const std::string& id, const std::string& type, const std::string& json, const std::string& dialog_id)
{
    std::cout << "[Display] render display template\n"
              << "\t id:" << id.c_str() << std::endl
              << "\t type:" << type.c_str() << std::endl
              << "\t dialog_id:" << dialog_id.c_str() << std::endl;
}

bool DisplayListener::clearDisplay(const std::string& id, bool unconditionally)
{
    std::cout << "[Display] clear display template\n"
              << "\t id:" << id.c_str() << std::endl
              << "\t unconditionally:" << unconditionally << std::endl;

    return false;
}

void DisplayListener::controlDisplay(const std::string& id, ControlType type, ControlDirection direction)
{
    std::cout << "[Display] control display template\n"
              << "\t id:" << id.c_str() << std::endl
              << "\t type:" << CONTROL_TYPE_TEXT.at(type) << std::endl
              << "\t direction:" << CONTROL_DIRECTION_TEXT.at(direction) << std::endl;
}

void DisplayListener::updateDisplay(const std::string& id, const std::string& json_payload)
{
    std::cout << "[Display] update display template\n"
              << "\t id:" << id.c_str() << std::endl;
}
