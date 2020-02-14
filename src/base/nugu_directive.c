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
#include "base/nugu_buffer.h"
#include "base/nugu_directive.h"

struct _nugu_directive {
	char *name_space;
	char *name;
	char *version;
	char *msg_id;
	char *dialog_id;
	char *json;
	char *referrer_id;
	char *groups;

	int is_active;
	int is_end;

	char *media_type;
	NuguBuffer *buf;
	DirectiveDataCallback callback;
	void *callback_userdata;

	int ref_count;
};

EXPORT_API NuguDirective *
nugu_directive_new(const char *name_space, const char *name,
		   const char *version, const char *msg_id,
		   const char *dialog_id, const char *referrer_id,
		   const char *json, const char *groups)
{
	NuguDirective *ndir;

	g_return_val_if_fail(name_space != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);
	g_return_val_if_fail(version != NULL, NULL);
	g_return_val_if_fail(msg_id != NULL, NULL);
	g_return_val_if_fail(dialog_id != NULL, NULL);
	g_return_val_if_fail(json != NULL, NULL);
	g_return_val_if_fail(groups != NULL, NULL);

	ndir = calloc(1, sizeof(NuguDirective));
	ndir->name_space = strdup(name_space);
	ndir->name = strdup(name);
	ndir->version = strdup(version);
	ndir->msg_id = strdup(msg_id);
	ndir->dialog_id = strdup(dialog_id);
	ndir->referrer_id = strdup(referrer_id);
	ndir->json = strdup(json);
	ndir->groups = strdup(groups);

	ndir->is_active = 0;
	ndir->buf = nugu_buffer_new(0);
	ndir->media_type = NULL;
	ndir->ref_count = 1;

	return ndir;
}

static void nugu_directive_free(NuguDirective *ndir)
{
	g_return_if_fail(ndir != NULL);

	nugu_info("destroy: %s.%s 'id=%s'", ndir->name_space, ndir->name,
		  ndir->msg_id);

	free(ndir->name_space);
	ndir->name_space = NULL;

	free(ndir->name);
	ndir->name = NULL;

	free(ndir->version);
	ndir->version = NULL;

	free(ndir->msg_id);
	ndir->msg_id = NULL;

	free(ndir->dialog_id);
	ndir->dialog_id = NULL;

	free(ndir->referrer_id);
	ndir->referrer_id = NULL;

	free(ndir->json);
	ndir->json = NULL;

	free(ndir->groups);
	ndir->groups = NULL;

	nugu_buffer_free(ndir->buf, 1);
	ndir->buf = NULL;

	if (ndir->media_type)
		free(ndir->media_type);

	memset(ndir, 0, sizeof(NuguDirective));
	free(ndir);
}

EXPORT_API void nugu_directive_ref(NuguDirective *ndir)
{
	g_return_if_fail(ndir != NULL);

	ndir->ref_count++;
}

EXPORT_API void nugu_directive_unref(NuguDirective *ndir)
{
	g_return_if_fail(ndir != NULL);

	ndir->ref_count--;
	if (ndir->ref_count > 0)
		return;

	nugu_directive_free(ndir);
}

EXPORT_API int nugu_directive_is_active(NuguDirective *ndir)
{
	g_return_val_if_fail(ndir != NULL, -1);

	return ndir->is_active;
}

EXPORT_API int nugu_directive_set_active(NuguDirective *ndir, int flag)
{
	g_return_val_if_fail(ndir != NULL, -1);
	g_return_val_if_fail(flag == 0 || flag == 1, -1);

	ndir->is_active = flag;

	return ndir->is_active;
}

EXPORT_API const char *nugu_directive_peek_namespace(NuguDirective *ndir)
{
	g_return_val_if_fail(ndir != NULL, NULL);

	return ndir->name_space;
}

EXPORT_API const char *nugu_directive_peek_name(NuguDirective *ndir)
{
	g_return_val_if_fail(ndir != NULL, NULL);

	return ndir->name;
}

EXPORT_API const char *nugu_directive_peek_groups(NuguDirective *ndir)
{
	g_return_val_if_fail(ndir != NULL, NULL);

	return ndir->groups;
}

EXPORT_API const char *nugu_directive_peek_version(NuguDirective *ndir)
{
	g_return_val_if_fail(ndir != NULL, NULL);

	return ndir->version;
}

EXPORT_API const char *nugu_directive_peek_msg_id(NuguDirective *ndir)
{
	g_return_val_if_fail(ndir != NULL, NULL);

	return ndir->msg_id;
}

EXPORT_API const char *nugu_directive_peek_dialog_id(NuguDirective *ndir)
{
	g_return_val_if_fail(ndir != NULL, NULL);

	return ndir->dialog_id;
}

EXPORT_API const char *nugu_directive_peek_referrer_id(NuguDirective *ndir)
{
	g_return_val_if_fail(ndir != NULL, NULL);

	return ndir->referrer_id;
}

EXPORT_API const char *nugu_directive_peek_json(NuguDirective *ndir)
{
	g_return_val_if_fail(ndir != NULL, NULL);

	return ndir->json;
}

EXPORT_API int nugu_directive_set_media_type(NuguDirective *ndir,
					     const char *type)
{
	g_return_val_if_fail(ndir != NULL, -1);

	if (ndir->media_type) {
		free(ndir->media_type);
		ndir->media_type = NULL;
	}

	if (type == NULL)
		return 0;

	ndir->media_type = strdup(type);

	return 0;
}

EXPORT_API const char *nugu_directive_peek_media_type(NuguDirective *ndir)
{
	g_return_val_if_fail(ndir != NULL, NULL);

	return ndir->media_type;
}

EXPORT_API int nugu_directive_add_data(NuguDirective *ndir, size_t length,
				       unsigned char *data)
{
	g_return_val_if_fail(ndir != NULL, -1);

	if (length > 0) {
		size_t written;

		if (!data) {
			nugu_error("invalid input (data is NULL)");
			return -1;
		}

		written = nugu_buffer_add(ndir->buf, data, length);
		if (written != length) {
			nugu_error("buffer_add failed() (%zd != %zd)", written,
				   length);
			return -1;
		}
	}

	if (nugu_directive_is_active(ndir) == 0) {
		nugu_dbg("skip callback. directive is not active");
		return 0;
	}

	if (ndir->callback)
		ndir->callback(ndir, ndir->callback_userdata);

	return 0;
}

EXPORT_API int nugu_directive_close_data(NuguDirective *ndir)
{
	g_return_val_if_fail(ndir != NULL, -1);

	ndir->is_end = 1;

	return 0;
}

EXPORT_API int nugu_directive_is_data_end(NuguDirective *ndir)
{
	g_return_val_if_fail(ndir != NULL, -1);

	return ndir->is_end;
}

EXPORT_API unsigned char *nugu_directive_get_data(NuguDirective *ndir,
						  size_t *length)
{
	unsigned char *buf;
	size_t buf_size;

	g_return_val_if_fail(ndir != NULL, NULL);

	buf_size = nugu_buffer_get_size(ndir->buf);

	if (length)
		*length = buf_size;

	if (buf_size == 0)
		return NULL;

	buf = nugu_buffer_pop(ndir->buf, 0);

	return buf;
}

EXPORT_API size_t nugu_directive_get_data_size(NuguDirective *ndir)
{
	size_t size;

	g_return_val_if_fail(ndir != NULL, 0);

	size = nugu_buffer_get_size(ndir->buf);

	return size;
}

EXPORT_API int nugu_directive_set_data_callback(NuguDirective *ndir,
						DirectiveDataCallback callback,
						void *userdata)
{
	g_return_val_if_fail(ndir != NULL, -1);
	g_return_val_if_fail(callback != NULL, -1);

	ndir->callback = callback;
	ndir->callback_userdata = userdata;

	return 0;
}

EXPORT_API int nugu_directive_remove_data_callback(NuguDirective *ndir)
{
	g_return_val_if_fail(ndir != NULL, -1);

	ndir->callback = NULL;
	ndir->callback_userdata = NULL;

	return 0;
}
