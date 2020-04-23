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

#define NUGU_SPEAKER_MIN_VOLUME 0 /** @def Set speaker minimum volume to 0 */
#define NUGU_SPEAKER_MAX_VOLUME 100 /** @def Set speaker maximum volume to 100 */
#define NUGU_SPEAKER_DEFAULT_VOLUME 50 /** @def Set speaker default volume to 50 */
#define NUGU_SPEAKER_DEFAULT_STEP 10 /** @def Set speaker default volume step to 10 */

/**
 * @brief SpeakerType
 */
enum class SpeakerType {
    NUGU, /**< General NUGU Speaker type */
    CALL, /**< Call type */
    ALARM /**< Alarm type */
};

/**
 * @brief SpeakerInfo
 */
class SpeakerInfo {
public:
    SpeakerInfo() = default;
    ~SpeakerInfo() = default;

    SpeakerType type = SpeakerType::NUGU; /**< Speaker type  */
    int min = NUGU_SPEAKER_MIN_VOLUME; /**< Speaker min volume  */
    int max = NUGU_SPEAKER_MAX_VOLUME; /**< Speaker max volume  */
    int volume = NUGU_SPEAKER_DEFAULT_VOLUME; /**< Speaker current volume  */
    int step = NUGU_SPEAKER_DEFAULT_STEP; /**< Speaker default volume step  */
    bool mute = false; /**< Speaker mute state  */
    bool can_control = false; /**< Speaker controllability */
};

/**
 * @brief speaker listener interface
 * @see ISpeakerHandler
 */
class ISpeakerListener : public ICapabilityListener {
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
class ISpeakerHandler : virtual public ICapabilityInterface {
public:
    virtual ~ISpeakerHandler() = default;

    /**
     * @brief Set speaker information in SDK to be controlled by application.
     * @param[in] info speaker's information map
     */
    virtual void setSpeakerInfo(std::map<SpeakerType, SpeakerInfo*> info) = 0;
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
};

/**
 * @}
 */

} // NuguInterface

#endif /* __NUGU_SPEAKER_INTERFACE_H__ */
