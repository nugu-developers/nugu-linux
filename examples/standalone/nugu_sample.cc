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

#include <capability/capability_factory.hh>
#include <clientkit/nugu_client.hh>

#include "audio_player_listener.hh"
#include "delegation_listener.hh"
#include "extension_listener.hh"
#include "location_listener.hh"
#include "mic_listener.hh"
#include "speaker_listener.hh"
#include "speech_operator.hh"
#include "system_listener.hh"
#include "text_listener.hh"
#include "tts_listener.hh"

#include "nugu_sample_manager.hh"

using namespace NuguClientKit;

std::unique_ptr<NuguClient> nugu_client = nullptr;
std::unique_ptr<NuguSampleManager> nugu_sample_manager = nullptr;

// Capability instance
std::shared_ptr<ISystemHandler> system_handler = nullptr;
std::shared_ptr<IASRHandler> asr_handler = nullptr;
std::shared_ptr<ITTSHandler> tts_handler = nullptr;
std::shared_ptr<IAudioPlayerHandler> audio_player_handler = nullptr;
std::shared_ptr<ITextHandler> text_handler = nullptr;
std::shared_ptr<ISpeakerHandler> speaker_handler = nullptr;
std::shared_ptr<IMicHandler> mic_handler = nullptr;

// Capability listener
std::unique_ptr<SpeechOperator> speech_operator = nullptr;
std::unique_ptr<TTSListener> tts_listener = nullptr;
std::unique_ptr<AudioPlayerListener> aplayer_listener = nullptr;
std::unique_ptr<SystemListener> system_listener = nullptr;
std::unique_ptr<TextListener> text_listener = nullptr;
std::unique_ptr<ExtensionListener> extension_listener = nullptr;
std::unique_ptr<DelegationListener> delegation_listener = nullptr;
std::unique_ptr<LocationListener> location_listener = nullptr;
std::unique_ptr<SpeakerListener> speaker_listener = nullptr;
std::unique_ptr<MicListener> mic_listener = nullptr;

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
};

void registerCapabilities()
{
    if (!nugu_client)
        return;

    // make CapabilityAgent instances which require additional setting separately
    system_handler = std::shared_ptr<ISystemHandler>(
        CapabilityFactory::makeCapability<SystemAgent, ISystemHandler>(system_listener.get()));
    asr_handler = std::shared_ptr<IASRHandler>(
        CapabilityFactory::makeCapability<ASRAgent, IASRHandler>(speech_operator->getASRListener()));
    tts_handler = std::shared_ptr<ITTSHandler>(
        CapabilityFactory::makeCapability<TTSAgent, ITTSHandler>(tts_listener.get()));
    audio_player_handler = std::shared_ptr<IAudioPlayerHandler>(
        CapabilityFactory::makeCapability<AudioPlayerAgent, IAudioPlayerHandler>(aplayer_listener.get()));
    text_handler = std::shared_ptr<ITextHandler>(
        CapabilityFactory::makeCapability<TextAgent, ITextHandler>(text_listener.get()));
    speaker_handler = std::shared_ptr<ISpeakerHandler>(
        CapabilityFactory::makeCapability<SpeakerAgent, ISpeakerHandler>(speaker_listener.get()));
    mic_handler = std::shared_ptr<IMicHandler>(
        CapabilityFactory::makeCapability<MicAgent, IMicHandler>(mic_listener.get()));

    // set AudioPlayerAgent
    audio_player_handler->setSuspendPolicy(ICapabilityInterface::SuspendPolicy::PAUSE);

    // set MicAgent
    mic_handler->enable();

    // set ASRAgent
    asr_handler->setAttribute(ASRAttribute { nugu_sample_manager->getModelPath() });
    speech_operator->setASRHandler(asr_handler.get());

    // set SpeakerAgent
    SpeakerInfo nugu_speaker;
    nugu_speaker.type = SpeakerType::NUGU;
    nugu_speaker.can_control = true;

    SpeakerInfo call_speaker;
    call_speaker.type = SpeakerType::CALL;
    call_speaker.can_control = false;

    SpeakerInfo alarm_speaker;
    alarm_speaker.type = SpeakerType::ALARM;
    alarm_speaker.can_control = false;

    SpeakerInfo external_speaker;
    external_speaker.type = SpeakerType::EXTERNAL;
    external_speaker.can_control = false;

    std::map<SpeakerType, SpeakerInfo*> speakers;
    speakers[SpeakerType::NUGU] = &nugu_speaker;
    speakers[SpeakerType::CALL] = &call_speaker;
    speakers[SpeakerType::ALARM] = &alarm_speaker;
    speakers[SpeakerType::EXTERNAL] = &external_speaker;

    speaker_handler->setSpeakerInfo(speakers);
    speaker_listener->setSpeakerHandler(speaker_handler.get());
    speaker_listener->setVolumeNuguSpeakerCallback([&](int volume) {
        if (tts_handler && !tts_handler->setVolume(volume))
            return false;
        if (audio_player_handler && !audio_player_handler->setVolume(volume))
            return false;
        return true;
    });
    speaker_listener->setMuteNuguSpeakerCallback([&](bool mute) {
        if (!tts_handler)
            return false;
        tts_handler->stopTTS();
        if (audio_player_handler && !audio_player_handler->setMute(mute))
            return false;
        return true;
    });

    // create capability instance
    nugu_client->getCapabilityBuilder()
        ->add(system_handler.get())
        ->add(asr_handler.get())
        ->add(tts_handler.get())
        ->add(audio_player_handler.get())
        ->add(text_handler.get())
        ->add(speaker_handler.get())
        ->add(mic_handler.get())
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
    aplayer_listener = make_unique<AudioPlayerListener>();
    system_listener = make_unique<SystemListener>();
    text_listener = make_unique<TextListener>();
    extension_listener = make_unique<ExtensionListener>();
    delegation_listener = make_unique<DelegationListener>();
    location_listener = make_unique<LocationListener>();
    speaker_listener = make_unique<SpeakerListener>();
    mic_listener = make_unique<MicListener>();

    auto nugu_client_listener(make_unique<NuguClientListener>());
    auto network_manager_listener(make_unique<NetworkManagerListener>());

    nugu_client = make_unique<NuguClient>();
    nugu_client->setListener(nugu_client_listener.get());

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

    speech_operator->setWakeupHandler(wakeup_handler);
    nugu_sample_manager->setSpeechOperator(speech_operator.get())
        ->setNetworkCallback(NuguSampleManager::NetworkCallback {
            [&]() { return network_manager->connect(); },
            [&]() { return network_manager->disconnect(); } })
        ->setActionCallback(NuguSampleManager::ActionCallback {
            [&]() { nugu_core_container->getCapabilityHelper()->suspendAll(); },
            [&]() { nugu_core_container->getCapabilityHelper()->restoreAll(); } })
        ->setTextHandler(text_handler.get())
        ->setMicHandler(mic_handler.get())
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
