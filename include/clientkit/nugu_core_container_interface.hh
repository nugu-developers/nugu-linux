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

#ifndef __NUGU_CORE_CONTAINER_INTERFACE_H__
#define __NUGU_CORE_CONTAINER_INTERFACE_H__

#include <nugu.h>
#include <clientkit/capability_helper_interface.hh>
#include <clientkit/media_player_interface.hh>
#include <clientkit/network_manager_interface.hh>
#include <clientkit/nugu_timer_interface.hh>
#include <clientkit/speech_recognizer_interface.hh>
#include <clientkit/wakeup_interface.hh>

namespace NuguClientKit {

/**
 * @file nugu_core_container_interface.hh
 * @defgroup NuguCoreContainerInterface NuguCoreContainerInterface
 * @ingroup SDKNuguClientKit
 * @brief NuguCoreContainer Interface
 *
 * Container which have components factory and methods for manipulating NuguCore
 *
 * @{
 */

/**
 * @brief NuguCoreContainer interface
 */
class NUGU_API INuguCoreContainer {
public:
    virtual ~INuguCoreContainer() = default;

    /**
     * @brief Create WakeupHandler instance
     * @param[in] model_file WakeupModelFile object
     */
    virtual IWakeupHandler* createWakeupHandler(const WakeupModelFile& model_file) = 0;

    /**
     * @brief Create SpeechRecognizer instance
     * @param[in] model_path required model file path
     * @param[in] epd_attr epd attribute like timeout, max duration
     */
    virtual ISpeechRecognizer* createSpeechRecognizer(const std::string& model_path = "", const EpdAttribute& epd_attr = {}) = 0;

    /**
     * @brief Create MediaPlayer instance
     */
    virtual IMediaPlayer* createMediaPlayer() = 0;

    /**
     * @brief Create TTSPlayer instance
     */
    virtual ITTSPlayer* createTTSPlayer() = 0;

    /**
     * @brief Create NuguTimer instance
     * @param[in] singleshot By setting the singleshot to true, you can trigger
     * the timer only once. The default value of singleshot is false,
     * and the timer runs repeatedly.
     */
    virtual INuguTimer* createNuguTimer(bool singleshot = false) = 0;

    /**
     * @brief Get CapabilityHelper instance
     */
    virtual ICapabilityHelper* getCapabilityHelper() = 0;
};

/**
 * @}
 */

} // NuguClientKit

#endif /* __NUGU_CORE_CONTAINER_INTERFACE_H__ */
