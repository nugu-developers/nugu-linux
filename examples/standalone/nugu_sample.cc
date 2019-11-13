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

#include <glib.h>
#include <iostream>
#include <mutex>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <interface/nugu_configuration.hh>
#include <interface/wakeup_interface.hh>

#include "audio_player_listener.hh"
#include "delegation_listener.hh"
#include "display_listener.hh"
#include "extension_listener.hh"
#include "nugu_client.hh"
#include "nugu_log.h"
#include "speech_operator.hh"
#include "system_listener.hh"
#include "text_listener.hh"
#include "tts_listener.hh"

using namespace NuguClientKit;

// define cout color
const std::string C_RED = "\033[1;91m";
const std::string C_YELLOW = "\033[1;93m";
const std::string C_BLUE = "\033[1;94m";
const std::string C_CYAN = "\033[1;96m";
const std::string C_WHITE = "\033[1;97m";
const std::string C_RESET = "\033[0m";

void msg_error(const std::string& message)
{
    std::cout << C_RED
              << message
              << std::endl
              << C_RESET;
    std::cout.flush();
}

void msg_info(const std::string& message)
{
    std::cout << C_CYAN
              << message
              << std::endl
              << C_RESET;
    std::cout.flush();
}

class NuguClientListener;

GMainContext* context;
GMainLoop* loop;

const bool REGISTER_CAPABILITIES_MANUALLY = true; // have to set TRUE currently
std::unique_ptr<NuguClient> nugu_client = nullptr;
std::unique_ptr<NuguClientListener> nugu_client_listener;
std::unique_ptr<INetworkManagerListener> network_manager_listener;
std::unique_ptr<SpeechOperator> speech_operator;
std::unique_ptr<ITTSListener> tts_listener;
std::unique_ptr<IDisplayListener> display_listener;
std::unique_ptr<IAudioPlayerListener> aplayer_listener;
std::unique_ptr<ISystemListener> system_listener;
std::unique_ptr<ITextListener> text_listener;
std::unique_ptr<IExtensionListener> extension_listener;
std::unique_ptr<IDelegationListener> delegation_listener;
ITextHandler* text_handler;
IDisplayHandler* display_handler;
IAudioPlayerHandler* aplayer_handler;
INetworkManager* network_manager;

int text_input;
bool isConnected = false;

void quit()
{
    if (loop)
        g_main_loop_quit(loop);
}

void signal_handler(int signal)
{
    quit();
}

static void _show_prompt(void)
{
    if (text_input)
        std::cout << C_WHITE
                  << "\nEnter Service > "
                  << C_RESET;
    else {
        std::cout << C_YELLOW
                  << "=======================================================\n"
                  << C_RED
                  << "NUGU SDK Command (" << (isConnected ? "Connected" : "Disconnected") << ")\n"
                  << C_YELLOW
                  << "=======================================================\n"
                  << C_BLUE;

        if (isConnected)
            std::cout << "w : start wakeup\n"
                      << "l : start listening\n"
                      << "s : stop listening\n"
                      << "t : text input\n";

        std::cout << "c : connect\n"
                  << "d : disconnect\n"
                  << "q : quit\n"
                  << C_YELLOW
                  << "-------------------------------------------------------\n"
                  << C_WHITE
                  << "Select Command > "
                  << C_RESET;
    }

    fflush(stdout);
}

static gboolean on_key_input(GIOChannel* src, GIOCondition con, gpointer userdata)
{
    char keybuf[4096];

    if (fgets(keybuf, 4096, stdin) == NULL)
        return TRUE;

    if (strlen(keybuf) > 0) {
        if (keybuf[strlen(keybuf) - 1] == '\n')
            keybuf[strlen(keybuf) - 1] = '\0';
    }

    if (strlen(keybuf) < 1) {
        _show_prompt();
        return TRUE;
    }

    // handle case when NuguClient is disconnected
    if (!isConnected) {
        if (g_strcmp0(keybuf, "q") != 0 && g_strcmp0(keybuf, "c") != 0 && g_strcmp0(keybuf, "d") != 0) {
            msg_error("invalid input");
            _show_prompt();

            return TRUE;
        }
    }

    if (text_input) {
        text_input = 0;
        if (text_handler)
            text_handler->requestTextInput(keybuf);
    } else if (g_strcmp0(keybuf, "w") == 0) {
        text_input = 0;
        if (speech_operator)
            speech_operator->startWakeup();
    } else if (g_strcmp0(keybuf, "l") == 0) {
        text_input = 0;
        if (speech_operator)
            speech_operator->startListening();
    } else if (g_strcmp0(keybuf, "s") == 0) {
        text_input = 0;
        if (speech_operator)
            speech_operator->stopListening();
        _show_prompt();
    } else if (g_strcmp0(keybuf, "t") == 0) {
        text_input = 1;
        _show_prompt();
    } else if (g_strcmp0(keybuf, "c") == 0) {
        if (isConnected) {
            msg_info("It's already connected.");
            _show_prompt();
        } else {
            network_manager->connect();
        }
    } else if (g_strcmp0(keybuf, "d") == 0) {
        if (network_manager->disconnect()) {
            isConnected = false;
            _show_prompt();
        }
    } else if (g_strcmp0(keybuf, "q") == 0) {
        g_main_loop_quit((GMainLoop*)userdata);
    } else {
        msg_error("invalid input");
        _show_prompt();
    }

    return TRUE;
}

class NuguClientListener : public INuguClientListener {
public:
    NuguClientListener() = default;

    void onInitialized(void* userdata)
    {
    }

    void notify(std::string c_name, CapabilitySignal signal, void* data)
    {
        switch (signal) {
        case SYSTEM_INTERNAL_SERVICE_EXCEPTION:
            nugu_error("[%s] server exception", c_name.c_str());
            break;
        case DIALOG_REQUEST_ID:
            if (data)
                nugu_info("DIALOG_REQUEST_ID = %s", data);
            break;
        }
    }
};

class NetworkManagerListener : public INetworkManagerListener {
public:
    void onConnected()
    {
        msg_info("Network connected.");

        handleResult(true);
    }

    void onDisconnected()
    {
        msg_info("Network disconnected.");

        handleResult(false);
    }

    void onError(NetworkError error)
    {
        switch (error) {
        case NetworkError::TOKEN_ERROR:
            msg_error("Network error [TOKEN_ERROR].");
            break;
        case NetworkError::UNKNOWN:
            msg_error("Network error [UNKNOWN].");
            break;
        }

        handleResult(false);
    }

private:
    void handleResult(bool result)
    {
        isConnected = result;
        _show_prompt();
    }
};

void registerCapabilities()
{
    if (!nugu_client)
        return;

    speech_operator = std::unique_ptr<SpeechOperator>(new SpeechOperator());
    tts_listener = std::unique_ptr<ITTSListener>(new TTSListener());
    display_listener = std::unique_ptr<IDisplayListener>(new DisplayListener());
    aplayer_listener = std::unique_ptr<IAudioPlayerListener>(new AudioPlayerListener());
    system_listener = std::unique_ptr<ISystemListener>(new SystemListener());
    text_listener = std::unique_ptr<ITextListener>(new TextListener());
    extension_listener = std::unique_ptr<IExtensionListener>(new ExtensionListener());
    delegation_listener = std::unique_ptr<IDelegationListener>(new DelegationListener());

    // create capability instance. It's possible to set user customized capability using below builder
    nugu_client->getCapabilityBuilder()
        ->add(CapabilityType::ASR, speech_operator->getASRListener())
        ->add(CapabilityType::TTS, tts_listener.get())
        ->add(CapabilityType::AudioPlayer, aplayer_listener.get())
        ->add(CapabilityType::System, system_listener.get())
        ->add(CapabilityType::Display, display_listener.get())
        ->add(CapabilityType::Text, text_listener.get())
        ->add(CapabilityType::Extension, extension_listener.get())
        ->add(CapabilityType::Delegation, delegation_listener.get())
        ->construct();
}

int main(int argc, char** argv)
{
    int c;
    std::string model_path = "./";

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
            return 0;
        }
    }

    std::cout << "=======================================================\n";
    std::cout << "User Application\n";
    std::cout << " - Model path: " << model_path << std::endl;
    std::cout << "=======================================================\n";

    signal(SIGINT, &signal_handler);

    context = g_main_context_default();
    loop = g_main_loop_new(context, FALSE);

    GIOChannel* channel = g_io_channel_unix_new(STDIN_FILENO);
    g_io_add_watch(channel, (GIOCondition)(G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL), on_key_input, loop);
    g_io_channel_unref(channel);

    // if no token exist, exit application
    if (!getenv("NUGU_TOKEN")) {
        msg_error("< Token is empty");

        return EXIT_FAILURE;
    }

    nugu_client_listener = std::unique_ptr<NuguClientListener>(new NuguClientListener());
    network_manager_listener = std::unique_ptr<NetworkManagerListener>(new NetworkManagerListener());

    nugu_client = std::unique_ptr<NuguClient>(new NuguClient());
    nugu_client->setConfig(NuguConfig::Key::ACCESS_TOKEN, getenv("NUGU_TOKEN"));
    nugu_client->setConfig(NuguConfig::Key::MODEL_PATH, model_path);
    nugu_client->setListener(nugu_client_listener.get());

    if (REGISTER_CAPABILITIES_MANUALLY)
        registerCapabilities();

    network_manager = nugu_client->getNetworkManager();
    network_manager->addListener(network_manager_listener.get());

    if (!network_manager->connect()) {
        msg_error("< Cannot connect to NUGU Platform.");
        return EXIT_FAILURE;
    }

    if (!nugu_client->initialize()) {
        msg_error("< It failed to initialize NUGU SDK. Please Check authorization.");
    } else {
        text_handler = nugu_client->getTextHandler();

        if (speech_operator)
            speech_operator->setWakeupHandler(nugu_client->getWakeupHandler());

        g_main_loop_run(loop);
        g_main_loop_unref(loop);

        msg_info("mainloop exited");
    }

    nugu_client->deInitialize();

    return EXIT_SUCCESS;
}
