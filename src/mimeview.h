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

#ifndef __MIMEVIEW_H__
#define __MIMEVIEW_H__

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtktreestore.h>
#include <gtk/gtktreeselection.h>

typedef struct _MimeView	MimeView;

#include "textview.h"
#include "imageview.h"
#include "messageview.h"
#include "procmime.h"

typedef enum
{
	MIMEVIEW_TEXT,
	MIMEVIEW_IMAGE
} MimeViewType;

struct _MimeView
{
	GtkWidget *paned;

	GtkWidget *scrolledwin;
	GtkWidget *treeview;

	GtkTreeStore *store;
	GtkTreeSelection *selection;

	GtkWidget *mime_vbox;

	MimeViewType type;

	GtkWidget *popupmenu;
	GtkItemFactory *popupfactory;
	GtkWidget *reply_separator;
	GtkWidget *reply_menuitem;

	GtkTreePath *opened;

	TextView *textview;
	ImageView *imageview;

	MessageView *messageview;

	/* deprecated: use MessageView */
	MimeInfo *mimeinfo__;
	gchar *file__;

	gchar *drag_file;

	gboolean has_attach_file;
};

MimeView *mimeview_create	(void);
void mimeview_init		(MimeView	*mimeview);
void mimeview_show_message	(MimeView	*mimeview,
				 MimeInfo	*mimeinfo,
				 const gchar	*file);
void mimeview_clear		(MimeView	*mimeview);
void mimeview_destroy		(MimeView	*mimeview);

MimeInfo *mimeview_get_selected_part	(MimeView	*mimeview);

gboolean mimeview_step			(MimeView	*mimeview,
					 GtkScrollType	 type);

void mimeview_pass_key_press_event	(MimeView	*mimeview,
					 GdkEventKey	*event);

void mimeview_save_as			(MimeView	*mimeview);
void mimeview_save_all			(MimeView	*mimeview);

void mimeview_print			(MimeView	*mimeview);

void mimeview_launch_part		(MimeView	*mimeview,
					 MimeInfo	*partinfo);
void mimeview_open_part_with		(MimeView	*mimeview,
					 MimeInfo	*partinfo);
void mimeview_save_part_as		(MimeView	*mimeview,
					 MimeInfo	*partinfo);

void mimeview_print_part		(MimeView	*mimeview,
					 MimeInfo	*partinfo);

#endif /* __MIMEVIEW_H__ */
