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

#ifndef __NUGU_UUID_H__
#define __NUGU_UUID_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nugu_uuid.h
 * @defgroup uuid UUID
 * @ingroup SDKCore
 * @brief UUID generation functions
 *
 * @{
 */

/**
 * @brief Generate short type UUID
 * @return memory allocated UUID string. Developer must free the data manually.
 */
char *nugu_uuid_generate_short(void);

/**
 * @brief Generate time based UUID
 * @return memory allocated UUID string. Developer must free the data manually.
 */
char *nugu_uuid_generate_time(void);

/**
 * @brief Parsing the time based UUID and dump to debug log
 * @param[in] uuid time based UUID string
 */
void nugu_dump_timeuuid(const char *uuid);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
