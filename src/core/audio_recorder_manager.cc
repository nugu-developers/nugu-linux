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

#include "base/nugu_log.h"
#include "base/nugu_recorder.h"

#include "audio_recorder_manager.hh"

namespace NuguCore {

AudioRecorder::AudioRecorder(std::string& samplerate, std::string& format, std::string& channel)
    : samplerate(samplerate)
    , format(format)
    , channel(channel)
{
}

AudioRecorder::~AudioRecorder()
{
}

std::string& AudioRecorder::getFormat()
{
    return format;
}

std::string& AudioRecorder::getChannel()
{
    return channel;
}

std::string& AudioRecorder::getSamplerate()
{
    return samplerate;
}

bool AudioRecorder::start()
{
    return AudioRecorderManager::getInstance()->start(this);
}

bool AudioRecorder::stop()
{
    return AudioRecorderManager::getInstance()->stop(this);
}

bool AudioRecorder::isRecording()
{
    return AudioRecorderManager::getInstance()->isRecording(this);
}

bool AudioRecorder::isMute()
{
    return AudioRecorderManager::getInstance()->isMute();
}

int AudioRecorder::getAudioFrameSize()
{
    return AudioRecorderManager::getInstance()->getAudioFrameSize(this);
}

int AudioRecorder::getAudioFrameCount()
{
    return AudioRecorderManager::getInstance()->getAudioFrameCount(this);
}

bool AudioRecorder::getAudioFrame(char* data, int* size, int timeout)
{
    return AudioRecorderManager::getInstance()->getAudioFrame(this, data, size, timeout);
}

AudioRecorderManager* AudioRecorderManager::instance = nullptr;
AudioRecorderManager::AudioRecorderManager()
    : muted(false)
{
}

AudioRecorderManager::~AudioRecorderManager()
{
    for (auto container : nugu_recorders)
        nugu_recorder_free(container.second);
    nugu_recorders.clear();
}

AudioRecorderManager* AudioRecorderManager::getInstance()
{
    if (!instance) {
        instance = new AudioRecorderManager();
    }
    return instance;
}

void AudioRecorderManager::destroyInstance()
{
    if (instance) {
        delete instance;
        instance = nullptr;
    }
}

IAudioRecorder* AudioRecorderManager::requestRecorder(std::string& samplerate, std::string& format, std::string& channel)
{
    NuguAudioProperty property = convertNuguAudioProperty(samplerate, format, channel);
    std::string key = extractRecorderKey(samplerate, format, channel);

    if (nugu_recorders.find(key) != nugu_recorders.end()) {
        nugu_dbg("already created nugu recorder - key:%s", key.c_str());
    } else {
        NuguRecorder* nugu_recorder = nugu_recorder_new(key.c_str(), nugu_recorder_driver_get_default());
        nugu_recorder_set_property(nugu_recorder, property);
        nugu_recorders[key] = nugu_recorder;
        nugu_dbg("create new nugu recorder - key:%s", key.c_str());
    }

    AudioRecorder* recorder = new AudioRecorder(samplerate, format, channel);
    return recorder;
}

bool AudioRecorderManager::start(IAudioRecorder* recorder)
{
    std::lock_guard<std::mutex> lock(mutex);

    NuguRecorder* nugu_recorder = extractNuguRecorder(recorder);
    if (!nugu_recorder)
        return false;

    std::list<IAudioRecorder*> recorder_list = recorders[nugu_recorder];
    auto iter = std::find(recorder_list.begin(), recorder_list.end(), recorder);
    if (iter == recorder_list.end()) {
        recorder_list.emplace_back(recorder);
        recorders[nugu_recorder] = recorder_list;
    }

    nugu_dbg("start recorder: %p", recorder);

    return (nugu_recorder_start(nugu_recorder) >= 0);
}

bool AudioRecorderManager::stop(IAudioRecorder* recorder)
{
    std::lock_guard<std::mutex> lock(mutex);

    NuguRecorder* nugu_recorder = extractNuguRecorder(recorder);
    if (!nugu_recorder)
        return false;

    std::list<IAudioRecorder*> recorder_list = recorders[nugu_recorder];
    auto iter = std::find(recorder_list.begin(), recorder_list.end(), recorder);
    if (iter != recorder_list.end()) {
        recorder_list.remove(recorder);
        recorders[nugu_recorder] = recorder_list;
    }

    nugu_dbg("stop recorder: %p, list's size: %d", recorder, recorder_list.size());

    if (!recorder_list.size())
        return (nugu_recorder_stop(nugu_recorder) >= 0);

    return true;
}

bool AudioRecorderManager::isRecording(IAudioRecorder* recorder)
{
    std::lock_guard<std::mutex> lock(mutex);

    NuguRecorder* nugu_recorder = extractNuguRecorder(recorder);
    if (!nugu_recorder)
        return false;

    return (nugu_recorder_is_recording(nugu_recorder) >= 0);
}

bool AudioRecorderManager::isMute()
{
    std::lock_guard<std::mutex> lock(mutex);

    return muted;
}

bool AudioRecorderManager::setMute(bool mute)
{
    std::lock_guard<std::mutex> lock(mutex);

    if (muted == mute)
        return true;

    nugu_dbg("request to set mute %d -> %d", muted, mute);

    muted = mute;

    if (muted == false) {
        for (auto container : nugu_recorders)
            nugu_recorder_clear(container.second);
    }

    return true;
}

int AudioRecorderManager::getAudioFrameSize(IAudioRecorder* recorder)
{
    std::lock_guard<std::mutex> lock(mutex);

    NuguRecorder* nugu_recorder = extractNuguRecorder(recorder);
    if (!nugu_recorder)
        return false;

    int size;
    int number;

    if (nugu_recorder_get_frame_size(nugu_recorder, &size, &number) != 0)
        nugu_error("recorder error - get frame size");

    return size;
}

int AudioRecorderManager::getAudioFrameCount(IAudioRecorder* recorder)
{
    std::lock_guard<std::mutex> lock(mutex);

    if (muted)
        return 0;

    NuguRecorder* nugu_recorder = extractNuguRecorder(recorder);
    if (!nugu_recorder)
        return false;

    return nugu_recorder_get_frame_count(nugu_recorder);
}

bool AudioRecorderManager::getAudioFrame(IAudioRecorder* recorder, char* data, int* size, int timeout)
{
    std::lock_guard<std::mutex> lock(mutex);

    if (muted)
        return false;

    NuguRecorder* nugu_recorder = extractNuguRecorder(recorder);
    if (!nugu_recorder)
        return false;

    return (nugu_recorder_get_frame_timeout(nugu_recorder, data, size, timeout) >= 0);
}

NuguAudioProperty AudioRecorderManager::convertNuguAudioProperty(std::string& sample, std::string& format, std::string& channel)
{
    NuguAudioProperty property;

    std::transform(sample.begin(), sample.end(), sample.begin(), ::tolower);
    std::transform(format.begin(), format.end(), format.begin(), ::tolower);

    if (sample == "8k")
        property.samplerate = NUGU_AUDIO_SAMPLE_RATE_8K;
    else if (sample == "16k")
        property.samplerate = NUGU_AUDIO_SAMPLE_RATE_16K;
    else if (sample == "32k")
        property.samplerate = NUGU_AUDIO_SAMPLE_RATE_32K;
    else if (sample == "22k")
        property.samplerate = NUGU_AUDIO_SAMPLE_RATE_22K;
    else if (sample == "44k")
        property.samplerate = NUGU_AUDIO_SAMPLE_RATE_44K;
    else {
        nugu_error("not support the sample rate => %s", sample.c_str());
        property.samplerate = NUGU_AUDIO_SAMPLE_RATE_16K;
    }

    if (format == "s8")
        property.format = NUGU_AUDIO_FORMAT_S8;
    else if (format == "u8")
        property.format = NUGU_AUDIO_FORMAT_U8;
    else if (format == "s16le")
        property.format = NUGU_AUDIO_FORMAT_S16_LE;
    else if (format == "s16be")
        property.format = NUGU_AUDIO_FORMAT_S16_BE;
    else if (format == "u16le")
        property.format = NUGU_AUDIO_FORMAT_U16_LE;
    else if (format == "u16be")
        property.format = NUGU_AUDIO_FORMAT_U16_BE;
    else if (format == "s24le")
        property.format = NUGU_AUDIO_FORMAT_S24_LE;
    else if (format == "s24be")
        property.format = NUGU_AUDIO_FORMAT_S24_BE;
    else if (format == "u24le")
        property.format = NUGU_AUDIO_FORMAT_U24_LE;
    else if (format == "u24be")
        property.format = NUGU_AUDIO_FORMAT_U24_BE;
    else if (format == "s32le")
        property.format = NUGU_AUDIO_FORMAT_S32_LE;
    else if (format == "s32be")
        property.format = NUGU_AUDIO_FORMAT_S32_BE;
    else if (format == "u32le")
        property.format = NUGU_AUDIO_FORMAT_U32_LE;
    else if (format == "u32be")
        property.format = NUGU_AUDIO_FORMAT_U32_BE;
    else {
        nugu_error("not support the format => %s", format.c_str());
        property.format = NUGU_AUDIO_FORMAT_S16_LE;
    }

    property.channel = std::stoi(channel);
    if (property.channel == 0) {
        nugu_error("wrong channel parameter => %s", channel.c_str());
        property.channel = 1;
    }

    return property;
}

std::string AudioRecorderManager::extractRecorderKey(std::string& sample, std::string& format, std::string& channel)
{
    return sample + "," + format + "," + channel;
}

NuguRecorder* AudioRecorderManager::extractNuguRecorder(IAudioRecorder* recorder)
{
    if (!recorder)
        return nullptr;

    std::string key = extractRecorderKey(recorder->getSamplerate(), recorder->getFormat(), recorder->getChannel());
    if (nugu_recorders.find(key) == nugu_recorders.end())
        return nullptr;

    return nugu_recorders[key];
}

} // NuguCore
