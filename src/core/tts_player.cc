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
#include <glib.h>
#include <list>
#include <map>

#include "base/nugu_decoder.h"
#include "base/nugu_log.h"
#include "base/nugu_pcm.h"
#include "base/nugu_prof.h"

#include "nugu_timer.hh"
#include "tts_player.hh"

namespace NuguCore {

#define POSITION_POLLING_TIMEOUT_500MS 500
#define MEDIA_MUTE_SETTING -10
#define MEDIA_PLAYER_NAME "MediaPlayerAttachment"
#define MEDIA_SAMPLERATE_22K 22050

static int uniq_id = 0;

class TTSPlayerPrivate {
public:
    explicit TTSPlayerPrivate(int vol)
        : state(MediaPlayerState::IDLE)
        , player_name("")
        , player(nullptr)
        , decoder(nullptr)
        , volume(vol)
        , position(0)
        , duration(0)
        , seek_time(0)
        , seek_size(0)
        , total_size(0)
        , timeout(0)
        , mute(false)
        , count(0)
    {
    }
    ~TTSPlayerPrivate() {}

public:
    static std::map<TTSPlayer*, TTSPlayerPrivate*> mp_map;
    std::list<IMediaPlayerListener*> listeners;
    MediaPlayerState state;
    std::string player_name;
    NuguPcm* player;
    NuguDecoder* decoder;
    int volume;
    int position;
    int duration;
    int seek_time;
    size_t seek_size;
    size_t total_size;
    gint timeout;
    bool mute;
    int count;
};
std::map<TTSPlayer*, TTSPlayerPrivate*> TTSPlayerPrivate::mp_map;

TTSPlayer::TTSPlayer(int volume)
    : d(new TTSPlayerPrivate(volume))
{
    d->player_name = MEDIA_PLAYER_NAME + std::to_string(uniq_id++);
    nugu_dbg("player's name: %s", d->player_name.c_str());

    d->player = nugu_pcm_new(d->player_name.c_str(), nugu_pcm_driver_get_default(),
        (NuguAudioProperty) { NUGU_AUDIO_SAMPLE_RATE_22K, NUGU_AUDIO_FORMAT_S16_LE, 1 });
    if (!d->player) {
        nugu_error("couldn't create tts player");
        return;
    }
    nugu_pcm_add(d->player);

    d->decoder = nugu_decoder_new(nugu_decoder_driver_find_bytype(NUGU_DECODER_TYPE_OPUS), d->player);

    NuguMediaStatusCallback scb = [](enum nugu_media_status status, void* userdata) {
        TTSPlayer* tplayer = static_cast<TTSPlayer*>(userdata);

        switch (status) {
        case NUGU_MEDIA_STATUS_STOPPED:
            tplayer->setState(MediaPlayerState::STOPPED);
            break;
        case NUGU_MEDIA_STATUS_READY:
            tplayer->setState(MediaPlayerState::READY);
            break;
        case NUGU_MEDIA_STATUS_PLAYING:
            tplayer->setState(MediaPlayerState::PLAYING);
            break;
        case NUGU_MEDIA_STATUS_PAUSED:
            tplayer->setState(MediaPlayerState::PAUSED);
            break;
        default:
            tplayer->setState(MediaPlayerState::STOPPED);
            break;
        }
    };
    nugu_pcm_set_status_callback(d->player, scb, this);

    NuguMediaEventCallback ecb = [](enum nugu_media_event event, void* userdata) {
        TTSPlayer* tplayer = static_cast<TTSPlayer*>(userdata);
        TTSPlayerPrivate* d = TTSPlayerPrivate::mp_map[tplayer];

        if (!d)
            return;

        NuguPcm* player = nugu_pcm_find(d->player_name.c_str());

        if (!player)
            return;

        switch (event) {
        case NUGU_MEDIA_EVENT_MEDIA_SOURCE_CHANGED:
            break;
        case NUGU_MEDIA_EVENT_MEDIA_INVALID:
            break;
        case NUGU_MEDIA_EVENT_MEDIA_LOAD_FAILED:
            for (auto l : d->listeners)
                l->mediaEventReport(MediaPlayerEvent::LOADING_MEDIA_FAILED);
            break;
        case NUGU_MEDIA_EVENT_MEDIA_LOADED:
            for (auto l : d->listeners)
                l->mediaEventReport(MediaPlayerEvent::LOADING_MEDIA_SUCCESS);
            break;
        case NUGU_MEDIA_EVENT_END_OF_STREAM:
            d->state = MediaPlayerState::STOPPED;
            for (auto l : d->listeners)
                l->mediaEventReport(MediaPlayerEvent::PLAYING_MEDIA_FINISHED);
            break;
        default:
            break;
        }
    };
    nugu_pcm_set_event_callback(d->player, ecb, this);

    GSourceFunc tfunc = [](gpointer userdata) {
        TTSPlayer* tplayer = static_cast<TTSPlayer*>(userdata);
        TTSPlayerPrivate* d = TTSPlayerPrivate::mp_map[tplayer];

        if (!d)
            return (gboolean)TRUE;

        if (!tplayer->isPlaying() || !d->player)
            return (gboolean)TRUE;

        int position = nugu_pcm_get_position(d->player);
        if (position >= 0)
            tplayer->setPosition(position);

        return (gboolean)TRUE;
    };
    d->timeout = g_timeout_add(POSITION_POLLING_TIMEOUT_500MS, tfunc, this);
    d->mp_map[this] = d;
}

TTSPlayer::~TTSPlayer()
{
    d->listeners.clear();

    if (d->player) {
        nugu_pcm_set_status_callback(d->player, nullptr, nullptr);
        nugu_pcm_set_event_callback(d->player, nullptr, nullptr);
        nugu_pcm_stop(d->player);
        nugu_pcm_remove(d->player);
        nugu_pcm_free(d->player);
        d->player = nullptr;
    }

    if (d->decoder) {
        nugu_decoder_free(d->decoder);
        d->decoder = nullptr;
    }

    if (d->timeout) {
        g_source_remove(d->timeout);
        d->timeout = 0;
    }

    d->mp_map.erase(this);
    delete d;
}

void TTSPlayer::addListener(IMediaPlayerListener* listener)
{
    auto iter = std::find(d->listeners.begin(), d->listeners.end(), listener);
    if (iter == d->listeners.end())
        d->listeners.emplace_back(listener);
}

void TTSPlayer::removeListener(IMediaPlayerListener* listener)
{
    d->listeners.remove(listener);
}

bool TTSPlayer::write_audio(const char* data, int size)
{
    unsigned char* dbuf;
    size_t dsize;
    bool ret = true;

    dbuf = (unsigned char*)nugu_decoder_decode(d->decoder, (const void*)data, size, &dsize);

    if (d->seek_size) {
        if (d->seek_size >= dsize) {
            nugu_dbg("seek pcm audio => %d/%d", dsize, d->seek_size);
            d->seek_size -= dsize;
            dsize = 0;
        } else {
            nugu_dbg("seek pcm audio => %d/%d", dsize, d->seek_size);
            dsize -= d->seek_size;
            d->seek_size = 0;
        }
    }

    if (d->count == 0)
        nugu_prof_mark(NUGU_PROF_TYPE_TTS_FIRST_DECODING);

    d->count++;

    if (dsize)
        ret = (nugu_pcm_push_data(d->player, (const char*)dbuf, dsize, false) >= 0);

    free(dbuf);

    return ret;
}

void TTSPlayer::write_done()
{
    nugu_pcm_push_data_done(d->player);
    setDuration(nugu_pcm_get_duration(d->player));
}

bool TTSPlayer::setSource(const std::string& url)
{
    nugu_dbg("request to setSource mediaplayer.attachment");

    clearContent();
    // set prepare state until media probed
    setState(MediaPlayerState::PREPARE);

    return true;
}

bool TTSPlayer::play()
{
    nugu_dbg("request to play mediaplayer.attachment");
    StopB4Start();
    return (nugu_pcm_start(d->player) >= 0);
}

bool TTSPlayer::stop()
{
    nugu_dbg("request to stop mediaplayer.attachment");
    clearContent();
    return (nugu_pcm_stop(d->player) >= 0);
}

bool TTSPlayer::pause()
{
    nugu_dbg("request to pause mediaplayer.attachment");

    return (nugu_pcm_pause(d->player) >= 0);
}

bool TTSPlayer::resume()
{
    nugu_dbg("request to resume mediaplayer.attachment");

    return (nugu_pcm_resume(d->player) >= 0);
}

bool TTSPlayer::seek(int sec)
{
    nugu_dbg("request to seek(%d) mediaplayer.attachment", sec);

    if (sec < 0)
        sec = 0;

    if (d->position) {
        nugu_warn("not support to seek during playing attachment");
        return false;
    }

    d->seek_size = sec * MEDIA_SAMPLERATE_22K;
    d->seek_time = sec;
    return true;
}

int TTSPlayer::position()
{
    return d->position;
}

bool TTSPlayer::setPosition(int position)
{
    if (position < 0) {
        nugu_error("the value is wrong value: %d", position);
        return false;
    }

    position += d->seek_time;
    if (d->position == position) {
        // nugu_warn("already notify positionChanged");
        return true;
    }
    d->position = position;
    for (auto l : d->listeners)
        l->positionChanged(d->position);
    return true;
}

int TTSPlayer::duration()
{
    return d->duration;
}

bool TTSPlayer::setDuration(int duration)
{
    if (duration < 0) {
        nugu_error("the value is wrong value: %d", duration);
        return false;
    }

    if (d->duration == duration) {
        // nugu_warn("already notify durationChanged");
        return true;
    }
    d->duration = duration;
    for (auto l : d->listeners)
        l->durationChanged(d->duration);
    return true;
}

int TTSPlayer::volume()
{
    return d->volume;
}

bool TTSPlayer::setVolume(int volume)
{
    int prev_volume = d->volume;

    if (volume < 0) {
        nugu_error("the value is wrong value: %d", volume);
        return false;
    }

    if (volume == MEDIA_MUTE_SETTING) {
        nugu_pcm_set_volume(d->player, NUGU_SET_VOLUME_MIN);
        return true;
    }

    if (volume > NUGU_SET_VOLUME_MAX)
        d->volume = NUGU_SET_VOLUME_MAX;
    else if (volume < NUGU_SET_VOLUME_MIN)
        d->volume = NUGU_SET_VOLUME_MIN;
    else
        d->volume = volume;

    if (!d->mute)
        nugu_pcm_set_volume(d->player, d->volume);

    if (prev_volume != d->volume)
        for (auto l : d->listeners)
            l->volumeChanged(d->volume);

    return true;
}

bool TTSPlayer::mute()
{
    return d->mute;
}

bool TTSPlayer::setMute(bool mute)
{
    if (d->mute == mute)
        return true;

    d->mute = mute;
    for (auto l : d->listeners)
        l->muteChanged(d->mute);

    if (d->mute)
        nugu_pcm_set_volume(d->player, MEDIA_MUTE_SETTING);
    else
        nugu_pcm_set_volume(d->player, d->volume);

    return true;
}

bool TTSPlayer::loop()
{
    nugu_warn("not support to loop in mediaplayer.attachment");
    return false;
}

void TTSPlayer::setLoop(bool loop)
{
    nugu_warn("not support to setLoop in mediaplayer.attachment");
}

bool TTSPlayer::isPlaying()
{
    return d->state == MediaPlayerState::PLAYING;
}

MediaPlayerState TTSPlayer::state()
{
    return d->state;
}

bool TTSPlayer::setState(MediaPlayerState state)
{
    nugu_dbg("media.attachment state changed(%s -> %s)", stateString(d->state).c_str(), stateString(state).c_str());

    if (state == d->state) {
        return true;
    }

    d->state = state;
    for (auto l : d->listeners)
        l->mediaStateChanged(d->state);
    return true;
}

std::string TTSPlayer::stateString(MediaPlayerState state)
{
    std::string str;

    switch (state) {
    case MediaPlayerState::IDLE:
        str = "IDLE";
        break;
    case MediaPlayerState::PREPARE:
        str = "PREPARE";
        break;
    case MediaPlayerState::READY:
        str = "READY";
        break;
    case MediaPlayerState::PLAYING:
        str = "PLAYING";
        break;
    case MediaPlayerState::PAUSED:
        str = "PAUSED";
        break;
    case MediaPlayerState::STOPPED:
        str = "STOPPED";
        break;
    }

    return str;
}

std::string TTSPlayer::url()
{
    return "";
}

void TTSPlayer::clearContent()
{
    d->position = d->duration = 0;
    d->seek_time = d->seek_size = 0;
    d->count = 0;
}

void TTSPlayer::StopB4Start()
{
    // stop without reporting state changed
    MediaPlayerState prev_state = state();

    d->state = MediaPlayerState::STOPPED;
    nugu_pcm_stop(d->player);
    d->state = prev_state;
}

} // NuguCore
