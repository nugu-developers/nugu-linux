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

WakeupDetector::WakeupDetector()
{
    AudioInputProcessor::init("kwd");
}

void WakeupDetector::sendSyncWakeupEvent(WakeupState state)
{
    AudioInputProcessor::sendSyncEvent([&] {
        if (listener)
            listener->onWakeupState(state);
    });
}

void WakeupDetector::setListener(IWakeupDetectorListener* listener)
{
    this->listener = listener;
}

void WakeupDetector::loop(void)
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

        nugu_dbg("Wakeup Thread: kwd_is_running=%d", is_running);

        if (kwd_initialize(model_net_file.c_str(), model_search_file.c_str()) < 0) {
            nugu_error("kwd_initialize() failed");
            break;
        }

        if (nugu_recorder_start(rec) < 0) {
            nugu_error("nugu_recorder_start() failed.");
            break;
        }

        nugu_recorder_get_frame_size(rec, &pcm_size, &length);

        while (is_running) {
            char pcm_buf[pcm_size];

            if (nugu_recorder_is_recording(rec) == 0) {
                nugu_dbg("Wakeup Thread: not recording state");
                usleep(10 * 1000);
                continue;
            }

            if (nugu_recorder_get_frame_timeout(rec, pcm_buf, &pcm_size, 0) < 0) {
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
                is_running = false;
                break;
            }
        }

        nugu_recorder_stop(rec);
        kwd_deinitialize();

        if (g_atomic_int_get(&destroy) == 0)
            sendSyncWakeupEvent(WakeupState::DONE);
    }

    nugu_dbg("Wakeup Thread: exited");
}

void WakeupDetector::startWakeup(void)
{
    AudioInputProcessor::start([&] {
        if (listener)
            listener->onWakeupState(WakeupState::DETECTING);
    });
}

void WakeupDetector::stopWakeup(void)
{
    AudioInputProcessor::stop();
}

} // NuguCore
