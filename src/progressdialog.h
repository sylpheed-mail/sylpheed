/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2005 Hiroyuki Yamamoto
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

typedef struct _ProgressDialog	ProgressDialog;

struct _ProgressDialog
{
	GtkWidget *window;
	GtkWidget *label;
	GtkWidget *cancel_btn;
	GtkWidget *progressbar;
	GtkWidget *clist;
};

ProgressDialog *progress_dialog_create	(void);
void progress_dialog_set_label		(ProgressDialog	*progress,
					 gchar		*str);
void progress_dialog_set_value		(ProgressDialog	*progress,
					 gfloat		 value);
void progress_dialog_set_percentage	(ProgressDialog	*progress,
					 gfloat		 percentage);
void progress_dialog_destroy		(ProgressDialog	*progress);

#endif /* __PROGRESS_H__ */
