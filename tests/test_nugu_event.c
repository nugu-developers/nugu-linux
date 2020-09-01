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

#include "base/nugu_event.h"

#define TEST_UUID "7fc07dec-9734-4b64-a649-02ced9d341fb"

static void test_nugu_event_default(void)
{
	NuguEvent *ev;
	char *payload;

	g_assert(nugu_event_new(NULL, NULL, NULL) == NULL);
	g_assert(nugu_event_new("ASR", NULL, NULL) == NULL);
	g_assert(nugu_event_new(NULL, "Recognize", NULL) == NULL);

	ev = nugu_event_new("ASR", "Recognize", "1.0");
	g_assert(ev != NULL);

	g_assert_cmpstr(nugu_event_peek_namespace(ev), ==, "ASR");
	g_assert_cmpstr(nugu_event_peek_name(ev), ==, "Recognize");
	g_assert_cmpstr(nugu_event_peek_version(ev), ==, "1.0");

	/* msg_id != dialog_id */
	g_assert(nugu_event_peek_msg_id(ev) != NULL);
	g_assert(nugu_event_peek_dialog_id(ev) != NULL);
	g_assert_cmpstr(nugu_event_peek_msg_id(ev), !=,
			nugu_event_peek_dialog_id(ev));

	/* dialog_id is mandatory */
	g_assert(nugu_event_set_dialog_id(ev, "1234") == 0);
	g_assert(nugu_event_set_dialog_id(ev, TEST_UUID) == 0);
	g_assert_cmpstr(nugu_event_peek_dialog_id(ev), ==, TEST_UUID);
	g_assert(nugu_event_set_dialog_id(ev, NULL) == -1);
	g_assert(nugu_event_peek_dialog_id(ev) != NULL);
	g_assert_cmpstr(nugu_event_peek_dialog_id(ev), ==, TEST_UUID);

	/* context is mandatory */
	g_assert(nugu_event_set_context(ev, "{1}") == 0);
	g_assert(nugu_event_set_context(ev, "{2}") == 0);
	g_assert_cmpstr(nugu_event_peek_context(ev), ==, "{2}");
	g_assert(nugu_event_set_context(ev, NULL) == -1);
	g_assert(nugu_event_peek_context(ev) != NULL);
	g_assert_cmpstr(nugu_event_peek_context(ev), ==, "{2}");

	/* json is optional */
	g_assert(nugu_event_peek_json(ev) == NULL);
	g_assert(nugu_event_set_json(ev, "{1}") == 0);
	g_assert(nugu_event_set_json(ev, "{2}") == 0);
	g_assert_cmpstr(nugu_event_peek_json(ev), ==, "{2}");
	g_assert(nugu_event_set_json(ev, NULL) == 0);
	g_assert(nugu_event_peek_json(ev) == NULL);

	/* referrer_id is optional */
	g_assert(nugu_event_peek_referrer_id(ev) == NULL);
	g_assert(nugu_event_set_referrer_id(ev, "1234") == 0);
	g_assert(nugu_event_set_referrer_id(ev, "5678") == 0);
	g_assert_cmpstr(nugu_event_peek_referrer_id(ev), ==, "5678");
	g_assert(nugu_event_set_referrer_id(ev, NULL) == 0);
	g_assert(nugu_event_peek_referrer_id(ev) == NULL);

	g_assert(nugu_event_get_seq(ev) == 0);
	g_assert(nugu_event_increase_seq(ev) == 1);
	g_assert(nugu_event_get_seq(ev) == 1);
	g_assert(nugu_event_increase_seq(ev) == 2);
	g_assert(nugu_event_get_seq(ev) == 2);

	g_assert(nugu_event_get_type(ev) == NUGU_EVENT_TYPE_DEFAULT);
	g_assert(nugu_event_set_type(ev, NUGU_EVENT_TYPE_WITH_ATTACHMENT) == 0);
	g_assert(nugu_event_get_type(ev) == NUGU_EVENT_TYPE_WITH_ATTACHMENT);
	g_assert(nugu_event_set_type(ev, NUGU_EVENT_TYPE_DEFAULT) == 0);
	g_assert(nugu_event_get_type(ev) == NUGU_EVENT_TYPE_DEFAULT);

	payload = nugu_event_generate_payload(ev);
	g_assert(payload != NULL);
	free(payload);

	nugu_event_free(ev);
}

int main(int argc, char *argv[])
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
	g_type_init();
#endif

	g_test_init(&argc, &argv, NULL);
	g_log_set_always_fatal((GLogLevelFlags)G_LOG_FATAL_MASK);

	g_test_add_func("/nugu_event/default", test_nugu_event_default);

	return g_test_run();
}
