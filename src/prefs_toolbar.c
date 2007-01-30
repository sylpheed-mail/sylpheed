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
#include <gtk/gtkwindow.h>
#include <gtk/gtklabel.h>

#include "prefs_toolbar.h"
#include "prefs_display_items.h"

static PrefsDisplayItem all_items[] =
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
	{T_PREV,	"prev",		N_("Prev"),		FALSE, FALSE},
	{T_ADDRESS_BOOK,"address-book",	N_("Address"),		FALSE, FALSE},
	{T_PRINT,	"print",	N_("Print"),		FALSE, FALSE},
	{T_COMMON_PREFS,"common-prefs",	N_("Prefs"),		FALSE, FALSE},
	{T_ACCOUNT_PREFS,
			"account-prefs",N_("Account"),		FALSE, FALSE},

	{T_SEND,	"send",		N_("Send"),		FALSE, FALSE},
	{T_SEND_LATER,	"send-later",	N_("Send later"),	FALSE, FALSE},
	{T_DRAFT,	"draft",	N_("Draft"),		FALSE, FALSE},
	{T_INSERT_FILE,	"insert-file",	N_("Insert"),		FALSE, FALSE},
	{T_ATTACH_FILE,	"attach-file",	N_("Attach"),		FALSE, FALSE},
	{T_SIGNATURE,	"signature",	N_("Signature"),	FALSE, FALSE},
	{T_EDITOR,	"editor",	N_("Editor"),		FALSE, FALSE},
	{T_LINEWRAP,	"linewrap",	N_("Linewrap"),		FALSE, FALSE},

	{-1,		NULL,		NULL,			FALSE, FALSE}
};

static gint main_available_items[] =
{
	T_SEPARATOR,
	T_GET,
	T_GET_ALL,
	T_SEND_QUEUE,
	T_COMPOSE,
	T_REPLY,
	T_REPLY_ALL,
	T_FORWARD,
	T_DELETE,
	T_JUNK,
	T_EXECUTE,
	T_NEXT,
	T_PREV,
	T_ADDRESS_BOOK,
	T_PRINT,
	T_COMMON_PREFS,
	T_ACCOUNT_PREFS,
	-1
};

static gint compose_available_items[] =
{
	T_SEPARATOR,
	T_SEND,
	T_SEND_LATER,
	T_DRAFT,
	T_INSERT_FILE,
	T_ATTACH_FILE,
	T_SIGNATURE,
	T_EDITOR,
	T_LINEWRAP,
	T_ADDRESS_BOOK,
	-1
};

static gint default_main_items[] =
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

static gint default_compose_items[] =
{
	T_SEND,
	T_SEND_LATER,
	T_DRAFT,
	T_SEPARATOR,
	T_INSERT_FILE,
	T_ATTACH_FILE,
	T_SEPARATOR,
	T_SIGNATURE,
	T_SEPARATOR,
	T_EDITOR,
	T_LINEWRAP,
	T_SEPARATOR,
	T_ADDRESS_BOOK,
	-1
};

gint prefs_toolbar_open(ToolbarType type, gint *visible_items,
			GList **item_list)
{
	PrefsDisplayItemsDialog *dialog;
	GList *list;
	gint ret = 0;

	dialog = prefs_display_items_dialog_create();
	gtk_window_set_title(GTK_WINDOW(dialog->window),
			     _("Customize toolbar"));
	gtk_label_set_text(GTK_LABEL(dialog->label),
			   _("Select items to be displayed on the toolbar. You can modify\n"
			     "the order by using the Up / Down button, or dragging the items."));

	switch (type) {
	case TOOLBAR_MAIN:
		prefs_display_items_dialog_set_available
			(dialog, all_items, main_available_items);
		prefs_display_items_dialog_set_default_visible
			(dialog, default_main_items);
		break;
	case TOOLBAR_COMPOSE:
	default:
		prefs_display_items_dialog_set_available
			(dialog, all_items, compose_available_items);
		prefs_display_items_dialog_set_default_visible
			(dialog, default_compose_items);
		break;
	}

	prefs_display_items_dialog_set_visible(dialog, visible_items);

	gtk_widget_show(dialog->window);

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

	for (i = 0; all_items[i].id != -1; i++) {
		if (!strcmp(name, all_items[i].name))
			return &all_items[i];
	}

	return NULL;
}

const PrefsDisplayItem *prefs_toolbar_get_item_from_id(gint id)
{
	gint i;

	for (i = 0; all_items[i].id != -1; i++) {
		if (id == all_items[i].id)
			return &all_items[i];
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

const gchar *prefs_toolbar_get_default_main_setting_name_list(void)
{
	GString *str;
	gint i;
	static gchar *default_name_list = NULL;

	if (default_name_list)
		return default_name_list;

	str = g_string_new(NULL);

	for (i = 0; default_main_items[i] != -1; i++) {
		const PrefsDisplayItem *item;

		item = prefs_toolbar_get_item_from_id(default_main_items[i]);
		if (item) {
			g_string_append(str, item->name);
			if (default_main_items[i + 1] != -1)
				g_string_append_c(str, ',');
		}
	}

	default_name_list = g_string_free(str, FALSE);

	return default_name_list;
}

const gchar *prefs_toolbar_get_default_compose_setting_name_list(void)
{
	GString *str;
	gint i;
	static gchar *default_name_list = NULL;

	if (default_name_list)
		return default_name_list;

	str = g_string_new(NULL);

	for (i = 0; default_compose_items[i] != -1; i++) {
		const PrefsDisplayItem *item;

		item = prefs_toolbar_get_item_from_id(default_compose_items[i]);
		if (item) {
			g_string_append(str, item->name);
			if (default_compose_items[i + 1] != -1)
				g_string_append_c(str, ',');
		}
	}

	default_name_list = g_string_free(str, FALSE);

	return default_name_list;
}
