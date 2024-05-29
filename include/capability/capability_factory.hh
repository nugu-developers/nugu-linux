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

#include <nugu.h>
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
class NUGU_API ASRAgent;

/**
 * @brief TTSAgent
 */
class NUGU_API TTSAgent;

/**
 * @brief AudioPlayerAgent
 */
class NUGU_API AudioPlayerAgent;

/**
 * @brief SystemAgent
 */
class NUGU_API SystemAgent;

/**
 * @brief TextAgent
 */
class NUGU_API TextAgent;

/**
 * @brief SpeakerAgent
 */
class NUGU_API SpeakerAgent;

/**
 * @brief MicAgent
 */
class NUGU_API MicAgent;

/**
 * @brief SoundAgent
 */
class NUGU_API SoundAgent;

/**
 * @brief SessionAgent
 */
class NUGU_API SessionAgent;

/**
 * @brief DisplayAgent
 */
class NUGU_API DisplayAgent;

/**
 * @brief UtilityAgent
 */
class NUGU_API UtilityAgent;

/**
 * @brief ExtensionAgent
 */
class NUGU_API ExtensionAgent;

/**
 * @brief ChipsAgent
 */
class NUGU_API ChipsAgent;

/**
 * @brief NudgeAgent
 */
class NUGU_API NudgeAgent;

/**
 * @brief RoutineAgent
 */
class NUGU_API RoutineAgent;

/**
 * @brief PhoneCallAgent
 */
class NUGU_API PhoneCallAgent;

/**
 * @brief MessageAgent
 */
class NUGU_API MessageAgent;

/**
 * @brief BluetoothAgent
 */
class NUGU_API BluetoothAgent;

/**
 * @brief LocationAgent
 */
class NUGU_API LocationAgent;

/**
 * @brief BatteryAgent
 */
class NUGU_API BatteryAgent;

/**
 * @brief CapabilityFactory
 */
class NUGU_API CapabilityFactory {
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
