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

#include <iostream>

#include <base/nugu_prof.h>

#include "tts_listener.hh"

void TTSListener::setTTSHandler(ITTSHandler* tts_handler)
{
    this->tts_handler = tts_handler;
}

void TTSListener::onTTSState(TTSState state, const std::string& dialog_id)
{
    std::cout << "[TTS][id:" << dialog_id << "] ";

    switch (state) {
    case TTSState::TTS_SPEECH_START:
        tts_handler ? std::cout << "PLAYING (" << tts_handler->getPlayServiceId() << ")...\n"
                    : std::cout << "PLAYING...";

        break;
    case TTSState::TTS_SPEECH_STOP:
        std::cout << "PLAYING STOPPED\n";
        nugu_prof_dump(NUGU_PROF_TYPE_TTS_SPEAK_DIRECTIVE, NUGU_PROF_TYPE_TTS_STOPPED);
        break;
    case TTSState::TTS_SPEECH_FINISH:
        std::cout << "PLAYING FINISHED\n";
        nugu_prof_dump(NUGU_PROF_TYPE_TTS_SPEAK_DIRECTIVE, NUGU_PROF_TYPE_TTS_FINISHED);
        break;
    }
}

void TTSListener::onTTSText(const std::string& text, const std::string& dialog_id)
{
    std::cout << "[TTS][id:" << dialog_id << "] TEXT > \033[1;36m" << extractText(text) << "\033[0m" << std::endl;
}

void TTSListener::onTTSCancel(const std::string& dialog_id)
{
    std::cout << "[TTS][id:" << dialog_id << "] CANCEL... " << std::endl;
}

std::string TTSListener::extractText(std::string raw_text)
{
    // remove '< ~ >' tag
    std::string extracted_text;
    bool is_skip = false;

    for (const auto& character : raw_text) {
        if (character == '<')
            is_skip = true;
        else if (character == '>')
            is_skip = false;

        if (!is_skip && character != '>')
            extracted_text.push_back(character);
    }

    // replace '{W}' tag to wakeup word
    std::size_t wakeup_tag_found = extracted_text.find(WAKEUP_WORD_TAG);

    if (wakeup_tag_found != std::string::npos)
        extracted_text.replace(wakeup_tag_found, WAKEUP_WORD_TAG.length(), WAKEUP_WORD);

    return extracted_text;
}
