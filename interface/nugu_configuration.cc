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

namespace NuguClientKit {

namespace NuguConfig {
    namespace {
        NuguConfigType DEFAULT_VALUES = {
            { Key::WAKEUP_WITH_LISTENING, "false" },
            { Key::WAKEUP_WORD, "1" },
            { Key::ASR_EPD_TYPE, "CLIENT" },
            { Key::ASR_ENCODING, "COMPLETE" },
            { Key::MODEL_PATH, "./" },
            { Key::SERVER_TYPE, "PRD" },
            { Key::UUID_PHASE, "0" },
            { Key::USER_AGENT, NUGU_USERAGENT },
            { Key::GATEWAY_REGISTRY_DNS, "reg-http.sktnugu.com" }
        };

        std::string toChangeCase(const std::string& src_string, bool is_tolower = false)
        {
            if (src_string.empty())
                return src_string;

            std::string changed_string;
            changed_string.resize(src_string.size());
            std::transform(src_string.begin(), src_string.end(), changed_string.begin(), is_tolower ? tolower : toupper);

            return changed_string;
        }

        bool isEqualString(std::string str1, std::string str2)
        {
            if (str1.empty() || str2.empty() || str1.size() != str2.size())
                return false;

            return std::equal(str1.begin(), str1.end(), str2.begin(),
                [](const char& a, const char& b) {
                    return (std::tolower(a) == std::tolower(b));
                });
        }
    }

    const NuguConfigType getDefaultValues()
    {
        DEFAULT_VALUES[Key::USER_AGENT].append(toChangeCase(DEFAULT_VALUES[Key::SERVER_TYPE]));

        return DEFAULT_VALUES;
    }

    const NuguConfigType getDefaultValues(NuguConfigType& user_map)
    {
        std::string server_type;
        std::string uuid_phase;

        // set user defined SERVER_TYPE
        if (getenv("NUGU_SERVER_TYPE"))
            server_type = getenv("NUGU_SERVER_TYPE");

        if (server_type.empty())
            server_type = user_map[Key::SERVER_TYPE];

        if (server_type.empty())
            server_type = DEFAULT_VALUES[Key::SERVER_TYPE];

        // set user defined UUID_PHASE
        if (getenv("NUGU_UUID_PHASE")) {
            uuid_phase = getenv("NUGU_UUID_PHASE");

            if (!uuid_phase.empty())
                user_map[Key::UUID_PHASE] = uuid_phase;
        }

        DEFAULT_VALUES[Key::USER_AGENT].append(toChangeCase(server_type));

        if (!isEqualString(server_type, DEFAULT_VALUES[Key::SERVER_TYPE]))
            DEFAULT_VALUES[Key::GATEWAY_REGISTRY_DNS].insert(0, toChangeCase(server_type, true).append("-"));

        return DEFAULT_VALUES;
    }
}

} // NuguClientKit
