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

#ifndef __TEXT_LISTENER_H__
#define __TEXT_LISTENER_H__

#include <capability/text_interface.hh>

using namespace NuguCapability;

class TextListener : public ITextListener {
public:
    virtual ~TextListener() = default;
    void onState(TextState state, const std::string& dialog_id) override;
    void onComplete(const std::string& dialog_id) override;
    void onError(TextError error, const std::string& dialog_id) override;
    bool handleTextCommand(const std::string& text, const std::string& token) override;

private:
    TextState prev_state = TextState::IDLE;
    std::string prev_dialog_id;
};

#endif /* __TEXT_LISTENER_H__ */
