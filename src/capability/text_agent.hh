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

#ifndef __NUGU_TEXT_AGENT_H__
#define __NUGU_TEXT_AGENT_H__

#include "capability/text_interface.hh"
#include "clientkit/capability.hh"

namespace NuguCapability {

class TextAgent final : public Capability,
                        public ITextHandler {
public:
    TextAgent();
    virtual ~TextAgent();

    void setAttribute(TextAttribute&& attribute) override;
    void initialize() override;
    void deInitialize() override;

    void parsingDirective(const char* dname, const char* message) override;
    void updateInfoForContext(Json::Value& ctx) override;
    void receiveCommandAll(const std::string& command, const std::string& param) override;
    void setCapabilityListener(ICapabilityListener* clistener) override;

    std::string requestTextInput(const std::string& text) override;
    void notifyResponseTimeout();

private:
    void sendEventTextInput(const std::string& text, const std::string& token, EventResultCallback cb = nullptr);
    void parsingTextSource(const char* message);

    ITextListener* text_listener;
    INuguTimer* timer;
    INuguTimer* timer_msec;
    TextState cur_state;
    std::string cur_dialog_id;
    std::string dir_groups;

    // attribute
    int response_timeout;
};

} // NuguCapability

#endif
