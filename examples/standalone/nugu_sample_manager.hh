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
#include <memory>
#include <string>
#include <vector>

class NuguSampleManager {
public:
    using TextCommander = std::function<void(const std::string&, bool)>;
    using Command = std::pair<std::string, std::function<void(int& flag)>>;
    using Commands = std::pair<std::vector<std::string>, std::map<std::string, Command>>;
    using Commander = struct {
        bool is_connected;
        int text_input;
        TextCommander text_commander;
    };

    class CommandBuilder {
    public:
        CommandBuilder* add(const std::string& key, Command&& command);
        void compose(std::string&& divider = "");

    private:
        friend class NuguSampleManager;

        explicit CommandBuilder(NuguSampleManager* sample_manager);
        virtual ~CommandBuilder() = default;

        NuguSampleManager* parent;
    };

public:
    NuguSampleManager();
    virtual ~NuguSampleManager();

    static void error(std::string&& message);
    static void info(std::string&& message);

    bool handleArguments(const int& argc, char** argv);
    void handleNetworkResult(bool is_connected, bool is_show_cmd = true);
    void setTextCommander(TextCommander&& text_commander);

    const std::string& getModelPath();
    CommandBuilder* getCommandBuilder();

    void prepare();
    void runLoop();
    void reset();
    void quit();

private:
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

    GMainLoop* loop;
    GMainContext* context = nullptr;
    std::string model_path = "./";
    bool is_prepared = false;

    Commands commands {};
    CommandBuilder* command_builder = nullptr;
    Commander commander {};
    CommandText command_text {};
    bool is_show_prompt = true;
};

#endif /* __NUGU_SAMPLE_MANAGER_H__ */
