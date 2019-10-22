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

#include <unistd.h>

#include <interface/nugu_configuration.hh>
#include <keyword_detector.h>

#include "nugu_config.h"
#include "nugu_log.h"
#include "wakeup_detector.hh"

namespace NuguCore {

using namespace NuguClientKit;

class SyncEvent {
public:
    WakeupState wakeup_state;
    IWakeupDetectorListener* listener;
    std::condition_variable cond;
    std::mutex mutex;
};

static gboolean invoke_sync_event(gpointer userdata)
{
    SyncEvent* e = static_cast<SyncEvent*>(userdata);

    e->listener->onWakeupState(e->wakeup_state);
    e->mutex.lock();
    e->cond.notify_all();
    e->mutex.unlock();

    return FALSE;
}

WakeupDetector::WakeupDetector()
{
    NuguRecorderDriver* driver = nugu_recorder_driver_get_default();
    int ret;

    if (!driver) {
        nugu_error("there is no recorder driver");
        return;
    }

    rec_kwd = nugu_recorder_new("rec_kwd", driver);
    nugu_recorder_set_property(rec_kwd, (NuguAudioProperty){ AUDIO_SAMPLE_RATE_16K, AUDIO_FORMAT_S16_LE, 1 });

    thread_created = false;
    kwd_destroy = 0;
    kwd_thread = std::thread([this] { this->loopWakeup(); });

    ret = pthread_setname_np(kwd_thread.native_handle(), "kwdloop");
    if (ret < 0)
        nugu_error("pthread_setname_np() failed");

    /* Wait until kwd_thread creation */
    std::unique_lock<std::mutex> lock(kwd_mutex);
    if (thread_created == false)
        kwd_cond.wait(lock);
    lock.unlock();
}

WakeupDetector::~WakeupDetector()
{
    g_atomic_int_set(&kwd_destroy, 1);
    stopWakeup();
    kwd_mutex.lock();
    kwd_cond.notify_all();
    kwd_mutex.unlock();

    if (kwd_thread.joinable())
        kwd_thread.join();

    nugu_recorder_free(rec_kwd);
}

void WakeupDetector::sendSyncWakeupEvent(WakeupState state)
{
    g_return_if_fail(listener != nullptr);

    SyncEvent* data = new SyncEvent;

    data->wakeup_state = state;
    data->listener = listener;

    std::unique_lock<std::mutex> lock(data->mutex);

    g_main_context_invoke(NULL, invoke_sync_event, data);

    data->cond.wait(lock);
    lock.unlock();

    delete data;
}

void WakeupDetector::setListener(IWakeupDetectorListener* listener)
{
    this->listener = listener;
}

void WakeupDetector::loopWakeup(void)
{
    int pcm_size;
    int length;
    std::string model_net_file;
    std::string model_search_file;
    std::string model_path;

    nugu_dbg("Wakeup Thread: started");

    model_path = nugu_config_get(NuguConfig::Key::MODEL_PATH.c_str());
    if (model_path.size()) {
        nugu_dbg("model path: %s", model_path.c_str());

        model_net_file = model_path + "/" + WAKEUP_NET_MODEL_FILE;
        nugu_dbg("kwd model net-file: %s", model_net_file.c_str());

        model_search_file = model_path + "/" + WAKEUP_SEARCH_MODEL_FILE;
        nugu_dbg("kwd model search-file: %s", model_search_file.c_str());
    }

    kwd_mutex.lock();
    thread_created = true;
    kwd_cond.notify_all();
    kwd_mutex.unlock();

    while (g_atomic_int_get(&kwd_destroy) == 0) {
        std::unique_lock<std::mutex> lock(kwd_mutex);
        kwd_cond.wait(lock);
        lock.unlock();

        if (kwd_is_running == false)
            continue;

        nugu_dbg("Wakeup Thread: kwd_is_running=%d", kwd_is_running);

        if (kwd_initialize(model_net_file.c_str(), model_search_file.c_str()) < 0) {
            nugu_error("kwd_initialize() failed");
            break;
        }

        if (nugu_recorder_start(rec_kwd) < 0) {
            nugu_error("nugu_recorder_start() failed.");
            break;
        }

        nugu_recorder_get_frame_size(rec_kwd, &pcm_size, &length);

        while (kwd_is_running) {
            char pcm_buf[pcm_size];

            if (nugu_recorder_is_recording(rec_kwd) == 0) {
                nugu_dbg("Wakeup Thread: not recording state");
                usleep(10 * 1000);
                continue;
            }

            if (nugu_recorder_get_frame_timeout(rec_kwd, pcm_buf, &pcm_size, 0) < 0) {
                nugu_error("nugu_recorder_get_frame_timeout() failed");
                sendSyncWakeupEvent(WakeupState::FAIL);
                break;
            }

            if (pcm_size == 0) {
                nugu_error("pcm_size result is 0");
                sendSyncWakeupEvent(WakeupState::FAIL);
                break;
            }

            if (kwd_put_audio((short*)pcm_buf, pcm_size) == 1) {
                sendSyncWakeupEvent(WakeupState::DETECTED);
                kwd_is_running = false;
                break;
            }
        }

        nugu_recorder_stop(rec_kwd);
        kwd_deinitialize();

        if (g_atomic_int_get(&kwd_destroy) == 0)
            sendSyncWakeupEvent(WakeupState::DONE);
    }

    nugu_dbg("Wakeup Thread: exited");
}

void WakeupDetector::startWakeup(void)
{
    if (kwd_is_running) {
        nugu_dbg("Wakeup Thread is already running...");
        return;
    }

    if (listener)
        listener->onWakeupState(WakeupState::DETECTING);

    kwd_is_running = true;

    kwd_mutex.lock();
    kwd_cond.notify_all();
    kwd_mutex.unlock();
}

void WakeupDetector::stopWakeup(void)
{
    if (!kwd_is_running) {
        nugu_dbg("Wakeup Thread is not running...");
        return;
    }

    kwd_is_running = false;
}

} // NuguCore
