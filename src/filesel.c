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

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkfilechooserdialog.h>
#include <gtk/gtkstock.h>

#include "main.h"
#include "filesel.h"
#include "manage_window.h"
#include "alertpanel.h"
#include "utils.h"

static GSList *filesel_select_file_full	(const gchar		*title,
					 const gchar		*file,
					 GtkFileChooserAction	 action,
					 gboolean		 multiple);

static GtkWidget *filesel_create	(const gchar		*title,
					 GtkFileChooserAction	 action);


gchar *filesel_select_file(const gchar *title, const gchar *file,
			   GtkFileChooserAction action)
{
	GSList *list;
	gchar *selected = NULL;

	list = filesel_select_file_full(title, file, action, FALSE);
	if (list) {
		selected = (gchar *)list->data;
		slist_free_strings(list->next);
	}
	g_slist_free(list);

	return selected;
}

GSList *filesel_select_files(const gchar *title, const gchar *file,
			     GtkFileChooserAction action)
{
	return filesel_select_file_full(title, file, action, TRUE);
}

static GSList *filesel_select_file_full(const gchar *title, const gchar *file,
					GtkFileChooserAction action,
					gboolean multiple)
{
	static gchar *cwd = NULL;
	GtkWidget *dialog;
	gchar *prev_dir;
	GSList *list = NULL;

	if (!cwd)
		cwd = g_strdup(startup_dir);

	prev_dir = g_get_current_dir();
	change_dir(cwd);

	dialog = filesel_create(title, action);

	change_dir(prev_dir);
	g_free(prev_dir);

	manage_window_set_transient(GTK_WINDOW(dialog));

	if (file)
		gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog),
						  file);

	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog),
					     multiple);

	gtk_widget_show(dialog);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		list = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
		if (list) {
			g_free(cwd);
			cwd = gtk_file_chooser_get_current_folder
				(GTK_FILE_CHOOSER(dialog));
		}
	}

	manage_window_focus_out(dialog, NULL, NULL);
	gtk_widget_destroy(dialog);

	return list;
}

gchar *filesel_save_as(const gchar *file)
{
	gchar *filename;

	filename = filesel_select_file(_("Save as"), file,
				       GTK_FILE_CHOOSER_ACTION_SAVE);

	if (filename && is_file_exist(filename)) {
		AlertValue aval;

		aval = alertpanel(_("Overwrite"),
				  _("Overwrite existing file?"),
				  GTK_STOCK_OK, GTK_STOCK_CANCEL, NULL);
		if (G_ALERTDEFAULT != aval) {
			g_free(filename);
			filename = NULL;
		}
	}

	return filename;
}

static GtkWidget *filesel_create(const gchar *title,
				 GtkFileChooserAction action)
{
	GtkWidget *dialog;

	dialog = gtk_file_chooser_dialog_new
		(title, NULL, action,
		 action == GTK_FILE_CHOOSER_ACTION_SAVE ? GTK_STOCK_SAVE
		 : GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
		 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		 NULL);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_wmclass
		(GTK_WINDOW(dialog), "file_selection", "Sylpheed");

	MANAGE_WINDOW_SIGNALS_CONNECT(dialog);

	return dialog;
}
