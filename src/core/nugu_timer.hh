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

#ifndef __NUGU_TIMER_HH__
#define __NUGU_TIMER_HH__

#include <clientkit/nugu_timer_interface.hh>

namespace NuguCore {

using namespace NuguClientKit;

class NUGUTimerPrivate;
class NUGUTimer : public INuguTimer {
public:
    NUGUTimer(bool singleShot = false);
    virtual ~NUGUTimer();

    void setInterval(unsigned int sec) override;
    unsigned int getInterval() override;
    void setSingleShot(bool singleShot) override;
    bool getSingleShot() override;

    void stop() override;
    void start(unsigned int sec = 0) override;
    void restart(unsigned int sec = 0) override;

    void setCallback(timer_callback cb) override;
    void notifyCallback();

private:
    static void timerCallback(void* userdata);

public:
    NUGUTimerPrivate* d;
};

} // NuguCore

#endif
