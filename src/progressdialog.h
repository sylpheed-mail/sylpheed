/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2009 Hiroyuki Yamamoto
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

#ifndef __PROGRESS_H__
#define __PROGRESS_H__

#include <glib.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkliststore.h>
#include <gdk/gdkpixbuf.h>

typedef struct _ProgressDialog	ProgressDialog;

struct _ProgressDialog
{
	GtkWidget *window;
	GtkWidget *label;
	GtkWidget *cancel_btn;
	GtkWidget *progressbar;

	GtkWidget *treeview;
	GtkListStore *store;
};

typedef enum
{
	PROG_COL_PIXBUF,
	PROG_COL_NAME,
	PROG_COL_STATUS,
	PROG_COL_PROGRESS,
	PROG_COL_POINTER,
	PROG_N_COLS
} ProgressColumn;

ProgressDialog *progress_dialog_create		(void);
ProgressDialog *progress_dialog_simple_create	(void);

void progress_dialog_destroy		(ProgressDialog	*progress);

void progress_dialog_set_label		(ProgressDialog	*progress,
					 gchar		*str);
void progress_dialog_set_value		(ProgressDialog	*progress,
					 gfloat		 value);
void progress_dialog_set_percentage	(ProgressDialog	*progress,
					 gfloat		 percentage);

void progress_dialog_append		(ProgressDialog	*progress,
					 GdkPixbuf	*pixbuf,
					 const gchar	*name,
					 const gchar	*status,
					 const gchar	*progress_str,
					 gpointer	 data);
void progress_dialog_set_row		(ProgressDialog	*progress,
					 gint		 row,
					 GdkPixbuf	*pixbuf,
					 const gchar	*name,
					 const gchar	*status,
					 const gchar	*progress_str,
					 gpointer	 data);

void progress_dialog_set_row_pixbuf	(ProgressDialog	*progress,
					 gint		 row,
					 GdkPixbuf	*pixbuf);
void progress_dialog_set_row_name	(ProgressDialog	*progress,
					 gint		 row,
					 const gchar	*name);
void progress_dialog_set_row_status	(ProgressDialog	*progress,
					 gint		 row,
					 const gchar	*status);
void progress_dialog_set_row_progress	(ProgressDialog	*progress,
					 gint		 row,
					 const gchar	*progress_str);

void progress_dialog_scroll_to_row	(ProgressDialog	*progress,
					 gint		 row);

#endif /* __PROGRESS_H__ */
