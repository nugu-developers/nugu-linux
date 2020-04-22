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

#ifndef __NUGU_SAMPLE_MANAGER_H__
#define __NUGU_SAMPLE_MANAGER_H__

#include <functional>
#include <glib.h>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include <capability/mic_interface.hh>
#include <capability/text_interface.hh>

#include "speech_operator.hh"

class NuguSampleManager {
public:
    virtual ~NuguSampleManager() = default;

    using NetworkCallback = struct {
        std::function<bool()> connect;
        std::function<bool()> disconnect;
    };

    using ActionCallback = struct {
        std::function<void()> suspend_all_func;
        std::function<void()> restore_all_func;
    };

    using Commander = struct {
        bool is_connected;
        int text_input;
        NetworkCallback network_callback;
        ActionCallback action_callback;
        ITextHandler* text_handler;
        SpeechOperator* speech_operator;
        IMicHandler* mic_handler;
    };

    static void error(std::string&& message);
    static void info(std::string&& message);

    bool handleArguments(const int& argc, char** argv);
    void prepare();
    void runLoop(std::function<void()> quit_callback = nullptr);
    void quit();

    const std::string& getModelPath();

    NuguSampleManager* setNetworkCallback(NetworkCallback callback);
    NuguSampleManager* setTextHandler(ITextHandler* text_handler);
    NuguSampleManager* setSpeechOperator(SpeechOperator* speech_operator);
    NuguSampleManager* setMicHandler(IMicHandler* mic_handler);
    NuguSampleManager* setActionCallback(ActionCallback&& callback);
    void handleNetworkResult(bool is_connected, bool is_show_cmd = true);

private:
    using CommandMap = std::map<std::string, std::pair<std::string, std::function<void(NuguSampleManager*)>>>;
    using CommandKey = std::vector<std::string>;
    using CommandText = std::pair<std::string, std::string>;
    using CommandKeyIterator = std::vector<std::string>::const_iterator;

    void showPrompt(void);
    std::string composeCommand(const CommandKeyIterator& begin, const CommandKeyIterator& end);
    static gboolean onKeyInput(GIOChannel* src, GIOCondition con, gpointer userdata);

    static const char* C_RED;
    static const char* C_YELLOW;
    static const char* C_BLUE;
    static const char* C_CYAN;
    static const char* C_WHITE;
    static const char* C_RESET;
    static const CommandKey COMMAND_KEYS;
    static const CommandMap COMMAND_MAP;

    Commander commander {};
    CommandText command_text {};
    bool is_show_prompt = true;

    GMainLoop* loop;
    GMainContext* context = nullptr;
    std::string model_path = "./";
    bool is_prepared = false;
    std::function<void()> quit_func;
};

#endif /* __NUGU_SAMPLE_MANAGER_H__ */
