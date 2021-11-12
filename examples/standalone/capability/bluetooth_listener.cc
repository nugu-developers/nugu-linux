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

#include "bluetooth_listener.hh"
#include "bluetooth_status.hh"

BluetoothListener::BluetoothListener()
{
}

void BluetoothListener::setBTHandler(IBluetoothHandler* bt_handler)
{
    this->bt_handler = bt_handler;
}

void BluetoothListener::startDiscoverableMode(long duration_sec)
{
    if (bt_handler) {
        bt_handler->setDeviceInformation(BluetoothStatus::getInstance()->getDeviceInformation());
        bt_handler->startDiscoverableModeSucceeded(BluetoothStatus::getInstance()->hasPairedDevice());
    }
}

void BluetoothListener::finishDiscoverableMode()
{
    if (bt_handler) {
        bt_handler->setDeviceInformation(BluetoothStatus::getInstance()->getDeviceInformation());
        bt_handler->finishDiscoverableModeSucceeded();
    }
}

void BluetoothListener::play()
{
    if (bt_handler) {
        bt_handler->setDeviceInformation(BluetoothStatus::getInstance()->getDeviceInformation());
        bt_handler->mediaControlPlaySucceeded();
    }
}

void BluetoothListener::stop()
{
    if (bt_handler) {
        bt_handler->setDeviceInformation(BluetoothStatus::getInstance()->getDeviceInformation());
        bt_handler->mediaControlStopSucceeded();
    }
}

void BluetoothListener::pause()
{
    if (bt_handler) {
        bt_handler->setDeviceInformation(BluetoothStatus::getInstance()->getDeviceInformation());
        bt_handler->mediaControlPauseSucceeded();
    }
}

void BluetoothListener::next()
{
    if (bt_handler) {
        bt_handler->setDeviceInformation(BluetoothStatus::getInstance()->getDeviceInformation());
        bt_handler->mediaControlNextSucceeded();
    }
}

void BluetoothListener::previous()
{
    if (bt_handler) {
        bt_handler->setDeviceInformation(BluetoothStatus::getInstance()->getDeviceInformation());
        bt_handler->mediaControlPreviousSucceeded();
    }
}
