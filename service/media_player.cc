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

#include <glib.h>
#include <list>
#include <map>
#include <stdlib.h>
#include <string.h>

#include "media_player.hh"
#include "nugu_log.h"
#include "nugu_player.h"

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
        , position(0)
        , duration(0)
        , volume(vol)
        , mute(false)
    {
        pthread_mutex_lock(&mtx);

        player_name = MEDIA_PLAYER_NAME + std::to_string(uniq_id++);
        nugu_dbg("player's name: %s", player_name.c_str());

        pthread_mutex_unlock(&mtx);
    }

    virtual ~MediaPlayerPrivate() = default;

public:
    static std::map<MediaPlayer*, MediaPlayerPrivate*> mp_map;
    std::list<IMediaPlayerListener*> llist;
    MediaPlayerState state;
    std::string playurl;
    std::string player_name;
    NuguPlayer* player;
    int position;
    int duration;
    int volume;
    bool mute;
    gint timeout;
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

    mediaStatusCallback scb = [](enum nugu_media_status status,
                                  void* userdata) {
        MediaPlayer* mplayer = static_cast<MediaPlayer*>(userdata);

        switch (status) {
        case MEDIA_STATUS_STOPPED:
            mplayer->setState(MediaPlayerState::STOPPED);
            break;
        case MEDIA_STATUS_READY:
            mplayer->setState(MediaPlayerState::READY);
            break;
        case MEDIA_STATUS_PLAYING:
            mplayer->setState(MediaPlayerState::PLAYING);
            break;
        case MEDIA_STATUS_PAUSED:
            mplayer->setState(MediaPlayerState::PAUSED);
            break;
        default:
            mplayer->setState(MediaPlayerState::STOPPED);
            break;
        }
    };
    nugu_player_set_status_callback(d->player, scb, this);

    mediaEventCallback ecb = [](enum nugu_media_event event,
                                 void* userdata) {
        MediaPlayer* mplayer = static_cast<MediaPlayer*>(userdata);
        MediaPlayerPrivate* d = MediaPlayerPrivate::mp_map[mplayer];

        if (!d)
            return;

        NuguPlayer* player = nugu_player_find(d->player_name.c_str());

        if (!player)
            return;

        switch (event) {
        case MEDIA_EVENT_MEDIA_SOURCE_CHANGED:
            for (auto l : d->llist)
                l->mediaChanged(mplayer->url());
            break;
        case MEDIA_EVENT_MEDIA_INVALID:
            for (auto l : d->llist)
                l->mediaEventReport(MediaPlayerEvent::INVALID_MEDIA);
            break;
        case MEDIA_EVENT_MEDIA_LOAD_FAILED:
            for (auto l : d->llist)
                l->mediaEventReport(MediaPlayerEvent::LOADING_MEDIA);
            break;
        case MEDIA_EVENT_MEDIA_LOADED: {
            int duration = nugu_player_get_duration(player);
            if (duration > 0)
                mplayer->setDuration(duration);

            for (auto l : d->llist)
                l->mediaLoaded();
        } break;
        case MEDIA_EVENT_END_OF_STREAM:
            // ignore STOP event after EOF
            d->state = MediaPlayerState::STOPPED;

            for (auto l : d->llist)
                l->mediaFinished();
            break;
        default:
            break;
        }
    };
    nugu_player_set_event_callback(d->player, ecb, this);

    GSourceFunc tfunc = [](gpointer userdata) {
        MediaPlayer* mplayer = static_cast<MediaPlayer*>(userdata);
        MediaPlayerPrivate* d = MediaPlayerPrivate::mp_map[mplayer];

        if (!d)
            return (gboolean)TRUE;

        NuguPlayer* player = nugu_player_find(d->player_name.c_str());

        if (!mplayer->isPlaying() || !player)
            return (gboolean)TRUE;

        int position = nugu_player_get_position(player);
        if (position >= 0)
            mplayer->setPosition(position);

        return (gboolean)TRUE;
    };
    d->timeout = g_timeout_add(POSITION_POLLING_TIMEOUT_500MS, tfunc, this);
    d->mp_map[this] = d;
}

MediaPlayer::~MediaPlayer()
{
    d->llist.clear();

    if (d->player) {
        nugu_player_remove(d->player);
        nugu_player_free(d->player);
        d->player = NULL;
    }

    if (d->timeout) {
        g_source_remove(d->timeout);
        d->timeout = 0;
    }

    d->mp_map.erase(this);
    delete d;
}

void MediaPlayer::addListener(IMediaPlayerListener* listener)
{
    g_return_if_fail(listener != NULL);

    d->llist.push_back(listener);
}

void MediaPlayer::removeListener(IMediaPlayerListener* listener)
{
    g_return_if_fail(listener != NULL);

    d->llist.remove(listener);
}

bool MediaPlayer::setSource(std::string url)
{
    if (url.size() == 0) {
        nugu_error("must set the playurl to the media player");
        return false;
    }

    nugu_dbg("request to set source(%s) mediaplayer", url.c_str());

    // clear content's information
    d->position = d->duration = 0;
    d->state = MediaPlayerState::STOPPED;

    d->playurl = url;
    if (nugu_player_set_source(d->player, d->playurl.c_str()) < 0)
        return false;

    return true;
}

bool MediaPlayer::play()
{
    if (d->playurl.size() == 0) {
        nugu_error("must set the playurl to the media player");
        return false;
    }

    nugu_dbg("request to play mediaplayer");

    // set prepare state until media probed
    d->state = MediaPlayerState::PREPARE;

    if (nugu_player_start(d->player) < 0)
        return false;

    return true;
}

bool MediaPlayer::stop()
{
    nugu_dbg("request to stop mediaplayer");

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

    int next_position = sec + d->position;

    if (next_position < 0)
        sec = 0;
    else if (next_position > d->duration)
        sec = d->duration - d->position;

    if (nugu_player_seek(d->player, sec) < 0)
        return false;

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
    for (auto l : d->llist)
        l->positionChanged(d->position);
    return true;
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
    for (auto l : d->llist)
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
        for (auto l : d->llist)
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
    for (auto l : d->llist)
        l->muteChanged(d->mute);

    if (d->mute)
        nugu_player_set_volume(d->player, MEDIA_MUTE_SETTING);
    else
        nugu_player_set_volume(d->player, d->volume);

    return true;
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
    nugu_dbg("media state changed(%s -> %s)", stateString(d->state).c_str(),
        stateString(state).c_str());

    if (state == d->state) {
        return true;
    }

    d->state = state;
    for (auto l : d->llist)
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

} // NuguCore
