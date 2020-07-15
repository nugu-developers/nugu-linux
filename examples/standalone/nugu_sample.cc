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

#include <base/nugu_prof.h>
#include <clientkit/nugu_client.hh>

#include "capability_collection.hh"
#include "nugu_sample_manager.hh"

using namespace NuguClientKit;

std::unique_ptr<NuguClient> nugu_client = nullptr;
std::unique_ptr<NuguSampleManager> nugu_sample_manager = nullptr;
std::unique_ptr<CapabilityCollection> capa_collection = nullptr;
INuguCoreContainer* nugu_core_container = nullptr;

template <typename T, typename... Ts>
std::unique_ptr<T> make_unique(Ts&&... params)
{
    return std::unique_ptr<T>(new T(std::forward<Ts>(params)...));
}

void msg_error(std::string&& message)
{
    NuguSampleManager::error(std::move(message));
}

void msg_info(std::string&& message)
{
    NuguSampleManager::info(std::move(message));
}

class FocusManagerObserver : public IFocusManagerObserver {
public:
    void onFocusChanged(const FocusConfiguration& configuration, FocusState state, const std::string& name)
    {
        auto focus_manager(nugu_core_container->getCapabilityHelper()->getFocusManager());
        std::string msg;

        msg.append("==================================================\n[")
            .append(configuration.type)
            .append(" - ")
            .append(name)
            .append("] ")
            .append(focus_manager->getStateString(state))
            .append(" (priority: ")
            .append(std::to_string(configuration.priority))
            .append(")\n==================================================");

        msg_info(std::move(msg));
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

    void onEventSend(NuguEvent *nev)
    {
        std::string msg;
        msg.append("send event(")
            .append(nugu_event_peek_namespace(nev))
            .append(".")
            .append(nugu_event_peek_name(nev))
            .append(")\n\tmsg_id: ")
            .append(nugu_event_peek_msg_id(nev))
            .append("\n\tdialog_id: ")
            .append(nugu_event_peek_dialog_id(nev));

        if (nugu_event_peek_referrer_id(nev))
            msg.append("\n\treferrer_id: ")
                .append(nugu_event_peek_referrer_id(nev));

        msg_info(std::move(msg));
    }

    void onEventAttachmentSend(NuguEvent* nev, int seq, bool is_end, size_t length, unsigned char* data)
    {
        std::string msg;
        msg.append("send attachment(")
                    .append(nugu_event_peek_namespace(nev))
            .append(".")
            .append(nugu_event_peek_name(nev))
            .append(", seq=")
            .append(std::to_string(seq))
            .append(", is_end=")
            .append(std::to_string(is_end))
            .append(", length=")
            .append(std::to_string(length));

        msg_info(std::move(msg));
    }

    void onEventResult(const char* msg_id, bool success, int code)
    {
        std::string msg;
        msg.append("result event - ")
            .append(msg_id)
            .append("(")
            .append(std::to_string(success))
            .append(", code: ")
            .append(std::to_string(code))
            .append(")");

        msg_info(std::move(msg));
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
        ->add(capa_collection->getCapability<ISessionHandler>("Session"))
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

    nugu_client = make_unique<NuguClient>();
    capa_collection = make_unique<CapabilityCollection>();
    nugu_core_container = nugu_client->getNuguCoreContainer();

    registerCapabilities();

    if (!nugu_client->initialize()) {
        msg_error("< It failed to initialize NUGU SDK. Please Check authorization.");
        return EXIT_FAILURE;
    }

    auto focus_manager(nugu_core_container->getCapabilityHelper()->getFocusManager());
    auto focus_manager_observer(make_unique<FocusManagerObserver>());
    focus_manager->addObserver(focus_manager_observer.get());

    auto network_manager_listener(make_unique<NetworkManagerListener>());
    auto network_manager(nugu_client->getNetworkManager());
    network_manager->addListener(network_manager_listener.get());
    network_manager->setToken(getenv("NUGU_TOKEN"));
    network_manager->setUserAgent("0.2.0");

    if (!network_manager->connect()) {
        msg_error("< Cannot connect to NUGU Platform.");
        return EXIT_FAILURE;
    }

    auto wakeup_handler(std::unique_ptr<IWakeupHandler>(
        nugu_core_container->createWakeupHandler(nugu_sample_manager->getModelPath())));

    auto speech_operator(capa_collection->getSpeechOperator());
    speech_operator->setWakeupHandler(wakeup_handler.get());

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
            wakeup_handler.reset();
            nugu_client->deInitialize();

            msg_info("de-initialization done");
        });

    nugu_prof_dump(NUGU_PROF_TYPE_SDK_CREATED, NUGU_PROF_TYPE_MAX);

    return EXIT_SUCCESS;
}
