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

void AudioInputProcessor::init(std::string name)
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

    rec = nugu_recorder_new(name.append("_rec").c_str(), driver);
    nugu_recorder_set_property(rec, (NuguAudioProperty) { AUDIO_SAMPLE_RATE_16K, AUDIO_FORMAT_S16_LE, 1 });

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

} // NuguCore
