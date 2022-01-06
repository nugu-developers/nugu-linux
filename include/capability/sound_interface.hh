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

#ifndef __NUGU_SOUND_INTERFACE_H__
#define __NUGU_SOUND_INTERFACE_H__

#include <clientkit/capability_interface.hh>

namespace NuguCapability {

using namespace NuguClientKit;

/**
 * @file sound_interface.hh
 * @defgroup SoundInterface SoundInterface
 * @ingroup SDKNuguCapability
 * @brief Sound capability interface
 *
 * It's for handling request of play service which to play the specific audio resource file.
 *
 * @{
 */

/**
 * @brief Beep type list
 * @see ISoundListener::handleBeep
 */
enum class BeepType {
    RESPONSE_FAIL /**< the case which fallback play send to client */
};

/**
 * @brief sound listener interface
 * @see ISoundHandler
 */
class ISoundListener : virtual public ICapabilityListener {
public:
    virtual ~ISoundListener() = default;

    /**
     * @brief Handle beep sound which is related to received beep name.
     * @param[in] beep_type beep type
     * @param[in] dialog_id dialog request id
     */
    virtual void handleBeep(BeepType beep_type, const std::string& dialog_id) = 0;
};

/**
 * @brief sound handler interface
 * @see ISoundListener
 */
class ISoundHandler : virtual public ICapabilityInterface {
public:
    virtual ~ISoundHandler() = default;

    /**
     * @brief Send beep play result.
     * @param[in] is_succeeded result of beep play
     */
    virtual void sendBeepResult(bool is_succeeded) = 0;
};

/**
 * @}
 */

} // NuguCapability

#endif /* __NUGU_SOUND_INTERFACE_H__ */
