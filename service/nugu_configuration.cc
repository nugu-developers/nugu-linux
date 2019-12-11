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

#include <interface/nugu_configuration.hh>

namespace NuguInterface {

std::map<NuguConfig::Key, std::string> NuguConfig::configs;

void NuguConfig::loadDefaultValue()
{
    configs[Key::WAKEUP_WITH_LISTENING] = "false";
    configs[Key::WAKEUP_WORD] = "아리아";
    configs[Key::ASR_EPD_TYPE] = "CLIENT";
    configs[Key::ASR_EPD_TIMEOUT_SEC] = "7";
    configs[Key::ASR_EPD_MAX_DURATION_SEC] = "10";
    configs[Key::ASR_EPD_PAUSE_LENGTH_MSEC] = "700";
    configs[Key::ASR_ENCODING] = "COMPLETE";
    configs[Key::ASR_EPD_SAMPLERATE] = "16k";
    configs[Key::ASR_EPD_FORMAT] = "s16le";
    configs[Key::ASR_EPD_CHANNEL] = "1";
    configs[Key::KWD_SAMPLERATE] = "16k";
    configs[Key::KWD_FORMAT] = "s16le";
    configs[Key::KWD_CHANNEL] = "1";
    configs[Key::SERVER_RESPONSE_TIMEOUT_MSEC] = "10000";
    configs[Key::MODEL_PATH] = "./";
    configs[Key::TTS_ENGINE] = "skt";
}

const std::string NuguConfig::getValue(Key key)
{
    if (configs.find(key) != configs.end())
        return configs[key];

    return "";
}

void NuguConfig::setValue(Key key, const std::string& value)
{
    configs[key] = value;
}

} // NuguInterface
