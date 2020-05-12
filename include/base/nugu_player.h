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

#ifndef __NUGU_PLAYER_H__
#define __NUGU_PLAYER_H__

#include <base/nugu_audio.h>
#include <base/nugu_media.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nugu_player.h
 */

/**
 * @brief player object
 * @ingroup NuguPlayer
 */
typedef struct _nugu_player NuguPlayer;

/**
 * @brief player driver object
 * @ingroup NuguPlayerDriver
 */
typedef struct _nugu_player_driver NuguPlayerDriver;

/**
 * @defgroup NuguPlayer Media player
 * @ingroup SDKBase
 * @brief Media player functions
 *
 * Manage the functions for media content playback.
 *
 * @{
 */

/**
 * @brief Create new player object
 * @param[in] name name of player
 * @param[in] driver player driver
 * @return player object
 * @see nugu_player_free()
 */
NuguPlayer *nugu_player_new(const char *name, NuguPlayerDriver *driver);

/**
 * @brief Destroy the player object
 * @param[in] player player object
 * @see nugu_player_new()
 */
void nugu_player_free(NuguPlayer *player);

/**
 * @brief Add player object to managed list
 * @param[in] player player object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_player_find()
 * @see nugu_player_remove()
 */
int nugu_player_add(NuguPlayer *player);

/**
 * @brief Remove player object from managed list
 * @param[in] player player object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_player_find()
 * @see nugu_player_add()
 */
int nugu_player_remove(NuguPlayer *player);

/**
 * @brief Find a player object by name in the managed list
 * @param[in] name name of player object
 * @return player object
 * @see nugu_player_add()
 * @see nugu_player_remove()
 */
NuguPlayer *nugu_player_find(const char *name);

/**
 * @brief Set source url
 * @param[in] player player object
 * @param[in] url source url
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_player_set_source(NuguPlayer *player, const char *url);

/**
 * @brief Start the player
 * @param[in] player player object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_player_stop()
 * @see nugu_player_pause()
 */
int nugu_player_start(NuguPlayer *player);

/**
 * @brief Stop the player
 * @param[in] player player object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_player_start()
 */
int nugu_player_stop(NuguPlayer *player);

/**
 * @brief Pause the player
 * @param[in] player player object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_player_resume()
 */
int nugu_player_pause(NuguPlayer *player);

/**
 * @brief Resume the player
 * @param[in] player player object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_player_pause()
 * @see nugu_player_stop()
 */
int nugu_player_resume(NuguPlayer *player);

/**
 * @brief Seek the player
 * @param[in] player player object
 * @param[in] sec position in seconds
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_player_seek(NuguPlayer *player, int sec);

/**
 * @brief Set volume of player
 * @param[in] player player object
 * @param[in] vol volume value
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_player_get_volume()
 */
int nugu_player_set_volume(NuguPlayer *player, int vol);

/**
 * @brief Get volume of player
 * @param[in] player player object
 * @return result
 * @retval >0 success (volume)
 * @retval -1 failure
 * @see nugu_player_set_volume()
 */
int nugu_player_get_volume(NuguPlayer *player);

/**
 * @brief Get duration information of player
 * @param[in] player player object
 * @return result
 * @retval >0 success (duration)
 * @retval -1 failure
 */
int nugu_player_get_duration(NuguPlayer *player);

/**
 * @brief Get current playback position of player
 * @param[in] player player object
 * @return result
 * @retval >0 success (position)
 * @retval -1 failure
 */
int nugu_player_get_position(NuguPlayer *player);

/**
 * @brief Get status of player
 * @param[in] player player object
 * @return status
 */
enum nugu_media_status nugu_player_get_status(NuguPlayer *player);

/**
 * @brief Set player status callback
 * @param[in] player player object
 * @param[in] cb callback function
 * @param[in] userdata data to pass to the user callback
 */
void nugu_player_set_status_callback(NuguPlayer *player,
				     NuguMediaStatusCallback cb,
				     void *userdata);

/**
 * @brief Emit status to registered callback
 * @param[in] player player object
 * @param[in] status player status
 */
void nugu_player_emit_status(NuguPlayer *player, enum nugu_media_status status);

/**
 * @brief Set player event callback
 * @param[in] player player object
 * @param[in] cb callback function
 * @param[in] userdata data to pass to the user callback
 */
void nugu_player_set_event_callback(NuguPlayer *player,
				    NuguMediaEventCallback cb, void *userdata);

/**
 * @brief Emit event to registered callback
 * @param[in] player player object
 * @param[in] event player event
 */
void nugu_player_emit_event(NuguPlayer *player, enum nugu_media_event event);


/**
 * @brief Set custom data for driver
 * @param[in] player player object
 * @param[in] data custom data managed by driver
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_player_get_driver_data()
 */
int nugu_player_set_driver_data(NuguPlayer *player, void *data);

/**
 * @brief Get custom data for driver
 * @param[in] player player object
 * @return data
 * @see nugu_player_set_driver_data()
 */
void *nugu_player_get_driver_data(NuguPlayer *player);

/**
 * @}
 */

/**
 * @defgroup NuguPlayerDriver Media player driver
 * @ingroup SDKDriver
 * @brief Media player driver
 *
 * Manage player drivers that can play media content.
 * The player must support various protocols (http, https, hls, file, etc.).
 *
 * @{
 */

/**
 * @brief player driver operations
 * @see nugu_player_driver_new()
 */
struct nugu_player_driver_ops {
	/**
	 * @brief Called when player is created
	 * @see nugu_player_new()
	 */
	int (*create)(NuguPlayerDriver *driver, NuguPlayer *player);

	/**
	 * @brief Called when player is destroyed
	 * @see nugu_player_free()
	 */
	void (*destroy)(NuguPlayerDriver *driver, NuguPlayer *player);

	/**
	 * @brief Called when set the player source
	 * @see nugu_player_set_source()
	 */
	int (*set_source)(NuguPlayerDriver *driver, NuguPlayer *player,
			  const char *url);

	/**
	 * @brief Called when player is started
	 * @see nugu_player_start()
	 */
	int (*start)(NuguPlayerDriver *driver, NuguPlayer *player);

	/**
	 * @brief Called when player is stopped
	 * @see nugu_player_stop()
	 */
	int (*stop)(NuguPlayerDriver *driver, NuguPlayer *player);

	/**
	 * @brief Called when player is paused
	 * @see nugu_player_pause()
	 */
	int (*pause)(NuguPlayerDriver *driver, NuguPlayer *player);

	/**
	 * @brief Called when player is resumed
	 * @see nugu_player_resume()
	 */
	int (*resume)(NuguPlayerDriver *driver, NuguPlayer *player);

	/**
	 * @brief Called when playback position is changed by seek
	 * @see nugu_player_seek()
	 */
	int (*seek)(NuguPlayerDriver *driver, NuguPlayer *player, int sec);

	/**
	 * @brief Called when volume is changed
	 * @see nugu_player_set_volume()
	 */
	int (*set_volume)(NuguPlayerDriver *driver, NuguPlayer *player,
			  int vol);

	/**
	 * @brief Called when a playback duration is requested.
	 * @see nugu_player_get_duration()
	 */
	int (*get_duration)(NuguPlayerDriver *driver, NuguPlayer *player);

	/**
	 * @brief Called when a playback position is requested.
	 * @see nugu_player_get_position()
	 */
	int (*get_position)(NuguPlayerDriver *driver, NuguPlayer *player);
};

/**
 * @brief Create new player driver object
 * @param[in] name driver name
 * @param[in] ops operation table
 * @return player driver object
 */
NuguPlayerDriver *nugu_player_driver_new(const char *name,
					 struct nugu_player_driver_ops *ops);

/**
 * @brief Destroy the player driver object
 * @param[in] driver player driver
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_player_driver_free(NuguPlayerDriver *driver);

/**
 * @brief Register the driver to driver list
 * @param[in] driver player driver
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_player_driver_register(NuguPlayerDriver *driver);

/**
 * @brief Remove the driver from driver list
 * @param[in] driver player driver
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_player_driver_remove(NuguPlayerDriver *driver);

/**
 * @brief Set the default player driver
 * @param[in] driver player driver
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_player_driver_set_default(NuguPlayerDriver *driver);

/**
 * @brief Get the default player driver
 * @return player driver
 */
NuguPlayerDriver *nugu_player_driver_get_default(void);

/**
 * @brief Find a driver by name in the driver list
 * @param[in] name player driver name
 * @return player driver
 */
NuguPlayerDriver *nugu_player_driver_find(const char *name);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
