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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtkstatusbar.h>

#include "inputdialog.h"
#include "alertpanel.h"
#include "mainwindow.h"
#include "manage_window.h"
#include "gtkutils.h"

static void scan_tree_func(Folder *folder, FolderItem *item, gpointer data);

void setup(MainWindow *mainwin)
{
	gchar *path;
	Folder *folder;

	manage_window_focus_in(mainwin->window, NULL, NULL);
	path = input_dialog
		(_("Mailbox setting"),
		 _("Specify the location of mailbox.\n"
		   "If you are unsure, just select OK."),
		 "Mail");
	if (!path) return;
	if (folder_find_from_path(path)) {
		alertpanel_error(_("The mailbox `%s' already exists."), path);
		g_warning("The mailbox already exists.\n");
		g_free(path);
		return;
	}

	if (!strcmp(path, "Mail"))
		folder = folder_new(F_MH, _("Mailbox"), path);
	else
		folder = folder_new(F_MH, g_basename(path), path);
	g_free(path);

	if (folder->klass->create_tree(folder) < 0) {
		alertpanel_error(_("Creation of the mailbox failed.\n"
				   "Maybe some files already exist, or you don't have the permission to write there."));
		folder_destroy(folder);
		return;
	}

	folder_add(folder);
	folder_set_ui_func(folder, scan_tree_func, mainwin);
	folder->klass->scan_tree(folder);
	folder_set_ui_func(folder, NULL, NULL);
}

static void scan_tree_func(Folder *folder, FolderItem *item, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	gchar *str;

	if (item->path)
		str = g_strdup_printf(_("Scanning folder %s%c%s ..."),
				      LOCAL_FOLDER(folder)->rootpath,
				      G_DIR_SEPARATOR,
				      item->path);
	else
		str = g_strdup_printf(_("Scanning folder %s ..."),
				      LOCAL_FOLDER(folder)->rootpath);

	gtk_statusbar_push(GTK_STATUSBAR(mainwin->statusbar),
			   mainwin->mainwin_cid, str);
	gtkut_widget_draw_now(mainwin->statusbar);
	gtk_statusbar_pop(GTK_STATUSBAR(mainwin->statusbar),
			  mainwin->mainwin_cid);
	g_free(str);
}
