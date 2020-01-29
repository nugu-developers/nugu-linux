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

#ifndef __NUGU_AUDIO_RECORDER_INTERFACE_H__
#define __NUGU_AUDIO_RECORDER_INTERFACE_H__

#include <string>

namespace NuguCore {

class IAudioRecorder {
public:
    virtual ~IAudioRecorder() = default;

    virtual std::string& getFormat() = 0;
    virtual std::string& getChannel() = 0;
    virtual std::string& getSamplerate() = 0;

    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool isRecording() = 0;
    virtual bool isMute() = 0;

    virtual int getAudioFrameSize() = 0;
    virtual int getAudioFrameCount() = 0;
    virtual bool getAudioFrame(char* data, int* size, int timeout = 0) = 0;
};

} // IAudioRecorder

#endif /* __NUGU_AUDIO_RECORDER_INTERFACE_H__ */
