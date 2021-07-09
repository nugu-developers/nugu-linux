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

#ifndef __CAPABILITY_FACTORY_H__
#define __CAPABILITY_FACTORY_H__

#include <clientkit/capability_interface.hh>

namespace NuguCapability {

using namespace NuguClientKit;

/**
 * @file capability_factory.hh
 * @defgroup CapabilityFactory CapabilityFactory
 * @ingroup SDKNuguCapability
 * @brief CapabilityFactory
 *
 * It's a factory for creating each capability instance. Even if it create CapabilityAgent instance itself,
 * it return the related, abstracted handler for encapsulating implements.
 *
 * @{
 */

/**
 * @brief ASRAgent
 */
class ASRAgent;

/**
 * @brief TTSAgent
 */
class TTSAgent;

/**
 * @brief AudioPlayerAgent
 */
class AudioPlayerAgent;

/**
 * @brief SystemAgent
 */
class SystemAgent;

/**
 * @brief TextAgent
 */
class TextAgent;

/**
 * @brief SpeakerAgent
 */
class SpeakerAgent;

/**
 * @brief MicAgent
 */
class MicAgent;

/**
 * @brief SoundAgent
 */
class SoundAgent;

/**
 * @brief SessionAgent
 */
class SessionAgent;

/**
 * @brief DisplayAgent
 */
class DisplayAgent;

/**
 * @brief UtilityAgent
 */
class UtilityAgent;

/**
 * @brief ExtensionAgent
 */
class ExtensionAgent;

/**
 * @brief ChipsAgent
 */
class ChipsAgent;

/**
 * @brief NudgeAgent
 */
class NudgeAgent;

/**
 * @brief RoutineAgent
 */
class RoutineAgent;

/**
 * @brief PhoneCallAgent
 */
class PhoneCallAgent;

/**
 * @brief CapabilityFactory
 */
class CapabilityFactory {
public:
    CapabilityFactory() = delete;

    /**
     * @brief Create capability agent instance and return related capability handler.
     * @param[in] T capability agent class type
     * @param[in] V capability handler class type
     * @param[in] listener capability listener instance
     */
    template <typename T, typename V>
    static V* makeCapability(ICapabilityListener* listener = nullptr);
};

/**
 * @}
 */

} // NuguCapability

#endif /* __CAPABILITY_FACTORY_H__ */
