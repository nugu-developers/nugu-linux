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

#include <clientkit/capability_interface.hh>

namespace NuguCapability {

using namespace NuguClientKit;

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
 * @brief AudioPlayerEvent
 */
enum class AudioPlayerEvent {
    UNDERRUN, /**< This event is occurred when the content is reloaded because of bad quailty network */
    LOAD_FAILED, /**< This event is occurred when the content is not loaded successfully */
    LOAD_DONE, /**< This event is occurred when the content is loaded successfully */
    INVALID_URL /**< This event is occurred when the content is not valid url */
};

/**
 * @brief RepeatType
 */
enum class RepeatType {
    NONE, /**< Never repeat */
    ONE, /**< Just one time repeat */
    ALL /**< Repeat continuously */
};

/**
 * @brief ControlLyricsDirection
 */
enum class ControlLyricsDirection {
    PREVIOUS, /**< Previous direction */
    NEXT /**< Next direction */
};

/**
 * @brief audioplayer listener interface
 * @see IAudioPlayerHandler
 */
class IAudioPlayerListener : public ICapabilityListener {
public:
    virtual ~IAudioPlayerListener() = default;

    /**
     * @brief Report this to the user when the state of the audio player changes.
     * @param[in] state audioplayer's state
     * @param[in] dialog_id dialog request id
     */
    virtual void mediaStateChanged(AudioPlayerState state, const std::string& dialog_id) = 0;

    /**
     * @brief The audio player reports player's event being played to the user.
     */
    virtual void mediaEventReport(AudioPlayerEvent event, const std::string& dialog_id) = 0;

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
     * @param[in] dialog_id dialog request id
     */
    virtual void favoriteChanged(bool favorite, const std::string& dialog_id) = 0;

    /**
     * @brief The audio player reports to the user the current content's shuffle setting changed.
     * @param[in] shuffle media content's shuffle
     * @param[in] dialog_id dialog request id
     */
    virtual void shuffleChanged(bool shuffle, const std::string& dialog_id) = 0;

    /**
     * @brief The audio player reports to the user the current content's repeat setting changed.
     * @param[in] repeat media content's repeat
     * @param[in] dialog_id dialog request id
     */
    virtual void repeatChanged(RepeatType repeat, const std::string& dialog_id) = 0;

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
class IAudioPlayerHandler : virtual public ICapabilityInterface {
public:
    virtual ~IAudioPlayerHandler() = default;

    /**
     * @brief Request the audio player to play the current content.
     * @return dialog request id if a NUGU service request succeeds with user text, otherwise empty string
     */
    virtual std::string play() = 0;

    /**
     * @brief Request the audio player to stop the current content.
     * @return dialog request id if a NUGU service request succeeds with user text, otherwise empty string
     */
    virtual std::string stop() = 0;

    /**
     * @brief Request the audio player to play the next content.
     * @return dialog request id if a NUGU service request succeeds with user text, otherwise empty string
     */
    virtual std::string next() = 0;

    /**
     * @brief Request the audio player to play the previous content.
     * @return dialog request id if a NUGU service request succeeds with user text, otherwise empty string
     */
    virtual std::string prev() = 0;

    /**
     * @brief Request the audio player to pause the current content.
     * @return dialog request id if a NUGU service request succeeds with user text, otherwise empty string
     */
    virtual std::string pause() = 0;

    /**
     * @brief Request the audio player to resume the current content.
     * @return dialog request id if a NUGU service request succeeds with user text, otherwise empty string
     */
    virtual std::string resume() = 0;

    /**
     * @brief Request the audio player to move the current content section.
     * @param[in] sec content's position. It is moved from the current media position.
     */
    virtual void seek(int msec) = 0;

    /**
     * @brief Request the audio player to set favorite the content.
     * @param[in] favorite favorite value
     * @return dialog request id if a NUGU service request succeeds with user text, otherwise empty string
     */
    virtual std::string setFavorite(bool favorite) = 0;

    /**
     * @brief Request the audio player to set repeat the content.
     * @param[in] repeat repeat value
     * @return dialog request id if a NUGU service request succeeds with user text, otherwise empty string
     */
    virtual std::string setRepeat(RepeatType repeat) = 0;

    /**
     * @brief Request the audio player to set shuffle the content.
     * @param[in] shuffle shuffle value
     * @return dialog request id if a NUGU service request succeeds with user text, otherwise empty string
     */
    virtual std::string setShuffle(bool shuffle) = 0;

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
};

/**
 * @}
 */

} // NuguCapability

#endif /* __NUGU_AUDIO_PLAYER_INTERFACE_H__ */
