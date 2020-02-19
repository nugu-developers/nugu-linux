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
#include <string.h>
#include <stdlib.h>

#include <glib.h>

#include "base/nugu_log.h"
#include "base/nugu_directive_sequencer.h"

static GList *list_dir;
static NuguDirseqCallback _callback;
static void *_callback_userdata;

EXPORT_API int nugu_dirseq_set_callback(NuguDirseqCallback callback,
					void *userdata)
{
	_callback = callback;
	_callback_userdata = userdata;

	return 0;
}

EXPORT_API void nugu_dirseq_unset_callback(void)
{
	_callback = NULL;
	_callback_userdata = NULL;
}

EXPORT_API int nugu_dirseq_push(NuguDirective *ndir)
{
	NuguDirseqReturn ret = NUGU_DIRSEQ_FAILURE;

	list_dir = g_list_append(list_dir, ndir);
	nugu_dbg("added: %s.%s 'id=%s'", nugu_directive_peek_namespace(ndir),
		 nugu_directive_peek_name(ndir),
		 nugu_directive_peek_msg_id(ndir));

	nugu_directive_set_active(ndir, 1);

	if (_callback)
		ret = _callback(ndir, _callback_userdata);

	if (ret == NUGU_DIRSEQ_FAILURE) {
		nugu_info("fail to handle the directive. remove!");
		nugu_dirseq_complete(ndir);
	}

	return 0;
}

EXPORT_API int nugu_dirseq_complete(NuguDirective *ndir)
{
	GList *l;

	g_return_val_if_fail(ndir != NULL, -1);

	l = g_list_find(list_dir, ndir);
	if (!l)
		return -1;

	list_dir = g_list_delete_link(list_dir, l);

	nugu_directive_unref(ndir);

	nugu_dbg("remain directives: %d", g_list_length(list_dir));

	return 0;
}

static gint _compare_msgid(gconstpointer a, gconstpointer b)
{
	return g_strcmp0(nugu_directive_peek_msg_id((NuguDirective *)a), b);
}

EXPORT_API NuguDirective *nugu_dirseq_find_by_msgid(const char *msgid)
{
	GList *found;

	g_return_val_if_fail(msgid != NULL, NULL);
	g_return_val_if_fail(strlen(msgid) > 0, NULL);

	found = g_list_find_custom(list_dir, msgid, _compare_msgid);
	if (found)
		return found->data;

	nugu_dbg("can't find msgid('%s')", msgid);

	return NULL;
}

static void _free_directive(gpointer data)
{
	nugu_dbg("remove remain directive: %s.%s 'id=%s'",
		 nugu_directive_peek_namespace(data),
		 nugu_directive_peek_name(data),
		 nugu_directive_peek_msg_id(data));

	nugu_dirseq_complete(data);
}

void nugu_dirseq_deinitialize(void)
{
	_callback = NULL;
	_callback_userdata = NULL;

	if (list_dir)
		g_list_free_full(list_dir, _free_directive);
	list_dir = NULL;
}
