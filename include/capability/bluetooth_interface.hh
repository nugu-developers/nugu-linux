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

#ifndef __NUGU_BLUETOOTH_INTERFACE_H__
#define __NUGU_BLUETOOTH_INTERFACE_H__

#include <clientkit/capability_interface.hh>

namespace NuguCapability {

using namespace NuguClientKit;

/**
 * @file bluetooth_interface.hh
 * @defgroup BluetoothInterface BluetoothInterface
 * @ingroup SDKNuguCapability
 * @brief Bluetooth capability interface
 *
 * It handle directives for bluetooth control
 *
 * @{
 */

/**
 * @brief Bluetooth profiles information
 * @see BTDeviceInfo
 */
typedef struct {
    std::string name; /**< profile name (reference by https://www.bluetooth.com/specifications/specs/) */
    bool enable; /**< profile usage */
} BTProfile;

/**
 * @brief Bluetooth device information
 * @see IBluetoothListener::setDeviceInformation
 */
typedef struct {
    std::string device_name; /**< bluetooth adaptor's name */
    bool power_on; /**< bluetooth adaptor's power on state */
    std::vector<BTProfile> profiles; /**< paired 2nd device's profiles */
    bool is_paired_device; /**< whether paired 2nd device */
    bool is_active_device; /**< whether connected 2nd device */
    std::string active_device_id; /**< connected 2nd device's id */
    std::string active_device_name; /**< connected 2nd device's name */
    std::string active_device_streaming; /**< connected 2nd device's streaming status (INACTIVE / ACTIVE / PAUSED / UNUSABLE) */
} BTDeviceInfo;

/**
 * @brief bluetooth listener interface
 * @see IBluetoothHandler
 */
class IBluetoothListener : virtual public ICapabilityListener {
public:
    virtual ~IBluetoothListener() = default;

    /**
     * @brief Send command to switch discoverable on to bluetooth adaptor
     * @param[in] duration_sec duration time for discoverable on
     */
    virtual void startDiscoverableMode(long duration_sec) = 0;

    /**
     * @brief Send command to switch discoverable off to bluetooth adaptor
     */
    virtual void finishDiscoverableMode() = 0;

    /**
     * @brief Send command to play media to bluetooth adaptor
     */
    virtual void play() = 0;

    /**
     * @brief Send command to stop media to bluetooth adaptor
     */
    virtual void stop() = 0;

    /**
     * @brief Send command to pause media to bluetooth adaptor
     */
    virtual void pause() = 0;

    /**
     * @brief Send command to play next media to bluetooth adaptor
     */
    virtual void next() = 0;

    /**
     * @brief Send command to play previous media to bluetooth adaptor
     */
    virtual void previous() = 0;

    /**
     * @brief Request device information for bluetooth context
     * @param[in] device_info device information
     */
    virtual void requestContext(BTDeviceInfo& device_info) = 0;
};

/**
 * @brief bluetooth handler interface
 * @see IBluetoothListener
 */
class IBluetoothHandler : virtual public ICapabilityInterface {
public:
    virtual ~IBluetoothHandler() = default;

    /**
     * @brief Notify the success result of start discoverable mode
     * @param[in] has_paired_devices has paired devices flag
     */
    virtual void startDiscoverableModeSucceeded(bool has_paired_devices) = 0;

    /**
     * @brief Notify the fail result of start discoverable mode
     * @param[in] has_paired_devices has paired devices flag
     */
    virtual void startDiscoverableModeFailed(bool has_paired_devices) = 0;

    /**
     * @brief Notify the success result of finish discoverable mode
     */
    virtual void finishDiscoverableModeSucceeded() = 0;

    /**
     * @brief Notify the fail result of finish discoverable mode
     */
    virtual void finishDiscoverableModeFailed() = 0;

    /**
     * @brief Notify the success result of connect device
     */
    virtual void connectSucceeded() = 0;

    /**
     * @brief Notify the fail result of connect device
     */
    virtual void connectFailed() = 0;

    /**
     * @brief Notify the success result of disconnect device
     */
    virtual void disconnectSucceeded() = 0;

    /**
     * @brief Notify the fail result of disconnect device
     */
    virtual void disconnectFailed() = 0;

    /**
     * @brief Notify the success result of playing media
     */
    virtual void mediaControlPlaySucceeded() = 0;

    /**
     * @brief Notify the fail result of playing media
     */
    virtual void mediaControlPlayFailed() = 0;

    /**
     * @brief Notify the success result of stopping media
     */
    virtual void mediaControlStopSucceeded() = 0;

    /**
     * @brief Notify the fail result of stopping media
     */
    virtual void mediaControlStopFailed() = 0;

    /**
     * @brief Notify the success result of pausing media
     */
    virtual void mediaControlPauseSucceeded() = 0;

    /**
     * @brief Notify the fail result of pausing media
     */
    virtual void mediaControlPauseFailed() = 0;

    /**
     * @brief Notify the success result of playing next media
     */
    virtual void mediaControlNextSucceeded() = 0;

    /**
     * @brief Notify the fail result of playing next media
     */
    virtual void mediaControlNextFailed() = 0;

    /**
     * @brief Notify the success result of playing previous media
     */
    virtual void mediaControlPreviousSucceeded() = 0;

    /**
     * @brief Notify the fail result of playing previous media
     */
    virtual void mediaControlPreviousFailed() = 0;
};

/**
 * @}
 */

} // NuguCapability

#endif /* __NUGU_BLUETOOTH_INTERFACE_H__ */
