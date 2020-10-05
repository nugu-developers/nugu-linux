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

#ifndef __NUGU_FOCUS_MANAGER_H__
#define __NUGU_FOCUS_MANAGER_H__

#include <algorithm>
#include <map>
#include <memory>

#include "clientkit/focus_manager_interface.hh"

namespace NuguCore {

using namespace NuguClientKit;

class IFocusResourceObserver {
public:
    virtual ~IFocusResourceObserver() = default;

    virtual void onFocusChanged(const std::string& type, const std::string& name, FocusState state) = 0;
};

class FocusResource {
public:
    FocusResource(const std::string& type, const std::string& name, int priority, IFocusResourceListener* listener, IFocusResourceObserver* observer);
    virtual ~FocusResource() = default;

    void setState(FocusState state);

    bool operator<(const FocusResource& resource);
    bool operator>(const FocusResource& resource);

public:
    std::string type;
    std::string name;
    int priority;
    FocusState state;
    IFocusResourceListener* listener;
    IFocusResourceObserver* observer;
};

class FocusManager : public IFocusManager,
                     public IFocusResourceObserver {
public:
    FocusManager();
    virtual ~FocusManager() = default;

    void reset();
    bool requestFocus(const std::string& type, const std::string& name, IFocusResourceListener* listener) override;
    bool releaseFocus(const std::string& type, const std::string& name) override;

    bool holdFocus(const std::string& type) override;
    bool unholdFocus(const std::string& type) override;

    void setConfigurations(std::vector<FocusConfiguration>& configurations) override;
    void stopAllFocus() override;
    void stopForegroundFocus() override;

    std::string getStateString(FocusState state) override;

    void addObserver(IFocusManagerObserver* observer) override;
    void removeObserver(IFocusManagerObserver* observer) override;

    void onFocusChanged(const std::string& type, const std::string& name, FocusState state) override;

    int getFocusResourcePriority(const std::string& type);
    void printConfigurations();

private:
    std::map<std::string, int> configuration_map;
    std::map<int, std::shared_ptr<FocusResource>> focus_resource_ordered_map;
    std::map<std::string, bool> focus_hold_map;
    std::vector<IFocusManagerObserver*> observers;
};

} // NuguCore

#endif /* __NUGU_FOCUS_MANAGER_H__ */
