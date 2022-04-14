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

#include <glib.h>
#include <memory>

#include "clientkit/nugu_client.hh"
#include "dialog_ux_state_aggregator.hh"

using namespace NuguClientKit;

static const char* PLAY_SERVICE_ID = "play_service_id";

class FakeExecutor {
public:
    enum class DialogType {
        General,
        GeneralWithChips,
        DM,
        Nudge
    };

public:
    explicit FakeExecutor(ISessionManager* session_manager)
        : session_manager(session_manager)
    {
    }

    void addASRListener(IASRListener* asr_listener)
    {
        this->asr_listener = asr_listener;
    }

    void addTTSListener(ITTSListener* tts_listener)
    {
        this->tts_listener = tts_listener;
    }

    void addChipsListener(IChipsListener* chips_listener)
    {
        this->chips_listener = chips_listener;
    }

    void addInteractionControlManagerListener(IInteractionControlManagerListener* ic_manager_listener)
    {
        this->ic_manager_listener = ic_manager_listener;
    }

    void setDialogType(DialogType type)
    {
        dialog_type = type;
    }

    void wakeup()
    {
        if (tts_state == TTSState::TTS_SPEECH_START)
            notifyTTSState(TTSState::TTS_SPEECH_STOP);

        notifyASRState(ASRState::LISTENING);
    }

    void startSpeech()
    {
        notifyASRState(ASRState::RECOGNIZING);
    }

    void endSpeech()
    {
        notifyASRState(ASRState::BUSY);
    }

    void onReceiveResponse()
    {
        if (dialog_type == DialogType::DM)
            ic_manager_listener->onHasMultiTurn();

        notifyASRState(ASRState::IDLE);
        asr_listener->onComplete("Response", dialog_id);

        if (dialog_type == DialogType::DM) {
            ic_manager_listener->onModeChanged(true);
            session_manager->set(dialog_id, Session { "session_id", PLAY_SERVICE_ID });
            session_manager->activate(dialog_id);
        } else if (dialog_type == DialogType::Nudge) {
            ic_manager_listener->onModeChanged(true);
            chips_listener->onReceiveRender(composeChipsInfo());
        } else if (dialog_type == DialogType::GeneralWithChips) {
            chips_listener->onReceiveRender(composeChipsInfo());
        }
    }

    void onTTSStarted()
    {
        notifyTTSState(TTSState::TTS_SPEECH_START);
    }

    void onTTSFinished()
    {
        notifyTTSState(TTSState::TTS_SPEECH_FINISH);

        if (dialog_type != DialogType::General && dialog_type != DialogType::GeneralWithChips)
            notifyASRState(ASRState::LISTENING, ASRInitiator::EXPECT_SPEECH);
    }

    void onDialogFinished()
    {
        if (dialog_type == DialogType::DM) {
            ic_manager_listener->onModeChanged(false);
            session_manager->deactivate(dialog_id);
        } else if (dialog_type == DialogType::Nudge) {
            ic_manager_listener->onModeChanged(false);
        }

        notifyASRState(ASRState::IDLE);
    }

private:
    void notifyASRState(ASRState state, ASRInitiator initiator = ASRInitiator::WAKE_UP_WORD)
    {
        if (state == ASRState::LISTENING)
            dialog_id = "dialog_id_" + std::to_string(++dialog_id_index);

        asr_listener->onState(state, dialog_id, initiator);
    }
    void notifyTTSState(TTSState state)
    {
        tts_state = state;
        tts_listener->onTTSState(tts_state, dialog_id);
    }
    ChipsInfo composeChipsInfo()
    {
        ChipsInfo chips_info;
        chips_info.play_service_id = PLAY_SERVICE_ID;
        chips_info.target = ChipsTarget::DM;
        chips_info.contents.emplace_back(ChipsInfo::Content { ChipsType::NUDGE, "text", "token" });

        return chips_info;
    }

    ISessionManager* session_manager = nullptr;
    IASRListener* asr_listener = nullptr;
    ITTSListener* tts_listener = nullptr;
    IChipsListener* chips_listener = nullptr;
    IInteractionControlManagerListener* ic_manager_listener = nullptr;
    DialogType dialog_type = DialogType::General;
    TTSState tts_state = TTSState::TTS_SPEECH_FINISH;
    std::string dialog_id;
    int dialog_id_index = 0;
};

class DialogUXStateAggregatorListener : public IDialogUXStateAggregatorListener {
public:
    void onStateChanged(DialogUXState state, bool multi_turn, const ChipsInfo& chips, bool session_active) override
    {
        this->state = state;
        this->is_multi_turn = multi_turn;
        this->chips = chips;
    }

    void onASRResult(const std::string& text) override
    {
        asr_result = text;
    }

    const DialogUXState& getState() const
    {
        return state;
    }

    const std::string& getASRResult() const
    {
        return asr_result;
    }

    const bool& isMultiTurn() const
    {
        return is_multi_turn;
    }

    const ChipsInfo& getChips() const
    {
        return chips;
    }

private:
    DialogUXState state = DialogUXState::Idle;
    std::string asr_result;
    bool is_multi_turn = false;
    ChipsInfo chips;
};

using TestFixture = struct {
    std::shared_ptr<NuguClient> nugu_client;
    std::shared_ptr<DialogUXStateAggregator> dialog_ux_state_aggregator;
    std::shared_ptr<DialogUXStateAggregatorListener> listener;
    std::shared_ptr<FakeExecutor> executor;
};

static void setup(TestFixture* fixture, gconstpointer user_data)
{
    fixture->nugu_client = std::make_shared<NuguClient>();
    auto session_manager(fixture->nugu_client->getNuguCoreContainer()->getCapabilityHelper()->getSessionManager());

    fixture->listener = std::make_shared<DialogUXStateAggregatorListener>();
    fixture->dialog_ux_state_aggregator = std::make_shared<DialogUXStateAggregator>(session_manager);
    fixture->dialog_ux_state_aggregator->addListener(fixture->listener.get());

    fixture->executor = std::make_shared<FakeExecutor>(session_manager);
    fixture->executor->addASRListener(fixture->dialog_ux_state_aggregator.get());
    fixture->executor->addTTSListener(fixture->dialog_ux_state_aggregator.get());
    fixture->executor->addChipsListener(fixture->dialog_ux_state_aggregator.get());
    fixture->executor->addInteractionControlManagerListener(fixture->dialog_ux_state_aggregator.get());
}

static void teardown(TestFixture* fixture, gconstpointer user_data)
{
    fixture->executor.reset();
    fixture->dialog_ux_state_aggregator.reset();
    fixture->listener.reset();
    fixture->nugu_client.reset();
}

static void test_dialog_ux_state_aggregator_asr_result(TestFixture* fixture, gconstpointer ignored)
{
    const auto& asr_result(fixture->listener->getASRResult());

    g_assert(asr_result.empty());

    fixture->executor->wakeup();
    fixture->executor->startSpeech();
    fixture->executor->endSpeech();
    fixture->executor->onReceiveResponse();
    g_assert(!asr_result.empty());
}

static void sub_test_dialog_ux_state_aggregator_progress_asr(TestFixture* fixture)
{
    const auto& state(fixture->listener->getState());

    g_assert(state == DialogUXState::Idle);

    fixture->executor->wakeup();
    g_assert(state == DialogUXState::Listening);

    fixture->executor->startSpeech();
    g_assert(state == DialogUXState::Recognizing);

    fixture->executor->endSpeech();
    g_assert(state == DialogUXState::Thinking);
}

static void test_dialog_ux_state_aggregator_general(TestFixture* fixture, gconstpointer ignored)
{
    const auto& state(fixture->listener->getState());
    const auto& multi_turn(fixture->listener->isMultiTurn());

    fixture->executor->setDialogType(FakeExecutor::DialogType::General);
    sub_test_dialog_ux_state_aggregator_progress_asr(fixture);

    fixture->executor->onReceiveResponse();
    g_assert(state == DialogUXState::Idle);

    fixture->executor->onTTSStarted();
    g_assert(state == DialogUXState::Speaking && !multi_turn);

    fixture->executor->onTTSFinished();
    g_assert(state == DialogUXState::Idle);
}

static void test_dialog_ux_state_aggregator_dm(TestFixture* fixture, gconstpointer ignored)
{
    const auto& state(fixture->listener->getState());
    const auto& multi_turn(fixture->listener->isMultiTurn());

    fixture->executor->setDialogType(FakeExecutor::DialogType::DM);
    sub_test_dialog_ux_state_aggregator_progress_asr(fixture);

    fixture->executor->onReceiveResponse();
    g_assert(state != DialogUXState::Idle);

    fixture->executor->onTTSStarted();
    g_assert(state == DialogUXState::Speaking && multi_turn);

    // progress dialog mode
    fixture->executor->onTTSFinished();
    g_assert(state == DialogUXState::Listening && multi_turn);

    fixture->executor->startSpeech();
    g_assert(state == DialogUXState::Recognizing);

    fixture->executor->endSpeech();
    g_assert(state == DialogUXState::Thinking);

    fixture->executor->onDialogFinished();
    g_assert(state == DialogUXState::Idle);
}

static void test_dialog_ux_state_aggregator_nudge(TestFixture* fixture, gconstpointer ignored)
{
    const auto& state(fixture->listener->getState());
    const auto& multi_turn(fixture->listener->isMultiTurn());
    const auto& chips(fixture->listener->getChips());

    fixture->executor->setDialogType(FakeExecutor::DialogType::Nudge);
    g_assert(chips.contents.empty());
    sub_test_dialog_ux_state_aggregator_progress_asr(fixture);

    fixture->executor->onReceiveResponse();
    g_assert(state == DialogUXState::Idle);

    fixture->executor->onTTSStarted();
    g_assert(state == DialogUXState::Speaking && !multi_turn);

    // progress dialog mode
    fixture->executor->onTTSFinished();
    g_assert(state == DialogUXState::Listening && multi_turn);
    g_assert(!chips.contents.empty());

    fixture->executor->startSpeech();
    g_assert(state == DialogUXState::Recognizing);

    fixture->executor->endSpeech();
    g_assert(state == DialogUXState::Thinking);

    fixture->executor->onDialogFinished();
    g_assert(state == DialogUXState::Idle);
    g_assert(chips.contents.empty());
}

static void test_dialog_ux_state_aggregator_interrupt_dialog(TestFixture* fixture, gconstpointer ignored)
{
    const auto& state(fixture->listener->getState());
    const auto& multi_turn(fixture->listener->isMultiTurn());

    auto checkDialogMode = [&](const bool& is_multi_turn) {
        // wakeup in TTS playing
        fixture->executor->onReceiveResponse();
        fixture->executor->onTTSStarted();
        fixture->executor->wakeup();
        g_assert(state == DialogUXState::Listening && is_multi_turn);

        fixture->executor->onDialogFinished();
        g_assert(state == DialogUXState::Idle);
    };

    fixture->executor->setDialogType(FakeExecutor::DialogType::DM);
    sub_test_dialog_ux_state_aggregator_progress_asr(fixture);
    checkDialogMode(multi_turn); // progress dialog mode if DM

    fixture->executor->setDialogType(FakeExecutor::DialogType::Nudge);
    sub_test_dialog_ux_state_aggregator_progress_asr(fixture);
    checkDialogMode(!multi_turn); //  progress not dialog mode if nudge
}

static void test_dialog_ux_state_aggregator_reset_chips(TestFixture* fixture, gconstpointer ignored)
{
    const auto& state(fixture->listener->getState());
    const auto& chips(fixture->listener->getChips());
    const auto& multi_turn(fixture->listener->isMultiTurn());

    fixture->executor->setDialogType(FakeExecutor::DialogType::GeneralWithChips);
    sub_test_dialog_ux_state_aggregator_progress_asr(fixture);

    fixture->executor->onReceiveResponse();
    fixture->executor->onTTSStarted();
    g_assert(state == DialogUXState::Speaking && !multi_turn);

    // interrupt TTS playing and try DM
    fixture->executor->setDialogType(FakeExecutor::DialogType::DM);
    fixture->executor->wakeup();
    g_assert(state == DialogUXState::Listening);

    fixture->executor->startSpeech();
    fixture->executor->endSpeech();
    fixture->executor->onReceiveResponse();
    fixture->executor->onTTSStarted();
    g_assert(state == DialogUXState::Speaking && multi_turn);

    // progress dialog mode
    fixture->executor->onTTSFinished();
    g_assert(state == DialogUXState::Listening && multi_turn);

    // Because DM has not chips in this test, it has to be empty.
    g_assert(chips.contents.empty());
}

#define G_TEST_ADD_FUNC(name, func) \
    g_test_add(name, TestFixture, nullptr, setup, func, teardown);

int main(int argc, char* argv[])
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
    g_type_init();
#endif

    g_test_init(&argc, &argv, (void*)NULL);
    g_log_set_always_fatal((GLogLevelFlags)G_LOG_FATAL_MASK);

    G_TEST_ADD_FUNC("/clientkit/DialogUXStateAggregator/ASRResult", test_dialog_ux_state_aggregator_asr_result);
    G_TEST_ADD_FUNC("/clientkit/DialogUXStateAggregator/General", test_dialog_ux_state_aggregator_general);
    G_TEST_ADD_FUNC("/clientkit/DialogUXStateAggregator/DM", test_dialog_ux_state_aggregator_dm);
    G_TEST_ADD_FUNC("/clientkit/DialogUXStateAggregator/nudge", test_dialog_ux_state_aggregator_nudge);
    G_TEST_ADD_FUNC("/clientkit/DialogUXStateAggregator/interruptDialog", test_dialog_ux_state_aggregator_interrupt_dialog);
    G_TEST_ADD_FUNC("/clientkit/DialogUXStateAggregator/resetChips", test_dialog_ux_state_aggregator_reset_chips);

    return g_test_run();
}
