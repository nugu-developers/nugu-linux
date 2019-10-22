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

#ifndef __NUGU_CONFIG_H__
#define __NUGU_CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nugu_config.h
 * @defgroup nugu_config Configuration
 * @ingroup SDKCore
 * @brief String type Key-Value store
 *
 * @{
 */

/**
 * @brief Predefined key name for token
 */
#define NUGU_CONFIG_KEY_TOKEN "token"

/**
 * @brief Predefined key name for user_agent
 */
#define NUGU_CONFIG_KEY_USER_AGENT "user_agent"

/**
 * @brief Predefined key name for gateway_registry_dns
 */
#define NUGU_CONFIG_KEY_GATEWAY_REGISTRY_DNS "gateway_registry_dns"

/**
 * @brief Initialize configuration hash table
 */
void nugu_config_initialize(void);

/**
 * @brief Destroy all configurations
 */
void nugu_config_deinitialize(void);

/**
 * @brief Get the value of configuration key
 * @param[in] key key name
 * @return string value
 */
const char *nugu_config_get(const char *key);

/**
 * @brief Store key and value to configuration
 * @param[in] key key name
 * @param[in] value  value
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_config_set(const char *key, const char *value);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
