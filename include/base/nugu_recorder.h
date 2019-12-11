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

#ifndef __NUGU_RECORDER_H__
#define __NUGU_RECORDER_H__

#include <base/nugu_audio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nugu_recorder.h
 */

/**
 * @brief recorder default frame size
 */
#define NUGU_RECORDER_FRAME_SIZE 512

/**
 * @brief recorder max frame size
 */
#define NUGU_RECORDER_MAX_FRAMES 100

/**
 * @brief recorder buffer size
 */
#define NUGU_RECORDER_BUFFER_SIZE                                              \
	(NUGU_RECORDER_FRAME_SIZE * NUGU_RECORDER_MAX_FRAMES)

/**
 * @brief recorder object
 * @ingroup NuguRecorder
 */
typedef struct _nugu_recorder NuguRecorder;

/**
 * @brief recorder driver object
 * @ingroup NuguRecorderDriver
 */
typedef struct _nugu_recorder_driver NuguRecorderDriver;

/**
 * @defgroup NuguRecorder Voice recorder
 * @ingroup SDKBase
 * @brief Voice recorder functions
 *
 * The recorder manages the recorded audio data by pushing it to the ringbuffer.
 *
 * @{
 */

/**
 * @brief Create new recorder object
 * @param[in] name recorder name
 * @param[in] driver recorder driver object
 * @return recorder object
 * @see nugu_recorder_free()
 */
NuguRecorder *nugu_recorder_new(const char *name, NuguRecorderDriver *driver);

/**
 * @brief Destroy the recorder object
 * @param[in] rec recorder object
 * @see nugu_recorder_new()
 */
void nugu_recorder_free(NuguRecorder *rec);

/**
 * @brief Add recorder object to managed list
 * @param[in] rec recorder object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_recorder_remove()
 * @see nugu_recorder_find()
 */
int nugu_recorder_add(NuguRecorder *rec);

/**
 * @brief Remove recorder object from managed list
 * @param[in] rec recorder object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_recorder_remove(NuguRecorder *rec);

/**
 * @brief Find a recorder object by name in the managed list
 * @param[in] name name of recorder object
 * @return recorder object
 * @see nugu_recorder_add()
 * @see nugu_recorder_remove()
 */
NuguRecorder *nugu_recorder_find(const char *name);

/**
 * @brief Set property to recorder object
 * @param[in] rec recorder object
 * @param[in] property property
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_recorder_set_property(NuguRecorder *rec, NuguAudioProperty property);

/**
 * @brief Start recording
 * @param[in] rec recorder object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_recorder_stop()
 */
int nugu_recorder_start(NuguRecorder *rec);

/**
 * @brief Stop recording
 * @param[in] rec recorder object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_recorder_start()
 */
int nugu_recorder_stop(NuguRecorder *rec);

/**
 * @brief Get the status of recording
 * @param[in] rec recorder object
 * @return result
 * @retval 0 idle
 * @retval 1 recording
 * @retval -1 failure
 */
int nugu_recorder_is_recording(NuguRecorder *rec);

/**
 * @brief Set private userdata for driver
 * @param[in] rec recorder object
 * @param[in] userdata userdata managed by driver
 * @see nugu_recorder_get_userdata()
 */
void nugu_recorder_set_userdata(NuguRecorder *rec, void *userdata);

/**
 * @brief Get private userdata for driver
 * @param[in] rec recorder object
 * @return userdata
 * @see nugu_recorder_set_userdata()
 */
void *nugu_recorder_get_userdata(NuguRecorder *rec);

/**
 * @brief Get frame size
 * @param[in] rec recorder object
 * @param[out] size frame size
 * @param[out] max max count
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_recorder_set_frame_size()
 */
int nugu_recorder_get_frame_size(NuguRecorder *rec, int *size, int *max);

/**
 * @brief Set frame size
 * @param[in] rec recorder object
 * @param[in] size frame size
 * @param[in] max max count
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_recorder_get_frame_size()
 */
int nugu_recorder_set_frame_size(NuguRecorder *rec, int size, int max);

/**
 * @brief Push recorded data
 * @param[in] rec recorder object
 * @param[in] data recorded data
 * @param[in] size size of recorded data
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_recorder_push_frame(NuguRecorder *rec, const char *data, int size);

/**
 * @brief Get recorded data
 * @param[in] rec recorder object
 * @param[out] data data
 * @param[out] size size of data
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_recorder_get_frame(NuguRecorder *rec, char *data, int *size);

/**
 * @brief Get recorded data with timeout
 * @param[in] rec recorder object
 * @param[out] data data
 * @param[out] size size of data
 * @param[in] timeout timeout milliseconds
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_recorder_get_frame_timeout(NuguRecorder *rec, char *data, int *size,
				    int timeout);

/**
 * @brief Get frame count
 * @param[in] rec recorder object
 * @return result
 * @retval >0 success (frame count)
 * @retval -1 failure
 */
int nugu_recorder_get_frame_count(NuguRecorder *rec);

/**
 * @}
 */

/**
 * @defgroup NuguRecorderDriver Audio recorder driver
 * @ingroup SDKDriver
 * @brief Audio recorder driver
 *
 * The recorder driver must support the ability to record in multiples
 * according to the Audio property (frame, samplerate, channel).
 *
 * @{
 */
struct nugu_recorder_driver_ops {
	/**
	 * @brief Called when recording is started
	 * @see nugu_recorder_start()
	 */
	int (*start)(NuguRecorderDriver *driver, NuguRecorder *rec,
		     NuguAudioProperty property);

	/**
	 * @brief Called when recording is stopped
	 * @see nugu_recorder_stop()
	 */
	int (*stop)(NuguRecorderDriver *driver, NuguRecorder *rec);
};

/**
 * @brief Create new recorder driver object
 * @param[in] name driver name
 * @param[in] ops operation table
 * @return recorder driver object
 * @see nugu_recorder_driver_free()
 */
NuguRecorderDriver *
nugu_recorder_driver_new(const char *name,
			 struct nugu_recorder_driver_ops *ops);

/**
 * @brief Destroy the recorder driver object
 * @param[in] driver recorder driver
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_recorder_driver_new()
 */
int nugu_recorder_driver_free(NuguRecorderDriver *driver);

/**
 * @brief Register the driver to driver list
 * @param[in] driver recorder driver
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_recorder_driver_remove()
 */
int nugu_recorder_driver_register(NuguRecorderDriver *driver);

/**
 * @brief Remove the driver from driver list
 * @param[in] driver recorder driver
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_recorder_driver_register()
 */
int nugu_recorder_driver_remove(NuguRecorderDriver *driver);

/**
 * @brief Set the default recorder driver
 * @param[in] driver recorder driver
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_recorder_driver_get_default()
 */
int nugu_recorder_driver_set_default(NuguRecorderDriver *driver);

/**
 * @brief Get the default recorder driver
 * @return recorder driver
 * @see nugu_recorder_driver_set_default()
 */
NuguRecorderDriver *nugu_recorder_driver_get_default(void);

/**
 * @brief Find a driver by name in the driver list
 * @param[in] name recorder driver name
 * @return recorder driver
 */
NuguRecorderDriver *nugu_recorder_driver_find(const char *name);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
