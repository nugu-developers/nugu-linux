/*
 * Copyright (c) 2021 SK Telecom Co., Ltd. All rights reserved.
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

#ifndef __NUGU_ENCODER_H__
#define __NUGU_ENCODER_H__

#include <base/nugu_pcm.h>
#include <base/nugu_buffer.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nugu_encoder.h
 */

/**
 * @brief encoder object
 * @ingroup NuguEncoder
 */
typedef struct _nugu_encoder NuguEncoder;

/**
 * @brief encoder driver object
 * @ingroup NuguEncoderDriver
 */
typedef struct _nugu_encoder_driver NuguEncoderDriver;

/**
 * @defgroup NuguEncoder Encoder
 * @ingroup SDKBase
 * @brief Encoder functions
 *
 * The encoder object encodes the pcm data to specific encoded data.
 *
 * @{
 */

/**
 * @brief Create new encoder object
 * @param[in] driver encoder driver
 * @param[in] property audio property(channel,type,sample-rate)
 * @return encoder object
 * @see nugu_encoder_free()
 */
NuguEncoder *nugu_encoder_new(NuguEncoderDriver *driver,
			      NuguAudioProperty property);

/**
 * @brief Destroy the encoder object
 * @param[in] enc encoder object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_encoder_new()
 */
int nugu_encoder_free(NuguEncoder *enc);

/**
 * @brief Set custom data for driver
 * @param[in] enc encoder object
 * @param[in] data custom data managed by driver
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_encoder_get_driver_data()
 */
int nugu_encoder_set_driver_data(NuguEncoder *enc, void *data);

/**
 * @brief Get custom data for driver
 * @param[in] enc encoder object
 * @return data
 * @see nugu_encoder_set_driver_data()
 */
void *nugu_encoder_get_driver_data(NuguEncoder *enc);

/**
 * @brief Encode the encoded data
 * @param[in] enc encoder object
 * @param[in] data pcm data
 * @param[in] data_len pcm data length
 * @param[out] output_len output buffer length
 * @return memory allocated encoded data. Developer must free the data manually.
 */
void *nugu_encoder_encode(NuguEncoder *enc, const void *data, size_t data_len,
			  size_t *output_len);

/**
 * @}
 */

/**
 * @defgroup NuguEncoderDriver Encoder driver
 * @ingroup SDKDriver
 * @brief Encoder driver
 *
 * The encoder driver performs a function of encoding the received pcm data.
 *
 * @{
 */

/**
 * @brief encoder type
 * @see nugu_encoder_driver_new()
 */
enum nugu_encoder_type {
	NUGU_ENCODER_TYPE_SPEEX, /**< SPEEX */
	NUGU_ENCODER_TYPE_OPUS, /**< OPUS */
	NUGU_ENCODER_TYPE_CUSTOM = 99 /**< Custom type */
};

/**
 * @brief encoder driver operations
 * @see nugu_encoder_driver_new()
 */
struct nugu_encoder_driver_ops {
	/**
	 * @brief Called when creating a new encoder.
	 * @see nugu_encoder_new()
	 */
	int (*create)(NuguEncoderDriver *driver, NuguEncoder *enc,
		      NuguAudioProperty property);

	/**
	 * @brief Called when a encoding request is received from the encoder.
	 * @see nugu_encoder_encode()
	 */
	int (*encode)(NuguEncoderDriver *driver, NuguEncoder *enc,
		      const void *data, size_t data_len, NuguBuffer *out_buf);
	/**
	 * @brief Called when the encoder is destroyed.
	 * @see nugu_encoder_free()
	 */
	int (*destroy)(NuguEncoderDriver *driver, NuguEncoder *enc);
};

/**
 * @brief Create new encoder driver
 * @param[in] name driver name
 * @param[in] type encoder type
 * @param[in] ops operation table
 * @return encoder driver object
 * @see nugu_encoder_driver_free()
 */
NuguEncoderDriver *nugu_encoder_driver_new(const char *name,
					   enum nugu_encoder_type type,
					   struct nugu_encoder_driver_ops *ops);

/**
 * @brief Destroy the encoder driver
 * @param[in] driver encoder driver object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_encoder_driver_free(NuguEncoderDriver *driver);

/**
 * @brief Register the driver to driver list
 * @param[in] driver encoder driver object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_encoder_driver_register(NuguEncoderDriver *driver);

/**
 * @brief Remove the driver from driver list
 * @param[in] driver encoder driver object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_encoder_driver_remove(NuguEncoderDriver *driver);

/**
 * @brief Find a driver by name in the driver list
 * @param[in] name encoder driver name
 * @return encoder driver object
 * @see nugu_encoder_driver_find_bytype()
 */
NuguEncoderDriver *nugu_encoder_driver_find(const char *name);

/**
 * @brief Find a driver by type in the driver list
 * @param[in] type encoder driver type
 * @return encoder driver object
 * @see nugu_encoder_driver_find
 */
NuguEncoderDriver *nugu_encoder_driver_find_bytype(enum nugu_encoder_type type);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
