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

struct _dirseq_callback {
	DirseqCallback callback;
	void *userdata;
};

static GList *list_dir;
static GList *list_callbacks;

static gint _compare_callback(gconstpointer a, gconstpointer b)
{
	struct _dirseq_callback *cb = ((GList *)a)->data;

	if (cb->callback == b)
		return 0;

	return -1;
}

EXPORT_API int nugu_dirseq_add_callback(DirseqCallback callback, void *userdata)
{
	struct _dirseq_callback *cb;

	g_return_val_if_fail(callback != NULL, -1);

	if (g_list_find_custom(list_callbacks, callback, _compare_callback)) {
		nugu_warn("callback already registered");
		return 0;
	}

	cb = malloc(sizeof(struct _dirseq_callback));
	cb->callback = callback;
	cb->userdata = userdata;

	list_callbacks = g_list_append(list_callbacks, cb);

	return 0;
}

EXPORT_API int nugu_dirseq_remove_callback(DirseqCallback callback)
{
	GList *l;

	g_return_val_if_fail(callback != NULL, -1);

	l = list_callbacks;
	while (l != NULL) {
		struct _dirseq_callback *cb = l->data;

		if (cb->callback != callback) {
			l = l->next;
			continue;
		}

		free(cb);
		list_callbacks = g_list_delete_link(list_callbacks, l);
		l = list_callbacks;
	}

	return 0;
}

static DirseqReturn nugu_dirseq_emit_callback(NuguDirective *ndir)
{
	GList *l;
	struct _dirseq_callback *cb;
	DirseqReturn ret;

	g_return_val_if_fail(ndir != NULL, -1);

	for (l = list_callbacks; l; l = l->next) {
		cb = l->data;
		if (!cb)
			continue;
		if (!cb->callback)
			continue;

		ret = cb->callback(ndir, cb->userdata);
		if (ret != DIRSEQ_CONTINUE)
			break;
	}

	return ret;
}

EXPORT_API int nugu_dirseq_push(NuguDirective *ndir)
{
	DirseqReturn ret;

	list_dir = g_list_append(list_dir, ndir);
	nugu_dbg("added: %s.%s 'id=%s'", nugu_directive_peek_namespace(ndir),
	    nugu_directive_peek_name(ndir), nugu_directive_peek_msg_id(ndir));

	nugu_directive_set_active(ndir, 1);
	ret = nugu_dirseq_emit_callback(ndir);
	if (ret == DIRSEQ_REMOVE) {
		nugu_dbg("directive removed");
		list_dir = g_list_remove(list_dir, ndir);
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
	NuguDirective *ndir = NULL;

	g_return_val_if_fail(msgid != NULL, NULL);
	g_return_val_if_fail(strlen(msgid) > 0, NULL);

	found = g_list_find_custom(list_dir, msgid, _compare_msgid);
	if (found)
		ndir = found->data;

	if (ndir)
		return ndir;

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
	if (list_callbacks)
		g_list_free_full(list_callbacks, g_free);
	list_callbacks = NULL;

	if (list_dir)
		g_list_free_full(list_dir, _free_directive);
	list_dir = NULL;
}
