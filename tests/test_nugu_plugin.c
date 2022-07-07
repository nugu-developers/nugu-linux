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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "base/nugu_plugin.h"

static struct nugu_plugin_desc test_plugin_desc = {
	.name = "test",
	.priority = NUGU_PLUGIN_PRIORITY_DEFAULT,
	.version = "0.1",
	.load = NULL,
	.unload = NULL,
	.init = NULL
};

static int load_ok(void)
{
	return 0;
}

static int load_fail(void)
{
	return -1;
}

static int init_ok(NuguPlugin *p)
{
	char *buf;

	buf = nugu_plugin_get_data(p);
	if (!buf)
		return 0;

	snprintf(buf + strlen(buf), 10, "%s",
		 nugu_plugin_get_description(p)->name);

	return 0;
}

static int init_fail(NuguPlugin *p)
{
	return -1;
}

static struct nugu_plugin_desc p1_desc = { .name = "p1",
					   .priority =
						   NUGU_PLUGIN_PRIORITY_DEFAULT,
					   .version = "0.1",
					   .load = load_ok,
					   .unload = NULL,
					   .init = init_ok };

static struct nugu_plugin_desc p2_desc = {
	.name = "p2",
	.priority = NUGU_PLUGIN_PRIORITY_DEFAULT - 1,
	.version = "0.1",
	.load = load_ok,
	.unload = NULL,
	.init = init_ok
};

static struct nugu_plugin_desc p3_desc = {
	.name = "p3",
	.priority = NUGU_PLUGIN_PRIORITY_DEFAULT + 1,
	.version = "0.1",
	.load = load_ok,
	.unload = NULL,
	.init = init_ok
};

static void test_plugin_priority(void)
{
	NuguPlugin *p1;
	NuguPlugin *p2;
	NuguPlugin *p3;
	char check[10];

	check[0] = '\0';

	p1 = nugu_plugin_new(&p1_desc);
	g_assert(p1 != NULL);
	g_assert(nugu_plugin_add(p1) == 0);
	g_assert(nugu_plugin_set_data(p1, check) == 0);

	p2 = nugu_plugin_new(&p2_desc);
	g_assert(p1 != NULL);
	g_assert(nugu_plugin_add(p2) == 0);
	g_assert(nugu_plugin_set_data(p2, check) == 0);

	p3 = nugu_plugin_new(&p3_desc);
	g_assert(p1 != NULL);
	g_assert(nugu_plugin_add(p3) == 0);
	g_assert(nugu_plugin_set_data(p3, check) == 0);

	g_assert(nugu_plugin_find("p1") == p1);
	g_assert(nugu_plugin_find("p2") == p2);
	g_assert(nugu_plugin_find("p3") == p3);

	/*
	 * The order in which the plug-ins are added is p1, p2, p3, but the
	 * initialization order is p2, p1, p3 according to their priority.
	 */
	nugu_plugin_initialize();

	g_assert_cmpstr(check, ==, "p2p1p3");

	nugu_plugin_deinitialize();
}

static void test_plugin_init_fail(void)
{
	NuguPlugin *p;

	test_plugin_desc.load = load_ok;
	test_plugin_desc.init = init_fail;

	p = nugu_plugin_new(&test_plugin_desc);
	g_assert(p != NULL);
	g_assert(nugu_plugin_add(p) == 0);

	nugu_plugin_initialize();

	/*
	 * The 'p' plugin must be deallocated and removed automatically during
	 * initialize step due to return -1 from init_fail().
	 */
	g_assert(nugu_plugin_remove(p) == -1);

	nugu_plugin_deinitialize();
}

static void test_plugin_load(void)
{
	NuguPlugin *plugin;
	NuguPlugin *plugin2;
	int (*custom_add)(int a, int b);

	/* File does not exist. */
#define PLUGIN_NOFILE RUNPATH "/xxx.so"
	g_assert(nugu_plugin_new_from_file(PLUGIN_NOFILE) == NULL);

	/* Not the nugu plugin (can't get pre-defined symbol) */
#define PLUGIN_DUMMY RUNPATH "/plugin_dummy.so"
	g_assert(nugu_plugin_new_from_file(PLUGIN_DUMMY) == NULL);

	/* nugu plugin */
#define PLUGIN_NUGU RUNPATH "/plugin_nugu.so"
	plugin = nugu_plugin_new_from_file(PLUGIN_NUGU);
	g_assert(plugin != NULL);

	/* There is no 'custom_add' symbol in this plugin. */
	g_assert(nugu_plugin_get_symbol(plugin, "custom_add") == NULL);

	g_assert(nugu_plugin_add(plugin) == 0);
	g_assert(nugu_plugin_add(plugin) == 1);

	/* Attempt to add plugin with same file path */
	plugin2 = nugu_plugin_new_from_file(PLUGIN_NUGU);
	g_assert(plugin2 != NULL);

	g_assert(nugu_plugin_add(plugin2) == -1);
	nugu_plugin_free(plugin2);

	g_assert(nugu_plugin_remove(plugin) == 0);
	nugu_plugin_free(plugin);

	/* nugu plugin with custom symbol */
#define PLUGIN_NUGU_CUSTOM RUNPATH "/plugin_nugu_custom.so"
	plugin = nugu_plugin_new_from_file(PLUGIN_NUGU_CUSTOM);
	g_assert(plugin != NULL);

	custom_add = nugu_plugin_get_symbol(plugin, "custom_add");
	g_assert(custom_add != NULL);
	g_assert(custom_add(1, 5) == 6);

	nugu_plugin_free(plugin);

	/* Load all plugins */
	nugu_plugin_initialize();

	nugu_plugin_load_directory(RUNPATH);

	g_assert(nugu_plugin_find("xxx") == NULL);
	g_assert(nugu_plugin_find("test_nugu") != NULL);

	plugin = nugu_plugin_find("test_nugu_custom");
	g_assert(plugin != NULL);
	custom_add = nugu_plugin_get_symbol(plugin, "custom_add");
	g_assert(custom_add != NULL);
	g_assert(custom_add(2, 6) == 8);

	nugu_plugin_deinitialize();
}

static void test_plugin_default(void)
{
	NuguPlugin *p;

	g_assert(nugu_plugin_new(NULL) == NULL);
	g_assert(nugu_plugin_new(&test_plugin_desc) == NULL);

	test_plugin_desc.load = load_fail;
	test_plugin_desc.init = init_fail;
	g_assert(nugu_plugin_new(&test_plugin_desc) == NULL);

	test_plugin_desc.load = load_ok;
	test_plugin_desc.init = init_ok;
	p = nugu_plugin_new(&test_plugin_desc);
	g_assert(p != NULL);

	nugu_plugin_initialize();

	g_assert(nugu_plugin_find("test") == NULL);
	g_assert(nugu_plugin_add(p) == 0);
	g_assert(nugu_plugin_add(p) == 1);
	g_assert(nugu_plugin_find("test") == p);

	nugu_plugin_deinitialize();

	g_assert(nugu_plugin_find("test") == NULL);
}

int main(int argc, char *argv[])
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
	g_type_init();
#endif

	g_test_init(&argc, &argv, NULL);
	g_log_set_always_fatal((GLogLevelFlags)G_LOG_FATAL_MASK);

	g_test_add_func("/plugin/default", test_plugin_default);
	g_test_add_func("/plugin/init_fail", test_plugin_init_fail);
	g_test_add_func("/plugin/priority", test_plugin_priority);
	g_test_add_func("/plugin/load", test_plugin_load);

	return g_test_run();
}
