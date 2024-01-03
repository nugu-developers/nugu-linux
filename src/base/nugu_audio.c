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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "base/nugu_audio.h"
#include "base/nugu_log.h"

#define AUDIO_ATTR_NUM NUGU_AUDIO_ATTRIBUTE_SYSTEM_SOUND
#define GET_AUDIO_ATTR_INDEX(audio_attr) (audio_attr - 1)

static char *audio_attr_str[AUDIO_ATTR_NUM];

static char *nugu_audio_get_default_attribute_str(NuguAudioAttribute attribute)
{
	char *attr_str;

	switch (attribute) {
	case NUGU_AUDIO_ATTRIBUTE_MUSIC:
		attr_str = NUGU_AUDIO_ATTRIBUTE_MUSIC_DEFAULT_STRING;
		break;
	case NUGU_AUDIO_ATTRIBUTE_RINGTONE:
		attr_str = NUGU_AUDIO_ATTRIBUTE_RINGTONE_DEFAULT_STRING;
		break;
	case NUGU_AUDIO_ATTRIBUTE_CALL:
		attr_str = NUGU_AUDIO_ATTRIBUTE_CALL_DEFAULT_STRING;
		break;
	case NUGU_AUDIO_ATTRIBUTE_NOTIFICATION:
		attr_str = NUGU_AUDIO_ATTRIBUTE_NOTIFICATION_DEFAULT_STRING;
		break;
	case NUGU_AUDIO_ATTRIBUTE_ALARM:
		attr_str = NUGU_AUDIO_ATTRIBUTE_ALARM_DEFAULT_STRING;
		break;
	case NUGU_AUDIO_ATTRIBUTE_VOICE_COMMAND:
		attr_str = NUGU_AUDIO_ATTRIBUTE_VOICE_COMMAND_DEFAULT_STRING;
		break;
	case NUGU_AUDIO_ATTRIBUTE_NAVIGATION:
		attr_str = NUGU_AUDIO_ATTRIBUTE_NAVIGATION_DEFAULT_STRING;
		break;
	case NUGU_AUDIO_ATTRIBUTE_SYSTEM_SOUND:
		attr_str = NUGU_AUDIO_ATTRIBUTE_SYSTEM_SOUND_DEFAULT_STRING;
		break;
	default:
		nugu_warn("not implement yet!!");
		attr_str = NUGU_AUDIO_ATTRIBUTE_MUSIC_DEFAULT_STRING;
		break;
	}
	return attr_str;
}

EXPORT_API void nugu_audio_set_attribute_str(const NuguAudioAttribute attribute,
					     const char *str)
{
	int index = GET_AUDIO_ATTR_INDEX(attribute);

	if (!str || !strlen(str))
		return;

	if (audio_attr_str[index])
		free(audio_attr_str[index]);

	audio_attr_str[index] = (char *)calloc(1, strlen(str) + 1);
	if (!audio_attr_str[index]) {
		nugu_error_nomem();
		return;
	}
	snprintf(audio_attr_str[index], strlen(str) + 1, "%s", str);
}

EXPORT_API const char *
nugu_audio_get_attribute_str(const NuguAudioAttribute attribute)
{
	int index = GET_AUDIO_ATTR_INDEX(attribute);

	if (index < AUDIO_ATTR_NUM && audio_attr_str[index])
		return audio_attr_str[index];

	return nugu_audio_get_default_attribute_str(attribute);
}

EXPORT_API NuguAudioAttribute nugu_audio_get_attribute(const char *str)
{
	if (str == NULL)
		return NUGU_AUDIO_ATTRIBUTE_MUSIC;

	if (!strncmp(str, NUGU_AUDIO_ATTRIBUTE_MUSIC_DEFAULT_STRING, 5))
		return NUGU_AUDIO_ATTRIBUTE_MUSIC;
	else if (!strncmp(str, NUGU_AUDIO_ATTRIBUTE_RINGTONE_DEFAULT_STRING, 5))
		return NUGU_AUDIO_ATTRIBUTE_RINGTONE;
	else if (!strncmp(str, NUGU_AUDIO_ATTRIBUTE_CALL_DEFAULT_STRING, 5))
		return NUGU_AUDIO_ATTRIBUTE_CALL;
	else if (!strncmp(str, NUGU_AUDIO_ATTRIBUTE_NOTIFICATION_DEFAULT_STRING,
			  5))
		return NUGU_AUDIO_ATTRIBUTE_NOTIFICATION;
	else if (!strncmp(str, NUGU_AUDIO_ATTRIBUTE_ALARM_DEFAULT_STRING, 5))
		return NUGU_AUDIO_ATTRIBUTE_ALARM;
	else if (!strncmp(str,
			  NUGU_AUDIO_ATTRIBUTE_VOICE_COMMAND_DEFAULT_STRING, 5))
		return NUGU_AUDIO_ATTRIBUTE_VOICE_COMMAND;
	else if (!strncmp(str, NUGU_AUDIO_ATTRIBUTE_NAVIGATION_DEFAULT_STRING,
			  5))
		return NUGU_AUDIO_ATTRIBUTE_NAVIGATION;
	else if (!strncmp(str, NUGU_AUDIO_ATTRIBUTE_SYSTEM_SOUND_DEFAULT_STRING,
			  5))
		return NUGU_AUDIO_ATTRIBUTE_SYSTEM_SOUND;
	else
		return NUGU_AUDIO_ATTRIBUTE_MUSIC;
}
