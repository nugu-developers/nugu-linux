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

#ifndef __NUGU_DECODER_H__
#define __NUGU_DECODER_H__

#include <base/nugu_pcm.h>
#include <base/nugu_buffer.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nugu_decoder.h
 */

/**
 * @brief decoder object
 * @ingroup NuguDecoder
 */
typedef struct _nugu_decoder NuguDecoder;

/**
 * @brief decoder driver object
 * @ingroup NuguDecoderDriver
 */
typedef struct _nugu_decoder_driver NuguDecoderDriver;

/**
 * @defgroup NuguDecoder Decoder
 * @ingroup SDKBase
 * @brief Decoder functions
 *
 * The decoder object decodes the encoded data. It also serves to pass the
 * decoded result to the PCM sink.
 *
 * @{
 */

/**
 * @brief Create new decoder object
 * @param[in] driver decoder driver
 * @param[in] sink pcm object
 * @return decoder object
 * @see nugu_decoder_free()
 * @see nugu_decoder_get_pcm()
 * @see nugu_pcm_new()
 */
NuguDecoder *nugu_decoder_new(NuguDecoderDriver *driver, NuguPcm *sink);

/**
 * @brief Destroy the decoder object
 * @param[in] dec decoder object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_decoder_new()
 */
int nugu_decoder_free(NuguDecoder *dec);

/**
 * @brief Decode the encoded data and pass the result to sink
 * @param[in] dec decoder object
 * @param[in] data encoded data
 * @param[in] data_len encoded data length
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_decoder_decode()
 */
int nugu_decoder_play(NuguDecoder *dec, const void *data, size_t data_len);

/**
 * @brief Set private userdata for driver
 * @param[in] dec decoder object
 * @param[in] userdata userdata managed by driver
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_decoder_get_userdata()
 */
int nugu_decoder_set_userdata(NuguDecoder *dec, void *userdata);

/**
 * @brief Get private userdata for driver
 * @param[in] dec decoder object
 * @return userdata
 * @see nugu_decoder_set_userdata()
 */
void *nugu_decoder_get_userdata(NuguDecoder *dec);

/**
 * @brief Decode the encoded data
 * @param[in] dec decoder object
 * @param[in] data encoded data
 * @param[in] data_len encoded data length
 * @param[out] output_len output buffer length
 * @return memory allocated decoded data. Developer must free the data manually.
 */
void *nugu_decoder_decode(NuguDecoder *dec, const void *data, size_t data_len,
			  size_t *output_len);

/**
 * @brief Get pcm(sink) object
 * @param[in] dec decoder object
 * @return pcm object
 * @see nugu_decoder_new()
 */
NuguPcm *nugu_decoder_get_pcm(NuguDecoder *dec);

/**
 * @}
 */

/**
 * @defgroup NuguDecoderDriver Decoder driver
 * @ingroup SDKDriver
 * @brief Decoder driver
 *
 * The decoder driver performs a function of decoding the received encoded data.
 *
 * @{
 */

/**
 * @brief decoder type
 * @see nugu_decoder_driver_new()
 */
enum decoder_type {
	DECODER_TYPE_OPUS, /**< OPUS */
	DECODER_TYPE_CUSTOM = 99 /**< Custom type */
};

/**
 * @brief decoder driver operations
 * @see nugu_decoder_driver_new()
 */
struct nugu_decoder_driver_ops {
	/**
	 * @brief Called when creating a new decoder.
	 * @see nugu_decoder_new()
	 */
	int (*create)(NuguDecoderDriver *driver, NuguDecoder *dec);

	/**
	 * @brief Called when a decoding request is received from the decoder.
	 * @see nugu_decoder_decode()
	 */
	int (*decode)(NuguDecoderDriver *driver, NuguDecoder *dec,
		      const void *data, size_t data_len, NuguBuffer *out_buf);
	/**
	 * @brief Called when the decoder is destroyed.
	 * @see nugu_decoder_free()
	 */
	int (*destroy)(NuguDecoderDriver *driver, NuguDecoder *dec);
};

/**
 * @brief Create new decoder driver
 * @param[in] name driver name
 * @param[in] type decoder type
 * @param[in] ops operation table
 * @return decoder driver object
 * @see nugu_decoder_driver_free()
 */
NuguDecoderDriver *nugu_decoder_driver_new(const char *name,
					   enum decoder_type type,
					   struct nugu_decoder_driver_ops *ops);

/**
 * @brief Destroy the decoder driver
 * @param[in] driver decoder driver object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_decoder_driver_free(NuguDecoderDriver *driver);

/**
 * @brief Register the driver to driver list
 * @param[in] driver decoder driver object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_decoder_driver_register(NuguDecoderDriver *driver);

/**
 * @brief Remove the driver from driver list
 * @param[in] driver decoder driver object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_decoder_driver_remove(NuguDecoderDriver *driver);

/**
 * @brief Find a driver by name in the driver list
 * @param[in] name decoder driver name
 * @return decoder driver object
 * @see nugu_decoder_driver_find_bytype()
 */
NuguDecoderDriver *nugu_decoder_driver_find(const char *name);

/**
 * @brief Find a driver by type in the driver list
 * @param[in] type decoder driver type
 * @return decoder driver object
 * @see nugu_decoder_driver_find
 */
NuguDecoderDriver *nugu_decoder_driver_find_bytype(enum decoder_type type);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
