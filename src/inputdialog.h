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

#ifndef __INPUTDIALOG_H__
#define __INPUTDIALOG_H__

#include <glib.h>
#include <gtk/gtkfilechooser.h>

gchar *input_dialog			(const gchar	*title,
					 const gchar	*message,
					 const gchar	*default_string);
gchar *input_dialog_with_invisible	(const gchar	*title,
					 const gchar	*message,
					 const gchar	*default_string);
gchar *input_dialog_combo		(const gchar	*title,
					 const gchar	*message,
					 const gchar	*default_string,
					 GList		*list,
					 gboolean	 case_sensitive);
gchar *input_dialog_query_password	(const gchar	*server,
					 const gchar	*user);

gchar *input_dialog_with_filesel	(const gchar	*title,
					 const gchar	*message,
					 const gchar	*default_string,
					 GtkFileChooserAction action);

#endif /* __INPUTDIALOG_H__ */
