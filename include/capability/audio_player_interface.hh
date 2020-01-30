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

#ifndef __NUGU_AUDIO_PLAYER_INTERFACE_H__
#define __NUGU_AUDIO_PLAYER_INTERFACE_H__

#include <capability/capability_interface.hh>
#include <capability/display_interface.hh>

namespace NuguCapability {

/**
 * @file audio_player_interface.hh
 * @defgroup AudioPlayerInterface AudioPlayerInterface
 * @ingroup SDKNuguCapability
 * @brief AudioPlayer capability interface
 *
 * Processes the media service request to control the content and
 * sends the current operation status of the content to the server.
 *
 * @{
 */

/**
 * @brief AudioPlayerState
 */
enum class AudioPlayerState {
    IDLE, /**< Enters IDLE state when first powered on */
    PLAYING, /**< Status playing in AudioPlayer */
    STOPPED, /**< Status stopped in AudioPlayer */
    PAUSED, /**< Status paused in AudioPlayer */
    FINISHED /**< Status playback finished in AudioPlayer */
};

/**
 * @brief RepeatType
 */
enum class RepeatType {
    NONE,
    ONE,
    ALL
};

/**
 * @brief audioplayer listener interface
 * @see IAudioPlayerHandler
 */
class IAudioPlayerListener : virtual public IDisplayListener {
public:
    virtual ~IAudioPlayerListener() = default;

    /**
     * @brief Report this to the user when the state of the audio player changes.
     * @param[in] state audioplayer's state
     */
    virtual void mediaStateChanged(AudioPlayerState state) = 0;

    /**
     * @brief The audio player reports the total duration of the content being played to the user.
     * @param[in] duration media content's duration
     */
    virtual void durationChanged(int duration) = 0;

    /**
     * @brief The audio player reports to the user the current playing time of the content being played.
     * @param[in] position media content's position
     */
    virtual void positionChanged(int position) = 0;

    /**
     * @brief The audio player reports to the user the current content's favorite setting changed.
     * @param[in] favorite media content's favorite
     */
    virtual void favoriteChanged(bool favorite) = 0;

    /**
     * @brief The audio player reports to the user the current content's shuffle setting changed.
     * @param[in] shuffle media content's shuffle
     */
    virtual void shuffleChanged(bool shuffle) = 0;

    /**
     * @brief The audio player reports to the user the current content's repeat setting changed.
     * @param[in] repeat media content's repeat
     */
    virtual void repeatChanged(RepeatType repeat) = 0;

    /**
     * @brief The audio player request to the user to cache the content if possible
     * @param[in] key content's unique key
     * @param[in] playurl content's playurl
     */
    virtual void requestContentCache(const std::string& key, const std::string& playurl) = 0;

    /**
     * @brief The audio player request to the user to get the cached content
     * @param[in] key content's unique key
     * @param[out] filepath cached content's filepath
     * @return return true if cached content, otherwise false
     */
    virtual bool requestToGetCachedContent(const std::string& key, std::string& filepath) = 0;
};

/**
 * @brief audioplayer handler interface
 * @see IAudioPlayerListener
 */
class IAudioPlayerHandler : virtual public IDisplayHandler {
public:
    virtual ~IAudioPlayerHandler() = default;

    /**
     * @brief Request the audio player to play the current content.
     */
    virtual void play() = 0;

    /**
     * @brief Request the audio player to stop the current content.
     */
    virtual void stop() = 0;

    /**
     * @brief Request the audio player to play the next content.
     */
    virtual void next() = 0;

    /**
     * @brief Request the audio player to play the previous content.
     */
    virtual void prev() = 0;

    /**
     * @brief Request the audio player to pause the current content.
     */
    virtual void pause() = 0;

    /**
     * @brief Request the audio player to resume the current content.
     */
    virtual void resume() = 0;

    /**
     * @brief Request the audio player to move the current content section.
     * @param[in] sec content's position. It is moved from the current media position.
     */
    virtual void seek(int msec) = 0;

    /**
     * @brief Request the audio player to set favorite the content.
     * @param[in] favorite favorite value
     */
    virtual void setFavorite(bool favorite) = 0;

    /**
     * @brief Request the audio player to set repeat the content.
     * @param[in] repeat repeat value
     */
    virtual void setRepeat(RepeatType repeat) = 0;

    /**
     * @brief Request the audio player to set shuffle the content.
     * @param[in] shuffle shuffle value
     */
    virtual void setShuffle(bool shuffle) = 0;

    /**
     * @brief set media player's volume
     * @param[in] volume volume level
     * @return result of set volume
     */
    virtual bool setVolume(int volume) = 0;

    /**
     * @brief set media player's mute
     * @param[in] mute volume mute
     * @return result of set mute
     */
    virtual bool setMute(bool mute) = 0;

    /**
     * @brief Add the Listener object
     * @param[in] listener listener object
     */
    virtual void addListener(IAudioPlayerListener* listener) = 0;

    /**
     * @brief Remove the Listener object
     * @param[in] listener listener object
     */
    virtual void removeListener(IAudioPlayerListener* listener) = 0;
    using IDisplayHandler::removeListener;
};

/**
 * @}
 */

} // NuguCapability

#endif /* __NUGU_AUDIO_PLAYER_INTERFACE_H__ */
