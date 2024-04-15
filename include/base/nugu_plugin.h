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

#ifndef __NUGU_PLUGIN_H__
#define __NUGU_PLUGIN_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nugu_plugin.h
 * @defgroup NuguPlugin Plugin
 * @ingroup SDKBase
 * @brief Plugin management
 *
 * The plugin is a feature provided to extend the functionality of sdk.
 * The plugin must have a shared library type that contains the
 * 'nugu_plugin_define_desc' symbol.
 * And there should be no 'lib' prefix in file name.
 *
 * @{
 */

/**
 * @brief A value representing high priority.
 */
#define NUGU_PLUGIN_PRIORITY_HIGH 10

/**
 * @brief A value representing default priority.
 */
#define NUGU_PLUGIN_PRIORITY_DEFAULT 100

/**
 * @brief A value representing low priority.
 */
#define NUGU_PLUGIN_PRIORITY_LOW 900

/**
 * @brief Default symbol name for dlsym()
 */
#define NUGU_PLUGIN_SYMBOL "nugu_plugin_define_desc"

/**
 * @brief Macros to easily define plugins
 */
#ifdef NUGU_PLUGIN_BUILTIN
#define NUGU_PLUGIN_DEFINE(p_name, p_prio, p_ver, p_load, p_unload, p_init)    \
	__attribute__((visibility("default"))) struct nugu_plugin_desc         \
		_builtin_plugin_##p_name = { .name = #p_name,                  \
					     .priority = p_prio,               \
					     .version = p_ver,                 \
					     .load = p_load,                   \
					     .unload = p_unload,               \
					     .init = p_init }
#else
#define NUGU_PLUGIN_DEFINE(p_name, p_prio, p_ver, p_load, p_unload, p_init)    \
	__attribute__((visibility("default"))) struct nugu_plugin_desc         \
		nugu_plugin_define_desc = { .name = #p_name,                   \
					    .priority = p_prio,                \
					    .version = p_ver,                  \
					    .load = p_load,                    \
					    .unload = p_unload,                \
					    .init = p_init }
#endif

/**
 * @brief Plugin object
 */
typedef struct _plugin NuguPlugin;

/**
 * @brief Plugin description
 */
struct nugu_plugin_desc {
	/**
	 * @brief Name of plugin
	 */
	const char *name;

	/**
	 * @brief Priority used in the init call order.
	 */
	unsigned int priority;

	/**
	 * @brief Version of plugin
	 */
	const char *version;

	/**
	 * @brief Function to be called after loading.
	 */
	int (*load)(void);

	/**
	 * @brief Function to be called after unloading.
	 */
	void (*unload)(NuguPlugin *p);

	/**
	 * @brief Function called by priority after all plugins have
	 * finished loading.
	 */
	int (*init)(NuguPlugin *p);
};

/**
 * @brief Create new plugin object
 * @param[in] desc plugin description
 * @return plugin object
 * @see nugu_plugin_new_from_file()
 * @see nugu_plugin_free()
 */
NuguPlugin *nugu_plugin_new(struct nugu_plugin_desc *desc);

/**
 * @brief Create new plugin object from file
 * @param[in] filepath plugin file path
 * @return plugin object
 * @see nugu_plugin_new()
 * @see nugu_plugin_free()
 */
NuguPlugin *nugu_plugin_new_from_file(const char *filepath);

/**
 * @brief Destroy the plugin
 * @param[in] p plugin object
 */
void nugu_plugin_free(NuguPlugin *p);

/**
 * @brief Add the plugin to managed list
 * @param[in] p plugin object
 * @return result
 * @retval 0 success
 * @retval 1 success (plugin is already registered)
 * @retval -1 failure (another plugin using the same file is already registered)
 * @see nugu_plugin_find()
 * @see nugu_plugin_remove()
 */
int nugu_plugin_add(NuguPlugin *p);

/**
 * @brief Remove the plugin to managed list
 * @param[in] p plugin object
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_plugin_add()
 * @see nugu_plugin_find()
 */
int nugu_plugin_remove(NuguPlugin *p);

/**
 * @brief Find a plugin by name in the managed list
 * @param[in] name name of plugin
 * @return plugin object
 * @see nugu_plugin_add()
 * @see nugu_plugin_remove()
 */
NuguPlugin *nugu_plugin_find(const char *name);

/**
 * @brief Set private data to plugin
 * @param[in] p plugin object
 * @param[in] data plugin specific data
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_plugin_get_data()
 */
int nugu_plugin_set_data(NuguPlugin *p, void *data);

/**
 * @brief Get private data from plugin
 * @param[in] p plugin object
 * @return data
 * @see nugu_plugin_set_data()
 */
void *nugu_plugin_get_data(NuguPlugin *p);

/**
 * @brief Get dlsym result from plugin
 * @param[in] p plugin object
 * @param[in] symbol_name symbol name to find
 * @return symbol address
 */
void *nugu_plugin_get_symbol(NuguPlugin *p, const char *symbol_name);

/**
 * @brief Get the plugin description
 * @param[in] p plugin object
 * @return plugin description
 */
const struct nugu_plugin_desc *nugu_plugin_get_description(NuguPlugin *p);

/**
 * @brief Load all plugin files from directory
 * @param[in] dirpath directory path
 * @return Number of plugins loaded
 * @retval -1 failure
 */
int nugu_plugin_load_directory(const char *dirpath);

/**
 * @brief Load all built-in plugins
 * @return Number of plugins loaded
 * @retval -1 failure
 */
int nugu_plugin_load_builtin(void);

/**
 * @brief Initialize plugin
 * @return Number of plugins initialized
 */
int nugu_plugin_initialize(void);

/**
 * @brief De-initialize plugin
 */
void nugu_plugin_deinitialize(void);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
