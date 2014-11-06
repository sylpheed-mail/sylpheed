/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2014 Hiroyuki Yamamoto
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
#include <gtk/gtkversion.h>

#include "eggtrayicon.h"
#include "trayicon.h"
#include "mainwindow.h"
#include "utils.h"
#include "gtkutils.h"
#include "stock_pixmap.h"
#include "menu.h"
#include "main.h"
#include "inc.h"
#include "compose.h"
#include "prefs_common.h"

#if GTK_CHECK_VERSION(2, 10, 0) || defined(GDK_WINDOWING_X11)

#if GTK_CHECK_VERSION(2, 10, 0)
#include <gtk/gtkstatusicon.h>
#endif

#ifdef G_OS_WIN32
#define TRAYICON_IMAGE		STOCK_PIXMAP_SYLPHEED_SMALL
#define TRAYICON_NEW_IMAGE	STOCK_PIXMAP_SYLPHEED_NEWMAIL_SMALL
#else
#define TRAYICON_IMAGE		STOCK_PIXMAP_SYLPHEED
#define TRAYICON_NEW_IMAGE	STOCK_PIXMAP_SYLPHEED_NEWMAIL
#endif

#define TRAYICON_NOTIFY_PERIOD	10000

static TrayIcon trayicon;
static GtkWidget *trayicon_menu;
static gboolean on_notify = FALSE;
static gboolean default_tooltip = FALSE;

#if GTK_CHECK_VERSION(2, 10, 0)

static void trayicon_activated		(GtkStatusIcon	*status_icon,
					 gpointer	 data);
static void trayicon_popup_menu_cb	(GtkStatusIcon	*status_icon,
					 guint		 button,
					 guint		 activate_time,
					 gpointer	 data);

#else

static GtkWidget *trayicon_img;
static GtkWidget *eventbox;
static GtkTooltips *trayicon_tip;

static void trayicon_button_pressed	(GtkWidget	*widget,
					 GdkEventButton	*event,
					 gpointer	 data);
static void trayicon_destroy_cb		(GtkWidget	*widget,
					 gpointer	 data);

#endif

static void trayicon_present		(GtkWidget	*widget,
					 gpointer	 data);
static void trayicon_inc		(GtkWidget	*widget,
					 gpointer	 data);
static void trayicon_inc_all		(GtkWidget	*widget,
					 gpointer	 data);
static void trayicon_send		(GtkWidget	*widget,
					 gpointer	 data);
static void trayicon_compose		(GtkWidget	*widget,
					 gpointer	 data);
static void trayicon_app_exit		(GtkWidget	*widget,
					 gpointer	 data);

TrayIcon *trayicon_create(MainWindow *mainwin)
{
	GtkWidget *menuitem;

#if GTK_CHECK_VERSION(2, 10, 0)
	GdkPixbuf *pixbuf;

	stock_pixbuf_gdk(NULL, TRAYICON_IMAGE, &pixbuf);
	trayicon.status_icon = gtk_status_icon_new_from_pixbuf(pixbuf);

	g_signal_connect(G_OBJECT(trayicon.status_icon), "activate",
			 G_CALLBACK(trayicon_activated), mainwin);
	g_signal_connect(G_OBJECT(trayicon.status_icon), "popup-menu",
			 G_CALLBACK(trayicon_popup_menu_cb), mainwin);
#else
	trayicon.widget = GTK_WIDGET(egg_tray_icon_new("Sylpheed"));
	g_signal_connect(G_OBJECT(trayicon.widget), "destroy",
			 G_CALLBACK(trayicon_destroy_cb), mainwin);

	eventbox = gtk_event_box_new();
	gtk_widget_show(eventbox);
	gtk_container_add(GTK_CONTAINER(trayicon.widget), eventbox);
	g_signal_connect(G_OBJECT(eventbox), "button_press_event",
			 G_CALLBACK(trayicon_button_pressed), mainwin);
	trayicon_img = stock_pixbuf_widget_scale(NULL, TRAYICON_IMAGE, 24, 24);
	gtk_widget_show(trayicon_img);
	gtk_container_add(GTK_CONTAINER(eventbox), trayicon_img);

	trayicon_tip = gtk_tooltips_new();
#endif
	on_notify = FALSE;
	default_tooltip = FALSE;
	trayicon_set_tooltip(NULL);

	if (!trayicon_menu) {
		trayicon_menu = gtk_menu_new();
		gtk_widget_show(trayicon_menu);
		MENUITEM_ADD_WITH_MNEMONIC(trayicon_menu, menuitem,
					   _("_Display Sylpheed"), 0);
		g_signal_connect(G_OBJECT(menuitem), "activate",
				 G_CALLBACK(trayicon_present), mainwin);
		MENUITEM_ADD(trayicon_menu, menuitem, NULL, 0);
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
					   _("E_xit"), 0);
		g_signal_connect(G_OBJECT(menuitem), "activate",
				 G_CALLBACK(trayicon_app_exit), mainwin);
	}

	return &trayicon;
}

#if GTK_CHECK_VERSION(2, 10, 0)

void trayicon_show(TrayIcon *tray_icon)
{
	gtk_status_icon_set_visible(tray_icon->status_icon, TRUE);
};

void trayicon_hide(TrayIcon *tray_icon)
{
	gtk_status_icon_set_visible(tray_icon->status_icon, FALSE);
}

void trayicon_destroy(TrayIcon *tray_icon)
{
	g_object_unref(tray_icon->status_icon);
	tray_icon->status_icon = NULL;
}

void trayicon_set_tooltip(const gchar *text)
{
	if (text) {
		default_tooltip = FALSE;
		gtk_status_icon_set_tooltip(trayicon.status_icon, text);
	} else if (!default_tooltip) {
		default_tooltip = TRUE;
		gtk_status_icon_set_tooltip(trayicon.status_icon,
					    _("Sylpheed"));
	}
}

static guint notify_tag = 0;

gboolean notify_timeout_cb(gpointer data)
{
	gdk_threads_enter();
	gtk_status_icon_set_blinking(trayicon.status_icon, FALSE);
	notify_tag = 0;
	gdk_threads_leave();

	return FALSE;
}

void trayicon_set_notify(gboolean enabled)
{
	if (enabled && !on_notify) {
		trayicon_set_stock_icon(TRAYICON_NEW_IMAGE);
		on_notify = TRUE;
	} else if (!enabled && on_notify) {
		trayicon_set_stock_icon(TRAYICON_IMAGE);
		on_notify = FALSE;
	}

	if (enabled && notify_tag == 0) {
		gtk_status_icon_set_blinking(trayicon.status_icon, enabled);
		notify_tag = g_timeout_add(TRAYICON_NOTIFY_PERIOD, notify_timeout_cb, NULL);
	} else if (!enabled && notify_tag > 0) {
		g_source_remove(notify_tag);
		notify_timeout_cb(NULL);
	}
}

void trayicon_set_stock_icon(StockPixmap icon)
{
	GdkPixbuf *pixbuf;

	stock_pixbuf_gdk(NULL, icon, &pixbuf);
	gtk_status_icon_set_from_pixbuf(trayicon.status_icon, pixbuf);
}

static void trayicon_activated(GtkStatusIcon *status_icon, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

#ifdef G_OS_WIN32
	if (prefs_common.toggle_window_on_trayicon_click &&
	    !mainwin->window_hidden && !mainwin->window_obscured)
		gtk_window_iconify(GTK_WINDOW(mainwin->window));
	else
		main_window_popup(mainwin);
#else
	if (prefs_common.toggle_window_on_trayicon_click &&
	    gtk_window_is_active(GTK_WINDOW(mainwin->window)))
		gtk_window_iconify(GTK_WINDOW(mainwin->window));
	else
		main_window_popup(mainwin);
#endif
}

static void trayicon_popup_menu_cb(GtkStatusIcon *status_icon, guint button,
				   guint activate_time, gpointer data)
{
	gtk_menu_popup(GTK_MENU(trayicon_menu), NULL, NULL, NULL, NULL,
		       button, activate_time);
}

#else /* GTK_CHECK_VERSION(2, 10, 0) */

void trayicon_show(TrayIcon *tray_icon)
{
	gtk_widget_show(tray_icon->widget);
};

void trayicon_hide(TrayIcon *tray_icon)
{
	gtk_widget_destroy(tray_icon->widget);
	tray_icon->widget = NULL;
}

void trayicon_destroy(TrayIcon *tray_icon)
{
	g_signal_handlers_disconnect_by_func(G_OBJECT(tray_icon->widget),
					     G_CALLBACK(trayicon_destroy_cb),
					     main_window_get());
	gtk_widget_destroy(tray_icon->widget);
	tray_icon->widget = NULL;
}

void trayicon_set_tooltip(const gchar *text)
{
	if (text) {
		default_tooltip = FALSE;
		gtk_tooltips_set_tip(trayicon_tip, trayicon.widget, text,
				     NULL);
	} else if (!default_tooltip) {
		default_tooltip = TRUE;
		gtk_tooltips_set_tip(trayicon_tip, trayicon.widget,
				     _("Sylpheed"), NULL);
	}
}

void trayicon_set_notify(gboolean enabled)
{
	if (enabled && !on_notify) {
		trayicon_set_stock_icon(TRAYICON_NEW_IMAGE);
		on_notify = TRUE;
	} else if (!enabled && on_notify) {
		trayicon_set_stock_icon(TRAYICON_IMAGE);
		on_notify = FALSE;
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

	if (!event)
		return;

	if (event->button == 1) {
		if (prefs_common.toggle_window_on_trayicon_click &&
		    gtk_window_is_active(GTK_WINDOW(mainwin->window)))
			gtk_window_iconify(GTK_WINDOW(mainwin->window));
		else
			main_window_popup(mainwin);
	} else if (event->button == 3) {
		gtk_menu_popup(GTK_MENU(trayicon_menu), NULL, NULL, NULL, NULL,
			       event->button, event->time);
	}
}

static gboolean trayicon_restore(gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	gdk_threads_enter();
	mainwin->tray_icon = trayicon_create(mainwin);
	gdk_threads_leave();
	return FALSE;
}

static void trayicon_destroy_cb(GtkWidget *widget, gpointer data)
{
	g_idle_add(trayicon_restore, data);
}

#endif /* GTK_CHECK_VERSION(2, 10, 0) */

static void trayicon_present(GtkWidget *widget, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	main_window_popup(mainwin);
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

static void trayicon_app_exit(GtkWidget *widget, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	if (mainwin->lock_count == 0 && !gtkut_window_modal_exist())
		app_will_exit(FALSE);
}

#else /* GTK_CHECK_VERSION(2, 10, 0) || defined(GDK_WINDOWING_X11) */

TrayIcon *trayicon_create(MainWindow *mainwin)
{
	return NULL;
}

void trayicon_show(TrayIcon *tray_icon)
{
}

void trayicon_hide(TrayIcon *tray_icon)
{
}

void trayicon_destroy(TrayIcon *tray_icon)
{
}

void trayicon_set_tooltip(const gchar *text)
{
}

void trayicon_set_notify(gboolean enabled)
{
}

void trayicon_set_stock_icon(StockPixmap icon)
{
}

#endif /* GTK_CHECK_VERSION(2, 10, 0) || defined(GDK_WINDOWING_X11) */
