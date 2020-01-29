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

#include <capability/capability_interface.hh>

namespace NuguCapability {

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

#define NUGU_SPEAKER_MIN_VOLUME 0
#define NUGU_SPEAKER_MAX_VOLUME 100
#define NUGU_SPEAKER_DEFAULT_VOLUME 50

enum class SpeakerType {
    NUGU,
    CALL,
    ALARM,
    EXTERNAL
};

/**
 * @brief SpeakerInfo
 */
class SpeakerInfo {
public:
    SpeakerInfo() = default;
    ~SpeakerInfo() = default;

    SpeakerType type = SpeakerType::NUGU;
    int min = NUGU_SPEAKER_MIN_VOLUME;
    int max = NUGU_SPEAKER_MAX_VOLUME;
    int volume = NUGU_SPEAKER_DEFAULT_VOLUME;
    bool mute = false;
    bool can_control = false;
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
     * @param[in] type speaker type
     * @param[in] volume volume level
     * @param[in] linear change volume method. if linear is true volume is changed gradually, otherwise immediately
     */
    virtual void requestSetVolume(SpeakerType type, int volume, bool linear) = 0;

    /**
     * @brief The SDK requests the mute setting received from the server.
     * @param[in] type speaker type
     * @param[in] mute volume mute
     */
    virtual void requestSetMute(SpeakerType type, bool mute) = 0;
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
     * @brief Inform the SDK of the volume setup result (success).
     * @param[in] type speaker type
     * @param[in] volume volume level
     */
    virtual void informVolumeSucceeded(SpeakerType type, int volume) = 0;

    /**
     * @brief Inform the SDK of the volume setup result (failure).
     * @param[in] type speaker type
     * @param[in] volume volume level
     */
    virtual void informVolumeFailed(SpeakerType type, int volume) = 0;

    /**
     * @brief Inform the SDK of the volume mute result (success).
     * @param[in] type speaker type
     * @param[in] mute volume mute
     */
    virtual void informMuteSucceeded(SpeakerType type, bool mute) = 0;

    /**
     * @brief Inform the SDK of the volume mute result (failure).
     * @param[in] type speaker type
     * @param[in] mute volume mute
     */
    virtual void informMuteFailed(SpeakerType type, bool mute) = 0;
};

/**
 * @}
 */

} // NuguInterface

#endif /* __NUGU_SPEAKER_INTERFACE_H__ */
