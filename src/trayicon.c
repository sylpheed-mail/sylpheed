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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtktooltips.h>
#include <gtk/gtkimage.h>

#include "eggtrayicon.h"
#include "trayicon.h"
#include "mainwindow.h"
#include "utils.h"
#include "eggtrayicon.h"
#include "stock_pixmap.h"

#ifdef GDK_WINDOWING_X11

static GtkWidget *trayicon;
static GtkWidget *trayicon_img;
static GtkWidget *eventbox;
static GtkTooltips *trayicon_tip;

static void trayicon_button_pressed	(GtkWidget	*widget,
					 GdkEventButton	*event,
					 gpointer	 data);
static void trayicon_destroy_cb		(GtkWidget	*widget,
					 gpointer	 data);

GtkWidget *trayicon_create(MainWindow *mainwin)
{
	trayicon = GTK_WIDGET(egg_tray_icon_new("Sylpheed"));
	g_signal_connect(G_OBJECT(trayicon), "destroy",
			 G_CALLBACK(trayicon_destroy_cb), mainwin);

	eventbox = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(trayicon), eventbox);
	g_signal_connect(G_OBJECT(eventbox), "button_press_event",
			 G_CALLBACK(trayicon_button_pressed), mainwin);
	trayicon_img = stock_pixbuf_widget_scale(NULL, STOCK_PIXMAP_SYLPHEED,
						 24, 24);
	gtk_container_add(GTK_CONTAINER(eventbox), trayicon_img);

	trayicon_tip = gtk_tooltips_new();
	gtk_tooltips_set_tip(trayicon_tip, trayicon, _("Sylpheed"), NULL);

	gtk_widget_show_all(trayicon);

	return trayicon;
}

void trayicon_set_tooltip(const gchar *text)
{
	gtk_tooltips_set_tip(trayicon_tip, trayicon, text, NULL);
}

void trayicon_set_stock_icon(StockPixmap icon)
{
	GdkPixbuf *pixbuf;
	GdkPixbuf *scaled_pixbuf;

	stock_pixbuf_gdk(NULL, icon, &pixbuf);
	scaled_pixbuf = gdk_pixbuf_scale_simple(pixbuf, 24, 24,
						GDK_INTERP_HYPER);
	gtk_image_set_from_pixbuf(GTK_IMAGE(trayicon_img), scaled_pixbuf);
	g_object_unref(scaled_pixbuf);
}

static void trayicon_button_pressed(GtkWidget *widget, GdkEventButton *event,
				    gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	if (event && event->button == 1)
		gtk_window_present(GTK_WINDOW(mainwin->window));
}

static gboolean trayicon_restore(gpointer data)
{
	trayicon_create((MainWindow *)data);
	return FALSE;
}

static void trayicon_destroy_cb(GtkWidget *widget, gpointer data)
{
	g_idle_add(trayicon_restore, data);
}

#else /* GDK_WINDOWING_X11 */

GtkWidget *trayicon_create(MainWindow *mainwin)
{
	return NULL;
}

void trayicon_set_tooltip(const gchar *text)
{
}

void trayicon_set_stock_icon(StockPixmap icon)
{
}

#endif /* GDK_WINDOWING_X11 */
