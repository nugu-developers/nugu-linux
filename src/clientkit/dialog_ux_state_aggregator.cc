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

#include "base/nugu_log.h"

#include "dialog_ux_state_aggregator.hh"

namespace NuguClientKit {

struct DialogUXStateAggregator::Impl {
    std::vector<IDialogUXStateAggregatorListener*> listeners;
    ISessionManager* session_manager = nullptr;
    DialogUXState dialog_ux_state = DialogUXState::Idle;
    ASRInitiator asr_initiator = ASRInitiator::WAKE_UP_WORD;
    ASRState asr_state = ASRState::IDLE;
    TTSState tts_state = TTSState::TTS_SPEECH_FINISH;
    bool is_multi_turn = false;
    std::string cur_dialog_id;
    std::string chips_dialog_id;
    ChipsInfo chips {};

    explicit Impl(ISessionManager* session_manager);
    void setState(DialogUXState state);
    void setIdleState();
    bool isSessionActive();
    std::string getStateStr(DialogUXState state);
};

/*******************************************************************************
 * define IDialogUXStateAggregatorListener (no-op)
 ******************************************************************************/

void IDialogUXStateAggregatorListener::onStateChanged(DialogUXState state, bool multi_turn, const ChipsInfo& chips, bool session_active)
{
    // no-op
}

void IDialogUXStateAggregatorListener::onASRResult(const std::string& text)
{
    // no-op
}

/*******************************************************************************
 * define DialogUXStateAggregator
 ******************************************************************************/

DialogUXStateAggregator::DialogUXStateAggregator(ISessionManager* session_manager)
    : pimpl(std::unique_ptr<Impl>(new Impl(session_manager)))
{
}

DialogUXStateAggregator::~DialogUXStateAggregator()
{
}

void DialogUXStateAggregator::addListener(IDialogUXStateAggregatorListener* listener)
{
    if (!listener)
        return;

    if (std::find(pimpl->listeners.cbegin(), pimpl->listeners.cend(), listener) == pimpl->listeners.cend())
        pimpl->listeners.emplace_back(listener);
}

void DialogUXStateAggregator::removeListener(IDialogUXStateAggregatorListener* listener)
{
    if (!listener)
        return;

    pimpl->listeners.erase(std::find(pimpl->listeners.cbegin(), pimpl->listeners.cend(), listener));
}

void DialogUXStateAggregator::onState(ASRState state, const std::string& dialog_id, ASRInitiator initiator)
{
    pimpl->asr_state = state;
    pimpl->asr_initiator = initiator;

    switch (state) {
    case ASRState::IDLE:
        pimpl->setIdleState();
        break;
    case ASRState::EXPECTING_SPEECH:
        // ignore
        break;
    case ASRState::LISTENING:
        pimpl->setState(DialogUXState::Listening);
        break;
    case ASRState::RECOGNIZING:
        pimpl->setState(DialogUXState::Recognizing);
        break;
    case ASRState::BUSY:
        pimpl->setState(DialogUXState::Thinking);
        break;
    };
}

void DialogUXStateAggregator::onNone(const std::string& dialog_id)
{
    // no-op
}

void DialogUXStateAggregator::onPartial(const std::string& text, const std::string& dialog_id)
{
    for (const auto& listener : pimpl->listeners)
        listener->onASRResult(text);
}

void DialogUXStateAggregator::onComplete(const std::string& text, const std::string& dialog_id)
{
    pimpl->cur_dialog_id = dialog_id;

    for (const auto& listener : pimpl->listeners)
        listener->onASRResult(text);
}

void DialogUXStateAggregator::onError(ASRError error, const std::string& dialog_id, bool listen_timeout_fail_beep)
{
    // no-op
}

void DialogUXStateAggregator::onCancel(const std::string& dialog_id)
{
    // no-op
}

void DialogUXStateAggregator::onTTSState(TTSState state, const std::string& dialog_id)
{
    pimpl->tts_state = state;

    switch (state) {
    case TTSState::TTS_SPEECH_START:
        pimpl->setState(DialogUXState::Speaking);
        break;
    case TTSState::TTS_SPEECH_STOP:
    case TTSState::TTS_SPEECH_FINISH:
        pimpl->setIdleState();
        break;
    };
}

void DialogUXStateAggregator::onTTSText(const std::string& text, const std::string& dialog_id)
{
    // no-op
}

void DialogUXStateAggregator::onTTSCancel(const std::string& dialog_id)
{
    // no-op
}

void DialogUXStateAggregator::onReceiveRender(const ChipsInfo& chips_info)
{
    pimpl->chips = chips_info;
    pimpl->chips_dialog_id = pimpl->cur_dialog_id;
}

void DialogUXStateAggregator::onHasMultiTurn()
{
    nugu_dbg("onHasMultiTurn");

    pimpl->is_multi_turn = true;
}

void DialogUXStateAggregator::onModeChanged(bool multi_turn)
{
    if (pimpl->is_multi_turn == multi_turn)
        return;

    nugu_dbg("is_multi_turn is changed(%d -> %d)", pimpl->is_multi_turn, multi_turn);

    pimpl->is_multi_turn = multi_turn;

    if (!pimpl->is_multi_turn)
        pimpl->setIdleState();
}

/*******************************************************************************
 * define Impl
 ******************************************************************************/
DialogUXStateAggregator::Impl::Impl(ISessionManager* session_manager)
    : session_manager(session_manager)
{
}

void DialogUXStateAggregator::Impl::setState(DialogUXState state)
{
    if (dialog_ux_state == state)
        return;

    dialog_ux_state = state;
    bool session_active = isSessionActive();
    bool multi_turn = false;
    ChipsInfo chips_info {};

    if (dialog_ux_state == DialogUXState::Listening && (asr_initiator == ASRInitiator::EXPECT_SPEECH || session_active)) {
        multi_turn = is_multi_turn;

        if (chips_dialog_id == cur_dialog_id)
            chips_info = chips;
    } else if (dialog_ux_state == DialogUXState::Speaking && session_active) {
        multi_turn = is_multi_turn;
    }

    nugu_info("state: %s, multi-turn: %d", getStateStr(dialog_ux_state).c_str(), multi_turn);

    for (const auto& listener : listeners)
        listener->onStateChanged(dialog_ux_state, multi_turn, chips_info, session_active);
}

void DialogUXStateAggregator::Impl::setIdleState()
{
    if (is_multi_turn || asr_state != ASRState::IDLE || tts_state == TTSState::TTS_SPEECH_START)
        return;

    setState(DialogUXState::Idle);
}

bool DialogUXStateAggregator::Impl::isSessionActive()
{
    return !session_manager->getActiveSessionInfo().empty();
}

std::string DialogUXStateAggregator::Impl::getStateStr(DialogUXState state)
{
    std::string state_str;

    switch (state) {
    case DialogUXState::Idle:
        state_str = "Idle";
        break;
    case DialogUXState::Listening:
        state_str = "Listening";
        break;
    case DialogUXState::Recognizing:
        state_str = "Recognizing";
        break;
    case DialogUXState::Speaking:
        state_str = "Speaking";
        break;
    case DialogUXState::Thinking:
        state_str = "Thinking";
        break;
    };

    return state_str;
}

} // NuguClientKit
