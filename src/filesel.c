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

#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkfilesel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>

#include "main.h"
#include "filesel.h"
#include "manage_window.h"
#include "gtkutils.h"

static GtkWidget *filesel;
static gboolean filesel_ack;
static gboolean filesel_fin;

static void filesel_create(const gchar *title);
static void filesel_ok_cb(GtkWidget *widget, gpointer data);
static void filesel_cancel_cb(GtkWidget *widget, gpointer data);
static gint delete_event(GtkWidget *widget, GdkEventAny *event, gpointer data);
static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data);

gchar *filesel_select_file(const gchar *title, const gchar *file)
{
	static gchar *filename = NULL;
	static gchar *cwd = NULL;

	filesel_create(title);

	manage_window_set_transient(GTK_WINDOW(filesel));

	if (filename) {
		g_free(filename);
		filename = NULL;
	}

	if (!cwd)
		cwd = g_strconcat(startup_dir, G_DIR_SEPARATOR_S, NULL);

	gtk_file_selection_set_filename(GTK_FILE_SELECTION(filesel), cwd);

	if (file) {
		gtk_file_selection_set_filename(GTK_FILE_SELECTION(filesel),
						file);
		gtk_editable_select_region
			(GTK_EDITABLE(GTK_FILE_SELECTION(filesel)->selection_entry),
			 0, -1);
	}

	gtk_widget_show(filesel);

	filesel_ack = filesel_fin = FALSE;

	while (filesel_fin == FALSE)
		gtk_main_iteration();

	if (filesel_ack) {
		const gchar *str;

		str = gtk_file_selection_get_filename
			(GTK_FILE_SELECTION(filesel));
		if (str && str[0] != '\0') {
			gchar *dir;

			filename = g_strdup(str);
			dir = g_dirname(str);
			g_free(cwd);
			cwd = g_strconcat(dir, G_DIR_SEPARATOR_S, NULL);
			g_free(dir);
		}
	}

	manage_window_focus_out(filesel, NULL, NULL);
	gtk_widget_destroy(filesel);
	GTK_EVENTS_FLUSH();

	return filename;
}

static void filesel_create(const gchar *title)
{
	filesel = gtk_file_selection_new(title);
	gtk_window_set_position(GTK_WINDOW(filesel), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(filesel), TRUE);
	gtk_window_set_wmclass
		(GTK_WINDOW(filesel), "file_selection", "Sylpheed");

	g_signal_connect(G_OBJECT(GTK_FILE_SELECTION(filesel)->ok_button),
			 "clicked", G_CALLBACK(filesel_ok_cb),
			 NULL);
	g_signal_connect
		(G_OBJECT(GTK_FILE_SELECTION(filesel)->cancel_button),
		 "clicked", G_CALLBACK(filesel_cancel_cb),
		 NULL);
	g_signal_connect(G_OBJECT(filesel), "delete_event",
			 G_CALLBACK(delete_event), NULL);
	g_signal_connect(G_OBJECT(filesel), "key_press_event",
			 G_CALLBACK(key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT(filesel);
}

static void filesel_ok_cb(GtkWidget *widget, gpointer data)
{
	filesel_ack = TRUE;
	filesel_fin = TRUE;
}

static void filesel_cancel_cb(GtkWidget *widget, gpointer data)
{
	filesel_ack = FALSE;
	filesel_fin = TRUE;
}

static gint delete_event(GtkWidget *widget, GdkEventAny *event, gpointer data)
{
	filesel_cancel_cb(NULL, NULL);
	return TRUE;
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		filesel_cancel_cb(NULL, NULL);
	return FALSE;
}
