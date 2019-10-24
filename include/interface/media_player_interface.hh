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

#ifndef __NUGU_MEDIA_PLAYER_INTERFACE_H__
#define __NUGU_MEDIA_PLAYER_INTERFACE_H__

#include <core/nugu_media.h>
#include <string>

namespace NuguInterface {

/**
 * @file media_player_interface.hh
 * @defgroup MediaPlayerInterface MediaPlayerInterface
 * @ingroup SDKNuguInterface
 * @brief Media Player Interface
 *
 * Playback media content and receive content information through the media player interface.
 *
 * @{
 */

/**
 * @brief MediaPlayerState
 */
enum class MediaPlayerState {
    IDLE, /**< Status idle in mediaplayer */
    PREPARE, /**< Status prepare to play in mediaplayer */
    READY, /**< Status ready to load playurl in mediaplayer */
    PLAYING, /**< Status playing in mediaplayer */
    PAUSED, /**< Status paused in mediaplayer */
    STOPPED /**< Status stopped in mediaplayer */
};

/**
 * @brief MediaPlayerEvent
 */
enum class MediaPlayerEvent {
    INVALID_MEDIA, /**< Failed to load media content */
    LOADING_MEDIA /**< Successful loading of media content */
};

/**
 * @brief mediaplayer listener interface
 * @see IMediaPlayer
 */
class IMediaPlayerListener {
public:
    /**
     * @brief Report changes in the mediaplayer state to the user.
     * @param[in] state mediaplayer state
     */
    virtual void mediaStateChanged(MediaPlayerState state) = 0;

    /**
     * @brief Report an event occurred during mediaplayer playback to the user.
     * @param[in] event mediaplayer playback event
     */
    virtual void mediaEventReport(MediaPlayerEvent event) = 0;

    /**
     * @brief Report the media player playing media content to the end.
     */
    virtual void mediaFinished() = 0;

    /**
     * @brief Report the media player loading media content successfully.
     */
    virtual void mediaLoaded() = 0;

    /**
     * @brief The media player reports that the media content has changed.
     * @param[in] url changed media content url
     */
    virtual void mediaChanged(std::string url) = 0;

    /**
     * @brief The media player reports the changed duration of the media content.
     * @param[in] duration duration of the media content
     */
    virtual void durationChanged(int duration) = 0;

    /**
     * @brief The media player reports the changed position of the media content.
     * @param[in] position position of the media content
     */
    virtual void positionChanged(int position) = 0;

    /**
     * @brief The media player reports the changed volume of the media content.
     * @param[in] volume volume of the media content
     */
    virtual void volumeChanged(int volume) = 0;

    /**
     * @brief The media player reports the changed mute of the media content.
     * @param[in] mute mute of the media content.
     */
    virtual void muteChanged(int mute) = 0;
};

/**
 * @brief mediaplayer interface
 * @see IMediaPlayerListener
 */
class IMediaPlayer {
public:
    virtual ~IMediaPlayer() = default;

    /**
     * @brief Add the Listener object
     * @param[in] listener listener object
     */
    virtual void addListener(IMediaPlayerListener* listener) = 0;

    /**
     * @brief Remove the Listener object
     * @param[in] listener listener object
     */
    virtual void removeListener(IMediaPlayerListener* listener) = 0;

    /**
     * @brief Sets the playurl of the media content to play in the media player.
     * @param[in] url media content url
     */
    virtual bool setSource(std::string url) = 0;

    /**
     * @brief Requset media player to play the media content.
     */
    virtual bool play() = 0;

    /**
     * @brief Requset media player to stop the media content.
     */
    virtual bool stop() = 0;

    /**
     * @brief Requset media player to pause the media content.
     */
    virtual bool pause() = 0;

    /**
     * @brief Requset media player to resume the media content.
     */
    virtual bool resume() = 0;

    /**
     * @brief Request the media player to move the current content section.
     * @param[in] sec content's position. It is moved from the current media position.
     */
    virtual bool seek(int sec) = 0;

    /**
     * @brief Get current position information of the media content from the media player.
     * @return media content's current position
     */
    virtual int position() = 0;
    /**
     * @brief Set current position of the media content to the media player.
     * @param[in] position media content's position.
     */
    virtual bool setPosition(int position) = 0;

    /**
     * @brief Get duration information of the media content from the media player.
     * @return media content's duration
     */
    virtual int duration() = 0;

    /**
     * @brief Set duration of the media content to the media player.
     * @param[in] duration media content's duration.
     */
    virtual bool setDuration(int duration) = 0;

    /**
     * @brief Get volume information of the media content from the media player.
     * @return media content's duration
     */
    virtual int volume() = 0;

    /**
     * @brief Set volume of the media content to the media player.
     * @param[in] volume media content's volume.
     */
    virtual bool setVolume(int volume) = 0;

    /**
     * @brief Get mute information of the media content from the media player.
     * @return media content's mute
     */
    virtual bool mute() = 0;

    /**
     * @brief Set mute of the media content to the media player.
     * @param[in] mute media content's mute.
     */
    virtual bool setMute(bool mute) = 0;

    /**
     * @brief Check whether the media player is playing media content.
     * @return Play or not
     */
    virtual bool isPlaying() = 0;

    /**
     * @brief Get state of the media player.
     * @return media player's state
     */
    virtual MediaPlayerState state() = 0;

    /**
     * @brief Set state of the media player.
     * @param[in] state media player's state
     */
    virtual bool setState(MediaPlayerState state) = 0;

    /**
     * @brief Get state name of the media player.
     * @param[in] state media player's state
     * @return media player's state name
     */
    virtual std::string stateString(MediaPlayerState state) = 0;

    /**
     * @brief Get url of media content from the media player.
     * @return media content's url
     */
    virtual std::string url() = 0;
};

/**
 * @}
 */

} // NuguInterface

#endif
