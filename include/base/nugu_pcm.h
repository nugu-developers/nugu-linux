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

#ifndef __NUGU_PCM_H__
#define __NUGU_PCM_H__

#include <stddef.h>
#include <nugu.h>
#include <base/nugu_audio.h>
#include <base/nugu_media.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nugu_pcm.h
 */

/**
 * @brief pcm object
 * @ingroup NuguPcm
 */
typedef struct _nugu_pcm NuguPcm;

/**
 * @brief pcm driver object
 * @ingroup NuguPcmDriver
 */
typedef struct _nugu_pcm_driver NuguPcmDriver;

/**
 * @defgroup NuguPcm PCM manipulation
 * @ingroup SDKBase
 * @brief PCM manipulation functions
 *
 * PCM data is managed by pushing it in the nugu buffer.
 *
 * @{
 */

/**
 * @brief Create new pcm object
 * @param[in] name pcm name
 * @param[in] driver driver object
 * @param[in] property audio property
 * @return pcm object
 * @see nugu_pcm_driver_find()
 * @see nugu_pcm_driver_get_default()
 * @see nugu_pcm_free()
 */
NUGU_API NuguPcm *nugu_pcm_new(const char *name, NuguPcmDriver *driver,
			       NuguAudioProperty property);

/**
 * @brief Destroy the pcm object
 * @param[in] pcm pcm object
 * @see nugu_pcm_new()
 */
NUGU_API void nugu_pcm_free(NuguPcm *pcm);

/**
 * @brief Add pcm object to managed list
 * @param[in] pcm pcm object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_pcm_remove()
 * @see nugu_pcm_find()
 */
NUGU_API int nugu_pcm_add(NuguPcm *pcm);

/**
 * @brief Remove pcm object from managed list
 * @param[in] pcm pcm object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_pcm_add()
 * @see nugu_pcm_find()
 */
NUGU_API int nugu_pcm_remove(NuguPcm *pcm);

/**
 * @brief Find a pcm object by name in the managed list
 * @param[in] name name of pcm object
 * @return pcm object
 * @see nugu_pcm_add()
 * @see nugu_pcm_remove()
 */
NUGU_API NuguPcm *nugu_pcm_find(const char *name);

/**
 * @brief Set audio attribute
 * @param[in] pcm pcm object
 * @param[in] attr audio attribute
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
NUGU_API int nugu_pcm_set_audio_attribute(NuguPcm *pcm,
					  NuguAudioAttribute attr);

/**
 * @brief Get audio attribute
 * @param[in] pcm pcm object
 * @return audio attribute
 * @retval -1 failure
 */
NUGU_API int nugu_pcm_get_audio_attribute(NuguPcm *pcm);

/**
 * @brief Get audio attribute
 * @param[in] pcm pcm object
 * @return audio attribute string
 * @retval NULL failure
 */
NUGU_API const char *nugu_pcm_get_audio_attribute_str(NuguPcm *pcm);

/**
 * @brief Start pcm playback
 * @param[in] pcm pcm object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_pcm_stop()
 */
NUGU_API int nugu_pcm_start(NuguPcm *pcm);

/**
 * @brief Stop pcm playback
 * @param[in] pcm pcm object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_pcm_start()
 */
NUGU_API int nugu_pcm_stop(NuguPcm *pcm);

/**
 * @brief Pause pcm playback
 * @param[in] pcm pcm object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_pcm_resume()
 */
NUGU_API int nugu_pcm_pause(NuguPcm *pcm);

/**
 * @brief Resume pcm playback
 * @param[in] pcm pcm object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_pcm_pause()
 */
NUGU_API int nugu_pcm_resume(NuguPcm *pcm);

/**
 * @brief Set volume of pcm
 * @param[in] pcm pcm object
 * @param[in] volume volume
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_pcm_get_volume()
 */
NUGU_API int nugu_pcm_set_volume(NuguPcm *pcm, int volume);

/**
 * @brief Get volume of pcm
 * @param[in] pcm pcm object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_pcm_set_volume()
 */
NUGU_API int nugu_pcm_get_volume(NuguPcm *pcm);

/**
 * @brief Get duration information of pcm
 * @param[in] pcm pcm object
 * @return result
 * @retval >0 success (duration)
 * @retval -1 failure
 */
NUGU_API int nugu_pcm_get_duration(NuguPcm *pcm);

/**
 * @brief Get current playback position of pcm
 * @param[in] pcm pcm object
 * @return result
 * @retval >0 success (position)
 * @retval -1 failure
 */
NUGU_API int nugu_pcm_get_position(NuguPcm *pcm);

/**
 * @brief Get status of pcm
 * @param[in] pcm pcm object
 * @return status
 */
NUGU_API enum nugu_media_status nugu_pcm_get_status(NuguPcm *pcm);

/**
 * @brief Set pcm status callback
 * @param[in] pcm pcm object
 * @param[in] cb callback function
 * @param[in] userdata data to pass to the user callback
 */
NUGU_API void nugu_pcm_set_status_callback(NuguPcm *pcm,
					   NuguMediaStatusCallback cb,
					   void *userdata);

/**
 * @brief Emit status to registered callback
 * @param[in] pcm pcm object
 * @param[in] status pcm status
 */
NUGU_API void nugu_pcm_emit_status(NuguPcm *pcm, enum nugu_media_status status);

/**
 * @brief Set pcm event callback
 * @param[in] pcm pcm object
 * @param[in] cb callback function
 * @param[in] userdata data to pass to the user callback
 */
NUGU_API void nugu_pcm_set_event_callback(NuguPcm *pcm,
					  NuguMediaEventCallback cb,
					  void *userdata);

/**
 * @brief Emit event to registered callback
 * @param[in] pcm pcm object
 * @param[in] event pcm event
 */
NUGU_API void nugu_pcm_emit_event(NuguPcm *pcm, enum nugu_media_event event);

/**
 * @brief Set custom data for driver
 * @param[in] pcm pcm object
 * @param[in] data custom data managed by driver
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_pcm_get_driver_data()
 */
NUGU_API int nugu_pcm_set_driver_data(NuguPcm *pcm, void *data);

/**
 * @brief Get custom data for driver
 * @param[in] pcm pcm object
 * @return data
 * @see nugu_pcm_set_driver_data()
 */
NUGU_API void *nugu_pcm_get_driver_data(NuguPcm *pcm);

/**
 * @brief Clear pcm buffer
 * @param[in] pcm pcm object
 */
NUGU_API void nugu_pcm_clear_buffer(NuguPcm *pcm);

/**
 * @brief Push playback pcm data
 * @param[in] pcm pcm object
 * @param[in] data pcm data
 * @param[in] size length of pcm data
 * @param[in] is_last last data(is_last=1) or not(is_last=0)
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
NUGU_API int nugu_pcm_push_data(NuguPcm *pcm, const char *data, size_t size,
				int is_last);

/**
 * @brief Set flag that push for all data is complete.
 * @param[in] pcm pcm object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_pcm_receive_is_last_data()
 */
NUGU_API int nugu_pcm_push_data_done(NuguPcm *pcm);

/**
 * @brief Get all data
 * @param[in] pcm pcm object
 * @param[out] data buffer to get pcm data
 * @param[in] size size of buffer
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
NUGU_API int nugu_pcm_get_data(NuguPcm *pcm, char *data, size_t size);

/**
 * @brief Get pcm data size
 * @param[in] pcm pcm object
 * @return size of pcm data
 */
NUGU_API size_t nugu_pcm_get_data_size(NuguPcm *pcm);

/**
 * @brief Get flag that all data pushes are complete.
 * @param[in] pcm pcm object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_pcm_push_data_done()
 */
NUGU_API int nugu_pcm_receive_is_last_data(NuguPcm *pcm);

/**
 * @}
 */

/**
 * @defgroup NuguPcmDriver PCM driver
 * @ingroup SDKDriver
 * @brief PCM driver
 *
 * Manage player drivers that can play PCM data.
 *
 * @{
 */

/**
 * @brief pcm driver operations
 * @see nugu_pcm_driver_new()
 */
struct nugu_pcm_driver_ops {
	/**
	 * @brief Called when pcm is created
	 * @see nugu_pcm_new()
	 */
	int (*create)(NuguPcmDriver *driver, NuguPcm *pcm,
		      NuguAudioProperty property);

	/**
	 * @brief Called when pcm is destroyed
	 * @see nugu_pcm_free()
	 */
	void (*destroy)(NuguPcmDriver *driver, NuguPcm *pcm);

	/**
	 * @brief Called when pcm is started
	 * @see nugu_pcm_start()
	 */
	int (*start)(NuguPcmDriver *driver, NuguPcm *pcm);

	/**
	 * @brief Called when a pcm data is pushed to pcm object
	 * @see nugu_pcm_push_data()
	 */
	int (*push_data)(NuguPcmDriver *driver, NuguPcm *pcm, const char *data,
			 size_t size, int is_last);

	/**
	 * @brief Called when pcm is stopped
	 * @see nugu_pcm_stop()
	 */
	int (*stop)(NuguPcmDriver *driver, NuguPcm *pcm);

	/**
	 * @brief Called when pcm is paused
	 * @see nugu_pcm_pause()
	 */
	int (*pause)(NuguPcmDriver *driver, NuguPcm *pcm);

	/**
	 * @brief called when pcm is resumed
	 * @see nugu_pcm_resume()
	 */
	int (*resume)(NuguPcmDriver *driver, NuguPcm *pcm);

	/**
	 * @brief called when pcm is needed to set volume
	 * @see nugu_pcm_resume()
	 */
	int (*set_volume)(NuguPcmDriver *driver, NuguPcm *pcm, int volume);

	/**
	 * @brief Called when a playback position is requested.
	 * @see nugu_pcm_get_position()
	 */
	int (*get_position)(NuguPcmDriver *driver, NuguPcm *pcm);
};

/**
 * @brief Create new pcm driver object
 * @param[in] name driver name
 * @param[in] ops operation table
 * @return pcm driver object
 * @see nugu_pcm_driver_free()
 */
NUGU_API NuguPcmDriver *nugu_pcm_driver_new(const char *name,
					    struct nugu_pcm_driver_ops *ops);

/**
 * @brief Destroy the pcm driver object
 * @param[in] driver pcm driver
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_pcm_driver_new()
 */
NUGU_API int nugu_pcm_driver_free(NuguPcmDriver *driver);

/**
 * @brief Register the driver to driver list
 * @param[in] driver pcm driver
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_pcm_driver_remove()
 */
NUGU_API int nugu_pcm_driver_register(NuguPcmDriver *driver);

/**
 * @brief Remove the driver from driver list
 * @param[in] driver pcm driver
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_pcm_driver_register()
 */
NUGU_API int nugu_pcm_driver_remove(NuguPcmDriver *driver);

/**
 * @brief Set the default pcm driver
 * @param[in] driver pcm driver
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_pcm_driver_get_default()
 */
NUGU_API int nugu_pcm_driver_set_default(NuguPcmDriver *driver);

/**
 * @brief Get the default pcm driver
 * @return pcm driver
 * @see nugu_pcm_driver_set_default()
 */
NUGU_API NuguPcmDriver *nugu_pcm_driver_get_default(void);

/**
 * @brief Find a driver by name in the driver list
 * @param[in] name pcm driver name
 * @return pcm driver
 */
NUGU_API NuguPcmDriver *nugu_pcm_driver_find(const char *name);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
