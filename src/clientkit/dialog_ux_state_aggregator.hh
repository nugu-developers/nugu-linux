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

#ifndef __DIALOG_UX_STATE_AGGREGATOR_H__
#define __DIALOG_UX_STATE_AGGREGATOR_H__

#include <memory>

#include "capability/asr_interface.hh"
#include "capability/chips_interface.hh"
#include "capability/tts_interface.hh"
#include "clientkit/dialog_ux_state_aggregator_interface.hh"
#include "clientkit/interaction_control_manager_interface.hh"
#include "clientkit/session_manager_interface.hh"

namespace NuguClientKit {

using namespace NuguCapability;

class DialogUXStateAggregator : public IASRListener,
                                public ITTSListener,
                                public IChipsListener,
                                public IInteractionControlManagerListener {
public:
    explicit DialogUXStateAggregator(ISessionManager* session_manager);
    virtual ~DialogUXStateAggregator();

    void addListener(IDialogUXStateAggregatorListener* listener);
    void removeListener(IDialogUXStateAggregatorListener* listener);

    // implements IASRListener
    void onState(ASRState state, const std::string& dialog_id, ASRInitiator initiator) override;
    void onNone(const std::string& dialog_id) override;
    void onPartial(const std::string& text, const std::string& dialog_id) override;
    void onComplete(const std::string& text, const std::string& dialog_id) override;
    void onError(ASRError error, const std::string& dialog_id, bool listen_timeout_fail_beep = true) override;
    void onCancel(const std::string& dialog_id) override;

    // implements ITTSListener
    void onTTSState(TTSState state, const std::string& dialog_id) override;
    void onTTSText(const std::string& text, const std::string& dialog_id) override;
    void onTTSCancel(const std::string& dialog_id) override;

    // implements IChipsListener
    void onReceiveRender(const ChipsInfo& chips_info) override;

    // implements IInteractionControlManagerListener
    void onHasMultiTurn() override;
    void onModeChanged(bool multi_turn) override;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl;
};

} // NuguClientKit
#endif /* __DIALOG_UX_STATE_AGGREGATOR_H__ */
