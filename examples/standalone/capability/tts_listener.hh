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

#include <capability/tts_interface.hh>

#include "capability_listener.hh"

using namespace NuguCapability;

class TTSListener : public ITTSListener,
                    public CapabilityListener {
public:
    virtual ~TTSListener() = default;
    void setCapabilityHandler(ICapabilityInterface* handler) override;
    void onTTSState(TTSState state, const std::string& dialog_id) override;
    void onTTSText(const std::string& text, const std::string& dialog_id) override;
    void onTTSCancel(const std::string& dialog_id) override;

private:
    std::string extractText(std::string raw_text);

    const std::string WAKEUP_WORD_TAG = "{W}";
    const std::string WAKEUP_WORD = "아리아"; // It skip to get wakeup word from NuguCoreContainer

    ITTSHandler* tts_handler = nullptr;
};

#endif /* __TTS_LISTENER_H__ */
