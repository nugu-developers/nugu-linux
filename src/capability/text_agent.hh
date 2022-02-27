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
                        public ITextHandler,
                        public IFocusResourceListener,
                        public IPlaySyncManagerListener {
public:
    TextAgent();
    virtual ~TextAgent() = default;

    void setAttribute(TextAttribute&& attribute) override;
    void initialize() override;
    void deInitialize() override;

    void parsingDirective(const char* dname, const char* message) override;
    void updateInfoForContext(Json::Value& ctx) override;
    void receiveCommandAll(const std::string& command, const std::string& param) override;
    bool getProperty(const std::string& property, std::string& value) override;
    void setCapabilityListener(ICapabilityListener* clistener) override;

    std::string requestTextInput(const std::string& text, const std::string& token = "", bool include_dialog_attribute = true) override;

    // implements IFocusResourceListener
    void onFocusChanged(FocusState state) override;

    // implements IPlaySyncManagerListener
    void onStackChanged(const std::pair<std::string, std::string>& ps_ids) override;

private:
    using TextInputParam = struct {
        std::string text;
        std::string token;
        std::string ps_id;
    };

    using ExpectTypingInfo = struct {
        bool is_handle = false;
        std::string playstack;
        Json::Value payload;
    };

    std::string requestTextInput(TextInputParam&& text_input_param, bool routine_play, bool include_dialog_attribute = true);
    void sendEventTextInput(const TextInputParam& text_input_param, bool include_dialog_attribute = true, EventResultCallback cb = nullptr);
    void sendEventTextSourceFailed(const TextInputParam& text_input_param, EventResultCallback cb = nullptr);
    void sendEventTextRedirectFailed(const TextInputParam& text_input_param, const Json::Value& payload, EventResultCallback cb = nullptr);
    void sendEventFailed(std::string&& event_name, const TextInputParam& text_input_param, Json::Value&& extra_payload, EventResultCallback cb = nullptr);
    void parsingTextSource(const char* message);
    void parsingTextRedirect(const char* message);
    void parsingExpectTyping(const char* message);
    bool handleTextCommonProcess(const TextInputParam& text_input_param);
    void notifyEventResponse(const std::string& msg_id, const std::string& data, bool success) override;
    void notifyResponseTimeout();
    void startInteractionControl(InteractionMode&& mode);
    void finishInteractionControl();
    void requestFocus();
    void releaseFocus();

    const std::string FAIL_EVENT_ERROR_CODE = "NOT_SUPPORTED_STATE";

    ITextListener* text_listener;
    INuguTimer* timer;
    INuguTimer* timer_msec;
    TextState cur_state;
    std::string cur_dialog_id;
    std::string dir_groups;
    std::string cur_playstack;
    ExpectTypingInfo expect_typing;
    InteractionMode interaction_mode;
    bool handle_interaction_control;
    FocusState focus_state;

    // attribute
    int response_timeout;
};

} // NuguCapability

#endif
