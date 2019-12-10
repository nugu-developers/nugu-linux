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

#ifndef __NUGU_AUDIO_INPUT_PROCESSOR_H__
#define __NUGU_AUDIO_INPUT_PROCESSOR_H__

#include <condition_variable>
#include <functional>
#include <glib.h>
#include <mutex>
#include <thread>
#include "audio_recorder_manager.hh"

#include <core/nugu_recorder.h>

namespace NuguCore {

class AudioInputProcessor {
public:
    AudioInputProcessor() = default;
    virtual ~AudioInputProcessor();

protected:
    void init(std::string name, std::string& sample, std::string& format, std::string& channel);
    void start(std::function<void()> extra_func = nullptr);
    void stop(void);
    void sendSyncEvent(std::function<void()> action = nullptr);

    virtual void loop(void) = 0;

    bool is_initialized = false;
    bool is_running = false;
    bool thread_created = false;
    gint destroy;
    std::thread thread;
    std::condition_variable cond;
    std::mutex mutex;
    IAudioRecorder* recorder = nullptr;
};

} // NuguCore

#endif /* __NUGU_AUDIO_INPUT_PROCESSOR_H__ */
