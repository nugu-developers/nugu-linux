
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

#ifndef __NUGU_SDK_MANAGER_H__
#define __NUGU_SDK_MANAGER_H__

#include <map>
#include <memory>

#include <clientkit/nugu_client.hh>

#include "bluetooth_status.hh"
#include "capability_collection.hh"
#include "nugu_sample_manager.hh"
#include "speaker_status.hh"
#include "speech_recognizer_aggregator_listener.hh"

class SpeakerController {
public:
    using VolumeControl = std::function<bool(int)>;
    using MuteControl = std::function<bool(bool)>;

public:
    virtual ~SpeakerController() = default;

    void setSpeakerHandler(ISpeakerHandler* handler);
    void setVolumeControl(const VolumeControl& volume_control);
    void setMuteControl(const MuteControl& mute_control);

    void setVolumeUp();
    void setVolumeDown();
    void toggleMute();

private:
    void adjustVolume(int volume);

    ISpeakerHandler* speaker_handler = nullptr;
    VolumeControl volume_control = nullptr;
    MuteControl mute_control = nullptr;
};

class NuguSDKManager : public IFocusManagerObserver,
                       public INetworkManagerListener,
                       public IInteractionControlManagerListener {
public:
    explicit NuguSDKManager(NuguSampleManager* manager);
    virtual ~NuguSDKManager() = default;

    void setup();

    // implements IFocusManagerObserver
    void onFocusChanged(const FocusConfiguration& configuration, FocusState state, const std::string& name) override;

    // implements INetworkManagerListener
    void onStatusChanged(NetworkStatus status) override;
    void onError(NetworkError error) override;

    // implements IInteractionControlManagerListener
    void onHasMultiTurn() override;
    void onModeChanged(bool is_multi_turn) override;

private:
    void composeExecuteCommands();
    void composeSDKCommands();
    void createInstance();
    void registerCapabilities();
    void setDefaultSoundLayerPolicy();
    void setAdditionalExecutor();
    void deleteInstance();

    void initSDK();
    void deInitSDK(bool is_exit = false);
    void reset();
    void exit();

    void changeWakeupWord();

    const int TEXT_INPUT_TYPE_1 = 1;
    const int TEXT_INPUT_TYPE_2 = 2;

    const std::string WAKEUP_WORD_ARIA = "아리아";
    const std::string WAKEUP_WORD_TINKERBELL = "팅커벨";
    const std::string DEFAULT_WAKEUP_WORD = WAKEUP_WORD_ARIA;

    std::unique_ptr<SpeakerController> speaker_controller = nullptr;
    std::unique_ptr<NuguClient> nugu_client = nullptr;
    std::unique_ptr<CapabilityCollection> capa_collection = nullptr;
    std::unique_ptr<SpeechRecognizerAggregatorListener> speech_recognizer_aggregator_listener = nullptr;
    ISpeechRecognizerAggregator* speech_recognizer_aggregator = nullptr;
    NuguSampleManager* nugu_sample_manager = nullptr;
    INuguCoreContainer* nugu_core_container = nullptr;
    IPlaySyncManager* playsync_manager = nullptr;
    INetworkManager* network_manager = nullptr;
    ITextHandler* text_handler = nullptr;
    IMicHandler* mic_handler = nullptr;
    IBluetoothHandler* bluetooth_handler = nullptr;
    BluetoothStatus* bluetooth_status = nullptr;
    SpeakerStatus* speaker_status = nullptr;

    std::function<void()> on_fail_func = nullptr;

    bool sdk_initialized = false;
    bool is_network_error = false;

    std::string wakeup_word = DEFAULT_WAKEUP_WORD;
    std::map<std::string, WakeupModelFile> wakeup_model_files;
};

#endif /* __NUGU_SDK_MANAGER_H__ */
