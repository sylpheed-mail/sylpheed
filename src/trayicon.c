/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto
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
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>

#include "eggtrayicon.h"
#include "trayicon.h"
#include "mainwindow.h"
#include "utils.h"
#include "gtkutils.h"
#include "eggtrayicon.h"
#include "stock_pixmap.h"
#include "menu.h"
#include "main.h"
#include "inc.h"
#include "compose.h"
#include "about.h"

#ifdef GDK_WINDOWING_X11

static GtkWidget *trayicon;
static GtkWidget *trayicon_img;
static GtkWidget *eventbox;
static GtkTooltips *trayicon_tip;
static GtkWidget *trayicon_menu;
static gboolean default_tooltip = FALSE;

static void trayicon_button_pressed	(GtkWidget	*widget,
					 GdkEventButton	*event,
					 gpointer	 data);
static void trayicon_destroy_cb		(GtkWidget	*widget,
					 gpointer	 data);

static void trayicon_inc		(GtkWidget	*widget,
					 gpointer	 data);
static void trayicon_inc_all		(GtkWidget	*widget,
					 gpointer	 data);
static void trayicon_send		(GtkWidget	*widget,
					 gpointer	 data);
static void trayicon_compose		(GtkWidget	*widget,
					 gpointer	 data);
static void trayicon_about		(GtkWidget	*widget,
					 gpointer	 data);
static void trayicon_app_exit		(GtkWidget	*widget,
					 gpointer	 data);

GtkWidget *trayicon_create(MainWindow *mainwin)
{
	GtkWidget *menuitem;

	trayicon = GTK_WIDGET(egg_tray_icon_new("Sylpheed"));
	g_signal_connect(G_OBJECT(trayicon), "destroy",
			 G_CALLBACK(trayicon_destroy_cb), mainwin);

	eventbox = gtk_event_box_new();
	gtk_widget_show(eventbox);
	gtk_container_add(GTK_CONTAINER(trayicon), eventbox);
	g_signal_connect(G_OBJECT(eventbox), "button_press_event",
			 G_CALLBACK(trayicon_button_pressed), mainwin);
	trayicon_img = stock_pixbuf_widget_scale(NULL, STOCK_PIXMAP_SYLPHEED,
						 24, 24);
	gtk_widget_show(trayicon_img);
	gtk_container_add(GTK_CONTAINER(eventbox), trayicon_img);

	default_tooltip = FALSE;
	trayicon_tip = gtk_tooltips_new();
	trayicon_set_tooltip(NULL);

	if (!trayicon_menu) {
		trayicon_menu = gtk_menu_new();
		gtk_widget_show(trayicon_menu);
		MENUITEM_ADD_WITH_MNEMONIC(trayicon_menu, menuitem,
					   _("Get from _current account"), 0);
		g_signal_connect(G_OBJECT(menuitem), "activate",
				 G_CALLBACK(trayicon_inc), mainwin);
		MENUITEM_ADD_WITH_MNEMONIC(trayicon_menu, menuitem,
					   _("Get from _all accounts"), 0);
		g_signal_connect(G_OBJECT(menuitem), "activate",
				 G_CALLBACK(trayicon_inc_all), mainwin);
		MENUITEM_ADD_WITH_MNEMONIC(trayicon_menu, menuitem,
					   _("_Send queued messages"), 0);
		g_signal_connect(G_OBJECT(menuitem), "activate",
				 G_CALLBACK(trayicon_send), mainwin);

		MENUITEM_ADD(trayicon_menu, menuitem, NULL, 0);
		MENUITEM_ADD_WITH_MNEMONIC(trayicon_menu, menuitem,
					   _("Compose _new message"), 0);
		g_signal_connect(G_OBJECT(menuitem), "activate",
				 G_CALLBACK(trayicon_compose), mainwin);

		MENUITEM_ADD(trayicon_menu, menuitem, NULL, 0);
		MENUITEM_ADD_WITH_MNEMONIC(trayicon_menu, menuitem,
					   _("_About"), 0);
		g_signal_connect(G_OBJECT(menuitem), "activate",
				 G_CALLBACK(trayicon_about), NULL);
		MENUITEM_ADD_WITH_MNEMONIC(trayicon_menu, menuitem,
					   _("E_xit"), 0);
		g_signal_connect(G_OBJECT(menuitem), "activate",
				 G_CALLBACK(trayicon_app_exit), mainwin);
	}

	return trayicon;
}

void trayicon_set_tooltip(const gchar *text)
{
	if (text) {
		default_tooltip = FALSE;
		gtk_tooltips_set_tip(trayicon_tip, trayicon, text, NULL);
	} else if (!default_tooltip) {
		default_tooltip = TRUE;
		gtk_tooltips_set_tip(trayicon_tip, trayicon, _("Sylpheed"),
				     NULL);
	}
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
	GtkWindow *window = GTK_WINDOW(mainwin->window);

	if (!event)
		return;

	if (event->button == 1) {
		if (mainwin->window_hidden || mainwin->window_obscured) {
			gtk_window_set_skip_taskbar_hint(window, FALSE);
			gtk_window_present(window);
		} else {
			gtk_window_iconify(window);
			gtk_window_set_skip_taskbar_hint(window, TRUE);
		}
	} else if (event->button == 3) {
		gtk_menu_popup(GTK_MENU(trayicon_menu), NULL, NULL, NULL, NULL,
			       event->button, event->time);
	}
}

static gboolean trayicon_restore(gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	mainwin->tray_icon = trayicon_create(mainwin);
	return FALSE;
}

static void trayicon_destroy_cb(GtkWidget *widget, gpointer data)
{
	g_idle_add(trayicon_restore, data);
}

static void trayicon_inc(GtkWidget *widget, gpointer data)
{
	if (!inc_is_active() && !gtkut_window_modal_exist())
		inc_mail((MainWindow *)data);
}

static void trayicon_inc_all(GtkWidget *widget, gpointer data)
{
	if (!inc_is_active() && !gtkut_window_modal_exist())
		inc_all_account_mail((MainWindow *)data, FALSE);
}

static void trayicon_send(GtkWidget *widget, gpointer data)
{
	if (!gtkut_window_modal_exist())
		main_window_send_queue((MainWindow *)data);
}

static void trayicon_compose(GtkWidget *widget, gpointer data)
{
	if (!gtkut_window_modal_exist())
		compose_new(NULL, NULL, NULL, NULL);
}

static void trayicon_about(GtkWidget *widget, gpointer data)
{
	if (!gtkut_window_modal_exist())
		about_show();
}

static void trayicon_app_exit(GtkWidget *widget, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	if (mainwin->lock_count == 0 && !gtkut_window_modal_exist())
		app_will_exit(FALSE);
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
