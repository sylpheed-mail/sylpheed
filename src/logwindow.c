/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto
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
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktextview.h>
#include <gtk/gtkstyle.h>

#include "intl.h"
#include "logwindow.h"
#include "utils.h"
#include "gtkutils.h"

#define MAX_LINES	500
#define TRIM_LINES	25

static LogWindow *logwindow;

static void hide_cb		(GtkWidget	*widget,
				 LogWindow	*logwin);
static gboolean key_pressed	(GtkWidget	*widget,
				 GdkEventKey	*event,
				 LogWindow	*logwin);

LogWindow *log_window_create(void)
{
	LogWindow *logwin;
	GtkWidget *window;
	GtkWidget *scrolledwin;
	GtkWidget *text;
	GtkTextBuffer *buffer;
	GtkTextIter iter;

	debug_print("Creating log window...\n");
	logwin = g_new0(LogWindow, 1);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), _("Protocol log"));
	gtk_window_set_wmclass(GTK_WINDOW(window), "log_window", "Sylpheed");
	gtk_window_set_policy(GTK_WINDOW(window), TRUE, TRUE, FALSE);
	gtk_widget_set_size_request(window, 520, 400);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(gtk_widget_hide_on_delete), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(key_pressed), logwin);
	g_signal_connect(G_OBJECT(window), "hide",
			 G_CALLBACK(hide_cb), logwin);
	gtk_widget_realize(window);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(window), scrolledwin);
	gtk_widget_show(scrolledwin);

	text = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	gtk_text_buffer_get_start_iter(buffer, &iter);
	gtk_text_buffer_create_mark(buffer, "end", &iter, FALSE);
	gtk_container_add(GTK_CONTAINER(scrolledwin), text);
	gtk_widget_show(text);

	logwin->window = window;
	logwin->scrolledwin = scrolledwin;
	logwin->text = text;
	logwin->lines = 0;

	logwindow = logwin;

	return logwin;
}

void log_window_init(LogWindow *logwin)
{
	GtkTextBuffer *buffer;
	GdkColormap *colormap;
	GdkColor color[3] =
		{{0, 0, 0xafff, 0}, {0, 0xefff, 0, 0}, {0, 0xefff, 0, 0}};
	gboolean success[3];
	gint i;

	//gtkut_widget_disable_theme_engine(logwin->text);

	logwin->msg_color   = color[0];
	logwin->warn_color  = color[1];
	logwin->error_color = color[2];

	colormap = gdk_window_get_colormap(logwin->window->window);
	gdk_colormap_alloc_colors(colormap, color, 3, FALSE, TRUE, success);

	for (i = 0; i < 3; i++) {
		if (success[i] == FALSE) {
			GtkStyle *style;

			g_warning("LogWindow: color allocation failed\n");
			style = gtk_widget_get_style(logwin->window);
			logwin->msg_color = logwin->warn_color =
			logwin->error_color = style->black;
			break;
		}
	}

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(logwin->text));
	gtk_text_buffer_create_tag(buffer, "message",
				   "foreground-gdk", &logwindow->msg_color,
				   NULL);
	gtk_text_buffer_create_tag(buffer, "warn",
				   "foreground-gdk", &logwindow->warn_color,
				   NULL);
	gtk_text_buffer_create_tag(buffer, "error",
				   "foreground-gdk", &logwindow->error_color,
				   NULL);
}

void log_window_show(LogWindow *logwin)
{
	GtkTextView *text = GTK_TEXT_VIEW(logwin->text);
	GtkTextBuffer *buffer;
	GtkTextMark *mark;

	buffer = gtk_text_view_get_buffer(text);

	gtk_widget_hide(logwin->window);

	mark = gtk_text_buffer_get_mark(buffer, "end");
	gtk_text_view_scroll_mark_onscreen(text, mark);

	gtk_widget_show(logwin->window);
}

void log_window_append(const gchar *str, LogType type)
{
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter iter;
	GdkColor *color = NULL;
	gchar *head = NULL;
	const gchar *tag;

	g_return_if_fail(logwindow != NULL);

	text = GTK_TEXT_VIEW(logwindow->text);
	buffer = gtk_text_view_get_buffer(text);
	gtk_text_buffer_get_iter_at_offset(buffer, &iter, -1);

	switch (type) {
	case LOG_MSG:
		color = &logwindow->msg_color;
		tag = "message";
		head = "* ";
		break;
	case LOG_WARN:
		color = &logwindow->warn_color;
		tag = "warn";
		head = "** ";
		break;
	case LOG_ERROR:
		color = &logwindow->error_color;
		tag = "error";
		head = "*** ";
		break;
	default:
		tag = NULL;
		break;
	}

	if (logwindow->lines == MAX_LINES) {
		//
	}

	if (head)
		gtk_text_buffer_insert_with_tags_by_name
			(buffer, &iter, head, -1, tag, NULL);
	gtk_text_buffer_insert_with_tags_by_name
		(buffer, &iter, str, -1, tag, NULL);

	mark = gtk_text_buffer_get_mark(buffer, "end");
	if (GTK_WIDGET_VISIBLE(text))
		gtk_text_view_scroll_mark_onscreen(text, mark);

	logwindow->lines++;
}

static void hide_cb(GtkWidget *widget, LogWindow *logwin)
{
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event,
			    LogWindow *logwin)
{
	if (event && event->keyval == GDK_Escape)
		gtk_widget_hide(logwin->window);
	return FALSE;
}
