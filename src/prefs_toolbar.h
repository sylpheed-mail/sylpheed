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

#ifndef __PREFS_TOOLBAR_H__
#define __PREFS_TOOLBAR_H__

#include <glib.h>
#include <gtk/gtkwidget.h>

typedef struct _PrefsToolbarItem	PrefsToolbarItem;

#include "prefs_display_items.h"

typedef enum
{
	TOOLBAR_MAIN,
	TOOLBAR_COMPOSE
} ToolbarType;

typedef enum
{
	T_SEPARATOR,

	/* Main */
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
	T_EXECUTE,
	T_NEXT,
	T_PREV,
	T_ADDRESS_BOOK,
	T_SEARCH,
	T_PRINT,
	T_STOP,
	T_COMMON_PREFS,
	T_ACCOUNT_PREFS,

	/* Compose */
	T_SEND,
	T_SEND_LATER,
	T_DRAFT,
	T_INSERT_FILE,
	T_ATTACH_FILE,
	T_SIGNATURE,
	T_EDITOR,
	T_LINEWRAP
} ToolbarItems;

struct _PrefsToolbarItem
{
	gint id;
	gboolean is_important;
	void (*callback) (GtkWidget *widget, gpointer data);
	gpointer data;
};

gint prefs_toolbar_open	(ToolbarType	 type,
			 gint		*visible_items,
			 GList	       **item_list);

const PrefsDisplayItem *prefs_toolbar_get_item_from_name
				(const gchar	*name);
const PrefsDisplayItem *prefs_toolbar_get_item_from_id	(gint id);

GList *prefs_toolbar_get_item_list_from_name_list
				(const gchar	*name_list);
gint *prefs_toolbar_get_id_list_from_name_list
				(const gchar	*name_list);

gchar *prefs_toolbar_get_name_list_from_item_list	(GList	*item_list);

const gchar *prefs_toolbar_get_default_main_setting_name_list	(void);
const gchar *prefs_toolbar_get_default_compose_setting_name_list(void);

#endif /* __PREFS_TOOLBAR_H__ */
