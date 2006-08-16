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

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktextview.h>
#include <gtk/gtkstyle.h>
#include <stdio.h>
#include <stdlib.h>

#include "sourcewindow.h"
#include "procmsg.h"
#include "codeconv.h"
#include "utils.h"
#include "gtkutils.h"
#include "prefs_common.h"

static void source_window_size_alloc_cb	(GtkWidget	*widget,
					 GtkAllocation	*allocation);
static gint source_window_delete_cb	(GtkWidget	*widget,
					 GdkEventAny	*event,
					 SourceWindow	*sourcewin);
static gboolean key_pressed		(GtkWidget	*widget,
					 GdkEventKey	*event,
					 SourceWindow	*sourcewin);

static void adj_value_changed		(GtkAdjustment	*adj,
					 SourceWindow	*sourcewin);

static void source_window_init()
{
}

SourceWindow *source_window_create(void)
{
	SourceWindow *sourcewin;
	GtkWidget *window;
	GtkWidget *scrolledwin;
	GtkWidget *text;
	static PangoFontDescription *font_desc = NULL;

	debug_print(_("Creating source window...\n"));
	sourcewin = g_new0(SourceWindow, 1);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), _("Source of the message"));
	gtk_window_set_wmclass(GTK_WINDOW(window), "source_window", "Sylpheed");
	gtk_window_set_policy(GTK_WINDOW(window), TRUE, TRUE, FALSE);
	gtk_widget_set_size_request(window, prefs_common.sourcewin_width,
				    prefs_common.sourcewin_height);
	g_signal_connect(G_OBJECT(window), "size_allocate",
			 G_CALLBACK(source_window_size_alloc_cb), sourcewin);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(source_window_delete_cb), sourcewin);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(key_pressed), sourcewin);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(window), scrolledwin);
	gtk_widget_show(scrolledwin);

	text = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text), 6);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(text), 6);
	if (!font_desc && prefs_common.textfont)
		font_desc = pango_font_description_from_string
			(prefs_common.textfont);
	if (font_desc)
		gtk_widget_modify_font(text, font_desc);
	gtk_container_add(GTK_CONTAINER(scrolledwin), text);
	gtk_widget_show(text);

	g_signal_connect(G_OBJECT(GTK_TEXT_VIEW(text)->vadjustment),
			 "value-changed",
			 G_CALLBACK(adj_value_changed), sourcewin);

	sourcewin->window = window;
	sourcewin->scrolledwin = scrolledwin;
	sourcewin->text = text;

	source_window_init();

	return sourcewin;
}

void source_window_show(SourceWindow *sourcewin)
{
	gtk_widget_show_all(sourcewin->window);
}

void source_window_destroy(SourceWindow *sourcewin)
{
	gtk_widget_destroy(sourcewin->window);
	g_free(sourcewin);
}

void source_window_show_msg(SourceWindow *sourcewin, MsgInfo *msginfo)
{
	gchar *file;
	gchar *title;
	FILE *fp;
	gchar buf[BUFFSIZE];
	GtkTextBuffer *buffer;
	GtkTextIter iter;

	g_return_if_fail(msginfo != NULL);

	file = procmsg_get_message_file(msginfo);
	g_return_if_fail(file != NULL);

	if ((fp = g_fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		g_free(file);
		return;
	}

	debug_print(_("Displaying the source of %s ...\n"), file);

	title = g_strdup_printf(_("%s - Source"), file);
	gtk_window_set_title(GTK_WINDOW(sourcewin->window), title);
	g_free(title);
	g_free(file);

	while (fgets(buf, sizeof(buf), fp) != NULL)
		source_window_append(sourcewin, buf);

	fclose(fp);

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(sourcewin->text));
	gtk_text_buffer_get_start_iter(buffer, &iter);
	gtk_text_buffer_place_cursor(buffer, &iter);
}

void source_window_append(SourceWindow *sourcewin, const gchar *str)
{
	GtkTextView *text = GTK_TEXT_VIEW(sourcewin->text);
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	gchar *out;

	buffer = gtk_text_view_get_buffer(text);

	out = conv_utf8todisp(str, NULL);
	gtk_text_buffer_get_iter_at_offset(buffer, &iter, -1);
	gtk_text_buffer_insert(buffer, &iter, out, -1);
	g_free(out);
}

static void source_window_size_alloc_cb(GtkWidget *widget,
					GtkAllocation *allocation)
{
	g_return_if_fail(allocation != NULL);

	prefs_common.sourcewin_width  = allocation->width;
	prefs_common.sourcewin_height = allocation->height;
}

static gint source_window_delete_cb(GtkWidget *widget, GdkEventAny *event,
				    SourceWindow *sourcewin)
{
	source_window_destroy(sourcewin);
	return TRUE;
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event,
			    SourceWindow *sourcewin)
{
	if (event && event->keyval == GDK_Escape) {
		source_window_destroy(sourcewin);
		return TRUE;
	}
	return FALSE;
}

static void adj_value_changed(GtkAdjustment *adj, SourceWindow *sourcewin)
{
	GtkTextBuffer *buffer;

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(sourcewin->text));
	if (gtk_text_buffer_get_selection_bounds(buffer, NULL, NULL))
		return;
	gtk_text_view_place_cursor_onscreen(GTK_TEXT_VIEW(sourcewin->text));
}
