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

#ifndef __MEDIA_PLAYER_MOCK_H__
#define __MEDIA_PLAYER_MOCK_H__

#include "base/nugu_player.h"

#ifdef __cplusplus
extern "C" {
#endif

int test_player_driver_initialize(void);
void test_player_driver_deinitialize(void);
void test_player_notify_event(NuguPlayer *player, enum nugu_media_event event);
int test_player_set_duration(NuguPlayer *player, int duration);
int test_player_set_position(NuguPlayer *player, int position);

#ifdef __cplusplus
}
#endif

#endif // __MEDIA_PLAYER_MOCK_H__
