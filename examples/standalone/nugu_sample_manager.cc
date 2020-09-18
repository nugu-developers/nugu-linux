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
#include <iostream>
#include <string.h>
#include <unistd.h>

#include "base/nugu_log.h"
#include "nugu.h"

#include "nugu_sample_manager.hh"

const char* NuguSampleManager::C_RED = "\033[1;91m";
const char* NuguSampleManager::C_YELLOW = "\033[1;93m";
const char* NuguSampleManager::C_BLUE = "\033[1;94m";
const char* NuguSampleManager::C_CYAN = "\033[1;96m";
const char* NuguSampleManager::C_WHITE = "\033[1;97m";
const char* NuguSampleManager::C_RESET = "\033[0m";

// NOLINTNEXTLINE (cert-err58-cpp)
const NuguSampleManager::CommandKey NuguSampleManager::COMMAND_KEYS {
    "w", "l", "s", "t", "t2", "m", "sa", "ra", "c", "d", "q"
};

// NOLINTNEXTLINE (cert-err58-cpp)
const NuguSampleManager::CommandMap NuguSampleManager::COMMAND_MAP {
    { "w", { "start listening with wakeup", [](NuguSampleManager* ns_mgr) {
                if (ns_mgr->commander.speech_operator)
                    ns_mgr->commander.speech_operator->startListeningWithWakeup();
            } } },
    { "l", { "start listening", [](NuguSampleManager* ns_mgr) {
                if (ns_mgr->commander.speech_operator)
                    ns_mgr->commander.speech_operator->startListening();
            } } },
    { "s", { "stop listening/wakeup", [](NuguSampleManager* ns_mgr) {
                if (ns_mgr->commander.speech_operator)
                    ns_mgr->commander.speech_operator->stopListeningAndWakeup();
            } } },
    { "t", { "text input", [](NuguSampleManager* ns_mgr) {
                ns_mgr->commander.text_input = 1;
                ns_mgr->showPrompt();
            } } },
    { "t2", { "text input (ignore dialog attribute)", [](NuguSampleManager* ns_mgr) {
                 ns_mgr->commander.text_input = 2;
                 ns_mgr->showPrompt();
             } } },
    { "m", { "set mic mute", [](NuguSampleManager* ns_mgr) {
                static bool mute = false;
                mute = !mute;
                !mute ? ns_mgr->commander.mic_handler->enable() : ns_mgr->commander.mic_handler->disable();
                ns_mgr->showPrompt();
            } } },
    { "sa", { "suspend all", [](NuguSampleManager* ns_mgr) {
                 if (ns_mgr->commander.action_callback.suspend_all_func)
                     ns_mgr->commander.action_callback.suspend_all_func();
             } } },
    { "ra", { "restore all", [](NuguSampleManager* ns_mgr) {
                 if (ns_mgr->commander.action_callback.restore_all_func)
                     ns_mgr->commander.action_callback.restore_all_func();
             } } },
    { "c", { "connect", [](NuguSampleManager* ns_mgr) {
                if (ns_mgr->commander.is_connected) {
                    info("It's already connected.");
                    ns_mgr->showPrompt();
                } else {
                    if (ns_mgr->commander.network_callback.connect)
                        ns_mgr->commander.network_callback.connect();
                }
            } } },
    { "d", { "disconnect", [](NuguSampleManager* ns_mgr) {
                if (ns_mgr->commander.network_callback.disconnect && ns_mgr->commander.network_callback.disconnect())
                    ns_mgr->commander.is_connected = false;
            } } },
    { "q", { "quit", [](NuguSampleManager* ns_mgr) {
                ns_mgr->quit();
            } } }
};

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

    // compose command text
    const CommandKeyIterator& command_pivot = std::find(COMMAND_KEYS.cbegin(), COMMAND_KEYS.cend(), "c");
    command_text.first = composeCommand(COMMAND_KEYS.cbegin(), command_pivot);
    command_text.second = composeCommand(command_pivot, COMMAND_KEYS.cend());

    context = g_main_context_default();
    loop = g_main_loop_new(context, FALSE);

    GIOChannel* channel = g_io_channel_unix_new(STDIN_FILENO);
    g_io_add_watch(channel, (GIOCondition)(G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL), onKeyInput, this);
    g_io_channel_unref(channel);

    is_prepared = true;
}

void NuguSampleManager::runLoop(std::function<void()> quit_callback)
{
    if (quit_callback != nullptr)
        quit_func = std::move(quit_callback);

    if (loop && is_prepared) {
        g_main_loop_run(loop);
        g_main_loop_unref(loop);

        info("mainloop exited");
    }
}

void NuguSampleManager::quit()
{
    if (quit_func)
        quit_func();

    g_main_loop_quit(loop);
}

const std::string& NuguSampleManager::getModelPath()
{
    return model_path;
}

NuguSampleManager* NuguSampleManager::setNetworkCallback(NetworkCallback callback)
{
    commander.network_callback = std::move(callback);

    return this;
}

NuguSampleManager* NuguSampleManager::setTextHandler(ITextHandler* text_handler)
{
    commander.text_handler = text_handler;

    return this;
}

NuguSampleManager* NuguSampleManager::setSpeechOperator(SpeechOperator* speech_operator)
{
    commander.speech_operator = speech_operator;

    return this;
}

NuguSampleManager* NuguSampleManager::setMicHandler(IMicHandler* mic_handler)
{
    commander.mic_handler = mic_handler;

    return this;
}

NuguSampleManager* NuguSampleManager::setActionCallback(ActionCallback&& callback)
{
    commander.action_callback = std::move(callback);

    return this;
}

void NuguSampleManager::handleNetworkResult(bool is_connected, bool is_show_cmd)
{
    commander.is_connected = is_connected;
    is_show_prompt = is_show_cmd;

    showPrompt();
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

        // display commands about nugu control
        if (commander.is_connected) {
            std::cout << command_text.first
                      << C_YELLOW
                      << "-------------------------------------------------------\n"
                      << C_BLUE;
        }

        // display commands about network, quit
        std::cout << command_text.second
                  << C_YELLOW
                  << "-------------------------------------------------------\n"
                  << C_WHITE
                  << "Select Command > "
                  << C_RESET;
    }

    fflush(stdout);
}

std::string NuguSampleManager::composeCommand(const CommandKeyIterator& begin, const CommandKeyIterator& end)
{
    std::string composed_command;

    std::for_each(begin, end, [&](const std::string& key) {
        try {
            composed_command.append(key)
                .append(" : ")
                .append(COMMAND_MAP.at(key).first)
                .append("\n");
        } catch (const std::out_of_range& oor) {
            // skip silently
        }
    });

    return composed_command;
}

gboolean NuguSampleManager::onKeyInput(GIOChannel* src, GIOCondition con, gpointer userdata)
{
    char keybuf[4096];
    NuguSampleManager* ns_mgr = static_cast<NuguSampleManager*>(userdata);

    if (fgets(keybuf, 4096, stdin) == NULL)
        return TRUE;

    if (strlen(keybuf) > 0) {
        if (keybuf[strlen(keybuf) - 1] == '\n')
            keybuf[strlen(keybuf) - 1] = '\0';
    }

    if (strlen(keybuf) < 1) {
        ns_mgr->showPrompt();
        return TRUE;
    }

    // handle case when NuguClient is disconnected
    if (!ns_mgr->commander.is_connected) {
        if (g_strcmp0(keybuf, "q") != 0 && g_strcmp0(keybuf, "c") != 0 && g_strcmp0(keybuf, "d") != 0) {
            error("invalid input");
            ns_mgr->showPrompt();
            return TRUE;
        }
    }

    if (ns_mgr->commander.text_input) {
        if (ns_mgr->commander.text_handler)
            ns_mgr->commander.text_handler->requestTextInput(keybuf, "", (ns_mgr->commander.text_input == 1));

        ns_mgr->commander.text_input = 0;
    } else {
        try {
            // It has to send ns_mgr (NuguSampleManager* instance) parameter mandatorily
            COMMAND_MAP.at(std::string { keybuf }).second(ns_mgr);
        } catch (const std::out_of_range& oor) {
            error("No command exist!");
        }
    }

    return TRUE;
}
