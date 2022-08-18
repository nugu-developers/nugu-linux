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
#include <list>
#include <map>

#include <glib.h>

#include "base/nugu_log.h"
#include "base/nugu_player.h"

#include "media_player.hh"
#include "nugu_timer.hh"

namespace NuguCore {

#define POSITION_POLLING_TIMEOUT_500MS 500
#define MEDIA_MUTE_SETTING -10
#define MEDIA_PLAYER_NAME "MediaPlayer"

static int uniq_id = 0;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

class MediaPlayerPrivate {
public:
    explicit MediaPlayerPrivate(int vol)
        : state(MediaPlayerState::IDLE)
        , playurl("")
        , player_name("")
        , player(NULL)
        , pos_timer(nullptr)
        , position(0)
        , duration(0)
        , volume(vol)
        , mute(false)
        , loop(false)
    {
        pthread_mutex_lock(&mtx);

        player_name = MEDIA_PLAYER_NAME + std::to_string(uniq_id++);
        nugu_dbg("player's name: %s", player_name.c_str());

        pthread_mutex_unlock(&mtx);
    }

    virtual ~MediaPlayerPrivate() = default;

public:
    static std::map<MediaPlayer*, MediaPlayerPrivate*> mp_map;
    std::list<IMediaPlayerListener*> listeners;
    MediaPlayerState state;
    std::string playurl;
    std::string player_name;
    NuguPlayer* player;
    NUGUTimer* pos_timer;
    int position;
    int duration;
    int volume;
    bool mute;
    bool loop;
};
std::map<MediaPlayer*, MediaPlayerPrivate*> MediaPlayerPrivate::mp_map;

MediaPlayer::MediaPlayer(int volume)
    : d(new MediaPlayerPrivate(volume))
{
    d->player = nugu_player_new(d->player_name.c_str(), nugu_player_driver_get_default());
    if (!d->player) {
        nugu_error("couldn't create media player");
        return;
    }
    nugu_player_add(d->player);

    NuguMediaStatusCallback scb = [](enum nugu_media_status status, void* userdata) {
        MediaPlayer* mplayer = static_cast<MediaPlayer*>(userdata);

        switch (status) {
        case NUGU_MEDIA_STATUS_STOPPED:
            mplayer->setState(MediaPlayerState::STOPPED);
            break;
        case NUGU_MEDIA_STATUS_READY:
            mplayer->setState(MediaPlayerState::READY);
            break;
        case NUGU_MEDIA_STATUS_PLAYING:
            mplayer->setState(MediaPlayerState::PLAYING);
            break;
        case NUGU_MEDIA_STATUS_PAUSED:
            mplayer->setState(MediaPlayerState::PAUSED);
            break;
        default:
            mplayer->setState(MediaPlayerState::STOPPED);
            break;
        }
    };
    nugu_player_set_status_callback(d->player, scb, this);

    NuguMediaEventCallback ecb = [](enum nugu_media_event event, void* userdata) {
        MediaPlayer* mplayer = static_cast<MediaPlayer*>(userdata);
        MediaPlayerPrivate* d = MediaPlayerPrivate::mp_map[mplayer];

        if (!d)
            return;

        NuguPlayer* player = nugu_player_find(d->player_name.c_str());

        if (!player)
            return;

        switch (event) {
        case NUGU_MEDIA_EVENT_MEDIA_SOURCE_CHANGED:
            for (auto l : d->listeners)
                l->mediaChanged(mplayer->url());
            break;
        case NUGU_MEDIA_EVENT_MEDIA_INVALID:
            for (auto l : d->listeners)
                l->mediaEventReport(MediaPlayerEvent::INVALID_MEDIA_URL);
            break;
        case NUGU_MEDIA_EVENT_MEDIA_LOAD_FAILED:
            for (auto l : d->listeners)
                l->mediaEventReport(MediaPlayerEvent::LOADING_MEDIA_FAILED);
            break;
        case NUGU_MEDIA_EVENT_MEDIA_LOADED: {
            int duration = nugu_player_get_duration(player);
            if (duration > 0)
                mplayer->setDuration(duration);

            for (auto l : d->listeners)
                l->mediaEventReport(MediaPlayerEvent::LOADING_MEDIA_SUCCESS);
        } break;
        case NUGU_MEDIA_EVENT_END_OF_STREAM:
            // ignore STOP event after EOF
            d->state = MediaPlayerState::STOPPED;

            mplayer->updatePosition();

            if (d->loop) {
                mplayer->play();
                break;
            }

            for (auto l : d->listeners)
                l->mediaEventReport(MediaPlayerEvent::PLAYING_MEDIA_FINISHED);
            break;
        case NUGU_MEDIA_EVENT_MEDIA_UNDERRUN:
            for (auto l : d->listeners)
                l->mediaEventReport(MediaPlayerEvent::PLAYING_MEDIA_UNDERRUN);
            break;

        default:
            break;
        }
    };
    nugu_player_set_event_callback(d->player, ecb, this);

    d->pos_timer = new NUGUTimer();
    d->pos_timer->setInterval(POSITION_POLLING_TIMEOUT_500MS);
    d->pos_timer->setCallback([&]() {
        if (!isPlaying())
            return;

        updatePosition();
    });
    d->mp_map[this] = d;

    // NOLINTNEXTLINE (clang-analyzer-optin.cplusplus.VirtualCall)
    setVolume(volume);
}

MediaPlayer::~MediaPlayer()
{
    d->listeners.clear();

    if (d->player) {
        nugu_player_remove(d->player);
        nugu_player_free(d->player);
        d->player = NULL;
    }

    if (d->pos_timer) {
        delete d->pos_timer;
        d->pos_timer = nullptr;
    }

    d->mp_map.erase(this);
    delete d;
}

void MediaPlayer::addListener(IMediaPlayerListener* listener)
{
    auto iter = std::find(d->listeners.begin(), d->listeners.end(), listener);
    if (iter == d->listeners.end())
        d->listeners.emplace_back(listener);
}

void MediaPlayer::removeListener(IMediaPlayerListener* listener)
{
    d->listeners.remove(listener);
}

void MediaPlayer::setAudioAttribute(NuguAudioAttribute attr)
{
    nugu_player_set_audio_attribute(d->player, attr);
}

bool MediaPlayer::setSource(const std::string& url)
{
    if (url.size() == 0) {
        nugu_error("must set the playurl to the media player");
        return false;
    }

    nugu_dbg("request to set source(%s) mediaplayer", url.c_str());

    // clear content's information
    d->position = d->duration = 0;
    d->loop = false;
    d->state = MediaPlayerState::STOPPED;

    d->playurl = url;
    if (nugu_player_set_source(d->player, d->playurl.c_str()) < 0)
        return false;

    // set prepare state until media probed
    setState(MediaPlayerState::PREPARE);

    return true;
}

bool MediaPlayer::play()
{
    if (d->playurl.size() == 0) {
        nugu_error("must set the playurl to the media player");
        return false;
    }

    nugu_dbg("request to play mediaplayer");

    d->pos_timer->restart();

    if (nugu_player_start(d->player) < 0)
        return false;

    return true;
}

bool MediaPlayer::stop()
{
    nugu_dbg("request to stop mediaplayer");

    d->pos_timer->stop();

    if (nugu_player_stop(d->player) < 0)
        return false;

    return true;
}

bool MediaPlayer::pause()
{
    nugu_dbg("request to pause mediaplayer");

    if (nugu_player_pause(d->player) < 0)
        return false;

    return true;
}

bool MediaPlayer::resume()
{
    nugu_dbg("request to resume mediaplayer");

    if (nugu_player_resume(d->player) < 0)
        return false;

    return true;
}

bool MediaPlayer::seek(int sec)
{
    nugu_dbg("request to seek(%d) mediaplayer", sec);

    if (sec < 0)
        sec = 0;

    if (nugu_player_seek(d->player, sec) < 0)
        return false;

    setPositionWithSeek(sec);

    return true;
}

int MediaPlayer::position()
{
    return d->position;
}

bool MediaPlayer::setPosition(int position)
{
    if (position < 0) {
        nugu_error("the value is wrong value: %d", position);
        return false;
    }
    if (d->position == position) {
        // nugu_warn("already notify positionChanged");
        return true;
    }
    d->position = position;
    for (auto l : d->listeners)
        l->positionChanged(d->position);
    return true;
}

void MediaPlayer::setPositionWithSeek(int position)
{
    // Seek position is set in advance only when a seek request is sent to the media player before playback.
    if (state() == MediaPlayerState::PREPARE)
        d->position = position;
}

int MediaPlayer::duration()
{
    return d->duration;
}

bool MediaPlayer::setDuration(int duration)
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

int MediaPlayer::volume()
{
    return d->volume;
}

bool MediaPlayer::setVolume(int volume)
{
    int prev_volume = d->volume;

    if (volume < 0) {
        nugu_error("the value is wrong value: %d", volume);
        return false;
    }

    if (volume == MEDIA_MUTE_SETTING) {
        nugu_player_set_volume(d->player, NUGU_SET_VOLUME_MIN);
        return true;
    }

    if (volume > NUGU_SET_VOLUME_MAX)
        d->volume = NUGU_SET_VOLUME_MAX;
    else if (volume < NUGU_SET_VOLUME_MIN)
        d->volume = NUGU_SET_VOLUME_MIN;
    else
        d->volume = volume;

    if (!d->mute)
        nugu_player_set_volume(d->player, d->volume);

    if (prev_volume != d->volume)
        for (auto l : d->listeners)
            l->volumeChanged(d->volume);

    return true;
}

bool MediaPlayer::mute()
{
    return d->mute;
}

bool MediaPlayer::setMute(bool mute)
{
    if (d->mute == mute)
        return true;

    d->mute = mute;
    for (auto l : d->listeners)
        l->muteChanged(d->mute);

    if (d->mute) {
        nugu_player_set_volume(d->player, MEDIA_MUTE_SETTING);
        // for flushing previous frame, because it's played as previous volume level in a short time.
        seek(position());
    } else {
        nugu_player_set_volume(d->player, d->volume);
    }

    return true;
}

bool MediaPlayer::loop()
{
    return d->loop;
}

void MediaPlayer::setLoop(bool loop)
{
    d->loop = loop;
}

bool MediaPlayer::isPlaying()
{
    return d->state == MediaPlayerState::PLAYING;
}

MediaPlayerState MediaPlayer::state()
{
    return d->state;
}

bool MediaPlayer::setState(MediaPlayerState state)
{
    nugu_dbg("media state changed(%s -> %s)", stateString(d->state).c_str(), stateString(state).c_str());

    if (state == d->state) {
        return true;
    }

    d->state = state;
    for (auto l : d->listeners)
        l->mediaStateChanged(d->state);
    return true;
}

std::string MediaPlayer::stateString(MediaPlayerState state)
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

std::string MediaPlayer::url()
{
    return d->playurl;
}

void MediaPlayer::updatePosition()
{
    int position = nugu_player_get_position(d->player);
    if (position >= 0)
        setPosition(position);
}

NuguPlayer* MediaPlayer::getNuguPlayer()
{
    return d->player;
}

} // NuguCore
