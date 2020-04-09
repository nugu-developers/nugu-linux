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

#ifndef __NUGU_LOG_H__
#define __NUGU_LOG_H__

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nugu_log.h
 * @defgroup Log Log
 * @ingroup SDKBase
 * @brief Log functions
 *
 * The log function supports standard error output and the syslog system,
 * and you can set the handler you want if necessary.
 *
 * Supported features:
 *  - Log level
 *  - Additional information: Timestamp, PID, TID, Filename, Line number
 *
 * The log function also supports setting logs at runtime using the
 * environment variables below.
 *  - NUGU_LOG: "stderr", "syslog" or "none"
 *
 * @{
 */

#ifndef NUGU_ANSI_COLOR_NORMAL
#define NUGU_ANSI_COLOR_NORMAL       "\e[0m"
#endif

#ifndef NUGU_ANSI_COLOR_BLACK
#define NUGU_ANSI_COLOR_BLACK        "\e[0;30m"
#endif

#ifndef NUGU_ANSI_COLOR_RED
#define NUGU_ANSI_COLOR_RED          "\e[0;31m"
#endif

#ifndef NUGU_ANSI_COLOR_GREEN
#define NUGU_ANSI_COLOR_GREEN        "\e[0;32m"
#endif

#ifndef NUGU_ANSI_COLOR_BROWN
#define NUGU_ANSI_COLOR_BROWN        "\e[0;33m"
#endif

#ifndef NUGU_ANSI_COLOR_BLUE
#define NUGU_ANSI_COLOR_BLUE         "\e[0;34m"
#endif

#ifndef NUGU_ANSI_COLOR_MAGENTA
#define NUGU_ANSI_COLOR_MAGENTA      "\e[0;35m"
#endif

#ifndef NUGU_ANSI_COLOR_CYAN
#define NUGU_ANSI_COLOR_CYAN         "\e[0;36m"
#endif

#ifndef NUGU_ANSI_COLOR_LIGHTGRAY
#define NUGU_ANSI_COLOR_LIGHTGRAY    "\e[0;37m"
#endif

#ifndef NUGU_ANSI_COLOR_DARKGRAY
#define NUGU_ANSI_COLOR_DARKGRAY     "\e[1;30m"
#endif

#ifndef NUGU_ANSI_COLOR_LIGHTRED
#define NUGU_ANSI_COLOR_LIGHTRED     "\e[1;31m"
#endif

#ifndef NUGU_ANSI_COLOR_LIGHTGREEN
#define NUGU_ANSI_COLOR_LIGHTGREEN   "\e[1;32m"
#endif

#ifndef NUGU_ANSI_COLOR_YELLOW
#define NUGU_ANSI_COLOR_YELLOW       "\e[1;33m"
#endif

#ifndef NUGU_ANSI_COLOR_LIGHTBLUE
#define NUGU_ANSI_COLOR_LIGHTBLUE    "\e[1;34m"
#endif

#ifndef NUGU_ANSI_COLOR_LIGHTMAGENTA
#define NUGU_ANSI_COLOR_LIGHTMAGENTA "\e[1;35m"
#endif

#ifndef NUGU_ANSI_COLOR_LIGHTCYAN
#define NUGU_ANSI_COLOR_LIGHTCYAN    "\e[1;36m"
#endif

#ifndef NUGU_ANSI_COLOR_WHITE
#define NUGU_ANSI_COLOR_WHITE        "\e[1;37m"
#endif

/**
 * @brief log levels.
 *
 * When building in release mode, logs at the warning, info, and debug
 * levels are all ignored.
 *
 * @see nugu_log()
 */
enum nugu_log_level {
	NUGU_LOG_LEVEL_ERROR, /**< Error level */
	NUGU_LOG_LEVEL_WARNING, /**< Warning level */
	NUGU_LOG_LEVEL_INFO, /**< Information level */
	NUGU_LOG_LEVEL_DEBUG /**< Debug level */
};

/**
 * @brief Back-end system that supports log output.
 * @see nugu_log_set_system()
 * @see nugu_log_set_handler()
 */
enum nugu_log_system {
	NUGU_LOG_SYSTEM_STDERR, /**< Standard error */
	NUGU_LOG_SYSTEM_STDOUT, /**< Standard output */
	NUGU_LOG_SYSTEM_SYSLOG, /**< syslog */
	NUGU_LOG_SYSTEM_NONE, /**< no log */
	NUGU_LOG_SYSTEM_CUSTOM /**< custom log handler by log_set_handler() */
};

/**
 * @brief logging prefix
 * @see nugu_log_set_prefix_fields()
 */
enum nugu_log_prefix {
	NUGU_LOG_PREFIX_NONE = 0, /**< No prefix */
	NUGU_LOG_PREFIX_TIMESTAMP = (1 << 0), /**< mm-dd HH:MM:SS.000 */
	NUGU_LOG_PREFIX_PID = (1 << 1), /**< Process ID */
	NUGU_LOG_PREFIX_TID = (1 << 2), /**< Thread ID */
	NUGU_LOG_PREFIX_LEVEL = (1 << 3), /**< D, I, W, E */
	NUGU_LOG_PREFIX_FILEPATH = (1 << 4), /**< Full path with file name */
	NUGU_LOG_PREFIX_FILENAME = (1 << 5), /**< File name */
	NUGU_LOG_PREFIX_FUNCTION = (1 << 6), /**< Function name */
	NUGU_LOG_PREFIX_LINE = (1 << 7), /**< Line number */
	NUGU_LOG_PREFIX_DEFAULT =
		(NUGU_LOG_PREFIX_TIMESTAMP | NUGU_LOG_PREFIX_PID |
		 NUGU_LOG_PREFIX_TID | NUGU_LOG_PREFIX_LEVEL |
		 NUGU_LOG_PREFIX_FILENAME | NUGU_LOG_PREFIX_LINE),
	/**< TIMESTAMP + PID + TID + LEVEL + FILENAME + LINE */
	NUGU_LOG_PREFIX_ALL =
		(NUGU_LOG_PREFIX_DEFAULT | NUGU_LOG_PREFIX_FUNCTION |
		 NUGU_LOG_PREFIX_FILEPATH) /**< All prefix */
};

/**
 * @brief logging modules
 * @see nugu_log_set_modules()
 * @see nugu_log_get_modules()
 */
enum nugu_log_module {
	NUGU_LOG_MODULE_DEFAULT = 0x01, /**< Default module */
	NUGU_LOG_MODULE_NETWORK = 0x02, /**< Network module */
	NUGU_LOG_MODULE_NETWORK_TRACE = 0x04, /**< Network trace module */
	NUGU_LOG_MODULE_AUDIO = 0x08, /**< Audio module */
	NUGU_LOG_MODULE_PROTOCOL = 0x10, /**< Protocol module */
	NUGU_LOG_MODULE_PROFILING = 0x20, /**< Profiling module */
	NUGU_LOG_MODULE_ALL = 0xFF /**< All modules */
};

#ifndef NUGU_LOG_MODULE
/**
 * @brief Set default log module
 * @see enum nugu_log_module
 * @see nugu_log_set_modules()
 * @see nugu_log_get_modules()
 */
#define NUGU_LOG_MODULE NUGU_LOG_MODULE_DEFAULT
#endif

/**
 * @brief logging function
 * @param[in] module log module
 * @param[in] level log level
 * @param[in] filename source file name (e.g. __FILE__)
 * @param[in] funcname function name (e.g. __FUNCTION__)
 * @param[in] line source file line number
 * @param[in] format printf format string
 * @see nugu_dbg()
 * @see nugu_info()
 * @see nugu_warn()
 * @see nugu_error()
 */
void nugu_log_print(enum nugu_log_module module, enum nugu_log_level level,
		    const char *filename, const char *funcname, int line,
		    const char *format, ...);

/**
 * @brief Custom log hook handler
 * @param[in] module log module
 * @param[in] level log level
 * @param[in] prefix generated additional information
 *                   (e.g. timestamp, line number)
 * @param[in] msg original log message
 * @param[in] user_data The user data passed from the callback function
 * @see nugu_log_handler()
 * @see nugu_log_set_prefix_fields()
 */
typedef void (*nugu_log_handler)(enum nugu_log_module module,
				 enum nugu_log_level level, const char *prefix,
				 const char *msg, void *user_data);

/**
 * @brief Set logging backend system
 * @see enum nugu_log_system
 */
int nugu_log_set_system(enum nugu_log_system log_system);

/**
 * @brief Get logging backend system
 * @see enum nugu_log_system
 */
enum nugu_log_system nugu_log_get_system(void);

/**
 * @brief Set custom log handler
 * @param[in] handler callback
 * @param[in] user_data The user data to be passed to the callback function
 * @see nugu_log_handler
 */
int nugu_log_set_handler(nugu_log_handler handler, void *user_data);

/**
 * @brief Set the additional information fields
 * @param[in] field_set bitmask by enum nugu_log_prefix
 * @see enum nugu_log_prefix
 */
void nugu_log_set_prefix_fields(enum nugu_log_prefix field_set);

/**
 * @brief Get the additional information fields
 * @return bitmask by enum nugu_log_prefix
 * @see enum nugu_log_prefix
 */
enum nugu_log_prefix nugu_log_get_prefix_fields(void);

/**
 * @brief Set the bitset of modules
 * @param[in] bitset bitmask by enum nugu_log_prefix
 * @see enum nugu_log_module
 */
void nugu_log_set_modules(unsigned int bitset);

/**
 * @brief Get the bitset of modules
 * @return bitmask by enum nugu_log_module
 * @see enum nugu_log_module
 */
unsigned int nugu_log_get_modules(void);

/**
 * @brief Hexdump the specific data to stderr
 * @param[in] module log module
 * @param[in] data memory address to hexdump
 * @param[in] data_size size
 * @param[in] header message to be printed at the top of hexdump
 * @param[in] footer message to be printed at the bottom of hexdump
 * @param[in] lineindent message to be printed at the beginning of each line
 */
void nugu_hexdump(enum nugu_log_module module, const uint8_t *data,
		  size_t data_size, const char *header, const char *footer,
		  const char *lineindent);

#ifdef __cplusplus
}
#endif

#ifndef __FILENAME__
#define __FILENAME__ __FILE__
#endif

/**
 * @brief Convenient macro to fill file, function and line information
 * @param[in] module log module name
 * @param[in] level logging level
 * @param[in] fmt printf format string
 * @see enum nugu_log_module
 * @see enum nugu_log_level
 */
#define nugu_log(module, level, fmt, ...)                                      \
	nugu_log_print(module, level, __FILENAME__, __PRETTY_FUNCTION__,       \
		       __LINE__, fmt, ##__VA_ARGS__)

#ifdef CONFIG_RELEASE
#define nugu_dbg(fmt, ...)
#define nugu_info(fmt, ...)
#define nugu_warn(fmt, ...)
#else

/**
 * @brief Debug level error message
 * @param[in] fmt printf format string
 * @see nugu_log()
 */
#define nugu_dbg(fmt, ...)                                                     \
	nugu_log(NUGU_LOG_MODULE, NUGU_LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)

/**
 * @brief Information level error message
 * @param[in] fmt printf format string
 * @see nugu_log()
 */
#define nugu_info(fmt, ...)                                                    \
	nugu_log(NUGU_LOG_MODULE, NUGU_LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)

/**
 * @brief Warning level error message
 * @param[in] fmt printf format string
 * @see nugu_log()
 */
#define nugu_warn(fmt, ...)                                                    \
	nugu_log(NUGU_LOG_MODULE, NUGU_LOG_LEVEL_WARNING, fmt, ##__VA_ARGS__)
#endif

/**
 * @brief Error level error message
 * @param[in] fmt printf format string
 * @see nugu_log()
 */
#define nugu_error(fmt, ...)                                                   \
	nugu_log(NUGU_LOG_MODULE, NUGU_LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)

#ifndef NUGU_ANSI_COLOR_SEND
#ifdef NUGU_LOG_USE_ANSICOLOR
#define NUGU_ANSI_COLOR_SEND NUGU_ANSI_COLOR_BROWN
#else
#define NUGU_ANSI_COLOR_SEND ""
#endif
#endif

#ifndef NUGU_ANSI_COLOR_RECV
#ifdef NUGU_LOG_USE_ANSICOLOR
#define NUGU_ANSI_COLOR_RECV NUGU_ANSI_COLOR_GREEN
#else
#define NUGU_ANSI_COLOR_RECV ""
#endif
#endif

#ifndef NUGU_LOG_MARK_SEND
#define NUGU_LOG_MARK_SEND "--> "
#endif

#ifndef NUGU_LOG_MARK_RECV
#define NUGU_LOG_MARK_RECV "<-- "
#endif

/**
 * @brief Protocol log message for sending
 * @param[in] level logging level
 * @param[in] fmt printf format string
 * @see nugu_log_print()
 * @see enum nugu_log_level
 */
#define nugu_log_protocol_send(level, fmt, ...)                                \
	nugu_log_print(NUGU_LOG_MODULE_PROTOCOL, level, NULL, NULL, -1,        \
		       NUGU_ANSI_COLOR_SEND NUGU_LOG_MARK_SEND fmt             \
			       NUGU_ANSI_COLOR_NORMAL,                         \
		       ##__VA_ARGS__)

/**
 * @brief Protocol log message for receiving
 * @param[in] level logging level
 * @param[in] fmt printf format string
 * @see nugu_log_print()
 * @see enum nugu_log_level
 */
#define nugu_log_protocol_recv(level, fmt, ...)                                \
	nugu_log_print(NUGU_LOG_MODULE_PROTOCOL, level, NULL, NULL, -1,        \
		       NUGU_ANSI_COLOR_RECV NUGU_LOG_MARK_RECV fmt             \
			       NUGU_ANSI_COLOR_NORMAL,                         \
		       ##__VA_ARGS__)

/**
 * @brief Default error message for 'Not enough memory'
 * @see nugu_error()
 */
#define nugu_error_nomem() nugu_error("Not enough memory")

/**
 * @}
 */

#endif
