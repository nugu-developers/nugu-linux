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
#include <stdarg.h>

#include <alsa/error.h>
#include <portaudio.h>

#include "base/nugu_log.h"
#include "base/nugu_plugin.h"

#define PLUGIN_DRIVER_NAME "portaudio"

static void snd_error_log(const char *file, int line, const char *function,
			  int err, const char *fmt, ...)
{
	char *buf;
	size_t buf_size;
	va_list arg;

	va_start(arg, fmt);
	buf_size = vsnprintf(NULL, 0, fmt, arg) + 1;
	va_end(arg);

	buf = malloc(buf_size);
	if (!buf) {
		nugu_error_nomem();
		return;
	}

	va_start(arg, fmt);
	/* NOLINTNEXTLINE(cert-err33-c) */
	vsnprintf(buf, 4096, fmt, arg);
	va_end(arg);

	nugu_log_print(NUGU_LOG_MODULE_AUDIO, NUGU_LOG_LEVEL_DEBUG, NULL, NULL,
		       -1, "[ALSA] <%s:%d> err=%d, %s", file, line, err, buf);

	free(buf);
}

static int init(NuguPlugin *p)
{
	nugu_dbg("'%s' plugin initialized",
		 nugu_plugin_get_description(p)->name);

	if (Pa_Initialize() != paNoError) {
		nugu_error("initialize is failed");
		return -1;
	}

	nugu_info("version: %s", Pa_GetVersionText());

	nugu_dbg("'%s' plugin initialized done",
		 nugu_plugin_get_description(p)->name);

	return 0;
}

static int load(void)
{
	nugu_dbg("plugin loaded");
	snd_lib_error_set_handler(snd_error_log);

	return 0;
}

static void unload(NuguPlugin *p)
{
	nugu_dbg("'%s' plugin unloaded", nugu_plugin_get_description(p)->name);

	Pa_Terminate();

	snd_lib_error_set_handler(NULL);

	nugu_dbg("'%s' plugin unloaded done",
		 nugu_plugin_get_description(p)->name);
}

NUGU_PLUGIN_DEFINE(
	/* NUGU SDK Plug-in description */
	PLUGIN_DRIVER_NAME, /* Plugin name */
	NUGU_PLUGIN_PRIORITY_DEFAULT + 1, /* Plugin priority */
	"0.0.2", /* Plugin version */
	load, /* dlopen */
	unload, /* dlclose */
	init /* initialize */
);
