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
#include <gtk/gtk.h>

#ifdef G_OS_WIN32
#  include <windows.h>
#endif

#include "notificationwindow.h"
#include "mainwindow.h"
#include "utils.h"
#include "gtkutils.h"

#define NOTIFICATIONWINDOW_NOTIFY_PERIOD	10000
#define NOTIFICATIONWINDOW_WIDTH		380
#define NOTIFICATIONWINDOW_HEIGHT		80
#define FADE_REFRESH_RATE			50	/* ms */
#define FADE_SPEED				5	/* pixels */
#define FADE_IN_LENGTH				10	/* counts */

typedef struct _NotificationWindow
{
	GtkWidget *window;

	GtkWidget *msglabel;
	GtkWidget *sublabel;

	guint notify_tag;

	gint x;
	gint y;
	gint width;
	gint height;
	gint fade_length;
	gint fade_count;
	gint notify_event_count;
	guint timeout;
} NotificationWindow;

static NotificationWindow notify_window;

static void notification_window_destroy(void);

#if GTK_CHECK_VERSION(2, 12, 0)
static gboolean notify_fadein_timeout_cb(gpointer	 data);
#endif

static gboolean notify_timeout_cb	(gpointer	 data);

static gboolean nwin_button_pressed	(GtkWidget	*widget,
					 GdkEventButton	*event,
					 gpointer	 data);
static gboolean nwin_entered		(GtkWidget	*widget,
					 GdkEventCrossing *event,
					 gpointer	 data);
static gboolean nwin_motion_notify	(GtkWidget	*widget,
					 GdkEventMotion *event,
					 gpointer	 data);

static void nwin_destroy_cb	(GtkWidget	*widget,
				 gpointer	 data);


static void get_work_area(GdkRectangle *rect)
{
#ifdef G_OS_WIN32
	RECT rc;

	SystemParametersInfoW(SPI_GETWORKAREA, 0, &rc, 0);
	rect->x = rc.left;
	rect->y = rc.top;
	rect->width = rc.right - rc.left;
	rect->height = rc.bottom - rc.top;
#else
	rect->x = 0;
	rect->y = 0;
	rect->width = gdk_screen_width();
	rect->height = gdk_screen_height();
#endif
}

gint notification_window_open(const gchar *message, const gchar *submessage,
			      guint timeout)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *msglabel;
	GtkWidget *sublabel;
	GdkRectangle rect;
	gint x, y;
	GtkRequisition requisition;
	gint window_width;
	gint window_height;

	window_width = NOTIFICATIONWINDOW_WIDTH * gtkut_get_dpi_multiplier();
	window_height = NOTIFICATIONWINDOW_HEIGHT * gtkut_get_dpi_multiplier();

	if (notify_window.window) {
		notification_window_destroy();
	}

	window = gtk_window_new(GTK_WINDOW_POPUP);
	gtk_window_set_title(GTK_WINDOW(window), _("Notification"));
	gtk_window_set_wmclass(GTK_WINDOW(window), "notification", "Sylpheed");
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_widget_set_events(window, GDK_EXPOSURE_MASK|GDK_BUTTON_MOTION_MASK|GDK_POINTER_MOTION_MASK|GDK_POINTER_MOTION_HINT_MASK|GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK|GDK_ENTER_NOTIFY_MASK|GDK_LEAVE_NOTIFY_MASK);
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(window), TRUE);
	gtk_window_set_gravity(GTK_WINDOW(window), GDK_GRAVITY_SOUTH_EAST);
	gtk_widget_set_size_request(window, window_width, -1);
#if GTK_CHECK_VERSION(2, 12, 0)
	gtk_window_set_opacity(GTK_WINDOW(window), 0.0);
#endif
	gtk_widget_realize(window);
	gdk_window_set_type_hint(window->window, GDK_WINDOW_TYPE_HINT_NOTIFICATION);

	/* move window bottom-right */
	get_work_area(&rect);
	x = rect.x + rect.width - window_width - 2;
	if (x < 0) x = 0;
	y = rect.y + rect.height - window_height - 2;
	if (y < 0) y = 0;
	gtk_window_move(GTK_WINDOW(window), x, y);

	g_signal_connect(G_OBJECT(window), "destroy",
			 G_CALLBACK(nwin_destroy_cb), NULL);
	g_signal_connect(G_OBJECT(window), "button_press_event",
			 G_CALLBACK(nwin_button_pressed), NULL);
	g_signal_connect(G_OBJECT(window), "enter_notify_event",
			 G_CALLBACK(nwin_entered), NULL);
	g_signal_connect(G_OBJECT(window), "motion_notify_event",
			 G_CALLBACK(nwin_motion_notify), NULL);

	vbox = gtk_vbox_new(FALSE, 8);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	msglabel = gtk_label_new(message);
	gtk_box_pack_start(GTK_BOX(vbox), msglabel, FALSE, FALSE, 0);

	sublabel = gtk_label_new("");
	gtk_label_set_ellipsize(GTK_LABEL(sublabel), PANGO_ELLIPSIZE_END);
	gtk_label_set_markup(GTK_LABEL(sublabel), submessage);
	gtk_box_pack_start(GTK_BOX(vbox), sublabel, FALSE, FALSE, 0);
	gtk_label_set_justify(GTK_LABEL(sublabel), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(sublabel), 0.0, 0.5);

	gtk_widget_show_all(window);

	/* adjust window size and position */
	gtk_widget_get_child_requisition(window, &requisition);
	notify_window.width = window_width;
	notify_window.height = MAX(requisition.height, window_height);
	gtk_widget_set_size_request(window, window_width, notify_window.height);
	y = rect.y + rect.height - notify_window.height - 2;
	if (y < 0) y = 0;
	gtk_window_move(GTK_WINDOW(window), x, y);

#if GTK_CHECK_VERSION(2, 12, 0)
	notify_window.notify_tag = g_timeout_add(FADE_REFRESH_RATE,
						 notify_fadein_timeout_cb,
						 NULL);
#else
	if (timeout > 0) {
		notify_window.notify_tag = g_timeout_add(timeout * 1000,
							 notify_timeout_cb, NULL);
	}
#endif

	debug_print("notification window created\n");

	notify_window.window = window;
	notify_window.msglabel = msglabel;
	notify_window.sublabel = sublabel;
	notify_window.x = x;
	notify_window.y = y;
	notify_window.fade_length = FADE_IN_LENGTH;
	notify_window.fade_count = 0;
	notify_window.notify_event_count = 0;
	notify_window.timeout = timeout;

	return 0;
}

void notification_window_set_message(const gchar *message,
				     const gchar *submessage)
{
	if (notify_window.window) {
		gtk_label_set_text(GTK_LABEL(notify_window.msglabel), message);
		gtk_label_set_markup(GTK_LABEL(notify_window.sublabel), submessage);
	}
}

void notification_window_close(void)
{
	notification_window_destroy();
}

static void notification_window_destroy(void)
{
	if (notify_window.window) {
		if (notify_window.notify_tag > 0) {
			g_source_remove(notify_window.notify_tag);
			notify_window.notify_tag = 0;
		}

		gtk_widget_destroy(notify_window.window);

		notify_window.window = NULL;
		notify_window.msglabel = NULL;
		notify_window.sublabel = NULL;

		debug_print("notification window removed\n");
	}
}

#if GTK_CHECK_VERSION(2, 12, 0)
static gboolean notify_fadein_timeout_cb(gpointer data)
{
	gdk_threads_enter();
	notify_window.fade_length--;
	notify_window.fade_count++;

	gtk_window_set_opacity(GTK_WINDOW(notify_window.window),
			       (gdouble)notify_window.fade_count / FADE_IN_LENGTH);

	if (notify_window.fade_length <= 0) {
		notify_window.fade_length = 0;
		notify_window.fade_count = 0;
		if (notify_window.timeout > 0) {
			notify_window.notify_tag =
				g_timeout_add(notify_window.timeout * 1000,
					      notify_timeout_cb, NULL);
		}
		gdk_threads_leave();
		return FALSE;
	}
	gdk_threads_leave();
	return TRUE;
}
#endif

static gboolean notify_fadeout_timeout_cb(gpointer data)
{
	gdk_threads_enter();
	notify_window.fade_length -= FADE_SPEED;
	notify_window.fade_count++;

	gtk_window_move(GTK_WINDOW(notify_window.window),
			notify_window.x, notify_window.y + notify_window.fade_count * FADE_SPEED);
#if GTK_CHECK_VERSION(2, 12, 0)
	gtk_window_set_opacity(GTK_WINDOW(notify_window.window),
			       (gdouble)notify_window.fade_length / (gdk_screen_height() - notify_window.y));
#endif

	if (notify_window.fade_length <= 0) {
		notification_window_destroy();
		gdk_threads_leave();
		return FALSE;
	}
	gdk_threads_leave();
	return TRUE;
}

static gboolean notify_timeout_cb(gpointer data)
{
	gdk_threads_enter();
	notify_window.fade_length = gdk_screen_height() - notify_window.y;
	notify_window.notify_tag = g_timeout_add(FADE_REFRESH_RATE,
						 notify_fadeout_timeout_cb,
						 NULL);
	gdk_threads_leave();

	return FALSE;
}

static gboolean nwin_button_pressed(GtkWidget *widget, GdkEventButton *event,
				    gpointer data)
{
	if (!event)
		return FALSE;

	notification_window_destroy();
	main_window_popup(main_window_get());

	return TRUE;
}

static gboolean nwin_entered(GtkWidget *widget, GdkEventCrossing *event,
			     gpointer data)
{
	return FALSE;
}

static gboolean nwin_motion_notify(GtkWidget *widget, GdkEventMotion *event,
				   gpointer data)
{
	if (notify_window.fade_count > 0 &&
	    notify_window.notify_event_count == 0) {
		notify_window.notify_event_count++;
		return FALSE;
	}

	if (notify_window.notify_tag > 0) {
		g_source_remove(notify_window.notify_tag);
		notify_window.notify_tag = 0;
	}
	notify_window.fade_count = 0;
	notify_window.notify_event_count = 0;
#if GTK_CHECK_VERSION(2, 12, 0)
	gtk_window_set_opacity(GTK_WINDOW(notify_window.window), 1.0);
#endif
	gtk_window_move(GTK_WINDOW(notify_window.window),
			notify_window.x, notify_window.y);
	if (notify_window.timeout > 0) {
		notify_window.notify_tag =
			g_timeout_add(notify_window.timeout * 1000,
				      notify_timeout_cb, NULL);
	}

	return FALSE;
}

static void nwin_destroy_cb(GtkWidget *widget, gpointer data)
{
}
