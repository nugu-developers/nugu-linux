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
#include <set>

#include "clientkit/capability.hh"
#include "speech_recognizer_aggregator.hh"

using namespace NuguClientKit;

// wakeup model file
static const char* ARIA_NET = "skt_trigger_am_aria.raw";
static const char* ARIA_SEARCH = "skt_trigger_search_aria.raw";
static const char* TINKERBELL_NET = "skt_trigger_am_tinkerbell.raw";
static const char* TINKERBELL_SEARCH = "skt_trigger_search_tinkerbell.raw";

// wakeup power
static const float WAKEUP_POWER_NOISE = 4.5f;
static const float WAKEUP_POWER_SPEECH = 8.5f;

static const ASRInitiator DEFAULT_ASR_INITIATOR = ASRInitiator::TAP;

enum class FakeAction {
    Wakeup,
    StartSpeech,
    EndSpeech,
    onReceiveResponse
};

class FakeExecutorListener {
public:
    virtual ~FakeExecutorListener() = default;

    virtual void onActionExecuted(FakeAction action) = 0;
};

class FakeExecutor {
public:
    void addListener(FakeExecutorListener* listener)
    {
        if (listener && listeners.find(listener) == listeners.end())
            listeners.emplace(listener);
    }

    void wakeup()
    {
        notifyActionExecuted(FakeAction::Wakeup);
    }

    void startSpeech()
    {
        notifyActionExecuted(FakeAction::StartSpeech);
    }

    void endSpeech()
    {
        notifyActionExecuted(FakeAction::EndSpeech);
    }

    void onReceiveResponse()
    {
        notifyActionExecuted(FakeAction::onReceiveResponse);
    }

private:
    void notifyActionExecuted(FakeAction action)
    {
        for (const auto& listener : listeners)
            listener->onActionExecuted(action);
    }

    std::set<FakeExecutorListener*> listeners;
};

class FakeWakeupHandler : public IWakeupHandler,
                          public FakeExecutorListener {
public:
    void setListener(IWakeupListener* listener) override
    {
        this->listener = listener;
    }

    bool startWakeup() override
    {
        is_detected = false;
        notifyWakeupState(WakeupDetectState::WAKEUP_DETECTING);

        return true;
    }

    void stopWakeup() override
    {
        if (!is_detected)
            notifyWakeupState(WakeupDetectState::WAKEUP_FAIL);
    }

    void changeModel(const WakeupModelFile& model_file) override
    {
        stopWakeup();

        this->model_file = model_file;
    }

    void onActionExecuted(FakeAction action) override
    {
        if (action == FakeAction::Wakeup) {
            is_detected = true;
            notifyWakeupState(WakeupDetectState::WAKEUP_DETECTED);
        }
    }

    const WakeupModelFile& getModelFile() const
    {
        return this->model_file;
    }

private:
    void notifyWakeupState(WakeupDetectState state)
    {
        if (listener)
            listener->onWakeupState(state, WAKEUP_POWER_NOISE, WAKEUP_POWER_SPEECH);
    }

    IWakeupListener* listener = nullptr;
    WakeupModelFile model_file {};
    bool is_detected = false;
};

class FakeASRHandler : public IASRHandler,
                       public Capability,
                       public FakeExecutorListener {
public:
    using WakeupPower = struct _WakeupPower {
        float noise;
        float speech;
    };

    using ASRResult = struct _ASRResult {
        std::string text;
        ASRError error;
    };

    FakeASRHandler()
        : Capability("ASR", "1.6")
    {
    }

    // define no-op
    void updateInfoForContext(NJson::Value& ctx) override { }
    void removeListener(IASRListener* listener) override { }
    void setAttribute(ASRAttribute&& attribute) override { }
    void setEpdAttribute(EpdAttribute&& attribute) override { }
    EpdAttribute getEpdAttribute() override { return {}; }

    void startRecognition(float power_noise, float power_speech, ASRInitiator initiator = ASRInitiator::TAP, AsrRecognizeCallback callback = nullptr) override
    {
        wakeup_power = { power_noise, power_speech };

        startRecognition(initiator);
    }

    void startRecognition(ASRInitiator initiator = ASRInitiator::TAP, AsrRecognizeCallback callback = nullptr) override
    {
        this->initiator = initiator;
        dialog_id = "dialog_id_" + std::to_string(++dialog_id_index);

        notifyASRState(ASRState::LISTENING);
    }

    void stopRecognition(bool cancel = false) override
    {
        notifyASRState(ASRState::IDLE);

        if (cancel)
            dialog_id.clear();

        if (listener)
            listener->onCancel(dialog_id);
    }

    void addListener(IASRListener* listener) override
    {
        this->listener = listener;
    }

    void onActionExecuted(FakeAction action) override
    {
        if (action == FakeAction::StartSpeech) {
            notifyASRState(ASRState::RECOGNIZING);
        } else if (action == FakeAction::EndSpeech) {
            notifyASRState(ASRState::BUSY);
        } else if (action == FakeAction::onReceiveResponse) {
            notifyASRResult();
            notifyASRState(ASRState::IDLE);
        }
    }

    void setTestResult(RecognitionResult::Status result_status)
    {
        this->result_status = result_status;
    }

    void clearDialogId()
    {
        dialog_id.clear();
    }

    const WakeupPower& getWakeupPower() const
    {
        return wakeup_power;
    }

    const ASRResult& getASRResult() const
    {
        return asr_result;
    }

    const std::string& getDialogId() const
    {
        return dialog_id;
    }

private:
    void notifyASRState(ASRState state)
    {
        if (listener)
            listener->onState(state, dialog_id, initiator);

        if (state == ASRState::IDLE) {
            initiator = DEFAULT_ASR_INITIATOR;
            wakeup_power = {};
        }
    }

    void notifyASRResult()
    {
        asr_result = {};

        switch (result_status) {
        case RecognitionResult::Status::Partial:
            if (listener)
                listener->onPartial(asr_result.text = "The Partial Recognition Result", dialog_id);
            break;
        case RecognitionResult::Status::Complete:
            if (listener)
                listener->onComplete(asr_result.text = "The Complete Recognition Result", dialog_id);
            break;
        case RecognitionResult::Status::None:
            if (listener)
                listener->onNone(dialog_id);
            break;
        case RecognitionResult::Status::Cancel:
            if (listener)
                listener->onCancel(dialog_id);
            break;
        case RecognitionResult::Status::Error:
            if (listener)
                listener->onError(asr_result.error = ASRError::RECOGNIZE_ERROR, dialog_id, false);
            break;
        }
    }

    IASRListener* listener = nullptr;
    ASRInitiator initiator = DEFAULT_ASR_INITIATOR;
    WakeupPower wakeup_power {};
    ASRResult asr_result {};
    RecognitionResult::Status result_status = RecognitionResult::Status::None;
    std::string dialog_id;
    int dialog_id_index = 0;
};

typedef struct _SpeechRecognizerAggregatorState {
    WakeupDetectState wakeup;
    ASRState asr;

    struct Extras {
        float power_noise;
        float power_speech;
        ASRInitiator asr_initiator;
    };

    Extras extras;
} SpeechRecognizerAggregatorState;

class SpeechRecognizerAggregatorListener : public ISpeechRecognizerAggregatorListener {
public:
    void onWakeupState(WakeupDetectState state, float power_noise, float power_speech) override
    {
        aggregate_state.wakeup = state;
        aggregate_state.extras.power_noise = power_noise;
        aggregate_state.extras.power_speech = power_speech;
    }

    void onASRState(ASRState state, const std::string& dialog_id, ASRInitiator initiator) override
    {
        aggregate_state.asr = state;
        aggregate_state.extras.asr_initiator = initiator;
        this->dialog_id = dialog_id;
    }

    void onResult(const RecognitionResult& result, const std::string& dialog_id) override
    {
        this->result = result;
        this->dialog_id = dialog_id;
    }

    const SpeechRecognizerAggregatorState& getState() const
    {
        return aggregate_state;
    }

    const std::string& getDialogId() const
    {
        return dialog_id;
    }

    const RecognitionResult& getRecognitionResult() const
    {
        return result;
    }

private:
    SpeechRecognizerAggregatorState aggregate_state { WakeupDetectState::WAKEUP_IDLE, ASRState::IDLE };
    RecognitionResult result {};
    std::string dialog_id;
};

using TestFixture = struct _TestFixture {
    std::shared_ptr<FakeWakeupHandler> wakeup_handler;
    std::shared_ptr<FakeASRHandler> asr_handler;
    std::shared_ptr<FakeExecutor> executor;
    std::shared_ptr<SpeechRecognizerAggregator> speech_recognizer_aggregator;
    std::shared_ptr<SpeechRecognizerAggregatorListener> listener;
};

static void setup(TestFixture* fixture, gconstpointer user_data)
{
    fixture->wakeup_handler = std::make_shared<FakeWakeupHandler>();
    fixture->asr_handler = std::make_shared<FakeASRHandler>();
    fixture->executor = std::make_shared<FakeExecutor>();
    fixture->listener = std::make_shared<SpeechRecognizerAggregatorListener>();
    fixture->speech_recognizer_aggregator = std::make_shared<SpeechRecognizerAggregator>();

    fixture->speech_recognizer_aggregator->addListener(fixture->listener.get());
    fixture->speech_recognizer_aggregator->setWakeupHandler(fixture->wakeup_handler);
    fixture->speech_recognizer_aggregator->setASRHandler(fixture->asr_handler.get());
    fixture->executor->addListener(fixture->wakeup_handler.get());
    fixture->executor->addListener(fixture->asr_handler.get());
}

static void teardown(TestFixture* fixture, gconstpointer user_data)
{
    fixture->listener.reset();
    fixture->speech_recognizer_aggregator.reset();
    fixture->executor.reset();
    fixture->asr_handler.reset();
    fixture->wakeup_handler.reset();
}

static void test_speech_recognizer_aggregator_set_wakeup_model(TestFixture* fixture, gconstpointer ignored)
{
    const auto& model_file = fixture->wakeup_handler->getModelFile();
    WakeupModelFile test_model_file;

    auto checkWakeupInfo = [&]() {
        g_assert(model_file.net == test_model_file.net);
        g_assert(model_file.search == test_model_file.search);
    };

    test_model_file = { ARIA_NET, ARIA_SEARCH };
    g_assert(fixture->speech_recognizer_aggregator->setWakeupModel(test_model_file));
    checkWakeupInfo();

    // validation check (maintain previous value)
    g_assert(!fixture->speech_recognizer_aggregator->setWakeupModel({}));
    checkWakeupInfo();

    // change model
    test_model_file = { TINKERBELL_NET, TINKERBELL_SEARCH };
    g_assert(fixture->speech_recognizer_aggregator->setWakeupModel(test_model_file));
    checkWakeupInfo();
}

static void test_speech_recognizer_aggregator_start_listening_with_trigger(TestFixture* fixture, gconstpointer ignored)
{
    const auto& state = fixture->listener->getState();

    g_assert(state.wakeup == WakeupDetectState::WAKEUP_IDLE);
    g_assert(state.asr == ASRState::IDLE);

    fixture->speech_recognizer_aggregator->startListeningWithTrigger();
    g_assert(state.wakeup == WakeupDetectState::WAKEUP_DETECTING);
    g_assert(state.asr == ASRState::IDLE);

    fixture->executor->wakeup();
    g_assert(state.wakeup == WakeupDetectState::WAKEUP_DETECTED);
    g_assert(state.asr == ASRState::LISTENING);

    fixture->executor->startSpeech();
    g_assert(state.asr == ASRState::RECOGNIZING);

    fixture->executor->endSpeech();
    g_assert(state.asr == ASRState::BUSY);

    fixture->executor->onReceiveResponse();
    g_assert(state.asr == ASRState::IDLE);
}

static void test_speech_recognizer_aggregator_start_listening(TestFixture* fixture, gconstpointer ignored)
{
    const auto& wakeup_power = fixture->asr_handler->getWakeupPower();
    const auto& state = fixture->listener->getState();
    const auto& source_dialog_id = fixture->asr_handler->getDialogId();
    const auto& target_dialog_id = fixture->listener->getDialogId();

    auto progressStartListening = [&](const FakeASRHandler::WakeupPower& test_wakeup_power, const ASRInitiator& test_initiator) {
        g_assert(wakeup_power.noise == test_wakeup_power.noise);
        g_assert(wakeup_power.speech == test_wakeup_power.speech);
        g_assert(state.extras.asr_initiator == test_initiator);
        g_assert(state.asr == ASRState::LISTENING);
        g_assert(source_dialog_id == target_dialog_id);

        fixture->executor->startSpeech();
        g_assert(state.asr == ASRState::RECOGNIZING);

        fixture->executor->endSpeech();
        g_assert(state.asr == ASRState::BUSY);

        fixture->executor->onReceiveResponse();
        g_assert(state.asr == ASRState::IDLE);
    };

    // no wakeup power
    fixture->speech_recognizer_aggregator->startListening();
    progressStartListening({ 0, 0 }, DEFAULT_ASR_INITIATOR);

    // has wakeup power
    fixture->speech_recognizer_aggregator->startListening(WAKEUP_POWER_NOISE, WAKEUP_POWER_SPEECH, ASRInitiator::PRESS_AND_HOLD);
    progressStartListening({ WAKEUP_POWER_NOISE, WAKEUP_POWER_SPEECH }, ASRInitiator::PRESS_AND_HOLD);
}

static void test_speech_recognizer_aggregator_stop_listening(TestFixture* fixture, gconstpointer ignored)
{
    const auto& dialog_id = fixture->asr_handler->getDialogId();
    const auto& state = fixture->listener->getState();

    auto progressStopListening = [&](bool cancel = false) {
        auto prev_asr_state = state.asr;
        fixture->speech_recognizer_aggregator->stopListening(cancel);
        g_assert(state.asr == ASRState::IDLE);
        g_assert((cancel || prev_asr_state == ASRState::IDLE) ? dialog_id.empty() : !dialog_id.empty());
        fixture->asr_handler->clearDialogId();
    };

    // stop before wakeup detecting
    fixture->speech_recognizer_aggregator->startListeningWithTrigger();
    g_assert(state.wakeup == WakeupDetectState::WAKEUP_DETECTING);
    g_assert(state.asr == ASRState::IDLE);
    progressStopListening();
    g_assert(state.wakeup != WakeupDetectState::WAKEUP_DETECTING);

    // stop after wakeup detected (cancel)
    fixture->speech_recognizer_aggregator->startListeningWithTrigger();
    fixture->executor->wakeup();
    g_assert(state.wakeup == WakeupDetectState::WAKEUP_DETECTED);
    g_assert(state.asr == ASRState::LISTENING);
    progressStopListening(true);

    // stop during ASRState::RECOGNIZING
    fixture->speech_recognizer_aggregator->startListening();
    fixture->executor->startSpeech();
    g_assert(state.asr == ASRState::RECOGNIZING);
    progressStopListening();

    // stop during ASRState::BUSY (cancel)
    fixture->speech_recognizer_aggregator->startListening();
    fixture->executor->startSpeech();
    fixture->executor->endSpeech();
    g_assert(state.asr == ASRState::BUSY);
    progressStopListening(true);
}

static void test_speech_recognizer_aggregator_consecutive_start_listening(TestFixture* fixture, gconstpointer ignored)
{
    const auto& wakeup_power = fixture->asr_handler->getWakeupPower();
    const auto& state = fixture->listener->getState();

    auto checkState = [&](const FakeASRHandler::WakeupPower& test_wakeup_power) {
        g_assert(state.asr == ASRState::LISTENING);
        g_assert(wakeup_power.noise == test_wakeup_power.noise);
        g_assert(wakeup_power.speech == test_wakeup_power.speech);
    };

    // consecutive startListening
    fixture->speech_recognizer_aggregator->startListening(WAKEUP_POWER_NOISE, WAKEUP_POWER_SPEECH);
    checkState({ WAKEUP_POWER_NOISE, WAKEUP_POWER_SPEECH });

    fixture->speech_recognizer_aggregator->startListening();
    checkState({}); // wakeup power is reset when ASR stopRecognition is called in startListening

    // consecutive startListeningWithTrigger
    fixture->speech_recognizer_aggregator->startListeningWithTrigger();
    g_assert(state.wakeup == WakeupDetectState::WAKEUP_DETECTING);
    g_assert(state.asr == ASRState::IDLE);

    fixture->executor->wakeup();
    g_assert(state.wakeup == WakeupDetectState::WAKEUP_DETECTED);
    g_assert(state.asr == ASRState::LISTENING);

    fixture->speech_recognizer_aggregator->startListeningWithTrigger();
    g_assert(state.wakeup == WakeupDetectState::WAKEUP_DETECTING);
    g_assert(state.asr == ASRState::IDLE);
}

static void test_speech_recognizer_aggregator_handle_separate_wakeup(TestFixture* fixture, gconstpointer ignored)
{
    const auto& state = fixture->listener->getState();

    // start wakeup not by SpeechRecognizerAggregator
    fixture->wakeup_handler->startWakeup();

    fixture->speech_recognizer_aggregator->startListening();
    g_assert(state.wakeup != WakeupDetectState::WAKEUP_DETECTING);
    g_assert(state.asr == ASRState::LISTENING);
}

static void test_speech_recognizer_aggregator_stop_before_change_wakeup(TestFixture* fixture, gconstpointer ignored)
{
    const auto& model_file = fixture->wakeup_handler->getModelFile();
    const auto& state = fixture->listener->getState();
    WakeupModelFile test_model_file;

    auto checkWakeupInfo = [&]() {
        g_assert(model_file.net == test_model_file.net);
        g_assert(model_file.search == test_model_file.search);
    };

    // change wakeup model to tinkerbell in startListeningWithTrigger
    fixture->speech_recognizer_aggregator->startListeningWithTrigger();
    g_assert(state.wakeup == WakeupDetectState::WAKEUP_DETECTING);
    g_assert(state.asr == ASRState::IDLE);

    test_model_file = { TINKERBELL_NET, TINKERBELL_SEARCH };
    fixture->speech_recognizer_aggregator->setWakeupModel(test_model_file);
    checkWakeupInfo();
    g_assert(state.wakeup != WakeupDetectState::WAKEUP_DETECTING);

    // change wakeup model to aria in startListening
    fixture->speech_recognizer_aggregator->startListening();
    g_assert(state.asr == ASRState::LISTENING);

    test_model_file = { ARIA_NET, ARIA_SEARCH };
    fixture->speech_recognizer_aggregator->setWakeupModel(test_model_file);
    checkWakeupInfo();
    g_assert(state.asr == ASRState::IDLE);
}

static void test_speech_recognizer_aggregator_no_wakeup_handler(TestFixture* fixture, gconstpointer ignored)
{
    auto speech_recognizer_aggregator = std::make_shared<SpeechRecognizerAggregator>();
    speech_recognizer_aggregator->addListener(fixture->listener.get());
    speech_recognizer_aggregator->setASRHandler(fixture->asr_handler.get());

    const auto& state = fixture->listener->getState();

    g_assert(state.wakeup == WakeupDetectState::WAKEUP_IDLE);
    g_assert(state.asr == ASRState::IDLE);

    speech_recognizer_aggregator->startListening();
    g_assert(state.asr == ASRState::LISTENING);

    speech_recognizer_aggregator->stopListening();
    g_assert(state.asr == ASRState::IDLE);

    // only activate when both wakeup/asr handler set
    speech_recognizer_aggregator->startListeningWithTrigger();
    g_assert(state.wakeup != WakeupDetectState::WAKEUP_DETECTING);
    g_assert(state.asr == ASRState::IDLE);

    g_assert(!speech_recognizer_aggregator->setWakeupModel({ ARIA_NET, ARIA_SEARCH }));
}

static void test_speech_recognizer_aggregator_set_user_wakeup_handler(TestFixture* fixture, gconstpointer ignored)
{
    const auto& wakeup_handler = fixture->speech_recognizer_aggregator->getWakeupHandler();
    auto user_wakeup_handler = std::make_shared<FakeWakeupHandler>();

    g_assert(wakeup_handler == fixture->wakeup_handler);
    g_assert(fixture->wakeup_handler.use_count() == 2);
    g_assert(wakeup_handler.use_count() == 2);
    g_assert(user_wakeup_handler.use_count() == 1);

    // set user wakeup handler
    fixture->speech_recognizer_aggregator->setWakeupHandler(user_wakeup_handler);
    g_assert(wakeup_handler == user_wakeup_handler);
    g_assert(fixture->wakeup_handler.use_count() == 1); // previous wakeup handler's reference count decreased.
    g_assert(user_wakeup_handler.use_count() == 2); // user wakeup handler's reference count increased.
    g_assert(wakeup_handler.use_count() == 2);
}

static void test_speech_recognizer_aggregator_notify_result(TestFixture* fixture, gconstpointer ignored)
{
    const auto& state = fixture->listener->getState();
    const auto& result = fixture->listener->getRecognitionResult();
    const auto& asr_result = fixture->asr_handler->getASRResult();
    const auto& source_dialog_id = fixture->asr_handler->getDialogId();
    const auto& target_dialog_id = fixture->listener->getDialogId();

    auto progressRecognition = [&](const RecognitionResult::Status& test_result_status, bool is_cancel = false) {
        fixture->asr_handler->setTestResult(test_result_status);
        fixture->speech_recognizer_aggregator->startListening();

        if (is_cancel) {
            fixture->speech_recognizer_aggregator->stopListening();
        } else {
            fixture->executor->startSpeech();
            fixture->executor->endSpeech();
            fixture->executor->onReceiveResponse();
        }

        g_assert(state.asr == ASRState::IDLE);
        g_assert(result.status == test_result_status);
        g_assert(source_dialog_id == target_dialog_id);
    };

    progressRecognition(RecognitionResult::Status::Partial);
    g_assert(asr_result.text == result.recognized_text);

    progressRecognition(RecognitionResult::Status::Complete);
    g_assert(asr_result.text == result.recognized_text);

    progressRecognition(RecognitionResult::Status::None);
    progressRecognition(RecognitionResult::Status::Cancel, true);

    progressRecognition(RecognitionResult::Status::Error);
    g_assert(asr_result.error == result.error);
    g_assert(!result.listen_timeout_fail_beep);
}

static void test_speech_recognizer_aggregator_reset(TestFixture* fixture, gconstpointer ignored)
{
    const auto& state = fixture->listener->getState();

    fixture->speech_recognizer_aggregator->startListeningWithTrigger();
    g_assert(state.wakeup == WakeupDetectState::WAKEUP_DETECTING);
    g_assert(state.asr == ASRState::IDLE);

    fixture->speech_recognizer_aggregator->reset();
    g_assert(state.wakeup != WakeupDetectState::WAKEUP_DETECTING);
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

    G_TEST_ADD_FUNC("/clientkit/SpeechRecognizerAggregator/setWakeupModel", test_speech_recognizer_aggregator_set_wakeup_model);
    G_TEST_ADD_FUNC("/clientkit/SpeechRecognizerAggregator/startListeningWithTrigger", test_speech_recognizer_aggregator_start_listening_with_trigger);
    G_TEST_ADD_FUNC("/clientkit/SpeechRecognizerAggregator/startListening", test_speech_recognizer_aggregator_start_listening);
    G_TEST_ADD_FUNC("/clientkit/SpeechRecognizerAggregator/stopListening", test_speech_recognizer_aggregator_stop_listening);
    G_TEST_ADD_FUNC("/clientkit/SpeechRecognizerAggregator/consecutiveStartListening", test_speech_recognizer_aggregator_consecutive_start_listening);
    G_TEST_ADD_FUNC("/clientkit/SpeechRecognizerAggregator/handleSeparateWakeup", test_speech_recognizer_aggregator_handle_separate_wakeup);
    G_TEST_ADD_FUNC("/clientkit/SpeechRecognizerAggregator/stopBeforeChangeWakeup", test_speech_recognizer_aggregator_stop_before_change_wakeup);
    G_TEST_ADD_FUNC("/clientkit/SpeechRecognizerAggregator/noWakeupHandler", test_speech_recognizer_aggregator_no_wakeup_handler);
    G_TEST_ADD_FUNC("/clientkit/SpeechRecognizerAggregator/setUserWakeupHandler", test_speech_recognizer_aggregator_set_user_wakeup_handler);
    G_TEST_ADD_FUNC("/clientkit/SpeechRecognizerAggregator/notifyResult", test_speech_recognizer_aggregator_notify_result);
    G_TEST_ADD_FUNC("/clientkit/SpeechRecognizerAggregator/reset", test_speech_recognizer_aggregator_reset);

    return g_test_run();
}
