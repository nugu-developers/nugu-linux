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

#include <endpoint_detector.h>

#include "base/nugu_log.h"
#include "speech_recognizer.hh"

namespace NuguCore {

// define default property values
static const char* ASR_EPD_SAMPLERATE = "16k";
static const char* ASR_EPD_FORMAT = "s16le";
static const char* ASR_EPD_CHANNEL = "1";
static const char* MODEL_PATH = "./";
static const int ASR_EPD_TIMEOUT_SEC = 7;
static const int ASR_EPD_MAX_DURATION_SEC = 10;
static const int ASR_EPD_PAUSE_LENGTH_MSEC = 700;

SpeechRecognizer::SpeechRecognizer()
{
    initialize(Attribute{});
}

SpeechRecognizer::~SpeechRecognizer()
{
    listener = nullptr;
}

SpeechRecognizer::SpeechRecognizer(Attribute&& attribute)
{
    initialize(std::move(attribute));
}

void SpeechRecognizer::sendSyncListeningEvent(ListeningState state)
{
    if (!listener)
        return;

    AudioInputProcessor::sendSyncEvent([&] {
        if (listener)
            listener->onListeningState(state);
    });
}

void SpeechRecognizer::setListener(ISpeechRecognizerListener* listener)
{
    this->listener = listener;
}

void SpeechRecognizer::initialize(Attribute&& attribute)
{
    std::string sample = !attribute.sample.empty() ? attribute.sample : ASR_EPD_SAMPLERATE;
    std::string format = !attribute.format.empty() ? attribute.format : ASR_EPD_FORMAT;
    std::string channel = !attribute.channel.empty() ? attribute.channel : ASR_EPD_CHANNEL;

    model_path = !attribute.model_path.empty() ? attribute.model_path : MODEL_PATH;
    epd_timeout = attribute.epd_timeout > 0 ? attribute.epd_timeout : ASR_EPD_TIMEOUT_SEC;
    epd_max_duration = attribute.epd_max_duration > 0 ? attribute.epd_max_duration : ASR_EPD_MAX_DURATION_SEC;
    epd_pause_length = attribute.epd_pause_length > 0 ? attribute.epd_pause_length : ASR_EPD_PAUSE_LENGTH_MSEC;

    AudioInputProcessor::init("asr", sample, format, channel);
}

void SpeechRecognizer::loop()
{
    unsigned char epd_buf[OUT_DATA_SIZE];
    EpdParam epd_param;
    int pcm_size;
    int length;
    int prev_epd_ret = 0;
    bool is_epd_end = false;
    std::string model_file;

    std::string samplerate = recorder->getSamplerate();
    if (samplerate == "8k")
        epd_param.sample_rate = 8000;
    else if (samplerate == "22k")
        epd_param.sample_rate = 22050;
    else if (samplerate == "32k")
        epd_param.sample_rate = 32000;
    else if (samplerate == "44k")
        epd_param.sample_rate = 44100;
    else
        epd_param.sample_rate = 16000;

    epd_param.max_speech_duration = epd_max_duration;
    epd_param.time_out = epd_timeout;
    epd_param.pause_length = epd_pause_length;

    nugu_dbg("Listening Thread: started");

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

        if (!recorder->start()) {
            nugu_error("recorder->start() failed.");
            break;
        }

        sendSyncListeningEvent(ListeningState::LISTENING);

        prev_epd_ret = 0;
        is_epd_end = false;
        pcm_size = recorder->getAudioFrameSize();

        while (is_running) {
            char pcm_buf[pcm_size];

            if (!recorder->isRecording()) {
                struct timespec ts;

                ts.tv_sec = 0;
                ts.tv_nsec = 10 * 1000 * 1000; /* 10ms */

                nugu_dbg("Listening Thread: not recording state");
                nanosleep(&ts, NULL);
                continue;
            }

            if (recorder->isMute()) {
                nugu_error("nugu recorder is mute");
                sendSyncListeningEvent(ListeningState::FAILED);
                break;
            }

            if (!recorder->getAudioFrame(pcm_buf, &pcm_size, 0)) {
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

        recorder->stop();
        epd_client_release();
        is_running = false;

        if (g_atomic_int_get(&destroy) == 0)
            sendSyncListeningEvent(ListeningState::DONE);
    }

    nugu_dbg("Listening Thread: exited");
}

bool SpeechRecognizer::startListening()
{
    return AudioInputProcessor::start([&] {
        if (listener)
            listener->onListeningState(ListeningState::READY);

        epd_ret = 0;
    });
}

void SpeechRecognizer::stopListening()
{
    AudioInputProcessor::stop();
}

bool SpeechRecognizer::isMute()
{
    if (!recorder)
        return false;

    return recorder->isMute();
}

} // NuguCore
