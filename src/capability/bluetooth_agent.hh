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

#ifndef __NUGU_BLUETOOTH_AGENT_H__
#define __NUGU_BLUETOOTH_AGENT_H__

#include "capability/bluetooth_interface.hh"
#include "clientkit/capability.hh"

namespace NuguCapability {

class BluetoothAgent final : public Capability,
                             public IFocusResourceListener,
                             public IBluetoothHandler {
public:
    BluetoothAgent();
    virtual ~BluetoothAgent() = default;

    void initialize() override;
    void deInitialize() override;
    void setCapabilityListener(ICapabilityListener* clistener) override;

    void parsingDirective(const char* dname, const char* message) override;
    void updateInfoForContext(Json::Value& ctx) override;

    void onFocusChanged(FocusState state) override;

    void setAudioPlayerState(const std::string& state) override;
    void startDiscoverableModeSucceeded(bool has_paired_devices) override;
    void startDiscoverableModeFailed(bool has_paired_devices) override;
    void finishDiscoverableModeSucceeded() override;
    void finishDiscoverableModeFailed() override;
    void connectSucceeded() override;
    void connectFailed() override;
    void disconnectSucceeded() override;
    void disconnectFailed() override;
    void mediaControlPlaySucceeded() override;
    void mediaControlPlayFailed() override;
    void mediaControlStopSucceeded() override;
    void mediaControlStopFailed() override;
    void mediaControlPauseSucceeded() override;
    void mediaControlPauseFailed() override;
    void mediaControlNextSucceeded() override;
    void mediaControlNextFailed() override;
    void mediaControlPreviousSucceeded() override;
    void mediaControlPreviousFailed() override;

private:
    void sendEventCommon(const std::string& ename, EventResultCallback cb = nullptr);
    void sendEventDiscoverableMode(const std::string& ename, bool has_paired_devices, EventResultCallback cb = nullptr);

    // parsing directive
    bool parsingCommon(const char* message, Json::Value& root);
    void parsingStartDiscoverableMode(const char* message);
    void parsingFinishDiscoverableMode(const char* message);
    void parsingPlay(const char* message);
    void parsingStop(const char* message);
    void parsingPause(const char* message);
    void parsingNext(const char* message);
    void parsingPrevious(const char* message);

    void executeOnForegroundAction();
    void executeOnBackgroundAction();
    void executeOnNoneAction();

    void printDeviceInformation(const BTDeviceInfo& device_info);

    IBluetoothListener* bt_listener = nullptr;
    std::string context_info;
    std::string ps_id;
    FocusState focus_state = FocusState::NONE;
    std::string player_state = "UNUSABLE";
};
} // NuguCapability

#endif /* __NUGU_BLUETOOTH_AGENT_H__ */
