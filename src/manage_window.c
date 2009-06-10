/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2002 Hiroyuki Yamamoto
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

#include <glib.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>

#include "manage_window.h"
#include "utils.h"
#include "gtkutils.h"

static GtkWidget *focus_window;

gint manage_window_focus_in(GtkWidget *widget, GdkEventFocus *event,
			    gpointer data)
{
	/* debug_print("Focus in event: window: %p\n", widget); */

	focus_window = widget;

	return FALSE;
}

gint manage_window_focus_out(GtkWidget *widget, GdkEventFocus *event,
			     gpointer data)
{
	/* debug_print("Focused window: %p\n", focus_window); */
	/* debug_print("Focus out event: window: %p\n", widget); */

#if 0
	if (focus_window == widget)
		focus_window = NULL;
#endif

	return FALSE;
}

gint manage_window_unmap(GtkWidget *widget, GdkEventAny *event, gpointer data)
{
	/* debug_print("unmap event: %p\n", widget); */

	if (focus_window == widget)
		focus_window = NULL;

	return FALSE;
}

gint manage_window_delete(GtkWidget *widget, GdkEventAny *event,
			  gpointer data)
{
	/* debug_print("delete event: %p\n", widget); */

	if (focus_window == widget)
		focus_window = NULL;

	return FALSE;
}

void manage_window_destroy(GtkWidget *widget, gpointer data)
{
	/* debug_print("destroy event: %p\n", widget); */

	if (focus_window == widget)
		focus_window = NULL;
}

void manage_window_set_transient(GtkWindow *window)
{
	/* debug_print("manage_window_set_transient(): window = %p, focus_window = %p\n",
		    window, focus_window); */

	if (window && focus_window) {
		if (!gtk_window_is_active(GTK_WINDOW(focus_window)))
			gtkut_window_popup(focus_window);
		gtk_window_set_transient_for(window, GTK_WINDOW(focus_window));
	}
}

void manage_window_signals_connect(GtkWindow *window)
{
	MANAGE_WINDOW_SIGNALS_CONNECT(window);
}

GtkWidget *manage_window_get_focus_window(void)
{
	return focus_window;
}
