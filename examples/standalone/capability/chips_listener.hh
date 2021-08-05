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

#ifndef __CHIPS_LISTENER_H__
#define __CHIPS_LISTENER_H__

#include <capability/chips_interface.hh>

using namespace NuguCapability;

class ChipsListener : public IChipsListener {
public:
    ChipsListener();
    virtual ~ChipsListener() = default;

    void onReceiveRender(ChipsInfo&& chips_info) override;

private:
    std::map<ChipsTarget, std::string> target_texts;
};

#endif /* __CHIPS_LISTENER_H__ */