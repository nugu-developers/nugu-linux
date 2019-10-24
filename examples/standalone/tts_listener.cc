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

#include "tts_listener.hh"

void TTSListener::onTTSState(TTSState state)
{
    std::cout << "[TTS] ";

    switch (state) {
    case TTSState::TTS_SPEECH_START:
        std::cout << "tts playing...\n";
        break;

    case TTSState::TTS_SPEECH_FINISH:
        std::cout << "tts playing finished\n";
        break;
    }
}

void TTSListener::onTTSText(const std::string& text)
{
    std::cout << "[TTS] text : ";
    std::cout << "\033[1;36m" << extractText(text) << "\033[0m" << std::endl;
}

std::string TTSListener::extractText(std::string raw_text)
{
    if (raw_text.empty())
        return raw_text;

    size_t first_tag_pos = raw_text.find_first_of(">");

    if (first_tag_pos == std::string::npos)
        return raw_text;

    size_t last_tag_pose = raw_text.find_last_of("<");

    if (last_tag_pose == std::string::npos)
        return raw_text;

    int text_count = last_tag_pose - first_tag_pos - 1;

    if (text_count <= 0)
        return raw_text;

    return raw_text.substr(first_tag_pos + 1, text_count);
}