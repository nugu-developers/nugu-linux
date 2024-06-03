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

#ifdef ENABLE_VENDOR_LIBRARY
#include <endpoint_detector.h>
#endif

#include "base/nugu_encoder.h"
#include "base/nugu_log.h"
#include "base/nugu_prof.h"

#include "nugu_timer.hh"
#include "speech_recognizer.hh"

#ifdef _WIN32
#include <windows.h>
#endif

#define EPD_MODEL_FILE "skt_epd_model.raw"
#define OUT_DATA_SIZE (1024 * 9)

namespace NuguCore {

// define default property values
static const char* ASR_EPD_SAMPLERATE = "16k";
static const char* ASR_EPD_FORMAT = "s16le";
static const char* ASR_EPD_CHANNEL = "1";
static const char* MODEL_PATH = "./";
static const int ASR_EPD_TIMEOUT_SEC = 7;
static const int ASR_EPD_MAX_DURATION_SEC = 10;
static const int ASR_EPD_PAUSE_LENGTH_MSEC = 700;

#ifdef ENABLE_VENDOR_LIBRARY
static EpdParam get_epd_param(const std::string& samplerate, int timeout, int max_duration, int pause_length)
{
    EpdParam epd_param;

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

    epd_param.input_type = EPD_DATA_TYPE_LINEAR_PCM16;
    epd_param.output_type = EPD_DATA_TYPE_LINEAR_PCM16;
    epd_param.max_speech_duration_secs = max_duration;
    epd_param.time_out_secs = timeout;
    epd_param.pause_length_msecs = pause_length;

    return epd_param;
}
#endif

#if defined(ENABLE_VENDOR_LIBRARY) || defined(ENABLE_VOICE_STREAMING)
static NuguAudioProperty get_audio_property(const std::string& samplerate)
{
    NuguAudioProperty prop;

    if (samplerate == "8k")
        prop.samplerate = NUGU_AUDIO_SAMPLE_RATE_8K;
    else if (samplerate == "22k")
        prop.samplerate = NUGU_AUDIO_SAMPLE_RATE_22K;
    else if (samplerate == "32k")
        prop.samplerate = NUGU_AUDIO_SAMPLE_RATE_32K;
    else if (samplerate == "44k")
        prop.samplerate = NUGU_AUDIO_SAMPLE_RATE_44K;
    else
        prop.samplerate = NUGU_AUDIO_SAMPLE_RATE_16K;

    prop.format = NUGU_AUDIO_FORMAT_S16_LE;
    prop.channel = 1;

    return prop;
}

static bool create_encoder(NuguAudioProperty prop, NuguEncoder** encoder)
{
    NuguEncoderDriver* encoder_driver;

    encoder_driver = nugu_encoder_driver_find_bytype(NUGU_ENCODER_TYPE_OPUS);
    if (!encoder_driver) {
        encoder_driver = nugu_encoder_driver_find_bytype(NUGU_ENCODER_TYPE_SPEEX);
        if (!encoder_driver) {
            nugu_error("can't find encoder driver");
            return false;
        }
    }

    *encoder = nugu_encoder_new(encoder_driver, prop);
    if (!*encoder) {
            nugu_error("can't create encoder");
            return false;
    }

    return true;
}
#endif

SpeechRecognizer::SpeechRecognizer(Attribute&& attribute)
    : epd_ret(-1)
    , listener(nullptr)
    , codec("SPEEX")
    , mime_type("application/octet-stream")
{
    initialize(std::move(attribute));
}

SpeechRecognizer::~SpeechRecognizer()
{
    listener = nullptr;
}

void SpeechRecognizer::sendListeningEvent(ListeningState state, const std::string& id)
{
    if (!listener)
        return;

    mutex.lock();
    AudioInputProcessor::sendEvent([=] {
        if (listener)
            listener->onListeningState(state, id);
    });
    mutex.unlock();
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

#ifdef ENABLE_VENDOR_LIBRARY
void SpeechRecognizer::loop()
{
    NUGUTimer* timer = new NUGUTimer(true);
    char* epd_buf = NULL;
    int epd_buf_alloc_size = 0;
    EpdParam epd_param;
    int pcm_size;
    int length;
    int prev_epd_ret = 0;
    bool is_epd_end = false;
    std::string model_file;
    NuguEncoder* encoder = NULL;
    NuguAudioProperty prop;

    std::string samplerate = recorder->getSamplerate();
    prop = get_audio_property(samplerate);
    epd_param = get_epd_param(samplerate, epd_timeout, epd_max_duration, epd_pause_length);

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

    std::string id = listening_id;

    while (g_atomic_int_get(&destroy) == 0) {
        char *pcm_buf = nullptr;
        std::unique_lock<std::mutex> lock(mutex);
        cond.wait(lock);
        lock.unlock();

        if (is_running == false)
            continue;

        is_started = true;
        id = listening_id;

        epd_param.max_speech_duration_secs = epd_max_duration;
        epd_param.time_out_secs = epd_timeout;
        epd_param.pause_length_msecs = epd_pause_length;

        nugu_dbg("epd_max_duration: %d", epd_max_duration);
        nugu_dbg("epd_timeout: %d", epd_timeout);
        nugu_dbg("epd_pause_length: %ld", epd_pause_length);
        nugu_dbg("Listening Thread: asr_is_running=%d", is_running);
        sendListeningEvent(ListeningState::READY, id);

        if (create_encoder(prop, &encoder) == false
            || epd_client_start(model_file.c_str(), epd_param) < 0
            || !recorder->start()) {
            nugu_error("create encoder or epd_client_start or record start failed");

            is_running = false;
            epd_client_release();
            if (encoder) {
                nugu_encoder_free(encoder);
                encoder = NULL;
            }

            sendListeningEvent(ListeningState::FAILED, id);
            sendListeningEvent(ListeningState::DONE, id);
            continue;
        }

        codec = nugu_encoder_get_codec(encoder);
        mime_type = nugu_encoder_get_mime_type(encoder);

        sendListeningEvent(ListeningState::LISTENING, id);

        prev_epd_ret = 0;
        is_epd_end = false;
        pcm_size = recorder->getAudioFrameSize();
        pcm_buf = (char *)calloc(1, pcm_size);
        if (!pcm_buf) {
            nugu_error_nomem();
            continue;
        }

        while (is_running) {
            if (!recorder->isRecording()) {
#ifndef _WIN32
                struct timespec ts;

                ts.tv_sec = 0;
                ts.tv_nsec = 10 * 1000 * 1000; /* 10ms */

                nugu_dbg("Listening Thread: not recording state");
                nanosleep(&ts, NULL);
#else
                nugu_dbg("Listening Thread: not recording state (10ms wait)");
                Sleep(10);
#endif
                continue;
            }

            if (recorder->isMute()) {
                nugu_error("nugu recorder is mute");
                sendListeningEvent(ListeningState::FAILED, id);
                break;
            }

            if (!recorder->getAudioFrame(pcm_buf, &pcm_size, 0)) {
                nugu_error("nugu_recorder_get_frame_timeout() failed");
                sendListeningEvent(ListeningState::FAILED, id);
                break;
            }

            if (pcm_size == 0) {
                nugu_error("pcm_size result is 0");
                if (is_running)
                    sendListeningEvent(ListeningState::FAILED, id);
                break;
            }

            epd_ret = epd_client_run(pcm_buf, pcm_size);
            if (epd_ret < 0 || epd_ret > EPD_END_CHECK) {
                nugu_error("epd_client_run() failed: %d", epd_ret);
                sendListeningEvent(ListeningState::FAILED, id);
                break;
            }

            length = epd_client_get_output_size();
            if (length > 0) {
                if (length > epd_buf_alloc_size) {
                    /* Resize buffer */
                    if (epd_buf)
                        free(epd_buf);

                    epd_buf = (char *)malloc(length);
                    epd_buf_alloc_size = length;
                }

                epd_client_get_output(epd_buf, length);
            }

            if (epd_ret == EPD_END_DETECTED
                || epd_ret == EPD_TIMEOUT
                || epd_ret == EPD_MAXSPEECH)
                is_epd_end = true;

            if (is_epd_end || length > 0) {
                unsigned char* encoded;
                size_t encoded_size = 0;

                encoded = (unsigned char*)nugu_encoder_encode(encoder, is_epd_end, epd_buf,
                    length, &encoded_size);
                if (encoded) {
                    /* Invoke the onRecordData callback in thread context */
                    if (listener && (is_epd_end || encoded_size != 0))
                        listener->onRecordData(encoded, encoded_size, is_epd_end);

                    free(encoded);
                } else {
                    nugu_error("nugu_encoder_encode(is_epd_end=%d, length=%d) failed",
                        is_epd_end, length);
                }
            }

            switch (epd_ret) {
            case EPD_END_DETECTED:
                nugu_prof_mark(NUGU_PROF_TYPE_ASR_END_POINT_DETECTED);
                sendListeningEvent(ListeningState::SPEECH_END, id);
                break;
            case EPD_END_DETECTING:
            case EPD_START_DETECTED:
                if (prev_epd_ret == EPD_START_DETECTING) {
                    nugu_prof_mark(NUGU_PROF_TYPE_ASR_RECOGNIZING_STARTED);
                    sendListeningEvent(ListeningState::SPEECH_START, id);
                }
                break;
            case EPD_TIMEOUT:
                nugu_prof_mark(NUGU_PROF_TYPE_ASR_TIMEOUT);
                sendListeningEvent(ListeningState::TIMEOUT, id);
                break;
            case EPD_MAXSPEECH:
                sendListeningEvent(ListeningState::SPEECH_END, id);
                break;
            }

            if (is_epd_end)
                break;

            prev_epd_ret = epd_ret;
        }

        if (g_atomic_int_get(&destroy) == 0)
            sendListeningEvent(ListeningState::DONE, id);

        if (pcm_buf) {
            free(pcm_buf);
            pcm_buf = nullptr;
        }

        is_running = false;
        recorder->stop();
        epd_client_release();
        nugu_encoder_free(encoder);
        encoder = NULL;

        if (!is_started) {
            is_running = false;
            timer->setCallback([&]() {
                nugu_dbg("request to start audio input");
                startListening(listening_id);
            });
            timer->start();
        }
    }

    delete timer;
    if (epd_buf)
        free(epd_buf);

    nugu_dbg("Listening Thread: exited");
}
#elif defined(ENABLE_VOICE_STREAMING)
void SpeechRecognizer::loop()
{
    NUGUTimer* timer = new NUGUTimer(true);
    NuguEncoder* encoder = NULL;
    NuguAudioProperty prop;

    int pcm_size;
    bool is_first = true;

    std::string samplerate = recorder->getSamplerate();
    prop = get_audio_property(samplerate);

    nugu_dbg("Listening Thread: started");

    mutex.lock();
    thread_created = true;
    cond.notify_all();
    mutex.unlock();

    std::string id = listening_id;

    while (g_atomic_int_get(&destroy) == 0) {
        char *pcm_buf = nullptr;
        std::unique_lock<std::mutex> lock(mutex);
        cond.wait(lock);
        lock.unlock();

        if (is_running == false)
            continue;

        is_started = true;
        id = listening_id;

        sendListeningEvent(ListeningState::READY, id);
        if (create_encoder(prop, &encoder) == false
            || !recorder->start()) {
            nugu_error("create encoder or record start failed");

            is_running = false;
            if (encoder) {
                nugu_encoder_free(encoder);
                encoder = NULL;
            }
            recorder->stop();

            sendListeningEvent(ListeningState::FAILED, id);
            sendListeningEvent(ListeningState::DONE, id);
            continue;
        }

        codec = nugu_encoder_get_codec(encoder);
        mime_type = nugu_encoder_get_mime_type(encoder);

        sendListeningEvent(ListeningState::LISTENING, id);

        pcm_size = recorder->getAudioFrameSize();
        pcm_buf = (char *)calloc(1, pcm_size);
        if (!pcm_buf) {
            nugu_error_nomem();
            continue;
        }

        while (is_running) {
            if (!recorder->isRecording()) {
#ifndef _WIN32
                struct timespec ts;

                ts.tv_sec = 0;
                ts.tv_nsec = 10 * 1000 * 1000; /* 10ms */

                nugu_dbg("Listening Thread: not recording state");
                nanosleep(&ts, NULL);
#else
                nugu_dbg("Listening Thread: not recording state (10ms wait)");
                Sleep(10);
#endif
                continue;
            }

            if (recorder->isMute()) {
                nugu_error("nugu recorder is mute");
                sendListeningEvent(ListeningState::FAILED, id);
                break;
            }

            if (!recorder->getAudioFrame(pcm_buf, &pcm_size, 0)) {
                nugu_error("nugu_recorder_get_frame_timeout() failed");
                sendListeningEvent(ListeningState::FAILED, id);
                break;
            }

            if (pcm_size == 0) {
                nugu_error("pcm_size result is 0");
                if (is_running)
                    sendListeningEvent(ListeningState::FAILED, id);
                break;
            }

            if (pcm_size > 0) {
                std::lock_guard<std::mutex> lock(mtx);
                unsigned char* encoded;
                size_t encoded_size = 0;

                encoded = (unsigned char*)nugu_encoder_encode(encoder, is_end, pcm_buf,
                    pcm_size, &encoded_size);
                if (encoded) {
                    /* Invoke the onRecordData callback in thread context */
                    if (listener && (is_end || encoded_size != 0))
                        listener->onRecordData(encoded, encoded_size, is_end);

                    free(encoded);
                } else {
                    nugu_error("nugu_encoder_encode(is_end=%d, pcm_size=%d) failed",
                        is_end, pcm_size);
                }

                if (is_first) {
                    nugu_prof_mark(NUGU_PROF_TYPE_ASR_RECOGNIZING_STARTED);
                    sendListeningEvent(ListeningState::SPEECH_START, id);
                    is_first = false;
                }

                if (is_end) {
                    nugu_prof_mark(NUGU_PROF_TYPE_ASR_END_POINT_DETECTED);
                    sendListeningEvent(ListeningState::SPEECH_END, id);
                    break;
                }
            }
        }

        if (g_atomic_int_get(&destroy) == 0)
            sendListeningEvent(ListeningState::DONE, id);

        if (pcm_buf) {
            free(pcm_buf);
            pcm_buf = nullptr;
        }

        is_running = false;
        is_first = true;
        is_end = false;
        recorder->stop();
        nugu_encoder_free(encoder);
        encoder = NULL;

        if (!is_started) {
            is_running = false;
            timer->setCallback([&]() {
                nugu_dbg("request to start audio input");
                startListening(listening_id);
            });
            timer->start();
        }
    }

    delete timer;

    nugu_dbg("Listening Thread: exited");
}
#else
void SpeechRecognizer::loop()
{
    mutex.lock();
    thread_created = true;
    cond.notify_all();
    mutex.unlock();

    std::string id = listening_id;

    while (g_atomic_int_get(&destroy) == 0) {
        std::unique_lock<std::mutex> lock(mutex);
        cond.wait(lock);
        lock.unlock();

        if (is_running == false)
            continue;

        id = listening_id;

        nugu_dbg("Listening Thread: asr_is_running=%d", is_running);
        sendListeningEvent(ListeningState::READY, id);

        nugu_error("nugu_epd is not supported");
        sendListeningEvent(ListeningState::FAILED, id);
        is_running = false;
    }
}
#endif

bool SpeechRecognizer::startListening(const std::string& id)
{
    listening_id = id;
    nugu_prof_mark(NUGU_PROF_TYPE_ASR_LISTENING_STARTED);
    return AudioInputProcessor::start([&] {
        epd_ret = 0;
    });
}

void SpeechRecognizer::stopListening()
{
    AudioInputProcessor::stop();
}

void SpeechRecognizer::finishListening()
{
#ifdef ENABLE_VOICE_STREAMING
    std::lock_guard<std::mutex> lock(mtx);

    is_end = true;
#endif
}

void SpeechRecognizer::setEpdAttribute(const EpdAttribute& attribute)
{
    epd_timeout = attribute.epd_timeout;
    epd_max_duration = attribute.epd_max_duration;
    epd_pause_length = attribute.epd_pause_length;
}

EpdAttribute SpeechRecognizer::getEpdAttribute()
{
    EpdAttribute attribute;
    attribute.epd_timeout = epd_timeout;
    attribute.epd_max_duration = epd_max_duration;
    attribute.epd_pause_length = epd_pause_length;
    return attribute;
}

bool SpeechRecognizer::isMute()
{
    if (!recorder)
        return false;

    return recorder->isMute();
}

std::string SpeechRecognizer::getCodec()
{
    return codec;
}

std::string SpeechRecognizer::getMimeType()
{
    return mime_type;
}

} // NuguCore
