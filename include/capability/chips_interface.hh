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

#ifndef __NUGU_CHIPS_INTERFACE_H__
#define __NUGU_CHIPS_INTERFACE_H__

#include <clientkit/capability_interface.hh>

namespace NuguCapability {

using namespace NuguClientKit;

/**
 * @file chips_interface.hh
 * @defgroup ChipsInterface ChipsInterface
 * @ingroup SDKNuguCapability
 * @brief Chips capability interface
 *
 * It's used for rendering voice command guide which is displayed on voice chrome in DM situation.
 *
 * @{
 */

/**
 * @brief Chips Target
 */
enum class ChipsTarget {
    DM, /**< active with ASR.ExpectSpeech + Session.Set directives */
    LISTEN, /**< active with ASR.ExpectSpeech directive */
    SPEAKING /**< active with TTS.Speak directive */
};

/**
 * @brief Chips Type
 */
enum class ChipsType {
    NUDGE, /**< Nudge UI type */
    ACTION, /**< Action Button type */
    GENERAL /**< Default type */
};

/**
 * @brief Model for holding chips Info.
 * @see IChipsListener::onReceiveRender
 */
typedef struct {
    struct Content {
        ChipsType type; /**< chips type */
        std::string text; /**< text for voice command guide */
        std::string token; /**< token which is used to send TextInput event */
    };

    std::string play_service_id; /**< playServiceId */
    ChipsTarget target; /**< target for rendering voice command guide */
    std::vector<Content> contents; /**< chip content list */
} ChipsInfo;

/**
 * @brief chips listener interface
 * @see IChipsHandler
 */
class IChipsListener : public ICapabilityListener {
public:
    virtual ~IChipsListener() = default;

    /**
     * @brief Notified when receiving Render directive from server.
     * @param[in] chips_info received datas for rendering voice command guide
     */
    virtual void onReceiveRender(ChipsInfo&& chips_info) = 0;
};

/**
 * @brief chips handler interface
 * @see IChipsListener
 */
class IChipsHandler : virtual public ICapabilityInterface {
public:
    virtual ~IChipsHandler() = default;
};

/**
 * @}
 */

} // NuguCapability

#endif /* __NUGU_CHIPS_INTERFACE_H__ */
