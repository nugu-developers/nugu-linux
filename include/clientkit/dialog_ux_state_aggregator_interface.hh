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

#ifndef __DIALOG_UX_STATE_AGGREGATOR_INTERFACE_H__
#define __DIALOG_UX_STATE_AGGREGATOR_INTERFACE_H__

#include <string>

#include <capability/chips_interface.hh>

namespace NuguClientKit {

using namespace NuguCapability;

/**
 * @file dialog_ux_state_aggregator_interface.hh
 * @defgroup DialogUXStateAggregatorInterface DialogUXStateAggregatorInterface
 * @ingroup SDKNuguClientKit
 * @brief DialogUXStateAggregator Interface
 *
 * Interface of DialogUXStateAggregator for receiving DialogUXState which is
 * composed by combination of ASR, TTS, Chips info.
 *
 * @{
 */

/**
 * @brief DialogUXState list
 */
enum class DialogUXState {
    Listening, /**< start to listen speech */
    Recognizing, /**< recognize speech */
    Thinking, /**< wait response */
    Speaking, /**< TTS playing */
    Idle /**< no action */
};

/**
 * @brief IDialogUXStateAggregatorListener interface
 */
class IDialogUXStateAggregatorListener {
public:
    virtual ~IDialogUXStateAggregatorListener() = default;

    /**
     * @brief Receive current DialogUXState and additional infos.
     * @param[in] state current DialogUXState
     * @param[in] multi_turn whether current dialog mode is multi-turn or not
     * @param[in] chips chips info
     * @param[in] session_active whether session is activated or not
     */
    virtual void onStateChanged(DialogUXState state, bool multi_turn, const ChipsInfo& chips, bool session_active);

    /**
     * @brief Receive ASR result.
     * @param[in] text result text
     */
    virtual void onASRResult(const std::string& text);
};

} // NuguClientKit

/**
 * @}
 */

#endif /* __DIALOG_UX_STATE_AGGREGATOR_INTERFACE_H__ */
