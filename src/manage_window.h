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

#ifndef __MANAGE_WINDOW_H__
#define __MANAGE_WINDOW_H__

#include <glib.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>

#define MANAGE_WINDOW_SIGNALS_CONNECT(window)				\
{									\
	g_signal_connect(G_OBJECT(window), "focus_in_event",		\
			 G_CALLBACK(manage_window_focus_in), NULL);	\
	g_signal_connect(G_OBJECT(window), "focus_out_event",		\
			 G_CALLBACK(manage_window_focus_out), NULL);	\
	g_signal_connect(G_OBJECT(window), "unmap_event",		\
			 G_CALLBACK(manage_window_unmap), NULL);	\
	g_signal_connect(G_OBJECT(window), "destroy",			\
			 G_CALLBACK(manage_window_destroy), NULL);	\
}

gint manage_window_focus_in		(GtkWidget	*widget,
					 GdkEventFocus	*event,
					 gpointer	 data);
gint manage_window_focus_out		(GtkWidget	*widget,
					 GdkEventFocus	*event,
					 gpointer	 data);
gint manage_window_unmap		(GtkWidget	*widget,
					 GdkEventAny	*event,
					 gpointer	 data);
gint manage_window_delete		(GtkWidget	*widget,
					 GdkEventAny	*event,
					 gpointer	 data);
void manage_window_destroy		(GtkWidget	*widget,
					 gpointer	 data);

void manage_window_set_transient	(GtkWindow	*window);
void manage_window_signals_connect	(GtkWindow	*window);

GtkWidget *manage_window_get_focus_window	(void);

#endif /* __MANAGE_WINDOW_H__ */
