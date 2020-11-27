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

#include "base/nugu_directive.h"

#define TEST_UUID_1 "7fc07dec-9734-4b64-a649-02ced9d341fb"
#define TEST_UUID_2 "7fc07dec-9734-4b64-a649-02ced9d341fc"

int flag;
unsigned char dummy[] = { 1, 2, 3, 4, 5 };

static void on_data(NuguDirective *ndir, int seq, void *userdata)
{
	int ud = GPOINTER_TO_INT(userdata);
	unsigned char *tmp;
	size_t length = 0;

	g_assert(ud == 99);
	g_assert(flag == 0);

	g_assert(nugu_directive_get_data_size(ndir) == sizeof(dummy));

	tmp = nugu_directive_get_data(ndir, &length);
	g_assert(tmp != NULL);
	g_assert(length == sizeof(dummy));
	free(tmp);

	flag = 1;
}

static void test_nugu_directive_callback(void)
{
	NuguDirective *ndir;

	ndir = nugu_directive_new("TTS", "Speak", "1.0", TEST_UUID_1,
				  TEST_UUID_2, TEST_UUID_1, "{}", "{}");
	g_assert(ndir != NULL);

	g_assert(nugu_directive_set_data_callback(ndir, on_data,
						  GINT_TO_POINTER(99)) == 0);

	g_assert(nugu_directive_set_active(ndir, 1) == 1);
	g_assert(nugu_directive_is_active(ndir) == 1);

	g_assert(flag == 0);
	g_assert(nugu_directive_add_data(ndir, sizeof(dummy), dummy) == 0);
	g_assert(flag == 1);

	g_assert(nugu_directive_remove_data_callback(ndir) == 0);
	g_assert(nugu_directive_add_data(ndir, sizeof(dummy), dummy) == 0);
	g_assert(flag == 1);

	nugu_directive_unref(ndir);
}

static void test_nugu_directive_default(void)
{
	NuguDirective *ndir;

	g_assert(nugu_directive_new(NULL, NULL, NULL, NULL, NULL, NULL, NULL,
				    NULL) == NULL);
	g_assert(nugu_directive_new("TTS", NULL, NULL, NULL, NULL, NULL, NULL,
				    NULL) == NULL);
	g_assert(nugu_directive_new(NULL, "Speak", NULL, NULL, NULL, NULL, NULL,
				    NULL) == NULL);
	g_assert(nugu_directive_new(NULL, NULL, "1.0", NULL, NULL, NULL, NULL,
				    NULL) == NULL);
	g_assert(nugu_directive_new(NULL, NULL, NULL, TEST_UUID_1, NULL, NULL,
				    NULL, NULL) == NULL);
	g_assert(nugu_directive_new(NULL, NULL, NULL, NULL, NULL, "{}", NULL,
				    NULL) == NULL);
	g_assert(nugu_directive_new(NULL, NULL, NULL, NULL, NULL, NULL, NULL,
				    "{AudioPlayer.Play}") == NULL);

	ndir = nugu_directive_new("TTS", "Speak", "1.0", TEST_UUID_1,
				  TEST_UUID_2, TEST_UUID_1, "{}", "{}");
	g_assert(ndir != NULL);

	g_assert_cmpstr(nugu_directive_peek_namespace(ndir), ==, "TTS");
	g_assert_cmpstr(nugu_directive_peek_name(ndir), ==, "Speak");
	g_assert_cmpstr(nugu_directive_peek_version(ndir), ==, "1.0");
	g_assert_cmpstr(nugu_directive_peek_msg_id(ndir), ==, TEST_UUID_1);
	g_assert_cmpstr(nugu_directive_peek_dialog_id(ndir), ==, TEST_UUID_2);
	g_assert_cmpstr(nugu_directive_peek_json(ndir), ==, "{}");

	g_assert(nugu_directive_is_active(ndir) == 0);
	g_assert(nugu_directive_set_active(ndir, -1) == -1);
	g_assert(nugu_directive_set_active(ndir, 0) == 0);
	g_assert(nugu_directive_set_active(ndir, 1) == 1);
	g_assert(nugu_directive_is_active(ndir) == 1);

	g_assert(nugu_directive_get_data_size(ndir) == 0);
	g_assert(nugu_directive_get_data(ndir, NULL) == NULL);

	g_assert(nugu_directive_is_data_end(ndir) == 0);
	g_assert(nugu_directive_close_data(ndir) == 0);
	g_assert(nugu_directive_is_data_end(ndir) == 1);

	g_assert(nugu_directive_set_blocking_policy(
			 NULL, NUGU_DIRECTIVE_MEDIUM_ANY, 1) == -1);

	/* default policy: NONE / non-block */
	g_assert(nugu_directive_get_blocking_medium(ndir) ==
		 NUGU_DIRECTIVE_MEDIUM_NONE);
	g_assert(nugu_directive_is_blocking(ndir) == 0);

	/* set/get policy */
	g_assert(nugu_directive_set_blocking_policy(
			 ndir, NUGU_DIRECTIVE_MEDIUM_ANY, 1) == 0);
	g_assert(nugu_directive_get_blocking_medium(ndir) ==
		 NUGU_DIRECTIVE_MEDIUM_ANY);
	g_assert_cmpstr(nugu_directive_get_blocking_medium_string(ndir), ==,
			"ANY");
	g_assert(nugu_directive_is_blocking(ndir) == 1);

	nugu_directive_unref(ndir);
}

int main(int argc, char *argv[])
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
	g_type_init();
#endif

	g_test_init(&argc, &argv, NULL);
	g_log_set_always_fatal((GLogLevelFlags)G_LOG_FATAL_MASK);

	g_test_add_func("/nugu_directive/default", test_nugu_directive_default);
	g_test_add_func("/nugu_directive/callback",
			test_nugu_directive_callback);

	return g_test_run();
}
