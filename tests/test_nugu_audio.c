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

#include "base/nugu_audio.h"

static void test_audio_default(void)
{
	// read default audio attribute string
	g_assert_cmpstr(
		nugu_audio_get_attribute_str(NUGU_AUDIO_ATTRIBUTE_MUSIC), ==,
		NUGU_AUDIO_ATTRIBUTE_MUSIC_DEFAULT_STRING);
	g_assert_cmpstr(
		nugu_audio_get_attribute_str(NUGU_AUDIO_ATTRIBUTE_RINGTONE), ==,
		NUGU_AUDIO_ATTRIBUTE_RINGTONE_DEFAULT_STRING);
	g_assert_cmpstr(nugu_audio_get_attribute_str(NUGU_AUDIO_ATTRIBUTE_CALL),
			==, NUGU_AUDIO_ATTRIBUTE_CALL_DEFAULT_STRING);
	g_assert_cmpstr(
		nugu_audio_get_attribute_str(NUGU_AUDIO_ATTRIBUTE_NOTIFICATION),
		==, NUGU_AUDIO_ATTRIBUTE_NOTIFICATION_DEFAULT_STRING);
	g_assert_cmpstr(
		nugu_audio_get_attribute_str(NUGU_AUDIO_ATTRIBUTE_ALARM), ==,
		NUGU_AUDIO_ATTRIBUTE_ALARM_DEFAULT_STRING);
	g_assert_cmpstr(nugu_audio_get_attribute_str(
				NUGU_AUDIO_ATTRIBUTE_VOICE_COMMAND),
			==, NUGU_AUDIO_ATTRIBUTE_VOICE_COMMAND_DEFAULT_STRING);
	g_assert_cmpstr(
		nugu_audio_get_attribute_str(NUGU_AUDIO_ATTRIBUTE_NAVIGATION),
		==, NUGU_AUDIO_ATTRIBUTE_NAVIGATION_DEFAULT_STRING);
	g_assert_cmpstr(
		nugu_audio_get_attribute_str(NUGU_AUDIO_ATTRIBUTE_SYSTEM_SOUND),
		==, NUGU_AUDIO_ATTRIBUTE_SYSTEM_SOUND_DEFAULT_STRING);
}

static void test_audio_attribute_str(void)
{
	// NULL
	nugu_audio_set_attribute_str(NUGU_AUDIO_ATTRIBUTE_MUSIC, NULL);
	g_assert_cmpstr(
		nugu_audio_get_attribute_str(NUGU_AUDIO_ATTRIBUTE_MUSIC), ==,
		NUGU_AUDIO_ATTRIBUTE_MUSIC_DEFAULT_STRING);

	// Empty String
	nugu_audio_set_attribute_str(NUGU_AUDIO_ATTRIBUTE_MUSIC, "");
	g_assert_cmpstr(
		nugu_audio_get_attribute_str(NUGU_AUDIO_ATTRIBUTE_MUSIC), ==,
		NUGU_AUDIO_ATTRIBUTE_MUSIC_DEFAULT_STRING);

	// Set Multimedia
	nugu_audio_set_attribute_str(NUGU_AUDIO_ATTRIBUTE_MUSIC, "Multimedia");
	g_assert_cmpstr(
		nugu_audio_get_attribute_str(NUGU_AUDIO_ATTRIBUTE_MUSIC), ==,
		"Multimedia");

	// Can set Multimedia other audio attribute
	nugu_audio_set_attribute_str(NUGU_AUDIO_ATTRIBUTE_VOICE_COMMAND,
				     "Multimedia");
	g_assert_cmpstr(nugu_audio_get_attribute_str(
				NUGU_AUDIO_ATTRIBUTE_VOICE_COMMAND),
			==, "Multimedia");
}

int main(int argc, char *argv[])
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
	g_type_init();
#endif

	g_test_init(&argc, &argv, NULL);
	g_log_set_always_fatal((GLogLevelFlags)G_LOG_FATAL_MASK);

	g_test_add_func("/audio/default", test_audio_default);
	g_test_add_func("/audio/set_audio_attribute_str",
			test_audio_attribute_str);

	return g_test_run();
}
