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

DisplayListener::DisplayListener()
{
    capability_name = "Display";
}

void DisplayListener::setDisplayHandler(IDisplayHandler* display_handler)
{
    if (display_handler)
        this->display_handler = display_handler;
}

void DisplayListener::renderDisplay(const std::string& id, const std::string& type, const std::string& json, const std::string& dialog_id)
{
    std::cout << "\033[1;94m"
              << "[" << capability_name << "] render display template\n"
              << "\t id:" << id.c_str() << std::endl
              << "\t type:" << type.c_str() << std::endl
              << "\t dialog_id:" << dialog_id.c_str()
              << "\033[0m" << std::endl;

    if (display_handler)
        display_handler->displayRendered(id);
}

bool DisplayListener::clearDisplay(const std::string& id, bool unconditionally, bool has_next)
{
    std::cout << "\033[1;94m"
              << "[" << capability_name << "] clear display template\n"
              << "\t id:" << id.c_str() << std::endl
              << "\t unconditionally:" << unconditionally << std::endl
              << "\t has_next:" << has_next
              << "\033[0m" << std::endl;

    pending_clear_id = id;

    g_idle_add(
        [](gpointer userdata) {
            static_cast<DisplayListener*>(userdata)->displayCleared();
            return FALSE;
        },
        this);

    return false;
}

void DisplayListener::controlDisplay(const std::string& id, ControlType type, ControlDirection direction)
{
    std::cout << "[" << capability_name << "] control display template\n"
              << "\t id:" << id.c_str() << std::endl
              << "\t type:" << CONTROL_TYPE_TEXT.at(type) << std::endl
              << "\t direction:" << CONTROL_DIRECTION_TEXT.at(direction) << std::endl;
}

void DisplayListener::updateDisplay(const std::string& id, const std::string& json_payload)
{
    std::cout << "[" << capability_name << "] update display template\n"
              << "\t id:" << id.c_str() << std::endl;
}

void DisplayListener::displayCleared()
{
    if (display_handler)
        display_handler->displayCleared(pending_clear_id);

    pending_clear_id.clear();
}
