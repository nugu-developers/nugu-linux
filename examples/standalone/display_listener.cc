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

#include <chrono>

#include "display_listener.hh"
#include "nugu_log.h"

void DisplayListener::renderDisplay(const std::string& type, const std::string& json)
{
    nugu_info("got received to render display template");
}

bool DisplayListener::clearDisplay(bool unconditionally)
{
    nugu_info("got received to clear display template");

    // clear display unconditionally
    if (unconditionally) {
        return true;
    }

    // It need to decide whether clear display immediately or not.
    // If you decide to clear immediately, return true, or return false

    return false;
}
