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

#include "audio_input_processor.hh"
#include "nugu_log.h"
#include <algorithm>

namespace NuguCore {

class SyncEvent {
public:
    std::function<void()> action;
    std::condition_variable cond;
    std::mutex mutex;
};

AudioInputProcessor::~AudioInputProcessor()
{
    g_atomic_int_set(&destroy, 1);

    stop();

    mutex.lock();
    cond.notify_all();
    mutex.unlock();

    if (thread.joinable())
        thread.join();

    nugu_recorder_free(rec);
}

void AudioInputProcessor::init(std::string name, std::string& sample, std::string& format, std::string& channel)
{
    if (is_initialized) {
        nugu_dbg("It's already initialized.");
        return;
    }

    NuguRecorderDriver* driver = nugu_recorder_driver_get_default();
    int ret;

    if (!driver) {
        nugu_error("there is no recorder driver");
        return;
    }

    setProperty(sample, format, channel);

    rec = nugu_recorder_new(name.append("_rec").c_str(), driver);
    nugu_recorder_set_property(rec, prop);

    thread_created = false;
    destroy = 0;
    thread = std::thread([this] { this->loop(); });

    ret = pthread_setname_np(thread.native_handle(), name.append("_loop").c_str());
    if (ret < 0)
        nugu_error("pthread_setname_np() failed");

    /* Wait until thread creation */
    std::unique_lock<std::mutex> lock(mutex);
    if (thread_created == false)
        cond.wait(lock);
    lock.unlock();

    is_initialized = true;
}

void AudioInputProcessor::start(std::function<void()> extra_func)
{
    if (is_running) {
        nugu_dbg("Thread is already running...");
        return;
    }

    if (extra_func)
        extra_func();

    is_running = true;

    mutex.lock();
    cond.notify_all();
    mutex.unlock();
}

void AudioInputProcessor::stop(void)
{
    if (!is_running) {
        nugu_dbg("Thread is not running...");
        return;
    }

    is_running = false;
}

static gboolean invoke_sync_event(gpointer userdata)
{
    SyncEvent* e = static_cast<SyncEvent*>(userdata);

    if (e->action)
        e->action();

    e->mutex.lock();
    e->cond.notify_all();
    e->mutex.unlock();

    return FALSE;
}

void AudioInputProcessor::sendSyncEvent(std::function<void()> action)
{
    g_return_if_fail(action != nullptr);

    SyncEvent* data = new SyncEvent;
    data->action = action;

    std::unique_lock<std::mutex> lock(data->mutex);

    g_main_context_invoke(NULL, invoke_sync_event, data);

    data->cond.wait(lock);
    lock.unlock();

    delete data;
}

void AudioInputProcessor::setProperty(std::string& sample, std::string& format, std::string& channel)
{
    std::transform(sample.begin(), sample.end(), sample.begin(), ::tolower);
    std::transform(format.begin(), format.end(), format.begin(), ::tolower);

    if (sample == "8k")
        prop.samplerate = AUDIO_SAMPLE_RATE_8K;
    else if (sample == "16k")
        prop.samplerate = AUDIO_SAMPLE_RATE_16K;
    else if (sample == "32k")
        prop.samplerate = AUDIO_SAMPLE_RATE_32K;
    else if (sample == "22k")
        prop.samplerate = AUDIO_SAMPLE_RATE_22K;
    else if (sample == "44k")
        prop.samplerate = AUDIO_SAMPLE_RATE_44K;
    else
        nugu_error("not support the sample rate => %s", sample.c_str());

    if (format == "s8")
        prop.format = AUDIO_FORMAT_S8;
    else if (format == "u8")
        prop.format = AUDIO_FORMAT_U8;
    else if (format == "s16le")
        prop.format = AUDIO_FORMAT_S16_LE;
    else if (format == "s16be")
        prop.format = AUDIO_FORMAT_S16_BE;
    else if (format == "u16le")
        prop.format = AUDIO_FORMAT_U16_LE;
    else if (format == "u16be")
        prop.format = AUDIO_FORMAT_U16_BE;
    else if (format == "s24le")
        prop.format = AUDIO_FORMAT_S24_LE;
    else if (format == "s24be")
        prop.format = AUDIO_FORMAT_S24_BE;
    else if (format == "u24le")
        prop.format = AUDIO_FORMAT_U24_LE;
    else if (format == "u24be")
        prop.format = AUDIO_FORMAT_U24_BE;
    else if (format == "s32le")
        prop.format = AUDIO_FORMAT_S32_LE;
    else if (format == "s32be")
        prop.format = AUDIO_FORMAT_S32_BE;
    else if (format == "u32le")
        prop.format = AUDIO_FORMAT_U32_LE;
    else if (format == "u32be")
        prop.format = AUDIO_FORMAT_U32_BE;
    else
        nugu_error("not support the format => %s", format.c_str());

    if (atoi(channel.c_str()))
        prop.channel = std::stoi(channel);
    else
        nugu_error("wrong channel parameter => %s", channel.c_str());
}

const NuguAudioProperty& AudioInputProcessor::getProperty()
{
    return prop;
}

} // NuguCore
