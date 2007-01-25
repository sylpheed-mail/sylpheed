/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2007 Hiroyuki Yamamoto
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>

#include "prefs_toolbar.h"
#include "prefs_display_items.h"

static PrefsDisplayItem available_items[] =
{
	{T_SEPARATOR,	"separator",	N_("Separator"),	TRUE, FALSE},
	{T_GET,		"get",		N_("Get"),		FALSE, FALSE},
	{T_GET_ALL,	"get-all",	N_("Get all"),		FALSE, FALSE},
	{T_SEND_QUEUE,	"send-queue",	N_("Send"),		FALSE, FALSE},
	{T_COMPOSE,	"compose",	N_("Compose"),		FALSE, FALSE},
	{T_REPLY,	"reply",	N_("Reply"),		FALSE, FALSE},
	{T_REPLY_ALL,	"reply-all",	N_("Reply all"),	FALSE, FALSE},
	{T_FORWARD,	"forward",	N_("Forward"),		FALSE, FALSE},
	{T_DELETE,	"delete",	N_("Delete"),		FALSE, FALSE},
	{T_JUNK,	"junk",		N_("Junk"),		FALSE, FALSE},
	{T_EXECUTE,	"execute",	N_("Execute"),		FALSE, FALSE},
	{T_NEXT,	"next",		N_("Next"),		FALSE, FALSE},
	{-1,		NULL,		NULL,			FALSE, FALSE}
};

static gint default_items[] =
{
	T_GET,
	T_GET_ALL,
	T_SEPARATOR,
	T_SEND_QUEUE,
	T_SEPARATOR,
	T_COMPOSE,
	T_REPLY,
	T_REPLY_ALL,
	T_FORWARD,
	T_SEPARATOR,
	T_DELETE,
	T_JUNK,
	T_SEPARATOR,
	T_NEXT,
	-1
};

gint prefs_toolbar_open(gint *visible_items, GList **item_list)
{
	PrefsDisplayItemsDialog *dialog;
	GList *list;
	gint ret = 0;

	dialog = prefs_display_items_dialog_create();
	prefs_display_items_dialog_set_available(dialog, available_items);
	prefs_display_items_dialog_set_default_visible(dialog, default_items);
	prefs_display_items_dialog_set_visible(dialog, visible_items);

	while (dialog->finished == FALSE)
		gtk_main_iteration();

	if (dialog->cancelled) {
		ret = -1;
		*item_list = NULL;
	} else {
		list = dialog->visible_items;
		dialog->visible_items = NULL;
		*item_list = list;
	}

	prefs_display_items_dialog_destroy(dialog);

	return ret;
}

const PrefsDisplayItem *prefs_toolbar_get_item_from_name(const gchar *name)
{
	gint i;

	for (i = 0; available_items[i].id != -1; i++) {
		if (!strcmp(name, available_items[i].name))
			return &available_items[i];
	}

	return NULL;
}

const PrefsDisplayItem *prefs_toolbar_get_item_from_id(gint id)
{
	gint i;

	for (i = 0; available_items[i].id != -1; i++) {
		if (id == available_items[i].id)
			return &available_items[i];
	}

	return NULL;
}

GList *prefs_toolbar_get_item_list_from_name_list(const gchar *name_list)
{
	gint i;
	gchar **array;
	GList *list = NULL;

	array = g_strsplit(name_list, ",", 0);

	for (i = 0; array[i] != NULL; i++) {
		gchar *name = array[i];
		const PrefsDisplayItem *item;

		g_strstrip(name);
		item = prefs_toolbar_get_item_from_name(name);
		if (item)
			list = g_list_append(list, (gpointer)item);
	}

	g_strfreev(array);

	return list;
}

gint *prefs_toolbar_get_id_list_from_name_list(const gchar *name_list)
{
	gint i;
	gchar **array;
	GArray *iarray;

	iarray = g_array_new(FALSE, FALSE, sizeof(gint));
	array = g_strsplit(name_list, ",", 0);

	for (i = 0; array[i] != NULL; i++) {
		gchar *name = array[i];
		const PrefsDisplayItem *item;

		g_strstrip(name);
		item = prefs_toolbar_get_item_from_name(name);
		if (item)
			g_array_append_val(iarray, item->id);
	}

	i = -1;
	g_array_append_val(iarray, i);

	g_strfreev(array);

	return (gint *)g_array_free(iarray, FALSE);
}

gchar *prefs_toolbar_get_name_list_from_item_list(GList *item_list)
{
	GString *str;
	GList *cur;

	str = g_string_new(NULL);

	for (cur = item_list; cur != NULL; cur = cur->next) {
		const PrefsDisplayItem *item = cur->data;

		g_string_append(str, item->name);
		if (cur->next)
			g_string_append_c(str, ',');
	}

	return g_string_free(str, FALSE);
}

const gchar *prefs_toolbar_get_default_setting_name_list(void)
{
	GString *str;
	gint i;
	static gchar *default_name_list = NULL;

	if (default_name_list)
		return default_name_list;

	str = g_string_new(NULL);

	for (i = 0; default_items[i] != -1; i++) {
		const PrefsDisplayItem *item;

		item = prefs_toolbar_get_item_from_id(default_items[i]);
		if (item) {
			g_string_append(str, item->name);
			if (default_items[i + 1] != -1)
				g_string_append_c(str, ',');
		}
	}

	default_name_list = g_string_free(str, FALSE);

	return default_name_list;
}
