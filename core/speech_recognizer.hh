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

#ifndef __NUGU_SPEECH_RECOGNIZER_H__
#define __NUGU_SPEECH_RECOGNIZER_H__

#include "audio_input_processor.hh"

#define EPD_MODEL_FILE "nugu_model_epd.raw"

namespace NuguCore {

enum class ListeningState {
    READY,
    LISTENING,
    SPEECH_START,
    SPEECH_END,
    TIMEOUT,
    FAILED,
    DONE
};

class ISpeechRecognizerListener {
public:
    virtual ~ISpeechRecognizerListener() = default;

    /* The callback is invoked in the main context. */
    virtual void onListeningState(ListeningState state) = 0;

    /* The callback is invoked in the thread context. */
    virtual void onRecordData(unsigned char* buf, int length) = 0;
};

class SpeechRecognizer : public AudioInputProcessor {
public:
    SpeechRecognizer();
    virtual ~SpeechRecognizer() = default;

    void setListener(ISpeechRecognizerListener* listener);
    void startListening(void);
    void stopListening(void);
    void startRecorder(void);
    void stopRecorder(void);

private:
    void loop(void) override;
    void sendSyncListeningEvent(ListeningState state);

    const unsigned int OUT_DATA_SIZE = 1024 * 9;
    int epd_ret = -1;
    ISpeechRecognizerListener* listener = nullptr;
};

} // NuguCore

#endif
