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

#include "bluetooth_status.hh"

#define NOTIFY_STATUS_CHANGED(status) \
    {                                 \
        cur_status = status;          \
        if (dev_status_cb)            \
            dev_status_cb(status);    \
    }

BluetoothStatus* BluetoothStatus::instance = nullptr;

BluetoothStatus::BluetoothStatus()
    : cur_status(DeviceConnectStatus::DeviceUnpaired)
{
    dev_info.device_name = "NUGU-Sample";
    dev_info.power_on = true;
    dev_info.is_active_device = false;
    dev_info.active_device_streaming = "UNUSABLE";
}

BluetoothStatus* BluetoothStatus::getInstance()
{
    if (!instance)
        instance = new BluetoothStatus();

    return instance;
}

void BluetoothStatus::destroyInstance()
{
    if (instance) {
        delete instance;
        instance = nullptr;
    }
}

void BluetoothStatus::setDeviceStatusCallback(DeviceStatusCallback callback)
{
    dev_status_cb = std::move(callback);
}

bool BluetoothStatus::hasPairedDevice()
{
    return (cur_status > DeviceConnectStatus::DeviceUnpaired);
}

void BluetoothStatus::pair2ndDevice()
{
    dev_info.profiles.clear();
    dev_info.profiles.push_back({ "HSP", true });
    dev_info.profiles.push_back({ "A2DP", true });
    dev_info.profiles.push_back({ "PBAP", true });
    dev_info.profiles.push_back({ "MAP", true });
    dev_info.is_paired_device = true;

    NOTIFY_STATUS_CHANGED(DeviceConnectStatus::DevicePaired);
}

void BluetoothStatus::unpair2ndDevice()
{
    dev_info.profiles.clear();
    dev_info.is_paired_device = false;

    NOTIFY_STATUS_CHANGED(DeviceConnectStatus::DeviceUnpaired);
}

void BluetoothStatus::connect2ndDevice(bool success)
{
    if (success) {
        dev_info.is_active_device = true;
        dev_info.active_device_id = "2ndDeviceID";
        dev_info.active_device_name = "2ndDeviceSmartPhone";
        NOTIFY_STATUS_CHANGED(DeviceConnectStatus::DeviceConnected);
    } else {
        dev_info.is_active_device = false;
        NOTIFY_STATUS_CHANGED(DeviceConnectStatus::DeviceConnectFailed);
    }
}

void BluetoothStatus::disconnect2ndDevice()
{
    dev_info.is_active_device = false;
    dev_info.active_device_id = "";
    dev_info.active_device_name = "";

    NOTIFY_STATUS_CHANGED(DeviceConnectStatus::DeviceDisconnected);
}

BTDeviceInfo BluetoothStatus::getDeviceInformation()
{
    return dev_info;
}

std::string BluetoothStatus::getDeviceConnectStatusString(DeviceConnectStatus status)
{
    std::string status_str;

    switch (status) {
    case DeviceConnectStatus::DeviceUnpaired:
        status_str = "DeviceUnpaired";
        break;
    case DeviceConnectStatus::DevicePaired:
        status_str = "DevicePaired";
        break;
    case DeviceConnectStatus::DeviceDisconnected:
        status_str = "DeviceDisconnected";
        break;
    case DeviceConnectStatus::DeviceConnectFailed:
        status_str = "DeviceConnectFailed";
        break;
    case DeviceConnectStatus::DeviceConnected:
        status_str = "DeviceConnected";
        break;
    }
    return status_str;
}