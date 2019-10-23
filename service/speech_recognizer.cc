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

#include <string.h>
#include <unistd.h>

#include "endpoint_detector.h"
#include "interface/nugu_configuration.hh"

#include "nugu_config.h"
#include "nugu_log.h"
#include "speech_recognizer.hh"

namespace NuguCore {

using namespace NuguClientKit;

SpeechRecognizer::SpeechRecognizer()
{
    AudioInputProcessor::init("asr");
}

void SpeechRecognizer::sendSyncListeningEvent(ListeningState state)
{
    AudioInputProcessor::sendSyncEvent([&] {
        if (listener)
            listener->onListeningState(state);
    });
}

void SpeechRecognizer::setListener(ISpeechRecognizerListener* listener)
{
    this->listener = listener;
}

void SpeechRecognizer::loop(void)
{
    unsigned char epd_buf[OUT_DATA_SIZE];
    EpdParam epd_param;
    int pcm_size;
    int length;
    int prev_epd_ret = 0;
    bool is_epd_end = false;
    std::string model_file;
    std::string model_path;

    epd_param.sample_rate = 16000;
    epd_param.max_speech_duration = 10;
    epd_param.time_out = 10;
    epd_param.pause_length = 700;

    nugu_dbg("Listening Thread: started");

    model_path = nugu_config_get(NuguConfig::Key::MODEL_PATH.c_str());
    if (model_path.size()) {
        nugu_dbg("model path: %s", model_path.c_str());

        model_file = model_path + "/" + EPD_MODEL_FILE;
        nugu_dbg("epd model file: %s", model_file.c_str());
    }

    mutex.lock();
    thread_created = true;
    cond.notify_all();
    mutex.unlock();

    while (g_atomic_int_get(&destroy) == 0) {
        std::unique_lock<std::mutex> lock(mutex);
        cond.wait(lock);
        lock.unlock();

        if (is_running == false)
            continue;

        nugu_dbg("Listening Thread: asr_is_running=%d", is_running);

        if (epd_client_start(model_file.c_str(), epd_param) < 0) {
            nugu_error("epd_client_start() failed");
            break;
        };

        if (nugu_recorder_start(rec) < 0) {
            nugu_error("nugu_recorder_start() failed.");
            break;
        }

        nugu_recorder_get_frame_size(rec, &pcm_size, &length);

        sendSyncListeningEvent(ListeningState::LISTENING);

        prev_epd_ret = 0;
        is_epd_end = false;

        while (is_running) {
            char pcm_buf[pcm_size];

            if (nugu_recorder_is_recording(rec) == 0) {
                nugu_dbg("Listening Thread: not recording state");
                usleep(10 * 1000);
                continue;
            }

            if (nugu_recorder_get_frame_timeout(rec, pcm_buf, &pcm_size, 0) < 0) {
                nugu_error("nugu_recorder_get_frame_timeout() failed");
                sendSyncListeningEvent(ListeningState::FAILED);
                break;
            }

            if (pcm_size == 0) {
                nugu_error("pcm_size result is 0");
                sendSyncListeningEvent(ListeningState::FAILED);
                break;
            }

            length = OUT_DATA_SIZE;
            epd_ret = epd_client_run((char*)epd_buf, &length, (short*)pcm_buf, pcm_size);

            if (epd_ret < 0 || epd_ret > EPD_END_CHECK) {
                nugu_error("epd_client_run() failed: %d", epd_ret);
                sendSyncListeningEvent(ListeningState::FAILED);
                break;
            }

            if (length > 0) {
                /* Invoke the onRecordData callback in thread context */
                if (listener)
                    listener->onRecordData((unsigned char*)epd_buf, length);
            }

            switch (epd_ret) {
            case EPD_END_DETECTED:
                sendSyncListeningEvent(ListeningState::SPEECH_END);
                is_epd_end = true;
                break;
            case EPD_END_DETECTING:
            case EPD_START_DETECTED:
                if (prev_epd_ret == EPD_START_DETECTING) {
                    sendSyncListeningEvent(ListeningState::SPEECH_START);
                }
                break;
            case EPD_TIMEOUT:
                sendSyncListeningEvent(ListeningState::TIMEOUT);
                is_epd_end = true;
                break;

            case EPD_MAXSPEECH:
                sendSyncListeningEvent(ListeningState::SPEECH_END);
                is_epd_end = true;
                break;
            }

            if (is_epd_end)
                break;

            prev_epd_ret = epd_ret;
        }

        nugu_recorder_stop(rec);
        epd_client_release();

        if (g_atomic_int_get(&destroy) == 0)
            sendSyncListeningEvent(ListeningState::DONE);
    }

    nugu_dbg("Listening Thread: exited");
}

void SpeechRecognizer::startListening(void)
{
    AudioInputProcessor::start([&] {
        if (listener)
            listener->onListeningState(ListeningState::READY);

        epd_ret = 0;
    });
}

void SpeechRecognizer::stopListening(void)
{
    AudioInputProcessor::stop();
}

void SpeechRecognizer::startRecorder(void)
{
    if (is_running && nugu_recorder_is_recording(rec) == 1)
        nugu_recorder_start(rec);
}

void SpeechRecognizer::stopRecorder(void)
{
    if (is_running && nugu_recorder_is_recording(rec) == 1)
        nugu_recorder_stop(rec);
}

} // NuguCore
