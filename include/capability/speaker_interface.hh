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

#ifndef __NUGU_SPEAKER_INTERFACE_H__
#define __NUGU_SPEAKER_INTERFACE_H__

#include <nugu.h>
#include <base/nugu_audio.h>
#include <clientkit/capability_interface.hh>

namespace NuguCapability {

using namespace NuguClientKit;

/**
 * @file speaker_interface.hh
 * @defgroup SpeakerInterface SpeakerInterface
 * @ingroup SDKNuguCapability
 * @brief Speaker capability interface
 *
 * Control the volumes for nugu, call, alarm and external.
 *
 * @{
 */

#define NUGU_SPEAKER_NUGU_STRING "NUGU"
#define NUGU_SPEAKER_MUSIC_STRING NUGU_AUDIO_ATTRIBUTE_MUSIC_DEFAULT_STRING
#define NUGU_SPEAKER_RINGTONE_STRING NUGU_AUDIO_ATTRIBUTE_RINGTONE_DEFAULT_STRING
#define NUGU_SPEAKER_CALL_STRING NUGU_AUDIO_ATTRIBUTE_CALL_DEFAULT_STRING
#define NUGU_SPEAKER_NOTIFICATION_STRING NUGU_AUDIO_ATTRIBUTE_NOTIFICATION_DEFAULT_STRING
#define NUGU_SPEAKER_ALARM_STRING NUGU_AUDIO_ATTRIBUTE_ALARM_DEFAULT_STRING
#define NUGU_SPEAKER_VOICE_COMMAND_STRING NUGU_AUDIO_ATTRIBUTE_VOICE_COMMAND_DEFAULT_STRING
#define NUGU_SPEAKER_NAVIGATION_STRING NUGU_AUDIO_ATTRIBUTE_NAVIGATION_DEFAULT_STRING
#define NUGU_SPEAKER_SYSTEM_SOUND_STRING NUGU_AUDIO_ATTRIBUTE_SYSTEM_SOUND_DEFAULT_STRING

#define NUGU_SPEAKER_MIN_VOLUME 0 /** @def Set speaker minimum volume to 0 */
#define NUGU_SPEAKER_MAX_VOLUME 100 /** @def Set speaker maximum volume to 100 */
#define NUGU_SPEAKER_DEFAULT_VOLUME 50 /** @def Set speaker default volume to 50 */
#define NUGU_SPEAKER_DEFAULT_STEP 10 /** @def Set speaker default volume step to 10 */
#define NUGU_SPEAKER_UNABLE_CONTROL -1 /** @def This property is set to be out of control. */

/**
 * @brief SpeakerType
 */
enum class SpeakerType {
    NUGU = 0, /**< General NUGU Speaker type: Music + Voice command */
    MUSIC = NUGU_AUDIO_ATTRIBUTE_MUSIC, /**< Music type */
    RINGTONE = NUGU_AUDIO_ATTRIBUTE_RINGTONE, /**< Ringtone type */
    CALL = NUGU_AUDIO_ATTRIBUTE_CALL, /**< Call type */
    NOTIFICATION = NUGU_AUDIO_ATTRIBUTE_NOTIFICATION, /**< Notification type */
    ALARM = NUGU_AUDIO_ATTRIBUTE_ALARM, /**< Alarm type */
    VOICE_COMMAND = NUGU_AUDIO_ATTRIBUTE_VOICE_COMMAND,
    /**< Voice Command type */
    NAVIGATION = NUGU_AUDIO_ATTRIBUTE_NAVIGATION, /**< Navigation type */
    SYSTEM_SOUND = NUGU_AUDIO_ATTRIBUTE_SYSTEM_SOUND /**< System Sound type */
};

/**
 * @brief SpeakerInfo
 */
class NUGU_API SpeakerInfo {
public:
    SpeakerInfo() = default;
    ~SpeakerInfo() = default;

    SpeakerType type = SpeakerType::NUGU; /**< Speaker type  */
    std::string group; /**< Speaker volume group  */
    int min = NUGU_SPEAKER_MIN_VOLUME; /**< Speaker min volume  */
    int max = NUGU_SPEAKER_MAX_VOLUME; /**< Speaker max volume  */
    int volume = NUGU_SPEAKER_DEFAULT_VOLUME; /**< Speaker current volume  */
    int step = NUGU_SPEAKER_DEFAULT_STEP; /**< Speaker default volume step  */
    int level = NUGU_SPEAKER_DEFAULT_VOLUME; /**< Speaker default volume level  */
    int mute = NUGU_SPEAKER_UNABLE_CONTROL; /**< Speaker mute state  */
    bool can_control = false; /**< Speaker controllability */
};

/**
 * @brief speaker listener interface
 * @see ISpeakerHandler
 */
class NUGU_API ISpeakerListener : virtual public ICapabilityListener {
public:
    virtual ~ISpeakerListener() = default;

    /**
     * @brief The SDK requests the volume setting received from the server.
     * @param[in] ps_id play service id
     * @param[in] type speaker type
     * @param[in] volume volume level
     * @param[in] linear change volume method. if linear is true volume is changed gradually, otherwise immediately
     * @see sendEventVolumeChanged()
     */
    virtual void requestSetVolume(const std::string& ps_id, SpeakerType type, int volume, bool linear) = 0;

    /**
     * @brief The SDK requests the mute setting received from the server.
     * @param[in] ps_id play service id
     * @param[in] type speaker type
     * @param[in] mute volume mute
     * @see sendEventMuteChanged()
     */
    virtual void requestSetMute(const std::string& ps_id, SpeakerType type, bool mute) = 0;
};

/**
 * @brief speaker handler interface
 * @see ISpeakerListener
 */
class NUGU_API ISpeakerHandler : virtual public ICapabilityInterface {
public:
    virtual ~ISpeakerHandler() = default;

    /**
     * @brief Set speaker information in SDK to be controlled by application.
     * @param[in] info speaker's information map
     */
    virtual void setSpeakerInfo(const std::map<SpeakerType, SpeakerInfo>& info) = 0;
    /**
     * @brief Inform volume changed by application to the SDK
     * @param[in] type speaker type
     * @param[in] volume volume level
     */
    virtual void informVolumeChanged(SpeakerType type, int volume) = 0;
    /**
     * @brief Inform mute changed by application to the SDK
     * @param[in] type speaker type
     * @param[in] mute volume mute
     */
    virtual void informMuteChanged(SpeakerType type, bool mute) = 0;
    /**
     * @brief Send event the result of request SetVolume to the SDK.
     * @param[in] ps_id play service id
     * @param[in] result result of SetVolume
     */
    virtual void sendEventVolumeChanged(const std::string& ps_id, bool result) = 0;

    /**
     * @brief Send event the result of request SetMute to the SDK.
     * @param[in] ps_id play service id
     * @param[in] result result of SetMute
     */
    virtual void sendEventMuteChanged(const std::string& ps_id, bool result) = 0;

    /**
     * @brief Get speaker's name
     * @param[in] type speaker type
     * @return result speaker's name
     */
    virtual std::string getSpeakerName(const SpeakerType& type) = 0;

    /**
     * @brief Get speaker's type according speaker's name
     * @param[in] name speaker name
     * @param[out] type speaker type
     * @return return true if the speaker type is found, otherwise false
     */
    virtual bool getSpeakerType(const std::string& name, SpeakerType& type) = 0;
};

/**
 * @}
 */

} // NuguInterface

#endif /* __NUGU_SPEAKER_INTERFACE_H__ */
