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

#include <algorithm>

#include <interface/nugu_configuration.hh>

namespace NuguInterface {

namespace NuguConfig {
    namespace {
        NuguConfigType DEFAULT_VALUES = {
            { Key::WAKEUP_WITH_LISTENING, "false" },
            { Key::WAKEUP_WORD, "아리아" },
            { Key::ASR_EPD_TYPE, "CLIENT" },
            { Key::ASR_EPD_TIMEOUT_SEC, "7" },
            { Key::ASR_EPD_MAX_DURATION_SEC, "10" },
            { Key::ASR_EPD_PAUSE_LENGTH_MSEC, "700" },
            { Key::ASR_ENCODING, "COMPLETE" },
            { Key::ASR_EPD_SAMPLERATE, "16k"},
            { Key::ASR_EPD_FORMAT, "s16le"},
            { Key::ASR_EPD_CHANNEL, "1"},
            { Key::KWD_SAMPLERATE, "16k"},
            { Key::KWD_FORMAT, "s16le"},
            { Key::KWD_CHANNEL, "1"},
            { Key::SERVER_RESPONSE_TIMEOUT_MSEC, "10000" },
            { Key::MODEL_PATH, "./" },
            { Key::TTS_ENGINE, "skt" }
        };
    }

    const NuguConfigType getDefaultValues()
    {
        return DEFAULT_VALUES;
    }
}

} // NuguInterface
