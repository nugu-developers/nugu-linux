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
#include <time.h>
#include <string.h>
#include <sys/syscall.h>
#include <syslog.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <inttypes.h>
#include <ctype.h>

#include <glib.h>

#include "base/nugu_log.h"

#define MAX_FIELDSIZE_FILENAME 30
#define MAX_FIELDSIZE_FUNCNAME 30

#define HEXDUMP_COLUMN_SIZE 32

#define ANSI_COLOR_NORMAL       "\e[0m"

#define ANSI_COLOR_BLACK        "\e[0;30m"
#define ANSI_COLOR_RED          "\e[0;31m"
#define ANSI_COLOR_GREEN        "\e[0;32m"
#define ANSI_COLOR_BROWN        "\e[0;33m"
#define ANSI_COLOR_BLUE         "\e[0;34m"
#define ANSI_COLOR_MAGENTA      "\e[0;35m"
#define ANSI_COLOR_CYAN         "\e[0;36m"
#define ANSI_COLOR_LIGHTGRAY    "\e[0;37m"

#define ANSI_COLOR_DARKGRAY     "\e[1;30m"
#define ANSI_COLOR_LIGHTRED     "\e[1;31m"
#define ANSI_COLOR_LIGHTGREEN   "\e[1;32m"
#define ANSI_COLOR_YELLOW       "\e[1;33m"
#define ANSI_COLOR_LIGHTBLUE    "\e[1;34m"
#define ANSI_COLOR_LIGHTMAGENTA "\e[1;35m"
#define ANSI_COLOR_LIGHTCYAN    "\e[1;36m"
#define ANSI_COLOR_WHITE        "\e[1;37m"

#ifdef __APPLE__
#define FMT_TID "%5" PRIu64 " "
#else
#define FMT_TID "%5d "
#endif

static enum nugu_log_system _log_system = NUGU_LOG_SYSTEM_STDERR;
static enum nugu_log_prefix _log_prefix_fields = NUGU_LOG_PREFIX_DEFAULT;
static nugu_log_handler _log_handler;
static void *_log_handler_user_data;
static int _log_override_enabled;
static int _log_override_checked;
static pthread_mutex_t _log_mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned int _log_module_bitset = NUGU_LOG_MODULE_ALL;

static struct _log_level_info {
	char mark;
	int syslog_level;
} _log_level_map[] = {
	[NUGU_LOG_LEVEL_ERROR] = { 'E', LOG_ERR },
	[NUGU_LOG_LEVEL_WARNING] = { 'W', LOG_WARNING },
	[NUGU_LOG_LEVEL_INFO] = { 'I', LOG_INFO },
	[NUGU_LOG_LEVEL_DEBUG] = { 'D', LOG_DEBUG }
};

#ifdef NUGU_ENV_LOG
static void _log_check_override_system(void)
{
	const char *env;

	env = getenv(NUGU_ENV_LOG);
	if (!env)
		return;

	if (!strncasecmp(env, "stderr", 7)) {
		_log_override_enabled = 1;
		_log_system = NUGU_LOG_SYSTEM_STDERR;
	} else if (!strncasecmp(env, "syslog", 7)) {
		_log_override_enabled = 1;
		_log_system = NUGU_LOG_SYSTEM_SYSLOG;
	} else if (!strncasecmp(env, "none", 5)) {
		_log_override_enabled = 1;
		_log_system = NUGU_LOG_SYSTEM_NONE;
	}
}
#endif

#ifdef NUGU_ENV_LOG_MODULE
static void _log_check_override_module(void)
{
	gchar **modules = NULL;
	const char *env;
	gint count = 0;
	gint i;
	unsigned int bitset = 0;

	env = getenv(NUGU_ENV_LOG_MODULE);
	if (!env)
		return;

	modules = g_strsplit(env, ",", -1);
	if (!modules)
		return;

	count = g_strv_length(modules);
	for (i = 0; i < count; i++) {
		if (!strncasecmp(modules[i], "all", 4)) {
			bitset = NUGU_LOG_MODULE_ALL;
			break;
		}

		if (!strncasecmp(modules[i], "default", 8))
			bitset = bitset | NUGU_LOG_MODULE_DEFAULT;
		if (!strncasecmp(modules[i], "network", 8))
			bitset = bitset | NUGU_LOG_MODULE_NETWORK;
		if (!strncasecmp(modules[i], "network_trace", 14))
			bitset = bitset | NUGU_LOG_MODULE_NETWORK_TRACE;
		if (!strncasecmp(modules[i], "audio", 6))
			bitset = bitset | NUGU_LOG_MODULE_AUDIO;
	}

	nugu_log_set_modules(bitset);
	g_strfreev(modules);
}
#endif

static void _log_check_override(void)
{
	if (_log_override_checked)
		return;

	_log_override_checked = 1;

#ifdef NUGU_ENV_LOG
	_log_check_override_system();
#endif

#ifdef NUGU_ENV_LOG_MODULE
	_log_check_override_module();
#endif
}

static int _log_make_prefix(char *prefix, enum nugu_log_level level,
			    const char *filename, const char *funcname,
			    int line)
{
	const char *pretty_filename = NULL;
	int len = 0;
	pid_t pid = 0;

	if (_log_prefix_fields & NUGU_LOG_PREFIX_TIMESTAMP) {
		struct timespec tp;
		struct tm ti;

		clock_gettime(CLOCK_REALTIME, &tp);
		localtime_r(&(tp.tv_sec), &ti);

		len += (int)strftime(prefix, 15, "%m-%d %H:%M:%S", &ti);
		len += snprintf(prefix + len, 6, ".%03ld ",
				tp.tv_nsec / 1000000);
	}

	if (_log_prefix_fields & NUGU_LOG_PREFIX_PID) {
		pid = getpid();
		if (len > 0)
			len += snprintf(prefix + len, 7, "%5d ", pid);
		else
			len += snprintf(prefix, 7, "%d ", pid);
	}

	if (_log_prefix_fields & NUGU_LOG_PREFIX_TID) {
#ifdef __APPLE__
		uint64_t tid = 0;

		pthread_threadid_np(NULL, &tid);
#else
		int tid = 0;

		tid = (pid_t)syscall(SYS_gettid);
#endif

#ifdef CONFIG_LOG_ANSICOLOR
#ifdef __APPLE__
		if (len > 0 && pid != 0 && (uint64_t)pid != tid)
#else
		if (len > 0 && pid != 0 && pid != tid)
#endif
			len += snprintf(prefix + len, 18,
					ANSI_COLOR_DARKGRAY
					FMT_TID ANSI_COLOR_NORMAL,
					tid);
		else
			len += snprintf(prefix + len, 7, FMT_TID, tid);
#else
		len += snprintf(prefix + len, 7, FMT_TID, tid);
#endif
	}

	if (_log_prefix_fields & NUGU_LOG_PREFIX_LEVEL) {
		prefix[len++] = _log_level_map[level].mark;
		prefix[len++] = ' ';
	}

	if ((_log_prefix_fields & NUGU_LOG_PREFIX_FILENAME) && filename) {
		pretty_filename = strrchr(filename, '/');
		if (!pretty_filename)
			pretty_filename = filename;
		else
			pretty_filename++;
	}

	if ((_log_prefix_fields & NUGU_LOG_PREFIX_FILEPATH) && filename)
		pretty_filename = filename;

	if (pretty_filename) {
		size_t field_len;

		field_len = strlen(pretty_filename);
		if (field_len > MAX_FIELDSIZE_FILENAME) {
			len += snprintf(prefix + len,
					MAX_FIELDSIZE_FILENAME + 5, "<~%s> ",
					pretty_filename + field_len -
						MAX_FIELDSIZE_FILENAME);
		} else
			len += snprintf(prefix + len, field_len + 5, "<%s> ",
					pretty_filename);

		/* Filename with line number */
		if ((_log_prefix_fields & NUGU_LOG_PREFIX_LINE) && line >= 0) {
			len--;
			len--;
			len += snprintf(prefix + len, 9, ":%d> ", line);
			*(prefix + len - 1) = ' ';
		}
	} else {
		/* Standalone line number */
		if ((_log_prefix_fields & NUGU_LOG_PREFIX_LINE) && line >= 0) {
			len += snprintf(prefix + len, 9, "<%d> ", line);
			*(prefix + len - 1) = ' ';
		}
	}

	if ((_log_prefix_fields & NUGU_LOG_PREFIX_FUNCTION) && funcname) {
		size_t field_len;

		field_len = strlen(funcname);
		if (field_len > MAX_FIELDSIZE_FUNCNAME) {
			len += snprintf(prefix + len,
					MAX_FIELDSIZE_FUNCNAME + 3, "~%s ",
					funcname + field_len -
						MAX_FIELDSIZE_FUNCNAME);
		} else
			len += snprintf(prefix + len, field_len + 2, "%s ",
					funcname);
	}

	/* Remove last space */
	if (len > 0) {
		if (*(prefix + len - 1) == ' ') {
			*(prefix + len - 1) = 0;
			len--;
		}
	}

	return len;
}

static void _log_formatted(enum nugu_log_module module,
			   enum nugu_log_level level, const char *filename,
			   const char *funcname, int line, const char *format,
			   va_list arg)
{
	char prefix[4096] = { 0 };
	int len = 0;

	if (_log_prefix_fields > NUGU_LOG_PREFIX_NONE)
		len = _log_make_prefix(prefix, level, filename, funcname, line);

	if (_log_system == NUGU_LOG_SYSTEM_STDERR) {
		pthread_mutex_lock(&_log_mutex);
		if (len > 0)
			fprintf(stderr, "%s ", prefix);
#ifdef CONFIG_LOG_ANSICOLOR
		switch (level) {
		case NUGU_LOG_LEVEL_DEBUG:
			break;
		case NUGU_LOG_LEVEL_INFO:
			fputs(ANSI_COLOR_LIGHTBLUE, stderr);
			break;
		case NUGU_LOG_LEVEL_WARNING:
			fputs(ANSI_COLOR_LIGHTGRAY, stderr);
			break;
		case NUGU_LOG_LEVEL_ERROR:
			fputs(ANSI_COLOR_LIGHTRED, stderr);
			break;
		default:
			break;
		}
#endif
		vfprintf(stderr, format, arg);
#ifdef CONFIG_LOG_ANSICOLOR
		fputs(ANSI_COLOR_NORMAL, stderr);
#endif
		fputc('\n', stderr);
		fflush(stderr);
		pthread_mutex_unlock(&_log_mutex);
	} else if (_log_system == NUGU_LOG_SYSTEM_CUSTOM && _log_handler) {
		char msg[4096];

		vsnprintf(msg, 4096, format, arg);
		_log_handler(module, level, prefix, msg,
			     _log_handler_user_data);
	}
}

EXPORT_API void nugu_log_print(enum nugu_log_module module,
			       enum nugu_log_level level, const char *filename,
			       const char *funcname, int line,
			       const char *format, ...)
{
	va_list arg;

	if (!_log_override_checked)
		_log_check_override();

	if ((_log_module_bitset & module) == 0)
		return;

	switch (_log_system) {
	case NUGU_LOG_SYSTEM_SYSLOG:
		va_start(arg, format);
		vsyslog(_log_level_map[level].syslog_level, format, arg);
		va_end(arg);
		break;

	case NUGU_LOG_SYSTEM_STDERR:
	case NUGU_LOG_SYSTEM_CUSTOM:
		va_start(arg, format);
		_log_formatted(module, level, filename, funcname, line, format,
			       arg);
		va_end(arg);
		break;

	case NUGU_LOG_SYSTEM_NONE:
	default:
		break;
	}
}

EXPORT_API int nugu_log_set_system(enum nugu_log_system log_system)
{
	if (log_system > NUGU_LOG_SYSTEM_CUSTOM) {
		nugu_error("invalid log system(%d)", log_system);
		return -EINVAL;
	}

	if (_log_override_enabled)
		return 0;

	_log_system = log_system;

	return 0;
}

EXPORT_API int nugu_log_set_handler(nugu_log_handler handler, void *user_data)
{
	if (!handler) {
		nugu_error("handler is NULL");
		return -EINVAL;
	}

	if (_log_override_enabled)
		return 0;

	_log_system = NUGU_LOG_SYSTEM_CUSTOM;
	_log_handler = handler;
	_log_handler_user_data = user_data;

	return 0;
}

EXPORT_API void nugu_log_set_prefix_fields(enum nugu_log_prefix field_set)
{
	_log_prefix_fields = field_set;
}

EXPORT_API enum nugu_log_prefix nugu_log_get_prefix_fields(void)
{
	return _log_prefix_fields;
}

EXPORT_API void nugu_log_set_modules(unsigned int bitset)
{
	_log_module_bitset = bitset;
}

EXPORT_API unsigned int nugu_log_get_modules(void)
{
	return _log_module_bitset;
}

EXPORT_API void nugu_hexdump(const uint8_t *data, size_t data_size)
{
	size_t i;
	size_t j;

	pthread_mutex_lock(&_log_mutex);

	for (i = 0; i < data_size; i += HEXDUMP_COLUMN_SIZE) {
		printf("0x%04X: ", (unsigned int)i);

		/* hex */
		for (j = i; j < i + HEXDUMP_COLUMN_SIZE; j++) {
			if (j >= data_size)
				printf("   ");
			else
				printf("%02X ", data[j]);

			if ((j + 1) % (HEXDUMP_COLUMN_SIZE / 2) == 0)
				printf("  ");
		}

		/* ascii */
		for (j = i; j < i + HEXDUMP_COLUMN_SIZE; j++) {
			if (j >= data_size)
				printf(" ");
			else if (isprint(data[j]))
				putchar(data[j]);
			else
				putchar('.');
		}

		printf("\n");
	}

	if (i % HEXDUMP_COLUMN_SIZE != 0)
		printf("\n");

	pthread_mutex_unlock(&_log_mutex);
}
