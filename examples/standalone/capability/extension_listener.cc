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

#include "extension_listener.hh"

void ExtensionListener::setExtensionHandler(IExtensionHandler* extension_handler)
{
    if (extension_handler)
        this->extension_handler = extension_handler;
}

void ExtensionListener::receiveAction(const std::string& data)
{
    std::cout << "[Extension] " << data << std::endl;

    if (extension_handler)
        extension_handler->ActionSucceeded();
}
