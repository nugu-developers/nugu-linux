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

#ifndef __NUGU_AUDIO_H__
#define __NUGU_AUDIO_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nugu_audio.h
 */

#define NUGU_AUDIO_ATTRIBUTE_MUSIC_DEFAULT_STRING "MUSIC"
#define NUGU_AUDIO_ATTRIBUTE_RINGTONE_DEFAULT_STRING "RINGTONE"
#define NUGU_AUDIO_ATTRIBUTE_CALL_DEFAULT_STRING "CALL"
#define NUGU_AUDIO_ATTRIBUTE_NOTIFICATION_DEFAULT_STRING "NOTIFICATION"
#define NUGU_AUDIO_ATTRIBUTE_ALARM_DEFAULT_STRING "ALARM"
#define NUGU_AUDIO_ATTRIBUTE_VOICE_COMMAND_DEFAULT_STRING "VOICE_COMMAND"
#define NUGU_AUDIO_ATTRIBUTE_NAVIGATION_DEFAULT_STRING "NAVIGATION"
#define NUGU_AUDIO_ATTRIBUTE_SYSTEM_SOUND_DEFAULT_STRING "SYSTEM_SOUND"

/**
 * @brief audio sample rate
 */
enum nugu_audio_sample_rate {
	NUGU_AUDIO_SAMPLE_RATE_8K, /**< 8K */
	NUGU_AUDIO_SAMPLE_RATE_16K, /**< 16K */
	NUGU_AUDIO_SAMPLE_RATE_32K, /**< 32K */
	NUGU_AUDIO_SAMPLE_RATE_22K, /**< 22K */
	NUGU_AUDIO_SAMPLE_RATE_44K, /**< 44K */
	NUGU_AUDIO_SAMPLE_RATE_MAX
};

/**
 * @brief audio format
 */
enum nugu_audio_format {
	NUGU_AUDIO_FORMAT_S8, /**< Signed 8 bits */
	NUGU_AUDIO_FORMAT_U8, /**< Unsigned 8 bits */
	NUGU_AUDIO_FORMAT_S16_LE, /**< Signed 16 bits little endian */
	NUGU_AUDIO_FORMAT_S16_BE, /**< Signed 16 bits big endian */
	NUGU_AUDIO_FORMAT_U16_LE, /**< Unsigned 16 bits little endian */
	NUGU_AUDIO_FORMAT_U16_BE, /**< Unsigned 16 bits big endian */
	NUGU_AUDIO_FORMAT_S24_LE, /**< Signed 24 bits little endian */
	NUGU_AUDIO_FORMAT_S24_BE, /**< Signed 24 bits big endian */
	NUGU_AUDIO_FORMAT_U24_LE, /**< Unsigned 24 bits little endian */
	NUGU_AUDIO_FORMAT_U24_BE, /**< Unsigned 24 bits big endian */
	NUGU_AUDIO_FORMAT_S32_LE, /**< Signed 32 bits little endian */
	NUGU_AUDIO_FORMAT_S32_BE, /**< Signed 32 bits big endian */
	NUGU_AUDIO_FORMAT_U32_LE, /**< Unsigned 32 bits little endian */
	NUGU_AUDIO_FORMAT_U32_BE, /**< Unsigned 32 bits big endian */
	NUGU_AUDIO_FORMAT_MAX
};

/**
 * @brief audio attribute
 */
enum nugu_audio_attribute {
	NUGU_AUDIO_ATTRIBUTE_MUSIC = 1, /**< audio attribute for music */
	NUGU_AUDIO_ATTRIBUTE_RINGTONE, /**< audio attribute for ringtone */
	NUGU_AUDIO_ATTRIBUTE_CALL, /**< audio attribute for call */
	NUGU_AUDIO_ATTRIBUTE_NOTIFICATION,
	/**< audio attribute for notification */
	NUGU_AUDIO_ATTRIBUTE_ALARM, /**< audio attribute for alarm */
	NUGU_AUDIO_ATTRIBUTE_VOICE_COMMAND,
	/**< audio attribute for voice command like tts */
	NUGU_AUDIO_ATTRIBUTE_NAVIGATION, /**< audio attribute for navigation */
	NUGU_AUDIO_ATTRIBUTE_SYSTEM_SOUND
	/**< audio attribute for system sound */
};

/**
 * @brief audio property
 */
struct nugu_audio_property {
	enum nugu_audio_sample_rate samplerate;
	enum nugu_audio_format format;
	int channel;
};

/**
 * @brief NuguAudioAttribute
 */
typedef enum nugu_audio_attribute NuguAudioAttribute;

/**
 * @brief NuguAudioProperty
 */
typedef struct nugu_audio_property NuguAudioProperty;

/**
 * @brief Set audio attribute string
 * @param[in] attribute audio attribute
 * @param[in] str audio attribute's string
 */
void nugu_audio_set_attribute_str(const NuguAudioAttribute attribute,
				  const char *str);

/**
 * @brief Get audio attribute string
 * @param[in] attribute audio attribute
 * @return audio attribute string
 */
const char *nugu_audio_get_attribute_str(const NuguAudioAttribute attribute);

/**
 * @brief Get audio attribute type from string
 * @param[in] str audio attribute's string
 * @return audio attribute
 */
NuguAudioAttribute nugu_audio_get_attribute(const char *str);

#ifdef __cplusplus
}
#endif

#endif
