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

#include <capability/display_interface.hh>
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
    UNDERRUN, /**< This event is occurred when the content is reloaded because of bad quality network */
    LOAD_FAILED, /**< This event is occurred when the content is not loaded successfully */
    LOAD_DONE, /**< This event is occurred when the content is loaded successfully */
    INVALID_URL, /**< This event is occurred when the content is not valid url */
    PAUSE_BY_DIRECTIVE, /**< This event is occurred when the agent receives a pause directive, it blocks content playback until another directive is received. */
    PAUSE_BY_FOCUS, /**< This event is occurred when the agent loses focus by another higher focus */
    PAUSE_BY_APPLICATION /**< This event is occurred when the application pause the mediaplayer directly access */
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
 * @brief audioplayer listener interface
 * @see IAudioPlayerHandler
 */
class IAudioPlayerListener : virtual public ICapabilityListener {
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

    virtual bool onResume(const std::string& dialog_id) = 0;
};

/**
 * @brief audioplayer's display listener interface
 * @see IAudioPlayerHandler
 */
class IAudioPlayerDisplayListener : virtual public IDisplayListener {
public:
    virtual ~IAudioPlayerDisplayListener() = default;

    /**
     * @brief SDK request information about device's lyrics page available
     * @param[in] id display template id
     * @param[out] visible show lyrics page visible
     * @return return device's lyrics page available
     */
    virtual bool requestLyricsPageAvailable(const std::string& id, bool& visible) = 0;

    /**
     * @brief Request to the user to show the lyrics page.
     * @param[in] id display template id
     * @return return true if show lyrics success, otherwise false.
     */
    virtual bool showLyrics(const std::string& id) = 0;

    /**
     * @brief Request to the user to hide the lyrics page.
     * @param[in] id display template id
     * @return return true if hide lyrics success, otherwise false.
     */
    virtual bool hideLyrics(const std::string& id) = 0;

    /**
     * @brief Request to update metadata the current display
     * @param[in] id display template id
     * @param[in] json_payload template in json format for display
     */
    virtual void updateMetaData(const std::string& id, const std::string& json_payload) = 0;
};

/**
 * @brief audioplayer handler interface
 * @see IAudioPlayerListener
 */
class IAudioPlayerHandler : virtual public ICapabilityInterface,
                            virtual public IDisplayHandler {
public:
    virtual ~IAudioPlayerHandler() = default;

    /**
     * @brief Request the audio player to play the current content.
     * @return dialog request id if a NUGU service request succeeds with user text, otherwise empty string
     */
    virtual std::string play() = 0;

    /**
     * @brief Request the audio player to stop the current content.
     * @param[in] direct_access control mediaplayer via sending event or directly access
     * @return dialog request id if a NUGU service request succeeds with user text, otherwise empty string (only used direct_access is false)
     */
    virtual std::string stop(bool direct_access = false) = 0;

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
     * @param[in] direct_access control mediaplayer via sending event or directly access
     * @return dialog request id if a NUGU service request succeeds with user text, otherwise empty string (only used direct_access is false)
     */
    virtual std::string pause(bool direct_access = false) = 0;

    /**
     * @brief Request the audio player to resume the current content.
     * @param[in] direct_access control mediaplayer via sending event or directly access
     * @return dialog request id if a NUGU service request succeeds with user text, otherwise empty string (only used direct_access is false)
     */
    virtual std::string resume(bool direct_access = false) = 0;

    /**
     * @brief Request the audio player to move the current content section.
     * @param[in] sec content's position. It is moved from the current media position.
     */
    virtual void seek(int msec) = 0;

    /**
     * @brief Send to request favorite command event with current favorite value.
     * @param[in] current_favorite current favorite value
     * @return dialog request id if a NUGU service request succeeds with user text, otherwise empty string
     */
    virtual std::string requestFavoriteCommand(bool current_favorite) = 0;

    /**
     * @brief Send to request repeat command event with current repeat value.
     * @param[in] current_repeat current repeat value
     * @return dialog request id if a NUGU service request succeeds with user text, otherwise empty string
     */
    virtual std::string requestRepeatCommand(RepeatType current_repeat) = 0;

    /**
     * @brief Send to request shuffle command event with current shuffle value.
     * @param[in] current_shuffle current shuffle value
     * @return dialog request id if a NUGU service request succeeds with user text, otherwise empty string
     */
    virtual std::string requestShuffleCommand(bool current_shuffle) = 0;

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
