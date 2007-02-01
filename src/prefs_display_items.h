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

#ifndef __PREFS_DISPLAY_ITEMS_H__
#define __PREFS_DISPLAY_ITEMS_H__

#include <glib.h>
#include <gtk/gtkwidget.h>

typedef struct _PrefsDisplayItem	PrefsDisplayItem;
typedef struct _PrefsDisplayItemsDialog	PrefsDisplayItemsDialog;

#include "stock_pixmap.h"

struct _PrefsDisplayItem
{
	gint id;
	gchar *name;
	gchar *label;
	gchar *description;
	StockPixmap icon;
	const gchar *stock_id;
	gboolean allow_multiple;
	gboolean in_use;
};

struct _PrefsDisplayItemsDialog
{
	GtkWidget *window;

	GtkWidget *label;

	GtkWidget *stock_clist;
	GtkWidget *shown_clist;

	GtkWidget *add_btn;
	GtkWidget *remove_btn;
	GtkWidget *up_btn;
	GtkWidget *down_btn;

	GtkWidget *default_btn;

	GtkWidget *confirm_area;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;

	const PrefsDisplayItem *all_items;
	GList *available_items;
	const gint *default_visible_ids;
	GList *visible_items;

	gboolean finished;
	gboolean cancelled;
};

PrefsDisplayItemsDialog *prefs_display_items_dialog_create	(void);

void prefs_display_items_dialog_set_available
					(PrefsDisplayItemsDialog *dialog,
					 PrefsDisplayItem        *all_items,
					 const gint		 *ids);
void prefs_display_items_dialog_set_default_visible
					(PrefsDisplayItemsDialog *dialog,
					 const gint		 *ids);
void prefs_display_items_dialog_set_visible
					(PrefsDisplayItemsDialog *dialog,
					 const gint		 *ids);

void prefs_display_items_dialog_destroy	(PrefsDisplayItemsDialog *dialog);

#endif /* __PREFS_DISPLAY_ITEMS_H__ */
