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

#ifndef __NUGU_SPEECH_RECOGNIZER_H__
#define __NUGU_SPEECH_RECOGNIZER_H__

#include "audio_input_processor.hh"
#include "clientkit/speech_recognizer_interface.hh"

namespace NuguCore {

using namespace NuguClientKit;

class SpeechRecognizer : public ISpeechRecognizer,
                         public AudioInputProcessor {
public:
    using Attribute = struct {
        std::string sample;
        std::string format;
        std::string channel;
        std::string model_path;
        int epd_timeout = 0;
        int epd_max_duration = 0;
        long epd_pause_length = 0;
    };

public:
    explicit SpeechRecognizer(Attribute&& attribute);
    virtual ~SpeechRecognizer();

    // implements ISpeechRecognizer
    void setListener(ISpeechRecognizerListener* listener) override;
    bool startListening(const std::string& id) override;
    void stopListening() override;
    void setEpdAttribute(const EpdAttribute& attribute) override;
    EpdAttribute getEpdAttribute() override;
    bool isMute() override;
    std::string getCodec() override;
    std::string getMimeType() override;

private:
    void initialize(Attribute&& attribute);
    void loop() override;
    void sendListeningEvent(ListeningState state, const std::string& id);

    int epd_ret;
    ISpeechRecognizerListener* listener;
    std::string codec;
    std::string mime_type;

    // attribute
    std::string model_path = "";
    int epd_timeout = 0;
    int epd_max_duration = 0;
    long epd_pause_length = 0;
};

} // NuguCore

#endif
