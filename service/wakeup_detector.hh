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

#ifndef __NUGU_WAKEUP_DETECTOR_H__
#define __NUGU_WAKEUP_DETECTOR_H__

#include <condition_variable>
#include <glib.h>
#include <mutex>
#include <thread>

#include <core/nugu_recorder.h>

#define WAKEUP_NET_MODEL_FILE "nugu_model_wakeup_net.raw"
#define WAKEUP_SEARCH_MODEL_FILE "nugu_model_wakeup_search.raw"

namespace NuguCore {

enum class WakeupState {
    FAIL,
    DETECTING,
    DETECTED,
    DONE
};

class IWakeupDetectorListener {
public:
    virtual ~IWakeupDetectorListener() = default;

    /* The callback is invoked in the main context. */
    virtual void onWakeupState(WakeupState state) = 0;
};

class WakeupDetector {
public:
    WakeupDetector();
    ~WakeupDetector();

    void setListener(IWakeupDetectorListener* listener);
    void startWakeup(void);
    void stopWakeup(void);

private:
    void loopWakeup(void);
    void sendSyncWakeupEvent(WakeupState state);

    IWakeupDetectorListener* listener = nullptr;
    std::thread kwd_thread;
    std::condition_variable kwd_cond;
    std::mutex kwd_mutex;
    gint kwd_destroy;
    bool kwd_is_running = false;
    NuguRecorder* rec_kwd = nullptr;
    bool thread_created;
};

} // NuguCore

#endif /* __NUGU_WAKEUP_DETECTOR_H__ */
