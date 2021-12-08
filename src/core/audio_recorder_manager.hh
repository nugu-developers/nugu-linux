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

#ifndef __NUGU_AUDIO_RECORDER_MANAGER_H__
#define __NUGU_AUDIO_RECORDER_MANAGER_H__

#include <list>
#include <map>
#include <mutex>
#include <string>

#include "base/nugu_recorder.h"

#include "audio_recorder_interface.hh"

namespace NuguCore {

class AudioRecorder final : public IAudioRecorder {
public:
    AudioRecorder(std::string& samplerate, std::string& format, std::string& channel);
    virtual ~AudioRecorder();

    std::string& getFormat() override;
    std::string& getChannel() override;
    std::string& getSamplerate() override;

    bool start() override;
    bool stop() override;
    bool isRecording() override;
    bool isMute() override;

    int getAudioFrameSize() override;
    int getAudioFrameCount() override;
    bool getAudioFrame(char* data, int* size, int timeout = 0) override;

private:
    std::string samplerate;
    std::string format;
    std::string channel;
};

class AudioRecorderManager {
public:
    AudioRecorderManager();
    virtual ~AudioRecorderManager();

    static AudioRecorderManager* getInstance();
    static void destroyInstance();

    IAudioRecorder* requestRecorder(std::string& samplerate, std::string& format, std::string& channel);

    bool start(IAudioRecorder* recorder);
    bool stop(IAudioRecorder* recorder);
    bool isRecording(IAudioRecorder* recorder);
    bool isMute();
    bool setMute(bool mute);

    int getAudioFrameSize(IAudioRecorder* recorder);
    int getAudioFrameCount(IAudioRecorder* recorder);
    bool getAudioFrame(IAudioRecorder* recorder, char* data, int* size, int timeout = 0);

private:
    NuguAudioProperty convertNuguAudioProperty(std::string& sample, std::string& format, std::string& channel);
    std::string extractRecorderKey(const std::string& sample, const std::string& format, const std::string& channel);
    NuguRecorder* extractNuguRecorder(IAudioRecorder* recorder);

private:
    static AudioRecorderManager* instance;
    std::map<std::string, NuguRecorder*> nugu_recorders;
    std::map<NuguRecorder*, std::list<IAudioRecorder*>> recorders;
    std::mutex mutex;
    bool muted;
};

} // NuguCore

#endif /* __NUGU_AUDIO_RECORDER_MANAGER_H__ */
