/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2008 Hiroyuki Yamamoto
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
#include <gtk/gtkmain.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkstock.h>

#include <string.h>

#include "prefs_toolbar.h"
#include "prefs_display_items.h"

static PrefsDisplayItem all_items[] =
{
	{T_SEPARATOR,	"separator",	N_("---- Separator ----"),
	 NULL,	0, NULL, TRUE, FALSE},
	{T_GET,		"get",		N_("Get"),
	 N_("Incorporate new mail"),
	 STOCK_PIXMAP_MAIL_RECEIVE, NULL, FALSE, FALSE},
	{T_GET_ALL,	"get-all",	N_("Get all"),
	 N_("Incorporate new mail of all accounts"),
	 STOCK_PIXMAP_MAIL_RECEIVE_ALL,	NULL, FALSE, FALSE},
	{T_REMOTE_MAILBOX,	"remote-mailbox",	N_("Remote mailbox"),
	 N_("POP3 Remote mailbox"),
	 STOCK_PIXMAP_REMOTE_MAILBOX, GTK_STOCK_NETWORK, FALSE, FALSE},
	{T_SEND_QUEUE,	"send-queue",	N_("Send"),
	 N_("Send queued message(s)"),
	 STOCK_PIXMAP_MAIL_SEND, NULL, FALSE, FALSE},
	{T_COMPOSE,	"compose",	N_("Compose"),
	 N_("Compose new message"),
	 STOCK_PIXMAP_MAIL_COMPOSE, NULL, FALSE, FALSE},
	{T_REPLY,	"reply",	N_("Reply"),
	 N_("Reply to the message"),
	 STOCK_PIXMAP_MAIL_REPLY, NULL, FALSE, FALSE},
	{T_REPLY_ALL,	"reply-all",	N_("Reply all"),
	 N_("Reply to all"),
	 STOCK_PIXMAP_MAIL_REPLY_TO_ALL, NULL, FALSE, FALSE},
	{T_FORWARD,	"forward",	N_("Forward"),
	 N_("Forward the message"),
	 STOCK_PIXMAP_MAIL_FORWARD, NULL, FALSE, FALSE},
	{T_DELETE,	"delete",	N_("Delete"),
	 N_("Delete the message"),
	 STOCK_PIXMAP_DELETE, NULL, FALSE, FALSE},
	{T_JUNK,	"junk",		N_("Junk"),
	 N_("Set as junk mail"),
	 STOCK_PIXMAP_SPAM, NULL, FALSE, FALSE},
	{T_NOTJUNK,	"not-junk",	N_("Not junk"),
	 N_("Set as not junk mail"),
	 STOCK_PIXMAP_NOTSPAM, NULL, FALSE, FALSE},
	{T_NEXT,	"next",		N_("Next"),
	 N_("Next unread message"),
	 STOCK_PIXMAP_NEXT, GTK_STOCK_GO_DOWN, FALSE, FALSE},
	{T_PREV,	"prev",		N_("Prev"),
	 N_("Previous unread message"),
	 STOCK_PIXMAP_PREV, GTK_STOCK_GO_UP, FALSE, FALSE},
	{T_SEARCH,	"search",	N_("Search"),
	 N_("Search messages"),
	 STOCK_PIXMAP_SEARCH, GTK_STOCK_FIND, FALSE, FALSE},
	{T_PRINT,	"print",	N_("Print"),
	 N_("Print message"),
	 STOCK_PIXMAP_PRINT, GTK_STOCK_PRINT, FALSE, FALSE},
	{T_STOP,	"stop",		N_("Stop"),
	 N_("Stop receiving"),
	 STOCK_PIXMAP_STOP, GTK_STOCK_STOP, FALSE, FALSE},
	{T_ADDRESS_BOOK,	"address-book",	N_("Address"),
	 N_("Address book"),
	 STOCK_PIXMAP_ADDRESS_BOOK, NULL, FALSE, FALSE},
	{T_EXECUTE,	"execute",	N_("Execute"),
	 N_("Execute marked process"),
	 STOCK_PIXMAP_EXECUTE, GTK_STOCK_EXECUTE, FALSE, FALSE},
	{T_COMMON_PREFS,	"common-prefs",	N_("Prefs"),
	 N_("Common preferences"),
	 STOCK_PIXMAP_COMMON_PREFS, GTK_STOCK_PREFERENCES, FALSE, FALSE},
	{T_ACCOUNT_PREFS,	"account-prefs", N_("Account"),
	 N_("Account preferences"),
	 STOCK_PIXMAP_ACCOUNT_PREFS, GTK_STOCK_PREFERENCES, FALSE, FALSE},

	{T_SEND,	"send",		N_("Send"),
	 N_("Send message"),
	 STOCK_PIXMAP_MAIL_SEND, NULL, FALSE, FALSE},
	{T_SEND_LATER,	"send-later",	N_("Send later"),
	 N_("Put into queue folder and send later"),
	 STOCK_PIXMAP_MAIL_SEND_QUEUE, NULL, FALSE, FALSE},
	{T_DRAFT,	"draft",	N_("Draft"),
	 N_("Save to draft folder"),
	 STOCK_PIXMAP_SAVE, GTK_STOCK_SAVE, FALSE, FALSE},
	{T_INSERT_FILE,	"insert-file",	N_("Insert"),
	 N_("Insert file"),
	 STOCK_PIXMAP_INSERT_FILE, NULL, FALSE, FALSE},
	{T_ATTACH_FILE,	"attach-file",	N_("Attach"),
	 N_("Attach file"),
	 STOCK_PIXMAP_MAIL_ATTACH, NULL, FALSE, FALSE},
	{T_SIGNATURE,	"signature",	N_("Signature"),
	 N_("Append signature"),
	 STOCK_PIXMAP_SIGN, NULL, FALSE, FALSE},
	{T_EDITOR,	"editor",	N_("Editor"),
	 N_("Edit with external editor"),
	 STOCK_PIXMAP_EDIT, GTK_STOCK_EDIT, FALSE, FALSE},
	{T_LINEWRAP,	"linewrap",	N_("Linewrap"),
	 N_("Wrap all long lines"),
	 STOCK_PIXMAP_LINEWRAP, NULL, FALSE, FALSE},

	{-1,		NULL,		NULL,			FALSE, FALSE}
};

static gint main_available_items[] =
{
	T_SEPARATOR,
	T_GET,
	T_GET_ALL,
	T_REMOTE_MAILBOX,
	T_SEND_QUEUE,
	T_COMPOSE,
	T_REPLY,
	T_REPLY_ALL,
	T_FORWARD,
	T_DELETE,
	T_JUNK,
	T_NOTJUNK,
	T_NEXT,
	T_PREV,
	T_SEARCH,
	T_PRINT,
	T_STOP,
	T_ADDRESS_BOOK,
	T_EXECUTE,
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
	T_COMMON_PREFS,
	T_ACCOUNT_PREFS,
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
	T_SEPARATOR,
	T_SEARCH,
	T_PRINT,
	T_ADDRESS_BOOK,
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
			     "the order by using the Up / Down button."));

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
