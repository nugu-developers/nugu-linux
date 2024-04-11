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
#include <pthread.h>

#include "base/nugu_log.h"

#include "audio_input_processor.hh"

namespace NuguCore {

class SyncEvent {
public:
    std::function<void()> action;
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

    if (recorder) {
        delete recorder;
        recorder = nullptr;
    }
}

void AudioInputProcessor::init(std::string&& name, std::string& sample, std::string& format, std::string& channel)
{
    if (is_initialized) {
        nugu_dbg("It's already initialized.");
        return;
    }

    recorder = AudioRecorderManager::getInstance()->requestRecorder(sample, format, channel);

    thread_created = false;
    destroy = 0;
    thread = std::thread([this] {
#ifdef HAVE_PTHREAD_SETNAME_NP
#ifdef __APPLE__
    int ret = pthread_setname_np("AudioInputProcessor");
    if (ret < 0)
        nugu_error("pthread_setname_np() failed");
#endif
#endif
        this->loop(); });

#ifdef HAVE_PTHREAD_SETNAME_NP
#ifndef __APPLE__
    int ret = pthread_setname_np(thread.native_handle(), name.append("_loop").c_str());
    if (ret < 0)
        nugu_error("pthread_setname_np() failed");
#endif
#endif

    /* Wait until thread creation */
    std::unique_lock<std::mutex> lock(mutex);
    cond.wait(lock, [this] {
        nugu_dbg("worker initialized: %d", thread_created);
        return thread_created;
    });

    is_initialized = true;
}

bool AudioInputProcessor::start(const std::function<void()>& extra_func)
{
    std::lock_guard<std::mutex> lock(mutex);

    if (is_running) {
        nugu_dbg("Thread is already running...");
        return true;
    }

    if (recorder->isMute()) {
        nugu_warn("recorder is muted...");
        return false;
    }

    if (extra_func)
        extra_func();

    is_running = true;
    is_started = false;
    cond.notify_all();

    return true;
}

void AudioInputProcessor::stop()
{
    std::lock_guard<std::mutex> lock(mutex);

    if (!is_running) {
        nugu_dbg("Thread is not running...");
        return;
    }

    is_running = false;

    if (recorder)
        recorder->stop();
}

static gboolean invoke_event(gpointer userdata)
{
    SyncEvent* e = static_cast<SyncEvent*>(userdata);

    if (e->action)
        e->action();

    delete e;
    return FALSE;
}

void AudioInputProcessor::sendEvent(const std::function<void()>& action)
{
    g_return_if_fail(action != nullptr);

    SyncEvent* data = new SyncEvent;
    data->action = action;

    g_idle_add(invoke_event, (void*)data);
}

} // NuguCore
