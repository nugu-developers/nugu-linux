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

#ifndef __NUGU_BATTERY_AGENT_H__
#define __NUGU_BATTERY_AGENT_H__

#include <vector>

#include <clientkit/capability_interface.hh>

using namespace NuguClientKit;

class IBatteryListener : public ICapabilityListener {
public:
    virtual ~IBatteryListener() = default;
};

class BatteryAgent : public ICapabilityInterface {
public:
    void setBatteryLevel(const std::string& level);
    void setCharging(bool charging);

    // implements ICapabilityInterface
    void setNuguCoreContainer(INuguCoreContainer* core_container) override;
    void initialize() override;
    void deInitialize() override;
    void suspend() override;
    void restore() override;
    std::string getName() override;
    std::string getVersion() override;
    void processDirective(NuguDirective* ndir) override;
    void updateInfoForContext(Json::Value& ctx) override;
    void receiveCommand(const std::string& from, const std::string& command, const std::string& param) override;
    void receiveCommandAll(const std::string& command, const std::string& param) override;
    void getProperty(const std::string& property, std::string& value) override;
    void getProperties(const std::string& property, std::list<std::string>& values) override;
    void setCapabilityListener(ICapabilityListener* clistener) override;

private:
    std::string battery_level = "10";
    bool battery_charging = false;

    INuguCoreContainer* core_container = nullptr;
    IBatteryListener* battery_listener = nullptr;
};

#endif /* __NUGU_BATTERY_AGENT_H__ */
