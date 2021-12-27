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

#ifndef __EXTENSION_LISTENER_H__
#define __EXTENSION_LISTENER_H__

#include <capability/extension_interface.hh>

#include "capability_listener.hh"

using namespace NuguCapability;

class ExtensionListener : public IExtensionListener,
                          public CapabilityListener {
public:
    virtual ~ExtensionListener() = default;

    void setCapabilityHandler(ICapabilityInterface* handler) override;
    void receiveAction(const std::string& data, const std::string& ps_id, const std::string& dialog_id) override;
    void requestContext(std::string& context_info) override;

private:
    IExtensionHandler* extension_handler = nullptr;
};

#endif /* __EXTENSION_LISTENER_H__ */
