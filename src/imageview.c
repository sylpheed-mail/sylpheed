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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkimage.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <stdlib.h>

#include "mainwindow.h"
#include "prefs_common.h"
#include "procmime.h"
#include "imageview.h"
#include "utils.h"

static gboolean get_resized_size(gint	 w,
				 gint	 h,
				 gint	 aw,
				 gint	 ah,
				 gint	*sw,
				 gint	*sh);

static gint button_press_cb	(GtkWidget	*widget,
				 GdkEventButton	*event,
				 gpointer	 data);
static void size_allocate_cb	(GtkWidget	*widget,
				 GtkAllocation	*allocation,
				 gpointer	 data);

ImageView *imageview_create(void)
{
	ImageView *imageview;
	GtkWidget *scrolledwin;

	debug_print(_("Creating image view...\n"));
	imageview = g_new0(ImageView, 1);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request
		(scrolledwin, prefs_common.mainview_width, -1);

	g_signal_connect(G_OBJECT(scrolledwin), "button_press_event",
			 G_CALLBACK(button_press_cb), imageview);
	g_signal_connect(G_OBJECT(scrolledwin), "size_allocate",
			 G_CALLBACK(size_allocate_cb), imageview);

	gtk_widget_show_all(scrolledwin);

	imageview->scrolledwin  = scrolledwin;
	imageview->image        = NULL;
	imageview->image_data   = NULL;
	imageview->resize       = FALSE;
	imageview->resizing     = FALSE;

	return imageview;
}

void imageview_init(ImageView *imageview)
{
}

void imageview_show_image(ImageView *imageview, MimeInfo *mimeinfo,
			  const gchar *file, gboolean resize)
{
	GdkPixbuf *pixbuf;
	GdkPixbuf *rotated;
	GError *error = NULL;

	g_return_if_fail(imageview != NULL);
	g_return_if_fail(imageview->scrolledwin != NULL);
	g_return_if_fail(imageview->scrolledwin->parent != NULL);

	if (file) {
		imageview_clear(imageview);
		pixbuf = gdk_pixbuf_new_from_file(file, &error);
		imageview->image_data = pixbuf;
	} else {
		pixbuf = (GdkPixbuf *)imageview->image_data;
	}

	if (error != NULL) {
		g_warning("%s\n", error->message);
		g_error_free(error);
	}

	if (!pixbuf) {
		g_warning(_("Can't load the image."));	
		return;
	}

	imageview->resize = resize;

	pixbuf = rotated = imageview_get_rotated_pixbuf(pixbuf);

	if (resize) {
		pixbuf = imageview_get_resized_pixbuf
			(pixbuf, imageview->scrolledwin->parent, 8);
	} else
		g_object_ref(pixbuf);

	g_object_unref(rotated);

	if (!imageview->image) {
		imageview->image = gtk_image_new_from_pixbuf(pixbuf);

		gtk_scrolled_window_add_with_viewport
			(GTK_SCROLLED_WINDOW(imageview->scrolledwin),
			 imageview->image);
	} else
		gtk_image_set_from_pixbuf(GTK_IMAGE(imageview->image), pixbuf);

	g_object_unref(pixbuf);

	gtk_widget_show(imageview->image);
}

void imageview_clear(ImageView *imageview)
{
	GtkAdjustment *hadj, *vadj;

	if (imageview->image)
		gtk_image_set_from_pixmap(GTK_IMAGE(imageview->image),
					  NULL, NULL);
	hadj = gtk_scrolled_window_get_hadjustment
		(GTK_SCROLLED_WINDOW(imageview->scrolledwin));
	gtk_adjustment_set_value(hadj, 0.0);
	vadj = gtk_scrolled_window_get_vadjustment
		(GTK_SCROLLED_WINDOW(imageview->scrolledwin));
	gtk_adjustment_set_value(vadj, 0.0);

	if (imageview->image_data) {
		g_object_unref(imageview->image_data);
		imageview->image_data = NULL;
	}
}

void imageview_destroy(ImageView *imageview)
{
	imageview_clear(imageview);
	g_free(imageview);
}

GdkPixbuf *imageview_get_resized_pixbuf(GdkPixbuf *pixbuf, GtkWidget *parent,
					gint margin)
{
	gint avail_width;
	gint avail_height;
	gint new_width;
	gint new_height;

	avail_width = parent->allocation.width;
	avail_height = parent->allocation.height;
	if (avail_width > margin) avail_width -= margin;
	if (avail_height > margin) avail_height -= margin;

	if (get_resized_size(gdk_pixbuf_get_width(pixbuf),
			     gdk_pixbuf_get_height(pixbuf),
			     avail_width, avail_height,
			     &new_width, &new_height))
		return gdk_pixbuf_scale_simple
			(pixbuf, new_width, new_height, GDK_INTERP_BILINEAR);

	g_object_ref(pixbuf);
	return pixbuf;
}

static gboolean get_resized_size(gint w, gint h, gint aw, gint ah,
				 gint *sw, gint *sh)
{
	gfloat wratio = 1.0;
	gfloat hratio = 1.0;
	gfloat ratio  = 1.0;

	if (w <= aw && h <= ah) {
		*sw = w;
		*sh = h;
		return FALSE;
	}

	if (w > aw)
		wratio = (gfloat)aw / (gfloat)w;
	if (h > ah)
		hratio = (gfloat)ah / (gfloat)h;

	ratio = (wratio > hratio) ? hratio : wratio;

	*sw = (gint)(w * ratio);
	*sh = (gint)(h * ratio);

	/* restrict minimum size */
	if (*sw < 16 || *sh < 16) {
		wratio = 16.0 / (gfloat)w;
		hratio = 16.0 / (gfloat)h;
		ratio = (wratio > hratio) ? wratio : hratio;
		if (ratio >= 1.0) {
			*sw = w;
			*sh = h;
		} else {
			*sw = (gint)(w * ratio);
			*sh = (gint)(h * ratio);
		}
	}

	return TRUE;
}

GdkPixbuf *imageview_get_rotated_pixbuf(GdkPixbuf *pixbuf)
{
#if GTK_CHECK_VERSION(2, 12, 0)
	return gdk_pixbuf_apply_embedded_orientation(pixbuf);
#else
	const gchar *orientation_str;
	gint orientation;
	GdkPixbuf *rotated;
	GdkPixbuf *new_pixbuf;

	orientation_str = gdk_pixbuf_get_option(pixbuf, "orientation");
	if (!orientation_str)
		return g_object_ref(pixbuf);

	orientation = strtol(orientation_str, NULL, 10);

	switch (orientation) {
	case 1:
		new_pixbuf = g_object_ref(pixbuf);
		break;
	case 2:
		new_pixbuf = gdk_pixbuf_flip(pixbuf, TRUE);
		break;
	case 3:
		new_pixbuf = gdk_pixbuf_rotate_simple(pixbuf, GDK_PIXBUF_ROTATE_UPSIDEDOWN);
		break;
	case 4:
		new_pixbuf = gdk_pixbuf_flip(pixbuf, FALSE);
		break;
	case 5:
		rotated = gdk_pixbuf_rotate_simple(pixbuf, GDK_PIXBUF_ROTATE_CLOCKWISE);
		new_pixbuf = gdk_pixbuf_flip(rotated, TRUE);
		g_object_unref(rotated);
		break;
	case 6:
		new_pixbuf = gdk_pixbuf_rotate_simple(pixbuf, GDK_PIXBUF_ROTATE_CLOCKWISE);
		break;
	case 7:
		rotated = gdk_pixbuf_rotate_simple(pixbuf, GDK_PIXBUF_ROTATE_CLOCKWISE);
		new_pixbuf = gdk_pixbuf_flip(rotated, FALSE);
		g_object_unref(rotated);
		break;
	case 8:
		new_pixbuf = gdk_pixbuf_rotate_simple(pixbuf, GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE);
		break;
	default:
		new_pixbuf = g_object_ref(pixbuf);
		break;
	}

	return new_pixbuf;
#endif
}

static gint button_press_cb(GtkWidget *widget, GdkEventButton *event,
			    gpointer data)
{
	ImageView *imageview = (ImageView *)data;

	if (event->button == 1 && imageview->image) {
		imageview_show_image(imageview, NULL, NULL, !imageview->resize);
		return TRUE;
	}
	return FALSE;
}

static void size_allocate_cb(GtkWidget *widget,GtkAllocation *allocation,
			     gpointer data)
{
	ImageView *imageview = (ImageView *)data;

	if (imageview->resize) {
		if (imageview->resizing) {
			imageview->resizing = FALSE;
			return;
		}
		if (!imageview->scrolledwin->parent)
			return;
		if (!imageview->image_data)
			return;
		imageview_show_image(imageview, NULL, NULL, TRUE);
		imageview->resizing = TRUE;
	}
}
