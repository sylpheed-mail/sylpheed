/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2013 Hiroyuki Yamamoto
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

#ifndef __FILESEL_H__
#define __FILESEL_H__

#include <glib.h>
#include <gtk/gtkfilechooser.h>

typedef struct _FileselFileType	FileselFileType;

struct _FileselFileType
{
	gchar *type;
	gchar *ext;
};

gchar *filesel_select_file	(const gchar		*title,
				 const gchar		*file,
				 GtkFileChooserAction	 action);
GSList *filesel_select_files	(const gchar		*title,
				 const gchar		*file,
				 GtkFileChooserAction	 action);

gchar *filesel_save_as		(const gchar		*file);
gchar *filesel_save_as_type	(const gchar		*file,
				 FileselFileType	*types,
				 gint			 default_type,
				 gint			*selected_type);
gchar *filesel_select_dir	(const gchar		*dir);

#endif /* __FILESEL_H__ */
