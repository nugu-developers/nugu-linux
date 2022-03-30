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

#include "base/nugu_audio.h"
#include "base/nugu_log.h"

EXPORT_API const char *
nugu_audio_get_attribute_str(const NuguAudioAttribute attribute)
{
	switch (attribute) {
	case NUGU_AUDIO_ATTRIBUTE_MUSIC:
		return "music";
	case NUGU_AUDIO_ATTRIBUTE_RINGTONE:
		return "ringtone";
	case NUGU_AUDIO_ATTRIBUTE_CALL:
		return "call";
	case NUGU_AUDIO_ATTRIBUTE_NOTIFICATION:
		return "notification";
	case NUGU_AUDIO_ATTRIBUTE_ALARM:
		return "alarm";
	case NUGU_AUDIO_ATTRIBUTE_VOICE_COMMAND:
		return "voice";
	case NUGU_AUDIO_ATTRIBUTE_NAVIGATION:
		return "navigation";
	case NUGU_AUDIO_ATTRIBUTE_SYSTEM_SOUND:
		return "system";
	default:
		nugu_warn("not implement yet!!");
		return "music";
	}
}
