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

#include "base/nugu_log.h"
#include "base/nugu_uuid.h"
#include "base/nugu_event.h"

#define TPL_EVENT                                                              \
	"{\n"                                                                  \
	"  \"context\": %s,\n"                                                 \
	"  \"event\": {\n"                                                     \
	"    \"header\": {\n"                                                  \
	"      \"dialogRequestId\": \"%s\",\n"                                 \
	"      \"messageId\": \"%s\",\n"                                       \
	"      \"name\": \"%s\",\n"                                            \
	"      \"namespace\": \"%s\",\n"                                       \
	"      \"version\": \"%s\"\n"                                          \
	"    },\n"                                                             \
	"    \"payload\": %s\n"                                                \
	"  }\n"                                                                \
	"}"

#define TPL_EVENT_REFERRER                                                     \
	"{\n"                                                                  \
	"  \"context\": %s,\n"                                                 \
	"  \"event\": {\n"                                                     \
	"    \"header\": {\n"                                                  \
	"      \"referrerDialogRequestId\": \"%s\",\n"                         \
	"      \"dialogRequestId\": \"%s\",\n"                                 \
	"      \"messageId\": \"%s\",\n"                                       \
	"      \"name\": \"%s\",\n"                                            \
	"      \"namespace\": \"%s\",\n"                                       \
	"      \"version\": \"%s\"\n"                                          \
	"    },\n"                                                             \
	"    \"payload\": %s\n"                                                \
	"  }\n"                                                                \
	"}"

struct _nugu_event {
	char *name_space;
	char *name;
	char *version;
	char *msg_id;
	char *dialog_id;
	char *referrer_id;

	int seq;
	char *json;
	char *context;

	enum nugu_event_type type;
};

EXPORT_API NuguEvent *nugu_event_new(const char *name_space, const char *name,
				     const char *version)
{
	NuguEvent *nev;

	g_return_val_if_fail(name_space != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);
	g_return_val_if_fail(version != NULL, NULL);

	nev = calloc(1, sizeof(NuguEvent));
	if (!nev) {
		nugu_error_nomem();
		return NULL;
	}

	nev->name_space = strdup(name_space);
	nev->name = strdup(name);
	nev->dialog_id = nugu_uuid_generate_time();
	nev->msg_id = nugu_uuid_generate_time();
	nev->referrer_id = NULL;
	nev->seq = 0;
	nev->version = strdup(version);
	nev->type = NUGU_EVENT_TYPE_DEFAULT;

	return nev;
}

EXPORT_API void nugu_event_free(NuguEvent *nev)
{
	g_return_if_fail(nev != NULL);

	free(nev->name_space);
	free(nev->name);
	free(nev->msg_id);
	free(nev->dialog_id);
	free(nev->version);

	if (nev->referrer_id)
		free(nev->referrer_id);

	if (nev->json)
		free(nev->json);

	if (nev->context)
		free(nev->context);

	memset(nev, 0, sizeof(NuguEvent));
	free(nev);
}

EXPORT_API const char *nugu_event_peek_namespace(NuguEvent *nev)
{
	g_return_val_if_fail(nev != NULL, NULL);

	return nev->name_space;
}

EXPORT_API const char *nugu_event_peek_name(NuguEvent *nev)
{
	g_return_val_if_fail(nev != NULL, NULL);

	return nev->name;
}

EXPORT_API const char *nugu_event_peek_version(NuguEvent *nev)
{
	g_return_val_if_fail(nev != NULL, NULL);

	return nev->version;
}

EXPORT_API int nugu_event_set_context(NuguEvent *nev, const char *context)
{
	g_return_val_if_fail(nev != NULL, -1);
	g_return_val_if_fail(context != NULL, -1);

	if (nev->context)
		free(nev->context);

	nev->context = strdup(context);

	return 0;
}

EXPORT_API const char *nugu_event_peek_context(NuguEvent *nev)
{
	g_return_val_if_fail(nev != NULL, NULL);

	return nev->context;
}

EXPORT_API int nugu_event_set_json(NuguEvent *nev, const char *json)
{
	g_return_val_if_fail(nev != NULL, -1);

	if (nev->json)
		free(nev->json);

	if (json)
		nev->json = strdup(json);
	else
		nev->json = NULL;

	return 0;
}

EXPORT_API const char *nugu_event_peek_json(NuguEvent *nev)
{
	return nev->json;
}

EXPORT_API const char *nugu_event_peek_msg_id(NuguEvent *nev)
{
	g_return_val_if_fail(nev != NULL, NULL);

	return nev->msg_id;
}

EXPORT_API int nugu_event_set_dialog_id(NuguEvent *nev, const char *dialog_id)
{
	g_return_val_if_fail(nev != NULL, -1);
	g_return_val_if_fail(dialog_id != NULL, -1);

	if (nev->dialog_id)
		free(nev->dialog_id);

	nev->dialog_id = strdup(dialog_id);

	return 0;
}

EXPORT_API const char *nugu_event_peek_dialog_id(NuguEvent *nev)
{
	g_return_val_if_fail(nev != NULL, NULL);

	return nev->dialog_id;
}

EXPORT_API int nugu_event_set_referrer_id(NuguEvent *nev,
					  const char *referrer_id)
{
	g_return_val_if_fail(nev != NULL, -1);

	if (nev->referrer_id)
		free(nev->referrer_id);

	if (referrer_id)
		nev->referrer_id = strdup(referrer_id);
	else
		nev->referrer_id = NULL;

	return 0;
}

EXPORT_API const char *nugu_event_peek_referrer_id(NuguEvent *nev)
{
	g_return_val_if_fail(nev != NULL, NULL);

	return nev->referrer_id;
}

EXPORT_API int nugu_event_get_seq(NuguEvent *nev)
{
	g_return_val_if_fail(nev != NULL, -1);

	return nev->seq;
}

EXPORT_API int nugu_event_increase_seq(NuguEvent *nev)
{
	g_return_val_if_fail(nev != NULL, -1);

	nev->seq++;

	return nev->seq;
}

EXPORT_API char *nugu_event_generate_payload(NuguEvent *nev)
{
	const char *payload;
	gchar *buf;

	g_return_val_if_fail(nev != NULL, NULL);

	if (nev->json)
		payload = nev->json;
	else
		payload = "{}";

	if (nev->referrer_id)
		buf = g_strdup_printf(TPL_EVENT_REFERRER, nev->context,
				      nev->referrer_id, nev->dialog_id,
				      nev->msg_id, nev->name, nev->name_space,
				      nev->version, payload);
	else
		buf = g_strdup_printf(TPL_EVENT, nev->context, nev->dialog_id,
				      nev->msg_id, nev->name, nev->name_space,
				      nev->version, payload);

	return buf;
}

EXPORT_API int nugu_event_set_type(NuguEvent *nev, enum nugu_event_type type)
{
	g_return_val_if_fail(nev != NULL, -1);

	nev->type = type;

	return 0;
}

EXPORT_API enum nugu_event_type nugu_event_get_type(NuguEvent *nev)
{
	g_return_val_if_fail(nev != NULL, NUGU_EVENT_TYPE_DEFAULT);

	return nev->type;
}
