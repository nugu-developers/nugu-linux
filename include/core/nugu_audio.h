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

/**
 * @brief audio sample rate
 */
enum nugu_audio_sample_rate {
	AUDIO_SAMPLE_RATE_8K, /**< 8K */
	AUDIO_SAMPLE_RATE_16K, /**< 16K */
	AUDIO_SAMPLE_RATE_32K, /**< 32K */
	AUDIO_SAMPLE_RATE_22K, /**< 22K */
	AUDIO_SAMPLE_RATE_44K, /**< 44K */
	AUDIO_SAMPLE_RATE_MAX
};

/**
 * @brief audio format
 */
enum nugu_audio_format {
	AUDIO_FORMAT_S8, /**< Signed 8 bits */
	AUDIO_FORMAT_U8, /**< Unsigned 8 bits */
	AUDIO_FORMAT_S16_LE, /**< Signed 16 bits little endian */
	AUDIO_FORMAT_S16_BE, /**< Signed 16 bits big endian */
	AUDIO_FORMAT_U16_LE, /**< Unsigned 16 bits little endian */
	AUDIO_FORMAT_U16_BE, /**< Unsigned 16 bits big endian */
	AUDIO_FORMAT_S24_LE, /**< Signed 24 bits little endian */
	AUDIO_FORMAT_S24_BE, /**< Signed 24 bits big endian */
	AUDIO_FORMAT_U24_LE, /**< Unsigned 24 bits little endian */
	AUDIO_FORMAT_U24_BE, /**< Unsigned 24 bits big endian */
	AUDIO_FORMAT_S32_LE, /**< Signed 32 bits little endian */
	AUDIO_FORMAT_S32_BE, /**< Signed 32 bits big endian */
	AUDIO_FORMAT_U32_LE, /**< Unsigned 32 bits little endian */
	AUDIO_FORMAT_U32_BE, /**< Unsigned 32 bits big endian */
	AUDIO_FORMAT_MAX
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
 * @brief NuguAudioProperty
 */
typedef struct nugu_audio_property NuguAudioProperty;

#ifdef __cplusplus
}
#endif

#endif
