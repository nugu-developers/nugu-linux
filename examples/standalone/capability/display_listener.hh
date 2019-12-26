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

#ifndef __DISPLAY_LISTENER_H__
#define __DISPLAY_LISTENER_H__

#include <interface/capability/display_interface.hh>

using namespace NuguInterface;

class DisplayListener : virtual public IDisplayListener {
public:
    DisplayListener();
    virtual ~DisplayListener() = default;

    void renderDisplay(const std::string& id, const std::string& type, const std::string& json, const std::string& dialog_id) override;
    bool clearDisplay(const std::string& id, bool unconditionally) override;

protected:
    std::string listener_name;
};

#endif /* __DISPLAY_LISTENER_H__ */
