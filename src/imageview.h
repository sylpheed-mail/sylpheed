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

#ifndef __IMAGEVIEW_H__
#define __IMAGEVIEW_H__

#include <glib.h>
#include <gtk/gtkwidget.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

typedef struct _ImageView	ImageView;

#include "messageview.h"
#include "procmime.h"

struct _ImageView
{
	GtkWidget *scrolledwin;
	GtkWidget *image;

	gpointer image_data;
	gboolean resize;
	gboolean resizing;

	MessageView *messageview;
};

ImageView *imageview_create	(void);
void imageview_init		(ImageView	*imageview);
void imageview_show_image	(ImageView	*imageview,
				 MimeInfo	*mimeinfo,
				 const gchar	*file,
				 gboolean	 resize);
void imageview_clear		(ImageView	*imageview);
void imageview_destroy		(ImageView	*imageview);

GdkPixbuf *imageview_get_resized_pixbuf	(GdkPixbuf	*pixbuf,
					 GtkWidget	*parent,
					 gint		 margin);

GdkPixbuf *imageview_get_rotated_pixbuf	(GdkPixbuf	*pixbuf);

#endif /* __IMAGEVIEW_H__ */
