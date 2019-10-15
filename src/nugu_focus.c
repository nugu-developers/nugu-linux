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

#include "nugu_log.h"
#include "nugu_focus.h"

static const char *const _type_str[] = { "WAKEWORD",   "ASR",   "ALERT", "TTS",
					 "ASR_EXPECT", "MEDIA", "CUSTOM" };

static const char *const _rsrc_str[] = { "MIC", "SPK" };

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

static GList *_focus_stack[2];

#define APPEND(list, x) (list = g_list_insert_sorted(list, (x), _compare_func))
#define PREPEND(list, x) (list = g_list_prepend(list, (x)))
#define REMOVE(list, x) (list = g_list_remove(list, (x)))
#define REMOVE_LINK(list, x) (list = g_list_remove_link(list, (x)))

static void dump_list(void)
{
	int rsrc;

	for (rsrc = 0; rsrc < 2; rsrc++) {
		GList *l;
		int i;

		if (g_list_length(_focus_stack[rsrc]) == 0) {
			nugu_dbg("Resource: %s, length = %d (empty)",
				 _rsrc_str[rsrc],
				 g_list_length(_focus_stack[rsrc]));
			continue;
		}

		nugu_dbg("Resource: %s, length = %d", _rsrc_str[rsrc],
			 g_list_length(_focus_stack[rsrc]));

		for (l = _focus_stack[rsrc], i = 0; l; l = l->next, i++) {
			struct _focus_item *item = l->data;

			nugu_dbg("\t[%d] %-10s (%d:%-10s)", i,
				 item->focus->name, item->focus->type,
				 _type_str[item->focus->type]);
		}
	}
}

static struct _focus_item *_find_item(NuguFocus *focus, NuguFocusResource rsrc)
{
	GList *l;

	for (l = _focus_stack[rsrc]; l; l = l->next) {
		struct _focus_item *item = l->data;

		if (item->focus == focus)
			return item;
	}

	return NULL;
}

static void _remove_items_by_focus(NuguFocus *focus, NuguFocusResource rsrc)
{
	struct _focus_item *item;

	do {
		item = _find_item(focus, rsrc);
		if (!item)
			break;

		REMOVE(_focus_stack[rsrc], item);
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
	_remove_items_by_focus(focus, NUGU_FOCUS_RESOURCE_MIC);
	_remove_items_by_focus(focus, NUGU_FOCUS_RESOURCE_SPK);

	g_free(focus->name);
	memset(focus, 0, sizeof(NuguFocus));
	free(focus);
}

EXPORT_API NuguFocusType nugu_focus_get_type(NuguFocus *focus)
{
	return focus->type;
}

EXPORT_API NuguFocus *nugu_focus_peek_top(NuguFocusResource rsrc)
{
	struct _focus_item *item;
	GList *l = _focus_stack[rsrc];

	if (l == NULL) {
		nugu_dbg("FOCUS-TOP[%s] => empty", _rsrc_str[rsrc]);
		return NULL;
	}

	item = l->data;
	nugu_dbg("FOCUS-TOP[%s] => %s", _rsrc_str[rsrc], item->focus->name);
	return item->focus;
}

EXPORT_API const char *nugu_focus_peek_name(NuguFocus *focus)
{
	return focus->name;
}

static NuguFocusResult _invoke_focus(struct _focus_item *item,
				     NuguFocusResource rsrc)
{
	nugu_info("FOCUS-GET[%s]: owner_type=%s, owner=%s", _rsrc_str[rsrc],
		  _type_str[item->focus->type], item->focus->name);

	return item->focus->focus_cb(item->focus, rsrc, item->event,
				     item->focus->userdata);
}

static NuguFocusResult _invoke_unfocus(struct _focus_item *item,
				       NuguFocusResource rsrc)
{
	nugu_info("FOCUS-LOST[%s]: owner_type=%s, owner=%s", _rsrc_str[rsrc],
		  _type_str[item->focus->type], item->focus->name);

	return item->focus->unfocus_cb(item->focus, rsrc, item->event,
				       item->focus->userdata);
}

static NuguFocusStealResult _invoke_stealfocus(struct _focus_item *item,
					       NuguFocusResource rsrc,
					       NuguFocus *target)
{
	NuguFocusStealResult ret;

	ret = item->focus->steal_cb(item->focus, rsrc, item->event, target,
				    item->focus->userdata);
	if (ret == NUGU_FOCUS_STEAL_ALLOW)
		nugu_info("FOCUS-STEAL-ALLOW[%s]: owner_type=%s, owner=%s",
			  _rsrc_str[rsrc], _type_str[item->focus->type],
			  item->focus->name);
	else
		nugu_info("FOCUS-STEAL-REJECT[%s]: owner_type=%s, owner=%s",
			  _rsrc_str[rsrc], _type_str[item->focus->type],
			  item->focus->name);

	return ret;
}

static struct _focus_item *_item_new(NuguFocus *focus, NuguFocusResource rsrc,
				     void *event)
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

EXPORT_API int nugu_focus_request(NuguFocus *focus, NuguFocusResource rsrc,
				  void *event)
{
	GList *l;
	struct _focus_item *item;
	NuguFocusResult ret;

	g_return_val_if_fail(focus != NULL, -1);

	nugu_info("FOCUS-REQUEST[%s]: owner_type=%s, owner=%s", _rsrc_str[rsrc],
		  _type_str[focus->type], focus->name);

	dump_list();

	l = _focus_stack[rsrc];
	if (l == NULL) {
		/* empty focus */
		item = _item_new(focus, rsrc, event);
		APPEND(_focus_stack[rsrc], item);
		if (_invoke_focus(item, rsrc) != NUGU_FOCUS_OK) {
			REMOVE(_focus_stack[rsrc], item);
			free(item);
		}

		dump_list();

		return 0;
	}

	item = l->data;

	/* focus intercept */
	if (_invoke_stealfocus(item, rsrc, focus) == NUGU_FOCUS_STEAL_ALLOW) {
		/* unfocus current focused item */
		ret = _invoke_unfocus(item, rsrc);
		REMOVE_LINK(_focus_stack[rsrc], l);
		if (ret == NUGU_FOCUS_REMOVE) {
			nugu_dbg("return REMOVE: remove focus item");
			free(item);
		} else if (ret == NUGU_FOCUS_PAUSE) {
			nugu_dbg("return PAUSE: insert by priority");
			APPEND(_focus_stack[rsrc], item);
		}

		/* remove duplicated focus */
		_remove_items_by_focus(focus, rsrc);

		/* set new item to focus */
		item = _item_new(focus, rsrc, event);
		PREPEND(_focus_stack[rsrc], item);
		if (_invoke_focus(item, rsrc) != NUGU_FOCUS_OK) {
			REMOVE(_focus_stack[rsrc], item);
			free(item);
		}

		dump_list();
		return 0;
	}

	/* remove duplicated focus */
	_remove_items_by_focus(focus, rsrc);

	/* add new item to reserved list */
	nugu_dbg("insert by priority");
	item = _item_new(focus, rsrc, event);
	APPEND(_focus_stack[rsrc], item);

	dump_list();
	return 0;
}

EXPORT_API int nugu_focus_release(NuguFocus *focus, NuguFocusResource rsrc)
{
	GList *l;
	struct _focus_item *item;
	NuguFocusResult ret;

	g_return_val_if_fail(focus != NULL, -1);

	nugu_info("FOCUS-RELEASE[%s]: owner_type=%s, owner=%s", _rsrc_str[rsrc],
		  _type_str[focus->type], focus->name);

	dump_list();

	l = _focus_stack[rsrc];
	if (l == NULL)
		return 0;

	item = l->data;
	if (item->focus != focus) {
		/* foscus is reserved item */
		_remove_items_by_focus(focus, rsrc);

		dump_list();
		return 0;
	}

	REMOVE(_focus_stack[rsrc], item);

	ret = _invoke_unfocus(item, rsrc);
	if (ret == NUGU_FOCUS_REMOVE) {
		nugu_info("return REMOVE: remove focus item");
		free(item);
	} else if (ret == NUGU_FOCUS_PAUSE) {
		nugu_dbg("return PAUSE: insert by priority");
		APPEND(_focus_stack[rsrc], item);
	}

	dump_list();

	/* no more reserved focus */
	l = _focus_stack[rsrc];
	if (l == NULL)
		return 0;

	/* set focus to next item */
	item = l->data;
	if (item->focus != focus)
		_invoke_focus(item, rsrc);

	return 0;
}
