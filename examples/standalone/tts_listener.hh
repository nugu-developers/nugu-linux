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

#ifndef __TTS_LISTENER_H__
#define __TTS_LISTENER_H__

#include <string>

#include <interface/capability/tts_interface.hh>

using namespace NuguInterface;

class TTSListener : public ITTSListener {
public:
    virtual ~TTSListener() = default;
    void onTTSState(TTSState state) override;
    void onTTSText(std::string text) override;

private:
    std::string extractText(std::string raw_text);
};

#endif /* __TTS_LISTENER_H__ */
