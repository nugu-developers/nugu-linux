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

#include <base/nugu_prof.h>

#include "nugu_sdk_manager.hh"

namespace {
static const char* WAKEUP_MODEL_ARIA_NET = "skt_trigger_am_aria.raw";
static const char* WAKEUP_MODEL_ARIA_SEARCH = "skt_trigger_search_aria.raw";
static const char* WAKEUP_MODEL_TINKERBELL_NET = "skt_trigger_am_tinkerbell.raw";
static const char* WAKEUP_MODEL_TINKERBELL_SEARCH = "skt_trigger_search_tinkerbell.raw";

void msg_error(std::string&& message)
{
    NuguSampleManager::error(std::move(message));
}

void msg_info(std::string&& message)
{
    NuguSampleManager::info(std::move(message));
}

template <typename T, typename... Ts>
std::unique_ptr<T> make_unique(Ts&&... params)
{
    return std::unique_ptr<T>(new T(std::forward<Ts>(params)...));
}
}

/*******************************************************************************
 * define SpeakerController
 ******************************************************************************/

void SpeakerController::setSpeakerHandler(ISpeakerHandler* handler)
{
    this->speaker_handler = handler;
}

void SpeakerController::setVolumeControl(const VolumeControl& volume_control)
{
    this->volume_control = volume_control;
}

void SpeakerController::setMuteControl(const MuteControl& mute_control)
{
    this->mute_control = mute_control;
}

void SpeakerController::setVolumeUp()
{
    int set_volume = SpeakerStatus::getInstance()->getNUGUVolume() + NUGU_SPEAKER_DEFAULT_STEP;

    if (set_volume > NUGU_SPEAKER_MAX_VOLUME)
        return;

    adjustVolume(set_volume);
}

void SpeakerController::setVolumeDown()
{
    int set_volume = SpeakerStatus::getInstance()->getNUGUVolume() - NUGU_SPEAKER_DEFAULT_STEP;

    if (set_volume < NUGU_SPEAKER_MIN_VOLUME)
        return;

    adjustVolume(set_volume);
}

void SpeakerController::adjustVolume(int volume)
{
    if (volume_control && volume_control(volume)) {
        SpeakerStatus::getInstance()->setNUGUVolume(volume);
        speaker_handler->informVolumeChanged(SpeakerType::NUGU, volume);
    }
}

void SpeakerController::toggleMute()
{
    int set_mute = !SpeakerStatus::getInstance()->isSpeakerMute();

    if (mute_control && mute_control(set_mute)) {
        SpeakerStatus::getInstance()->setSpeakerMute(set_mute);
        speaker_handler->informMuteChanged(SpeakerType::NUGU, set_mute);
    }
}

/*******************************************************************************
 * define NuguSDKManager
 ******************************************************************************/

NuguSDKManager::NuguSDKManager(NuguSampleManager* manager)
    : speaker_controller(make_unique<SpeakerController>())
    , nugu_sample_manager(manager)
    , bluetooth_status(BluetoothStatus::getInstance())
    , speaker_status(SpeakerStatus::getInstance())
{
    std::string model_path = nugu_sample_manager->getModelPath();

    wakeup_model_files = {
        { WAKEUP_WORD_ARIA,
            WakeupModelFile {
                model_path + WAKEUP_MODEL_ARIA_NET,
                model_path + WAKEUP_MODEL_ARIA_SEARCH } },
        { WAKEUP_WORD_TINKERBELL,
            WakeupModelFile {
                model_path + WAKEUP_MODEL_TINKERBELL_NET,
                model_path + WAKEUP_MODEL_TINKERBELL_SEARCH } }
    };
}

void NuguSDKManager::setup()
{
    on_fail_func = [&]() {
        nugu_sample_manager->reset();
        nugu_sample_manager->getCommandBuilder()
            ->add("q", { "quit", [&](int& flag) {
                            exit();
                        } })
            ->compose();
    };

    createInstance();
    composeExecuteCommands();
}

void NuguSDKManager::composeExecuteCommands()
{
    nugu_sample_manager->getCommandBuilder()
        ->add("i", { "initialize", [&](int& flag) {
                        initSDK();
                    } })
        ->add("d", { "deinitialize", [&](int& flag) {
                        deInitSDK();
                    } })
        ->add("q", { "quit", [&](int& flag) {
                        exit();
                    } })
        ->compose();
}

void NuguSDKManager::composeSDKCommands()
{
    nugu_sample_manager->getCommandBuilder()
        ->add("cw", { "change wakeup word", [&](int& flag) {
                         changeWakeupWord();
                     } })
        ->add("w", { "start listening with wakeup", [&](int& flag) {
                        speech_operator->startListeningWithWakeup();
                    } })
        ->add("l", { "start listening", [&](int& flag) {
                        speech_operator->startListening();
                    } })
        ->add("s", { "stop listening/wakeup", [&](int& flag) {
                        speech_operator->stopListeningAndWakeup();
                    } })
        ->add("c", { "cancel listening", [&](int& flag) {
                        speech_operator->cancelListening();
                    } })
        ->add("t", { "text input", [&](int& flag) {
                        flag = TEXT_INPUT_TYPE_1;
                    } })
        ->add("t2", { "text input (ignore dialog attribute)", [&](int& flag) {
                         flag = TEXT_INPUT_TYPE_2;
                     } })
        ->add("m", { "set mic mute/unmute", [&](int& flag) {
                        speaker_status->isMicMute() ? mic_handler->enable() : mic_handler->disable();
                    } })
        ->add("sm", { "set speaker mute/unmute", [&](int& flag) {
                         speaker_controller->toggleMute();
                     } })
        ->add("+", { "set speaker volume up", [&](int& flag) {
                        speaker_controller->setVolumeUp();
                    } })
        ->add("-", { "set speaker volume down", [&](int& flag) {
                        speaker_controller->setVolumeDown();
                    } })
        ->add("bp", { "pair virtual 2nd bt device", [&](int& flag) {
                         bluetooth_status->pair2ndDevice();
                     } })
        ->add("bu", { "unpair virtual 2nd bt device", [&](int& flag) {
                         bluetooth_status->unpair2ndDevice();
                     } })
        ->add("bc", { "connect virtual 2nd bt device success", [&](int& flag) {
                         bluetooth_status->connect2ndDevice(true);
                     } })
        ->add("bd", { "disconnect virtual 2nd bt device success", [&](int& flag) {
                         bluetooth_status->disconnect2ndDevice();
                     } })
        ->add("sa", { "suspend all", [&](int& flag) {
                         nugu_core_container->getCapabilityHelper()->suspendAll();
                     } })
        ->add("ra", { "restore all", [&](int& flag) {
                         nugu_core_container->getCapabilityHelper()->restoreAll();
                     } })
        ->compose("cw");
}

void NuguSDKManager::createInstance()
{
    nugu_client = make_unique<NuguClient>();
    capa_collection = make_unique<CapabilityCollection>();
    nugu_core_container = nugu_client->getNuguCoreContainer();
    speech_operator = capa_collection->getSpeechOperator();
    network_manager = nugu_client->getNetworkManager();

    registerCapabilities();

    auto capability_helper(nugu_core_container->getCapabilityHelper());
    capability_helper->getFocusManager()->addObserver(this);
    capability_helper->getInteractionControlManager()->addListener(this);
    playsync_manager = capability_helper->getPlaySyncManager();

    setDefaultSoundLayerPolicy();

    network_manager->addListener(this);
    network_manager->setToken(getenv("NUGU_TOKEN"));
    network_manager->setUserAgent("0.2.0");

    on_init_func = [&]() {
        wakeup_handler = std::unique_ptr<IWakeupHandler>(nugu_core_container->createWakeupHandler(wakeup_model_files[wakeup_word]));
        speech_operator->setWakeupHandler(wakeup_handler.get(), wakeup_word);
    };
}

void NuguSDKManager::registerCapabilities()
{
    if (!nugu_client)
        return;

    auto asr_handler(capa_collection->getCapability<IASRHandler>("ASR"));
    auto speaker_handler(capa_collection->getCapability<ISpeakerHandler>("Speaker"));
    text_handler = capa_collection->getCapability<ITextHandler>("Text");
    mic_handler = capa_collection->getCapability<IMicHandler>("Mic");
    bluetooth_handler = capa_collection->getCapability<IBluetoothHandler>("Bluetooth");

    // setup SpeakerController
    auto speaker_listener(capa_collection->getCapabilityListener<SpeakerListener>("Speaker"));
    speaker_controller->setSpeakerHandler(speaker_handler);
    speaker_controller->setVolumeControl(speaker_listener->getNuguSpeakerVolumeControl());
    speaker_controller->setMuteControl(speaker_listener->getNuguSpeakerMuteControl());

    asr_handler->setAttribute(ASRAttribute { nugu_sample_manager->getModelPath() });
    setAdditionalExecutor();

    // create capability instance
    nugu_client->getCapabilityBuilder()
        ->add(capa_collection->getCapability<ISystemHandler>("System"))
        ->add(capa_collection->getCapability<ISoundHandler>("Sound"))
        ->add(capa_collection->getCapability<ISessionHandler>("Session"))
        ->add(capa_collection->getCapability<IDisplayHandler>("Display"))
        ->add(capa_collection->getCapability<IUtilityHandler>("Utility"))
        ->add(capa_collection->getCapability<IExtensionHandler>("Extension"))
        ->add(capa_collection->getCapability<IChipsHandler>("Chips"))
        ->add(capa_collection->getCapability<INudgeHandler>("Nudge"))
        ->add(capa_collection->getCapability<IRoutineHandler>("Routine"))
        ->add(capa_collection->getCapability<IAudioPlayerHandler>("AudioPlayer"))
        ->add(capa_collection->getCapability<ITTSHandler>("TTS"))
        ->add(speaker_handler)
        ->add(asr_handler)
        ->add(text_handler)
        ->add(mic_handler)
        ->add(bluetooth_handler)
        ->construct();
}

void NuguSDKManager::setDefaultSoundLayerPolicy()
{
    auto capability_helper(nugu_core_container->getCapabilityHelper());
    auto focus_manager = capability_helper->getFocusManager();

    std::vector<FocusConfiguration> request_configuration {
        { CALL_FOCUS_TYPE, CALL_FOCUS_REQUEST_PRIORITY },
        { ASR_USER_FOCUS_TYPE, ASR_USER_FOCUS_REQUEST_PRIORITY },
        { INFO_FOCUS_TYPE, INFO_FOCUS_REQUEST_PRIORITY },
        { ASR_DM_FOCUS_TYPE, ASR_DM_FOCUS_REQUEST_PRIORITY },
        { ALERTS_FOCUS_TYPE, ALERTS_FOCUS_REQUEST_PRIORITY },
        { ASR_BEEP_FOCUS_TYPE, ASR_BEEP_FOCUS_REQUEST_PRIORITY },
        { MEDIA_FOCUS_TYPE, MEDIA_FOCUS_REQUEST_PRIORITY },
        { SOUND_FOCUS_TYPE, SOUND_FOCUS_REQUEST_PRIORITY },
        { DUMMY_FOCUS_TYPE, DUMMY_FOCUS_REQUEST_PRIORITY }
    };

    std::vector<FocusConfiguration> release_configuration {
        { CALL_FOCUS_TYPE, CALL_FOCUS_RELEASE_PRIORITY },
        { ASR_USER_FOCUS_TYPE, ASR_USER_FOCUS_RELEASE_PRIORITY },
        { ASR_DM_FOCUS_TYPE, ASR_DM_FOCUS_RELEASE_PRIORITY },
        { INFO_FOCUS_TYPE, INFO_FOCUS_RELEASE_PRIORITY },
        { ALERTS_FOCUS_TYPE, ALERTS_FOCUS_RELEASE_PRIORITY },
        { ASR_BEEP_FOCUS_TYPE, ASR_BEEP_FOCUS_RELEASE_PRIORITY },
        { MEDIA_FOCUS_TYPE, MEDIA_FOCUS_RELEASE_PRIORITY },
        { SOUND_FOCUS_TYPE, SOUND_FOCUS_RELEASE_PRIORITY },
        { DUMMY_FOCUS_TYPE, DUMMY_FOCUS_RELEASE_PRIORITY }
    };

    focus_manager->setConfigurations(request_configuration, release_configuration);
}

void NuguSDKManager::setAdditionalExecutor()
{
    nugu_sample_manager->setTextCommander(
        [&](std::string text, bool include_dialog_attribute) {
            text_handler->requestTextInput(text, "", include_dialog_attribute);
        });
    nugu_sample_manager->setPlayStackRetriever([&]() {
        std::string playstack_text;
        const auto& playstacks = playsync_manager->getAllPlayStackItems();

        for (const auto& item : playstacks)
            playstack_text.append(item).append("|");

        if (!playstack_text.empty())
            playstack_text.pop_back();

        return playstack_text;
    });
    nugu_sample_manager->setMicStatusRetriever([&]() {
        return speaker_status->isMicMute() ? "OFF" : "ON";
    });
    nugu_sample_manager->setVolumeStatusRetriever([&]() {
        return speaker_status->isSpeakerMute() ? "MUTE" : std::to_string(speaker_status->getNUGUVolume());
    });

    bluetooth_status->setDeviceStatusCallback([&](DeviceConnectStatus status) {
        msg_info("bluetooth device status changed => " + bluetooth_status->getDeviceConnectStatusString(status));

        if (status == DeviceConnectStatus::DeviceDisconnected)
            bluetooth_handler->disconnectSucceeded();
        else if (status == DeviceConnectStatus::DeviceConnectFailed)
            bluetooth_handler->connectFailed();
        else if (status == DeviceConnectStatus::DeviceConnected)
            bluetooth_handler->connectSucceeded();
    });
}

void NuguSDKManager::deleteInstance()
{
    wakeup_handler.reset();
    nugu_client.reset();
    capa_collection.reset();
    on_init_func = nullptr;

    speaker_status->destroyInstance();
    bluetooth_status->destroyInstance();
}

void NuguSDKManager::initSDK()
{
    if (sdk_initialized) {
        msg_info("The SDK is already initialized.");
        return;
    }

    composeSDKCommands();
    speaker_status->setDefaultValues(SpeakerStatus::DefaultValues { false, false, NUGU_SPEAKER_DEFAULT_VOLUME });

    if (!nugu_client->initialize()) {
        msg_error("> It failed to initialize NUGU SDK. Please Check authorization.");
        return;
    }

    if (!network_manager->connect()) {
        msg_error("> Cannot connect to NUGU Platform.");
        return;
    }

    sdk_initialized = true;

    nugu_prof_dump(NUGU_PROF_TYPE_SDK_CREATED, NUGU_PROF_TYPE_MAX);
}

void NuguSDKManager::deInitSDK(bool is_exit)
{
    if (!sdk_initialized) {
        if (!is_exit)
            msg_info("The SDK is already deinitialized.");

        return;
    }

    msg_info("de-initialization start");

    network_manager->disconnect();
    nugu_client->deInitialize();

    msg_info("de-initialization done");

    if (!is_exit)
        reset();

    sdk_initialized = false;
}

void NuguSDKManager::reset()
{
    nugu_sample_manager->reset();
    composeExecuteCommands();
}

void NuguSDKManager::exit()
{
    deInitSDK(true);
    deleteInstance();
    nugu_sample_manager->quit();
}

void NuguSDKManager::changeWakeupWord()
{
    wakeup_word = (wakeup_word == WAKEUP_WORD_ARIA) ? WAKEUP_WORD_TINKERBELL : WAKEUP_WORD_ARIA;

    if (speech_operator->changeWakeupWord(wakeup_model_files[wakeup_word], wakeup_word))
        nugu_client->setWakeupWord(wakeup_word);
}

/*******************************************************************************
 * implements Observer & Listener
 ******************************************************************************/

void NuguSDKManager::onFocusChanged(const FocusConfiguration& configuration, FocusState state, const std::string& name)
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

void NuguSDKManager::onStatusChanged(NetworkStatus status)
{
    switch (status) {
    case NetworkStatus::DISCONNECTED:
        msg_info("Network disconnected.");
        nugu_sample_manager->handleNetworkResult(false);

        if (is_network_error && on_fail_func)
            on_fail_func();

        break;
    case NetworkStatus::READY:
        msg_info("Network ready.");
        is_network_error = false;
        nugu_sample_manager->handleNetworkResult(true);

        if (on_init_func)
            on_init_func();

        break;
    case NetworkStatus::CONNECTED:
        msg_info("Network connected.");
        is_network_error = false;
        nugu_sample_manager->handleNetworkResult(true);

        if (on_init_func)
            on_init_func();

        break;
    case NetworkStatus::CONNECTING:
        msg_info("Network connection in progress.");
        nugu_sample_manager->handleNetworkResult(false, false);
        break;
    default:
        break;
    }
}

void NuguSDKManager::onError(NetworkError error)
{
    switch (error) {
    case NetworkError::FAILED:
        msg_error("Network error [FAILED].");
        break;
    case NetworkError::TOKEN_ERROR:
        msg_error("Network error [TOKEN_ERROR].");
        break;
    case NetworkError::UNKNOWN:
        msg_error("Network error [UNKNOWN].");
        break;
    }

    is_network_error = true;
    nugu_sample_manager->handleNetworkResult(false);
}

void NuguSDKManager::onHasMultiTurn()
{
    msg_info("[multi-turn] has multi-turn");
}
void NuguSDKManager::onModeChanged(bool is_multi_turn)
{
    msg_info(std::string { "[multi-turn] " } + (is_multi_turn ? "started" : "finished"));
}
