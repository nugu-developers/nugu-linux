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
#include <pthread.h>

#include <glib.h>

#include "base/nugu_log.h"
#include "base/nugu_prof.h"

#define BUFSIZE_TIMESTR 19
#define PAYLOAD_MAX 4096

struct nugu_prof_hints {
	const char *text;
	enum nugu_prof_type relative_type;
};

/**
 * Profiling data hints
 */
static const struct nugu_prof_hints _hints[NUGU_PROF_TYPE_MAX + 1] = {
	/* sdk */
	{ "Created", NUGU_PROF_TYPE_MAX },
	{ "Plugin init", NUGU_PROF_TYPE_SDK_CREATED },
	{ "Plugin done", NUGU_PROF_TYPE_SDK_PLUGIN_INIT_START },
	{ "Initialized", NUGU_PROF_TYPE_SDK_CREATED },

	/* network */
	{ "Connect request", NUGU_PROF_TYPE_MAX },
	{ "Registry request", NUGU_PROF_TYPE_NETWORK_CONNECT_REQUEST },
	{ "Registry response", NUGU_PROF_TYPE_NETWORK_REGISTRY_REQUEST },
	{ "Registry failed", NUGU_PROF_TYPE_NETWORK_REGISTRY_REQUEST },
	{ "Server request", NUGU_PROF_TYPE_NETWORK_CONNECT_REQUEST },
	{ "Server establish", NUGU_PROF_TYPE_NETWORK_SERVER_ESTABLISH_REQUEST },
	{ "Server failed", NUGU_PROF_TYPE_NETWORK_SERVER_ESTABLISH_REQUEST },
	{ "Connected", NUGU_PROF_TYPE_NETWORK_CONNECT_REQUEST },
	{ "Directive closed", NUGU_PROF_TYPE_MAX },
	{ "DNS failed", NUGU_PROF_TYPE_NETWORK_CONNECT_REQUEST },
	{ "SSL failed", NUGU_PROF_TYPE_NETWORK_CONNECT_REQUEST },
	{ "Internal network error", NUGU_PROF_TYPE_MAX },
	{ "Invalid token", NUGU_PROF_TYPE_MAX },

	/* /v2/ping */
	{ "PING request", NUGU_PROF_TYPE_MAX },
	{ "PING response", NUGU_PROF_TYPE_NETWORK_PING_REQUEST },
	{ "PING failed", NUGU_PROF_TYPE_NETWORK_PING_REQUEST },

	/* /v2/events event */
	{ "Event request", NUGU_PROF_TYPE_MAX },
	{ "Event response", NUGU_PROF_TYPE_NETWORK_EVENT_REQUEST },
	{ "Event failed", NUGU_PROF_TYPE_NETWORK_EVENT_REQUEST },

	/* /v2/events attachment */
	{ "Attachment request", NUGU_PROF_TYPE_MAX },
	{ "Attachment response", NUGU_PROF_TYPE_NETWORK_EVENT_REQUEST },
	{ "Attachment failed", NUGU_PROF_TYPE_NETWORK_EVENT_REQUEST },

	/* /v2/directives */
	{ "Last push-data", NUGU_PROF_TYPE_MAX },

	/* wakeup word */
	{ "wakeup detected", NUGU_PROF_TYPE_MAX },

	/* ASR */
	{ "Listening started", NUGU_PROF_TYPE_WAKEUP_KEYWORD_DETECTED },
	{ "ASR.Recognize", NUGU_PROF_TYPE_ASR_LISTENING_STARTED },
	{ "Recognizing", NUGU_PROF_TYPE_ASR_LISTENING_STARTED },
	{ "End detected", NUGU_PROF_TYPE_ASR_RECOGNIZING_STARTED },
	{ "Timeout", NUGU_PROF_TYPE_ASR_LISTENING_STARTED },
	{ "First attachment", NUGU_PROF_TYPE_ASR_RECOGNIZING_STARTED },
	{ "Last attachment", NUGU_PROF_TYPE_ASR_FIRST_ATTACHMENT },
	{ "ASR Result", NUGU_PROF_TYPE_ASR_RECOGNIZE },

	/* TTS */
	{ "TTS started", NUGU_PROF_TYPE_ASR_RESULT },
	{ "TTS first data", NUGU_PROF_TYPE_ASR_RESULT },
	{ "TTS last data", NUGU_PROF_TYPE_TTS_FIRST_ATTACHMENT },
	{ "TTS finished", NUGU_PROF_TYPE_ASR_RESULT },

	/* Audio */
	{ "Audio started", NUGU_PROF_TYPE_ASR_RESULT },
	{ "Audio finished", NUGU_PROF_TYPE_AUDIO_STARTED },

	/* end */
	{ "END", NUGU_PROF_TYPE_MAX }
};

/**
 * Profiling data store
 */
static struct nugu_prof_data _prof_data[NUGU_PROF_TYPE_MAX + 1];

static NuguProfCallback _callback;
static void *_callback_userdata;
static pthread_mutex_t _lock;
static gboolean _trace;

static void _fill_timestr(char *dest_buf, size_t bufsize,
			  const struct timespec *ts)
{
	struct tm tm_local;

	memset(dest_buf, ' ', bufsize);
	dest_buf[bufsize - 1] = '\0';

	if (bufsize < BUFSIZE_TIMESTR)
		return;

	localtime_r(&(ts->tv_sec), &tm_local);

	strftime(dest_buf, 15, "%m-%d %H:%M:%S", &tm_local);

	snprintf(dest_buf + 14, bufsize - 14, ".%03d",
		 (int)(ts->tv_nsec / 1000000));
}

EXPORT_API void nugu_prof_clear(void)
{
	pthread_mutex_lock(&_lock);
	memset(_prof_data, 0, sizeof(_prof_data));
	pthread_mutex_unlock(&_lock);
}

EXPORT_API void nugu_prof_enable_tracelog(void)
{
	pthread_mutex_lock(&_lock);
	_trace = TRUE;
	pthread_mutex_unlock(&_lock);
}

EXPORT_API void nugu_prof_disable_tracelog(void)
{
	pthread_mutex_lock(&_lock);
	_trace = FALSE;
	pthread_mutex_unlock(&_lock);
}

EXPORT_API void nugu_prof_set_callback(NuguProfCallback callback,
				       void *userdata)
{
	pthread_mutex_lock(&_lock);
	_callback = callback;
	_callback_userdata = userdata;
	pthread_mutex_unlock(&_lock);
}

/**
 * Profiling callback is not time critical, so it is called in idle time.
 */
static gboolean _emit_in_idle(gpointer userdata)
{
	NuguProfCallback cb;
	void *cb_userdata;
	struct nugu_prof_data *data = userdata;

	pthread_mutex_lock(&_lock);
	cb = _callback;
	cb_userdata = _callback_userdata;
	pthread_mutex_unlock(&_lock);

	if (_trace) {
		char timestr[25];

		_fill_timestr(timestr, sizeof(timestr), &data->timestamp);

		nugu_log_print(NUGU_LOG_MODULE_PROFILING, NUGU_LOG_LEVEL_DEBUG,
			       NULL, NULL, -1, "Profiling %d (%-20s) <%s>",
			       data->type, _hints[data->type].text, timestr);

		if (data->dialog_id[0] != '\0')
			nugu_log_print(NUGU_LOG_MODULE_PROFILING,
				       NUGU_LOG_LEVEL_DEBUG, NULL, NULL, -1,
				       " - dialog-id: %s", data->dialog_id);
		if (data->msg_id[0] != '\0')
			nugu_log_print(NUGU_LOG_MODULE_PROFILING,
				       NUGU_LOG_LEVEL_DEBUG, NULL, NULL, -1,
				       " - message-id: %s", data->msg_id);
		if (data->contents)
			nugu_log_print(NUGU_LOG_MODULE_PROFILING,
				       NUGU_LOG_LEVEL_DEBUG, NULL, NULL, -1,
				       " - contents: %s", data->contents);
	}

	if (cb != NULL)
		cb(data->type, data, cb_userdata);

	if (data->contents)
		free(data->contents);
	free(data);

	return FALSE;
}

static void _emit_callback(enum nugu_prof_type type, const char *contents)
{
	struct nugu_prof_data *data;

	if (_callback == NULL && _trace == FALSE)
		return;

	data = malloc(sizeof(struct nugu_prof_data));
	if (!data) {
		nugu_error_nomem();
		return;
	}

	memcpy(data, &_prof_data[type], sizeof(struct nugu_prof_data));

	if (contents)
		data->contents = strdup(contents);
	else
		data->contents = NULL;

	g_idle_add(_emit_in_idle, data);
}

EXPORT_API int nugu_prof_mark_data(enum nugu_prof_type type,
				   const char *dialog_id, const char *msg_id,
				   const char *contents)
{
	struct nugu_prof_data *tmp;

	g_return_val_if_fail(type < NUGU_PROF_TYPE_MAX, -1);

	tmp = &_prof_data[type];

	pthread_mutex_lock(&_lock);

	memset(tmp, 0, sizeof(struct nugu_prof_data));
	clock_gettime(CLOCK_REALTIME, &tmp->timestamp);
	tmp->type = type;

	if (dialog_id)
		memcpy(tmp->dialog_id, dialog_id, NUGU_MAX_UUID_STRING_SIZE);
	else
		memset(tmp->dialog_id, 0, NUGU_MAX_UUID_STRING_SIZE);

	if (msg_id)
		memcpy(tmp->msg_id, msg_id, NUGU_MAX_UUID_STRING_SIZE);
	else
		memset(tmp->msg_id, 0, NUGU_MAX_UUID_STRING_SIZE);

	if (_callback != NULL || _trace)
		_emit_callback(type, contents);

	pthread_mutex_unlock(&_lock);

	return 0;
}

EXPORT_API int nugu_prof_mark(enum nugu_prof_type type)
{
	g_return_val_if_fail(type < NUGU_PROF_TYPE_MAX, -1);

	pthread_mutex_lock(&_lock);

	memset(&_prof_data[type], 0, sizeof(struct nugu_prof_data));
	clock_gettime(CLOCK_REALTIME, &_prof_data[type].timestamp);
	_prof_data[type].type = type;

	if (_callback != NULL || _trace)
		_emit_callback(type, NULL);

	pthread_mutex_unlock(&_lock);

	return 0;
}

EXPORT_API struct nugu_prof_data *
nugu_prof_get_last_data(enum nugu_prof_type type)
{
	struct nugu_prof_data *tmp;

	g_return_val_if_fail(type < NUGU_PROF_TYPE_MAX, NULL);

	tmp = malloc(sizeof(struct nugu_prof_data));
	if (!tmp) {
		nugu_error_nomem();
		return NULL;
	}

	pthread_mutex_lock(&_lock);
	memcpy(tmp, &_prof_data[type], sizeof(struct nugu_prof_data));
	pthread_mutex_unlock(&_lock);

	return tmp;
}

EXPORT_API int nugu_prof_get_diff_msec_timespec(const struct timespec *ts1,
						const struct timespec *ts2)
{
	int sec;
	int nsec;

	if ((ts2->tv_nsec - ts1->tv_nsec) < 0) {
		sec = ts2->tv_sec - ts1->tv_sec - 1;
		nsec = ts2->tv_nsec - ts1->tv_nsec + 1000000000L;
	} else {
		sec = ts2->tv_sec - ts1->tv_sec;
		nsec = ts2->tv_nsec - ts1->tv_nsec;
	}

	return (nsec / 1000000) + (sec * 1000);
}

EXPORT_API int nugu_prof_get_diff_msec(const struct nugu_prof_data *prof1,
				       const struct nugu_prof_data *prof2)
{
	if (prof1 == NULL || prof2 == NULL)
		return 0;

	/* profile item empty */
	if (prof1->timestamp.tv_sec == 0 || prof2->timestamp.tv_sec == 0)
		return 0;

	return nugu_prof_get_diff_msec_timespec(&prof1->timestamp,
						&prof2->timestamp);
}

static void _fill_timeunit(int msec, char *dest, size_t dest_len)
{
	int value;
	const char *unit = "msec";
	char number[13]; /* '-2147483648 ' */

	g_return_if_fail(dest != NULL);
	g_return_if_fail(dest_len > 10);

	value = msec;
	if (value > 9999) {
		value = value / 1000;
		unit = "sec ";

		if (value > 9999) {
			value = value / 60;
			unit = "min ";

			if (value > 9999) {
				value = value / 60;
				unit = "hour";
			}
		}
	}

	snprintf(number, sizeof(number), "%5d ", value);

	/* set to '  --  ' to big number */
	if (number[6] != '\0') {
		number[0] = ' ';
		number[1] = ' ';
		number[2] = '-';
		number[3] = '-';
		number[4] = ' ';
		number[5] = ' ';
		number[6] = '\0';
	}

	memcpy(dest, number, 6);
	memcpy(dest + 6, unit, 4);
	dest[10] = '\0';
}

static void _fill_relative_part(enum nugu_prof_type type, char *dest,
				size_t dest_len)
{
	enum nugu_prof_type rel;
	int diff;
	char buf[11];

	if (dest == NULL || dest_len < 22)
		return;

	rel = _hints[type].relative_type;
	if (rel == NUGU_PROF_TYPE_MAX) {
		memset(dest, ' ', dest_len - 1);
		dest[dest_len - 1] = '\0';
		return;
	}

	diff = nugu_prof_get_diff_msec(&_prof_data[rel], &_prof_data[type]);

	_fill_timeunit(diff, buf, sizeof(buf));

	snprintf(dest, dest_len, "[%2d ~ %2d]: %s", rel, type, buf);
}

EXPORT_API void nugu_prof_dump(enum nugu_prof_type from, enum nugu_prof_type to)
{
	enum nugu_prof_type cur;
	char ts_str[255];
	char relative_part[22];
	char time_from[11];

	if ((nugu_log_get_modules() & NUGU_LOG_MODULE_PROFILING) == 0)
		return;

	nugu_log_print(NUGU_LOG_MODULE_PROFILING, NUGU_LOG_LEVEL_DEBUG, NULL,
		       NULL, -1, "--------------------------");

	nugu_log_print(NUGU_LOG_MODULE_PROFILING, NUGU_LOG_LEVEL_DEBUG, NULL,
		       NULL, -1, "Profiling: %d(%s) ~ %d(%s)", from,
		       _hints[from].text, to, _hints[to].text);

	pthread_mutex_lock(&_lock);

	/**
	 * output format:
	 *  type text: <timestamp> time [rel-part] [dialog-id] [message-id]
	 */
	for (cur = from; cur <= to; cur++) {
		const struct nugu_prof_data *prof;

		prof = &_prof_data[cur];
		if (prof->timestamp.tv_sec == 0)
			continue;

		_fill_timestr(ts_str, sizeof(ts_str), &prof->timestamp);
		_fill_timeunit(nugu_prof_get_diff_msec(&_prof_data[from], prof),
			       time_from, sizeof(time_from));
		_fill_relative_part(cur, relative_part, sizeof(relative_part));

		nugu_log_print(NUGU_LOG_MODULE_PROFILING, NUGU_LOG_LEVEL_DEBUG,
			       NULL, NULL, -1, "[%2d] %20s: <%s> %s %s %s %s",
			       cur, _hints[cur].text, ts_str, time_from,
			       relative_part, prof->dialog_id, prof->msg_id);
	}

	pthread_mutex_unlock(&_lock);

	nugu_log_print(NUGU_LOG_MODULE_PROFILING, NUGU_LOG_LEVEL_DEBUG, NULL,
		       NULL, -1, "--------------------------");
}
