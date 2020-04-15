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

#include <string.h>
#include <stdlib.h>

#include <glib.h>

#include "base/nugu_log.h"
#include "base/nugu_focus.h"

static const char *const _type_str[] = { "CALL",       "ALERT", "ASR",   "TTS",
					 "ASR_EXPECT", "MEDIA", "CUSTOM" };

struct _nugu_focus {
	char *name;
	NuguFocusType type;
	NuguFocusCallback focus_cb;
	NuguUnfocusCallback unfocus_cb;
	NuguStealfocusCallback steal_cb;
	void *userdata;
};

struct _focus_item {
	NuguFocus *focus;
	void *event;
};

static GList *_focus_stack;

#define APPEND(list, x) (list = g_list_insert_sorted(list, (x), _compare_func))
#define PREPEND(list, x) (list = g_list_prepend(list, (x)))
#define REMOVE(list, x) (list = g_list_remove(list, (x)))
#define REMOVE_LINK(list, x) (list = g_list_remove_link(list, (x)))

static void dump_list(void)
{
	GList *l;
	int i;

	if (g_list_length(_focus_stack) == 0) {
		nugu_dbg("Focus length = 0 (empty)");
		return;
	}

	nugu_dbg("Focus length = %d", g_list_length(_focus_stack));

	for (l = _focus_stack, i = 0; l; l = l->next, i++) {
		struct _focus_item *item = l->data;

		nugu_dbg("\t[%d] %-10s (%d:%-10s)", i, item->focus->name,
			 item->focus->type, _type_str[item->focus->type]);
	}
}

static struct _focus_item *_find_item(NuguFocus *focus)
{
	GList *l;

	for (l = _focus_stack; l; l = l->next) {
		struct _focus_item *item = l->data;

		if (item->focus == focus)
			return item;
	}

	return NULL;
}

static void _remove_items_by_focus(NuguFocus *focus)
{
	struct _focus_item *item;

	do {
		item = _find_item(focus);
		if (!item)
			break;

		REMOVE(_focus_stack, item);
		free(item);
	} while (item);
}

EXPORT_API NuguFocus *nugu_focus_new(const char *name, NuguFocusType type,
				     NuguFocusCallback focus_cb,
				     NuguUnfocusCallback unfocus_cb,
				     NuguStealfocusCallback steal_cb,
				     void *userdata)
{
	struct _nugu_focus *focus;

	focus = malloc(sizeof(struct _nugu_focus));
	if (!focus) {
		nugu_error_nomem();
		return NULL;
	}

	focus->name = g_strdup(name);
	focus->type = type;
	focus->focus_cb = focus_cb;
	focus->unfocus_cb = unfocus_cb;
	focus->steal_cb = steal_cb;
	focus->userdata = userdata;

	return focus;
}

EXPORT_API void nugu_focus_free(NuguFocus *focus)
{
	_remove_items_by_focus(focus);

	g_free(focus->name);
	memset(focus, 0, sizeof(NuguFocus));
	free(focus);
}

EXPORT_API NuguFocusType nugu_focus_get_type(NuguFocus *focus)
{
	return focus->type;
}

EXPORT_API NuguFocus *nugu_focus_peek_top(void)
{
	struct _focus_item *item;
	GList *l = _focus_stack;

	if (l == NULL) {
		nugu_dbg("FOCUS-TOP => empty");
		return NULL;
	}

	item = l->data;
	nugu_dbg("FOCUS-TOP => %s", item->focus->name);

	return item->focus;
}

EXPORT_API const char *nugu_focus_peek_name(NuguFocus *focus)
{
	return focus->name;
}

static void _invoke_focus(struct _focus_item *item)
{
	nugu_info("ON-FOCUS: name=%s (type=%s)", item->focus->name,
		  _type_str[item->focus->type]);

	item->focus->focus_cb(item->focus, item->event, item->focus->userdata);
}

static NuguFocusResult _invoke_unfocus(struct _focus_item *item,
				       NuguUnFocusMode mode)
{
	nugu_info("ON-UN-FOCUS: name=%s (type=%s)", item->focus->name,
		  _type_str[item->focus->type]);

	return item->focus->unfocus_cb(item->focus, mode, item->event,
				       item->focus->userdata);
}

static NuguFocusStealResult _invoke_stealfocus(struct _focus_item *item,
					       NuguFocus *target)
{
	NuguFocusStealResult ret;

	nugu_info("ON-STEAL-FOCUS: name=%s (type=%s)", item->focus->name,
		  _type_str[item->focus->type]);

	ret = item->focus->steal_cb(item->focus, item->event, target,
				    item->focus->userdata);
	if (ret == NUGU_FOCUS_STEAL_ALLOW)
		nugu_info(" - ALLOW");
	else
		nugu_info(" - REJECT");

	return ret;
}

static struct _focus_item *_item_new(NuguFocus *focus, void *event)
{
	struct _focus_item *item;

	item = malloc(sizeof(struct _focus_item));
	item->event = event;
	item->focus = focus;

	return item;
}

static int _compare_func(gconstpointer a, gconstpointer b)
{
	const struct _focus_item *item_a = a;
	const struct _focus_item *item_b = b;

	return item_a->focus->type - item_b->focus->type;
}

EXPORT_API int nugu_focus_request(NuguFocus *focus, void *event)
{
	GList *l;
	struct _focus_item *item;
	NuguFocusResult ret;

	g_return_val_if_fail(focus != NULL, -1);

	nugu_info("FOCUS-REQUEST: name=%s (type=%s)", focus->name,
		  _type_str[focus->type]);

	l = _focus_stack;
	if (l == NULL) {
		/* empty focus */
		item = _item_new(focus, event);
		APPEND(_focus_stack, item);
		_invoke_focus(item);

		dump_list();

		nugu_dbg("nugu_focus_request() done");
		return 0;
	}

	dump_list();

	item = l->data;

	/* focus intercept */
	if (_invoke_stealfocus(item, focus) == NUGU_FOCUS_STEAL_ALLOW) {
		/* unfocus current focused item */
		ret = _invoke_unfocus(item, NUGU_UNFOCUS_JUDGE);
		REMOVE_LINK(_focus_stack, l);
		if (ret == NUGU_FOCUS_REMOVE) {
			nugu_dbg("return REMOVE: remove focus item");
			free(item);
		} else if (ret == NUGU_FOCUS_PAUSE) {
			nugu_dbg("return PAUSE: insert by priority");
			APPEND(_focus_stack, item);
		}

		/* remove duplicated focus */
		_remove_items_by_focus(focus);

		/* set new item to focus */
		item = _item_new(focus, event);
		PREPEND(_focus_stack, item);
		_invoke_focus(item);

		dump_list();
		nugu_dbg("nugu_focus_request() done");
		return 0;
	}

	/* remove duplicated focus */
	_remove_items_by_focus(focus);

	/* add new item to reserved list */
	nugu_dbg("insert by priority");
	item = _item_new(focus, event);
	APPEND(_focus_stack, item);

	dump_list();
	nugu_dbg("nugu_focus_request() done");
	return 0;
}

EXPORT_API int nugu_focus_release(NuguFocus *focus)
{
	GList *l;
	struct _focus_item *item;
	NuguFocusResult ret;

	g_return_val_if_fail(focus != NULL, -1);

	nugu_info("FOCUS-RELEASE: name=%s (type=%s)", focus->name,
		  _type_str[focus->type]);

	l = _focus_stack;
	if (l == NULL) {
		nugu_dbg("empty");
		return 0;
	}

	item = l->data;
	if (item->focus != focus) {
		/* focus is reserved item */
		_remove_items_by_focus(focus);

		dump_list();
		nugu_dbg("nugu_focus_release() done");
		return 0;
	}

	REMOVE(_focus_stack, item);

	ret = _invoke_unfocus(item, NUGU_UNFOCUS_JUDGE);
	if (ret == NUGU_FOCUS_REMOVE) {
		nugu_info(" - return REMOVE: remove focus item");
		free(item);
	} else if (ret == NUGU_FOCUS_PAUSE) {
		nugu_info(" - return PAUSE: insert by priority");
		APPEND(_focus_stack, item);
	}

	dump_list();

	/* no more reserved focus */
	l = _focus_stack;
	if (l == NULL) {
		nugu_dbg("nugu_focus_release() done");
		return 0;
	}

	/* set focus to next item */
	item = l->data;
	if (item->focus != focus)
		_invoke_focus(item);

	nugu_dbg("nugu_focus_release() done");

	return 0;
}

EXPORT_API void nugu_focus_release_all(void)
{
	nugu_info("FOCUS-RELEASE-ALL");

	if (_focus_stack == NULL) {
		nugu_dbg("empty");
		return;
	}

	while (_focus_stack != NULL) {
		struct _focus_item *item = _focus_stack->data;

		REMOVE(_focus_stack, item);
		_invoke_unfocus(item, NUGU_UNFOCUS_FORCE);
		nugu_info(" - REMOVE: remove focus item[name: %s, type: %s]",
			  item->focus->name, _type_str[item->focus->type]);
		free(item);
	}

	dump_list();
	nugu_dbg("nugu_focus_release_all() done");
}
