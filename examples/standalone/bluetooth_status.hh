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

#ifndef __BLUETOOTH_STATUS_H__
#define __BLUETOOTH_STATUS_H__

#include <capability/bluetooth_interface.hh>

using namespace NuguCapability;

enum class DeviceConnectStatus {
    DeviceUnpaired,
    DevicePaired,
    DeviceDisconnected,
    DeviceConnectFailed,
    DeviceConnected
};

class BluetoothStatus {
public:
    using DeviceStatusCallback = std::function<void(DeviceConnectStatus)>;
public:
    static BluetoothStatus* getInstance();
    static void destroyInstance();

    BluetoothStatus(const BluetoothStatus&) = delete;
    BluetoothStatus(BluetoothStatus&&) = delete;

    BluetoothStatus& operator=(const BluetoothStatus&) = delete;
    BluetoothStatus& operator=(BluetoothStatus&&) = delete;

    void setDeviceStatusCallback(DeviceStatusCallback callback);

    bool hasPairedDevice();

    void pair2ndDevice();
    void unpair2ndDevice();
    void connect2ndDevice(bool success);
    void disconnect2ndDevice();

    BTDeviceInfo getDeviceInformation();
    std::string getDeviceConnectStatusString(DeviceConnectStatus status);

private:
    BluetoothStatus();
    virtual ~BluetoothStatus() = default;

    static BluetoothStatus* instance;
    DeviceStatusCallback dev_status_cb;
    BTDeviceInfo dev_info;
    DeviceConnectStatus cur_status;
};

#endif /* __BLUETOOTH_STATUS_H__ */
