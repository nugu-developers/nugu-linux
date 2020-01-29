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

#include <clientkit/nugu_client.hh>
#include <interface/nugu_configuration.hh>

#include "audio_player_listener.hh"
#include "delegation_listener.hh"
#include "display_listener.hh"
#include "extension_listener.hh"
#include "location_listener.hh"
#include "mic_listener.hh"
#include "speech_operator.hh"
#include "system_listener.hh"
#include "text_listener.hh"
#include "tts_listener.hh"

#include "nugu_sample_manager.hh"

using namespace NuguClientKit;

std::unique_ptr<NuguClient> nugu_client = nullptr;
std::unique_ptr<NuguSampleManager> nugu_sample_manager = nullptr;
std::unique_ptr<SpeechOperator> speech_operator = nullptr;
std::unique_ptr<TTSListener> tts_listener = nullptr;
std::unique_ptr<DisplayListener> display_listener = nullptr;
std::unique_ptr<AudioPlayerListener> aplayer_listener = nullptr;
std::unique_ptr<SystemListener> system_listener = nullptr;
std::unique_ptr<TextListener> text_listener = nullptr;
std::unique_ptr<ExtensionListener> extension_listener = nullptr;
std::unique_ptr<DelegationListener> delegation_listener = nullptr;
std::unique_ptr<LocationListener> location_listener = nullptr;
std::unique_ptr<MicListener> mic_listener = nullptr;

template <typename T, typename... Ts>
std::unique_ptr<T> make_unique(Ts&&... params)
{
    return std::unique_ptr<T>(new T(std::forward<Ts>(params)...));
}

template <class T>
T getCapabilityHandler(const std::string& cname)
{
    if (nugu_client)
        return dynamic_cast<T>(nugu_client->getCapabilityHandler(cname));

    return nullptr;
}

void msg_error(const std::string& message)
{
    NuguSampleManager::error(message);
}

void msg_info(const std::string& message)
{
    NuguSampleManager::info(message);
}

class NuguClientListener : public INuguClientListener {
public:
    void onInitialized(void* userdata)
    {
    }

    void notify(std::string c_name, CapabilitySignal signal, void* data)
    {
        switch (signal) {
        case CapabilitySignal::DIALOG_REQUEST_ID:
            if (data)
                std::cout << "[NuguClient] DIALOG_REQUEST_ID = " << data << std::endl;
            break;
        }
    }
};

class NetworkManagerListener : public INetworkManagerListener {
public:
    void onStatusChanged(NetworkStatus status)
    {
        switch (status) {
        case NetworkStatus::DISCONNECTED:
            msg_info("Network disconnected.");
            nugu_sample_manager->handleNetworkResult(false);
            break;
        case NetworkStatus::CONNECTED:
            msg_info("Network connected.");
            nugu_sample_manager->handleNetworkResult(true);
            break;
        case NetworkStatus::CONNECTING:
            msg_info("Network connection in progress.");
            nugu_sample_manager->handleNetworkResult(false, false);
            break;
        default:
            break;
        }
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

        nugu_sample_manager->handleNetworkResult(false);
    }
};

void registerCapabilities()
{
    if (!nugu_client)
        return;

    // create capability instance. It's possible to set user customized capability using below builder
    nugu_client->getCapabilityBuilder()
        ->add("ASR", speech_operator->getASRListener())
        ->add("TTS", tts_listener.get())
        ->add("AudioPlayer", aplayer_listener.get())
        ->add("System", system_listener.get())
        ->add("Display", display_listener.get())
        ->add("Text", text_listener.get())
        ->add("Extension", extension_listener.get())
        ->add("Delegation", delegation_listener.get())
        ->add("Location", location_listener.get())
        ->add("Mic", mic_listener.get())
        ->construct();
}

int main(int argc, char** argv)
{
    nugu_sample_manager = make_unique<NuguSampleManager>();
    if (!nugu_sample_manager->handleArguments(argc, argv))
        return EXIT_FAILURE;

    if (!getenv("NUGU_TOKEN")) {
        msg_error("< Token is empty");
        return EXIT_FAILURE;
    }

    nugu_sample_manager->prepare();

    speech_operator = make_unique<SpeechOperator>();
    tts_listener = make_unique<TTSListener>();
    display_listener = make_unique<DisplayListener>();
    aplayer_listener = make_unique<AudioPlayerListener>();
    system_listener = make_unique<SystemListener>();
    text_listener = make_unique<TextListener>();
    extension_listener = make_unique<ExtensionListener>();
    delegation_listener = make_unique<DelegationListener>();
    location_listener = make_unique<LocationListener>();
    mic_listener = make_unique<MicListener>();

    auto nugu_client_listener(make_unique<NuguClientListener>());
    auto network_manager_listener(make_unique<NetworkManagerListener>());

    nugu_client = make_unique<NuguClient>();
    nugu_client->setConfig(NuguConfig::Key::MODEL_PATH, nugu_sample_manager->getModelPath());
    nugu_client->setListener(nugu_client_listener.get());

    registerCapabilities();

    INetworkManager* network_manager = nugu_client->getNetworkManager();
    network_manager->addListener(network_manager_listener.get());
    network_manager->setToken(getenv("NUGU_TOKEN"));

    if (!network_manager->connect()) {
        msg_error("< Cannot connect to NUGU Platform.");
        return EXIT_FAILURE;
    }

    if (!nugu_client->initialize()) {
        msg_error("< It failed to initialize NUGU SDK. Please Check authorization.");
    } else {
        speech_operator->setWakeupHandler(nugu_client->getWakeupHandler());
        speech_operator->setASRHandler(getCapabilityHandler<IASRHandler*>("ASR"));

        nugu_sample_manager->setTextHandler(getCapabilityHandler<ITextHandler*>("Text"))
            ->setSpeechOperator(speech_operator.get())
            ->setNetworkCallback(NuguSampleManager::NetworkCallback {
                [&]() { return network_manager->connect(); },
                [&]() { return network_manager->disconnect(); } })
            ->setMicHandler(getCapabilityHandler<IMicHandler*>("Mic"))
            ->runLoop();
    }

    nugu_client->deInitialize();

    return EXIT_SUCCESS;
}
