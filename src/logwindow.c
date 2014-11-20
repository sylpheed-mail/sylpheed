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
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktextview.h>
#include <gtk/gtkstyle.h>

#include "logwindow.h"
#include "prefs_common.h"
#include "utils.h"
#include "gtkutils.h"
#include "codeconv.h"

#define TRIM_LINES	25

static LogWindow *logwindow;

#if USE_THREADS
static GThread *main_thread;
#endif

static void log_window_print_func	(const gchar	*str);
static void log_window_message_func	(const gchar	*str);
static void log_window_warning_func	(const gchar	*str);
static void log_window_error_func	(const gchar	*str);

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
	gtk_widget_set_size_request(window, 520 * gtkut_get_dpi_multiplier(), 400 * gtkut_get_dpi_multiplier());
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
	logwin->lines = 1;

#if USE_THREADS
	logwin->aqueue = g_async_queue_new();

	main_thread = g_thread_self();
	debug_print("main_thread = %p\n", main_thread);
#endif

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

	set_log_ui_func_full(log_window_print_func, log_window_message_func,
			     log_window_warning_func, log_window_error_func,
			     log_window_flush);
}

void log_window_show(LogWindow *logwin)
{
	GtkTextView *text = GTK_TEXT_VIEW(logwin->text);
	GtkTextBuffer *buffer;
	GtkTextMark *mark;

	buffer = gtk_text_view_get_buffer(text);
	mark = gtk_text_buffer_get_mark(buffer, "end");
	gtk_text_view_scroll_mark_onscreen(text, mark);

	gtk_window_present(GTK_WINDOW(logwin->window));
}

static void log_window_append_real(const gchar *str, LogType type)
{
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	GdkColor *color = NULL;
	gchar *head = NULL;
	const gchar *tag;
	gint line_limit = prefs_common.logwin_line_limit;

	g_return_if_fail(logwindow != NULL);

#if USE_THREADS
	if (g_thread_self() != main_thread) {
		g_fprintf(stderr, "log_window_append_real called from non-main thread (%p)\n", g_thread_self());
		return;
	}
#endif

	gdk_threads_enter();

	text = GTK_TEXT_VIEW(logwindow->text);
	buffer = gtk_text_view_get_buffer(text);

	if (line_limit > 0 && logwindow->lines >= line_limit) {
		GtkTextIter start, end;

		gtk_text_buffer_get_start_iter(buffer, &start);
		end = start;
		gtk_text_iter_forward_lines(&end, TRIM_LINES);
		gtk_text_buffer_delete(buffer, &start, &end);
		logwindow->lines = gtk_text_buffer_get_line_count(buffer);
	}

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

	gtk_text_buffer_get_end_iter(buffer, &iter);

	if (head)
		gtk_text_buffer_insert_with_tags_by_name
			(buffer, &iter, head, -1, tag, NULL);

	if (!g_utf8_validate(str, -1, NULL)) {
		gchar *str_;

		str_ = conv_utf8todisp(str, NULL);
		if (str_) {
			gtk_text_buffer_insert_with_tags_by_name
				(buffer, &iter, str_, -1, tag, NULL);
			g_free(str_);
		}
	} else {
		gtk_text_buffer_insert_with_tags_by_name
			(buffer, &iter, str, -1, tag, NULL);
	}

	if (GTK_WIDGET_VISIBLE(text)) {
		GtkTextMark *mark;
		mark = gtk_text_buffer_get_mark(buffer, "end");
		gtk_text_view_scroll_mark_onscreen(text, mark);
	}

	logwindow->lines++;

	gdk_threads_leave();
}

void log_window_append(const gchar *str, LogType type)
{
#if USE_THREADS
	if (g_thread_self() != main_thread) {
		log_window_append_queue(str, type);
		return;
	}

	log_window_flush();
#endif
	log_window_append_real(str, type);
}

typedef struct _LogData
{
	gchar *str;
	LogType type;
} LogData;

void log_window_append_queue(const gchar *str, LogType type)
{
#if USE_THREADS
	LogData *logdata;

	logdata = g_new(LogData, 1);
	logdata->str = g_strdup(str);
	logdata->type = type;

	g_async_queue_push(logwindow->aqueue, logdata);
#endif
}

void log_window_flush(void)
{
#if USE_THREADS
	LogData *logdata;

	if (g_thread_self() != main_thread) {
		g_fprintf(stderr, "log_window_flush called from non-main thread (%p)\n", g_thread_self());
		return;
	}

	while ((logdata = g_async_queue_try_pop(logwindow->aqueue))) {
		log_window_append_real(logdata->str, logdata->type);
		g_free(logdata->str);
		g_free(logdata);
	}
#endif
}

static void log_window_print_func(const gchar *str)
{
	log_window_append(str, LOG_NORMAL);
}

static void log_window_message_func(const gchar *str)
{
	log_window_append(str, LOG_MSG);
}

static void log_window_warning_func(const gchar *str)
{
	log_window_append(str, LOG_WARN);
}

static void log_window_error_func(const gchar *str)
{
	log_window_append(str, LOG_ERROR);
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
