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

#include "nugu_sample_manager.hh"
#include "nugu_sdk_manager.hh"

using namespace NuguClientKit;

int main(int argc, char** argv)
{
    auto nugu_sample_manager(std::make_shared<NuguSampleManager>());
    auto nugu_sdk_manager(std::make_shared<NuguSDKManager>(nugu_sample_manager.get()));

    if (!nugu_sample_manager->handleArguments(argc, argv))
        return EXIT_FAILURE;

    if (!getenv("NUGU_TOKEN")) {
        NuguSampleManager::error("> Token is empty");
        return EXIT_FAILURE;
    }

    nugu_sample_manager->prepare();
    nugu_sdk_manager->setup();
    nugu_sample_manager->runLoop();

    return EXIT_SUCCESS;
}
