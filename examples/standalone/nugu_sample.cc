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

#include "capability_collection.hh"
#include "nugu_sample_manager.hh"
#include "speech_operator.hh"

using namespace NuguClientKit;

std::unique_ptr<NuguClient> nugu_client = nullptr;
std::unique_ptr<NuguSampleManager> nugu_sample_manager = nullptr;
std::unique_ptr<CapabilityCollection> capa_collection = nullptr;

template <typename T, typename... Ts>
std::unique_ptr<T> make_unique(Ts&&... params)
{
    return std::unique_ptr<T>(new T(std::forward<Ts>(params)...));
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

    void onEventSent(const char* ename, const char* msg_id, const char* dialog_id, const char* referrer_id)
    {
        std::string msg = "send event(" + std::string(ename) + ") msg_id - " + std::string(msg_id);
        msg += "\n\tdialog_id: " + std::string(dialog_id);
        if (referrer_id)
            msg += ", referrer_id: " + std::string(referrer_id);
        msg_info(msg);
    }

    void onEventResult(const char* msg_id, bool success, int code)
    {
        std::string msg = "result event - " + std::string(msg_id) + "(" + std::to_string(success) + ", code: " + std::to_string(code) + ")";
        msg_info(msg);
    }
};

void registerCapabilities()
{
    if (!nugu_client || !nugu_sample_manager)
        return;

    auto asr_handler(capa_collection->getCapability<IASRHandler>("ASR"));
    auto text_handler(capa_collection->getCapability<ITextHandler>("Text"));
    auto mic_handler(capa_collection->getCapability<IMicHandler>("Mic"));

    asr_handler->setAttribute(ASRAttribute { nugu_sample_manager->getModelPath() });
    nugu_sample_manager->setTextHandler(text_handler)
        ->setMicHandler(mic_handler);

    // create capability instance
    nugu_client->getCapabilityBuilder()
        ->add(capa_collection->getCapability<ISystemHandler>("System"))
        ->add(capa_collection->getCapability<ISpeakerHandler>("Speaker"))
        ->add(capa_collection->getCapability<ITTSHandler>("TTS"))
        ->add(capa_collection->getCapability<IAudioPlayerHandler>("AudioPlayer"))
        ->add(capa_collection->getCapability<ISoundHandler>("Sound"))
        ->add(asr_handler)
        ->add(text_handler)
        ->add(mic_handler)
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

    auto nugu_client_listener(make_unique<NuguClientListener>());
    auto network_manager_listener(make_unique<NetworkManagerListener>());

    nugu_client = make_unique<NuguClient>();
    nugu_client->setListener(nugu_client_listener.get());
    capa_collection = make_unique<CapabilityCollection>();

    registerCapabilities();

    auto network_manager(nugu_client->getNetworkManager());
    network_manager->addListener(network_manager_listener.get());
    network_manager->setToken(getenv("NUGU_TOKEN"));

    if (!network_manager->connect()) {
        msg_error("< Cannot connect to NUGU Platform.");
        return EXIT_FAILURE;
    }

    if (!nugu_client->initialize()) {
        msg_error("< It failed to initialize NUGU SDK. Please Check authorization.");
        return EXIT_FAILURE;
    }

    auto nugu_core_container(nugu_client->getNuguCoreContainer());
    auto wakeup_handler(nugu_core_container->createWakeupHandler(nugu_sample_manager->getModelPath()));
    auto speech_operator(capa_collection->getSpeechOperator());

    speech_operator->setWakeupHandler(wakeup_handler);
    nugu_sample_manager->setSpeechOperator(speech_operator)
        ->setNetworkCallback(NuguSampleManager::NetworkCallback {
            [&]() { return network_manager->connect(); },
            [&]() { return network_manager->disconnect(); } })
        ->setActionCallback(NuguSampleManager::ActionCallback {
            [&]() { nugu_core_container->getCapabilityHelper()->suspendAll(); },
            [&]() { nugu_core_container->getCapabilityHelper()->restoreAll(); } })
        ->runLoop([&] {
            msg_info("de-initialization start");

            // release resource
            if (wakeup_handler) {
                delete wakeup_handler;
                wakeup_handler = nullptr;
            }

            nugu_client->deInitialize();

            msg_info("de-initialization done");
        });

    return EXIT_SUCCESS;
}
