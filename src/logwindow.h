/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2009 Hiroyuki Yamamoto
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

#ifndef __LOGWINDOW_H__
#define __LOGWINDOW_H__

#include <glib.h>
#include <gtk/gtkwidget.h>

typedef struct _LogWindow	LogWindow;

typedef enum
{
	LOG_NORMAL,
	LOG_MSG,
	LOG_WARN,
	LOG_ERROR
} LogType;

struct _LogWindow
{
	GtkWidget *window;
	GtkWidget *scrolledwin;
	GtkWidget *text;

	GdkColor msg_color;
	GdkColor warn_color;
	GdkColor error_color;

	gint lines;

#if USE_THREADS
	GAsyncQueue *aqueue;
#endif
};

LogWindow *log_window_create(void);
void log_window_init(LogWindow *logwin);
void log_window_show(LogWindow *logwin);

void log_window_append(const gchar *str, LogType type);
void log_window_append_queue(const gchar *str, LogType type);

void log_window_flush(void);

#endif /* __LOGWINDOW_H__ */
