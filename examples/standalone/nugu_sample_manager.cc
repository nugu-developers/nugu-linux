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

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string.h>
#include <unistd.h>

#include <base/nugu_log.h>
#include <nugu.h>

#include "nugu_sample_manager.hh"

const char* NuguSampleManager::C_RED = "\033[1;91m";
const char* NuguSampleManager::C_GREEN = "\033[1;92m";
const char* NuguSampleManager::C_YELLOW = "\033[1;93m";
const char* NuguSampleManager::C_BLUE = "\033[1;94m";
const char* NuguSampleManager::C_CYAN = "\033[1;96m";
const char* NuguSampleManager::C_WHITE = "\033[1;97m";
const char* NuguSampleManager::C_RESET = "\033[0m";

void NuguSampleManager::error(std::string&& message)
{
    std::cout << C_RED
              << message
              << std::endl
              << C_RESET;
    std::cout.flush();
}

void NuguSampleManager::info(std::string&& message)
{
    std::cout << C_CYAN
              << message
              << std::endl
              << C_RESET;
    std::cout.flush();
}

NuguSampleManager::CommandBuilder::CommandBuilder(NuguSampleManager* sample_manager)
{
    parent = sample_manager;
}

NuguSampleManager::CommandBuilder* NuguSampleManager::CommandBuilder::add(const std::string& key, Command&& command)
{
    if (!key.empty()) {
        parent->commands.first.emplace_back(key);
        parent->commands.second.emplace(key, command);
    }

    return this;
}

void NuguSampleManager::CommandBuilder::compose(std::string&& divider)
{
    if (!divider.empty()) {
        auto pivot(std::find(parent->commands.first.cbegin(), parent->commands.first.cend(), divider));
        parent->command_text.second = parent->composeCommand(parent->commands.first.cbegin(), pivot);
        parent->command_text.first = parent->composeCommand(pivot, parent->commands.first.cend());
    } else {
        parent->command_text.second = parent->composeCommand(parent->commands.first.cbegin(), parent->commands.first.cend());
        parent->showPrompt();
    }
}

NuguSampleManager::NuguSampleManager()
    : command_builder(new CommandBuilder(this))
{
}

NuguSampleManager::~NuguSampleManager()
{
    if (command_builder) {
        delete command_builder;
        command_builder = nullptr;
    }
}

bool NuguSampleManager::handleArguments(const int& argc, char** argv)
{
    int c;

    nugu_log_set_system(NUGU_LOG_SYSTEM_NONE);

    while ((c = getopt(argc, argv, "dm:")) != -1) {
        switch (c) {
        case 'd':
            nugu_log_set_system(NUGU_LOG_SYSTEM_STDERR);
            break;
        case 'm':
            model_path = optarg;
            break;
        default:
            std::cout << "Usage: " << argv[0] << " [-d] [-m model-path]" << std::endl;
            return false;
        }
    }

    return true;
}

void NuguSampleManager::handleNetworkResult(bool is_connected, bool is_show_cmd)
{
    commander.is_connected = is_connected;
    is_show_prompt = is_show_cmd;

    if (commander.is_connected)
        showPrompt();
}

void NuguSampleManager::setTextCommander(TextCommander&& text_commander)
{
    commander.text_commander = text_commander;
}

void NuguSampleManager::setPlayStackRetriever(StatusRetriever&& playstack_retriever)
{
    commander.playstack_retriever = playstack_retriever;
}

void NuguSampleManager::setMicStatusRetriever(StatusRetriever&& mic_status_retriever)
{
    commander.mic_status_retriever = mic_status_retriever;
}

const std::string& NuguSampleManager::getModelPath()
{
    return model_path;
}

NuguSampleManager::CommandBuilder* NuguSampleManager::getCommandBuilder()
{
    return command_builder;
}

void NuguSampleManager::prepare()
{
    if (is_prepared) {
        nugu_warn("It's already prepared.");
        return;
    }

    std::cout << "=======================================================\n";
    std::cout << "User Application (SDK Version: " << NUGU_VERSION << ")\n";
    std::cout << " - Model path: " << model_path << std::endl;
    std::cout << "=======================================================\n";

    context = g_main_context_default();
    loop = g_main_loop_new(context, FALSE);

    GIOChannel* channel = g_io_channel_unix_new(STDIN_FILENO);
    g_io_add_watch(channel, (GIOCondition)(G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL), onKeyInput, this);
    g_io_channel_unref(channel);

    is_prepared = true;
}

void NuguSampleManager::runLoop()
{
    if (loop && is_prepared) {
        g_main_loop_run(loop);
        g_main_loop_unref(loop);

        info("mainloop exited");
    }
}

void NuguSampleManager::reset()
{
    commands.first.clear();
    commands.second.clear();
    command_text = std::make_pair("", "");
    is_show_prompt = true;
}

void NuguSampleManager::quit()
{
    g_main_loop_quit(loop);
}

void NuguSampleManager::showPrompt(void)
{
    if (!is_show_prompt)
        return;

    if (commander.text_input)
        std::cout << C_WHITE
                  << "\nEnter Service > "
                  << C_RESET;
    else {
        std::cout << C_YELLOW
                  << std::endl
                  << "=======================================================\n"
                  << C_RED
                  << "NUGU SDK Command (" << (commander.is_connected ? "Connected" : "Disconnected") << ")\n"
                  << C_YELLOW
                  << "=======================================================\n"
                  << C_BLUE;

        // display commands about nugu service control
        if (commander.is_connected) {
            std::cout << command_text.first
                      << C_YELLOW
                      << "-------------------------------------------------------\n"
                      << C_BLUE;
        }

        // display commands about sdk life cycle control
        std::cout << command_text.second
                  << C_YELLOW
                  << "-------------------------------------------------------\n";

        // show current PlayStack info
        if (commander.is_connected) {
            std::cout << C_GREEN
                      << "[MIC] " << (commander.mic_status_retriever ? commander.mic_status_retriever() : "") << " / "
                      << "[PlayStack] " << (commander.playstack_retriever ? commander.playstack_retriever() : "")
                      << std::endl;
        }

        std::cout << C_WHITE
                  << "Select Command > "
                  << C_RESET;
    }

    fflush(stdout);
}

std::string NuguSampleManager::composeCommand(const CommandKeyIterator& begin, const CommandKeyIterator& end)
{
    std::stringstream command_stream;

    std::for_each(begin, end, [&](const std::string& key) {
        try {
            command_stream << "[" << std::setw(2) << key << "] : "
                           << commands.second.at(key).first << std::endl;
        } catch (const std::out_of_range& oor) {
            // skip silently
        }
    });

    return command_stream.str();
}

gboolean NuguSampleManager::onKeyInput(GIOChannel* src, GIOCondition con, gpointer userdata)
{
    char keybuf[4096];
    NuguSampleManager* ns_mgr = static_cast<NuguSampleManager*>(userdata);

    if (fgets(keybuf, 4096, stdin) == NULL)
        return TRUE;

    if (strlen(keybuf) > 0 && keybuf[strlen(keybuf) - 1] == '\n')
        keybuf[strlen(keybuf) - 1] = '\0';

    if (strlen(keybuf) < 1) {
        ns_mgr->showPrompt();
        return TRUE;
    }

    if (ns_mgr->commander.text_input) {
        if (ns_mgr->commander.text_commander)
            ns_mgr->commander.text_commander(keybuf, (ns_mgr->commander.text_input == 1));
        ns_mgr->commander.text_input = 0;
    } else {
        try {
            ns_mgr->commands.second.at(std::string { keybuf }).second(ns_mgr->commander.text_input);

            if (ns_mgr->commander.text_input)
                ns_mgr->showPrompt();
        } catch (const std::out_of_range& oor) {
            error("No command exist!");
        }
    }

    return TRUE;
}
