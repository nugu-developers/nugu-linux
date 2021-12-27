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

void ExtensionListener::setCapabilityHandler(ICapabilityInterface* handler)
{
    if (handler)
        this->extension_handler = dynamic_cast<IExtensionHandler*>(handler);
}

void ExtensionListener::receiveAction(const std::string& data, const std::string& ps_id, const std::string& dialog_id)
{
    std::cout << "[Extension] receive action\n"
              << "- playServiceId:" << ps_id << std::endl
              << "- dialogId:" << dialog_id << std::endl
              << "- data:" << data << std::endl;

    if (extension_handler)
        extension_handler->actionSucceeded();
}

void ExtensionListener::requestContext(std::string& context_info)
{
}
