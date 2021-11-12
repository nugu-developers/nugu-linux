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

#ifndef __BLUETOOTH_LISTENER_H__
#define __BLUETOOTH_LISTENER_H__

#include <capability/bluetooth_interface.hh>

using namespace NuguCapability;

class BluetoothListener : public IBluetoothListener {
public:
    BluetoothListener();
    virtual ~BluetoothListener() = default;

    void setBTHandler(IBluetoothHandler* bt_handler);
    void startDiscoverableMode(long duration_sec) override;
    void finishDiscoverableMode() override;
    void play() override;
    void stop() override;
    void pause() override;
    void next() override;
    void previous() override;

private:
    IBluetoothHandler* bt_handler = nullptr;
};

#endif /* __BLUETOOTH_LISTENER_H__ */
