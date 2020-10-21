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

#include "nugu_timer.hh"
#include <base/nugu_timer.h>

namespace NuguCore {

#define DEFAULT_TIMEOUT_MSEC 1

class NUGUTimerPrivate {
public:
    NUGUTimerPrivate()
        : timer(nullptr)
        , cb(nullptr)
    {
    }
    ~NUGUTimerPrivate()
    {
    }

public:
    NuguTimer* timer;
    NUGUTimer::timer_callback cb;
};

NUGUTimer::NUGUTimer(bool singleShot)
    : d(new NUGUTimerPrivate())
{
    d->timer = nugu_timer_new(DEFAULT_TIMEOUT_MSEC);
    nugu_timer_set_singleshot(d->timer, singleShot);
    nugu_timer_set_callback(d->timer, timerCallback, this);
}

NUGUTimer::~NUGUTimer()
{
    if (d->timer) {
        nugu_timer_stop(d->timer);
        nugu_timer_delete(d->timer);
        d->timer = nullptr;
    }
    delete d;
}

void NUGUTimer::setInterval(unsigned int msec)
{
    nugu_timer_set_interval(d->timer, msec);
}

unsigned int NUGUTimer::getInterval()
{
    if (!d->timer)
        return -1;

    return nugu_timer_get_interval(d->timer);
}

void NUGUTimer::setSingleShot(bool singleShot)
{
    nugu_timer_set_singleshot(d->timer, singleShot);
}

bool NUGUTimer::getSingleShot()
{
    return nugu_timer_get_singleshot(d->timer);
}

void NUGUTimer::stop()
{
    nugu_timer_stop(d->timer);
}

void NUGUTimer::start(unsigned int msec)
{
    if (msec)
        setInterval(msec);

    nugu_timer_start(d->timer);
}

void NUGUTimer::restart(unsigned int msec)
{
    stop();
    start(msec);
}

void NUGUTimer::setCallback(timer_callback cb)
{
    if (!d->timer)
        return;

    d->cb = std::move(cb);
}

void NUGUTimer::notifyCallback()
{
    if (!d->timer)
        return;

    if (d->cb)
        d->cb();
}

void NUGUTimer::timerCallback(void* userdata)
{
    NUGUTimer* ntimer = static_cast<NUGUTimer*>(userdata);
    ntimer->notifyCallback();
}
}