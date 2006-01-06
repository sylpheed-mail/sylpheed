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

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkstock.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "prefs_search_folder.h"
#include "prefs_ui.h"
#include "mainwindow.h"
#include "manage_window.h"
#include "alertpanel.h"
#include "foldersel.h"
#include "folder.h"
#include "filter.h"
#include "prefs_filter.h"
#include "prefs_filter_edit.h"
#include "menu.h"
#include "utils.h"
#include "gtkutils.h"

typedef struct _PrefsSearchFolderDialog	PrefsSearchFolderDialog;

struct _PrefsSearchFolderDialog
{
	PrefsDialog *dialog;
	FolderItem *item;

	GtkWidget *bool_optmenu;

	FilterCondEdit *cond_edit;

	GtkWidget *folder_entry;
	GtkWidget *folder_btn;

	GtkWidget *subfolder_checkbtn;
	GtkWidget *case_checkbtn;
};

static PrefsSearchFolderDialog *prefs_search_folder_create(FolderItem *item);
static void prefs_search_folder_set_dialog(PrefsSearchFolderDialog *dialog);

static gint prefs_search_folder_delete_cb(GtkWidget		  *widget,
					  GdkEventAny		  *event,
					  PrefsSearchFolderDialog *dialog);
static gint prefs_search_folder_key_press_cb
					 (GtkWidget		  *widget,
					  GdkEventKey		  *event,
					  PrefsSearchFolderDialog *dialog);

static void prefs_search_folder_ok_cb	 (GtkWidget		  *widget,
					  PrefsSearchFolderDialog *dialog);
static void prefs_search_folder_apply_cb (GtkWidget		  *widget,
					  PrefsSearchFolderDialog *dialog);
static void prefs_search_folder_cancel_cb(GtkWidget		  *widget,
					  PrefsSearchFolderDialog *dialog);


void prefs_search_folder_open(FolderItem *item)
{
	PrefsSearchFolderDialog *dialog;

	g_return_if_fail(item != NULL);

	dialog = prefs_search_folder_create(item);
	manage_window_set_transient(GTK_WINDOW(dialog->dialog->window));
	prefs_search_folder_set_dialog(dialog);
	gtk_widget_show(dialog->dialog->window);
}

static PrefsSearchFolderDialog *prefs_search_folder_create(FolderItem *item)
{
	PrefsSearchFolderDialog *new_dialog;
	PrefsDialog *dialog;
	GtkWidget *vbox;
	GtkWidget *bool_hbox;
	GtkWidget *bool_optmenu;
	GtkWidget *bool_menu;
	GtkWidget *menuitem;

	GtkWidget *scrolledwin;
	FilterCondEdit *cond_edit;

	GtkWidget *folder_hbox;
	GtkWidget *folder_label;
	GtkWidget *folder_entry;
	GtkWidget *folder_btn;

	GtkWidget *checkbtn_hbox;
	GtkWidget *subfolder_checkbtn;
	GtkWidget *case_checkbtn;

	new_dialog = g_new0(PrefsSearchFolderDialog, 1);

	dialog = g_new0(PrefsDialog, 1);
	prefs_dialog_create(dialog);

	gtk_widget_set_size_request(dialog->window, 600, -1);
	gtk_window_set_title(GTK_WINDOW(dialog->window),
			     _("Search folder properties"));
	gtk_notebook_set_show_border(GTK_NOTEBOOK(dialog->notebook), FALSE);
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(dialog->notebook), FALSE);

	g_signal_connect(G_OBJECT(dialog->window), "delete_event",
			 G_CALLBACK(prefs_search_folder_delete_cb), new_dialog);
	g_signal_connect(G_OBJECT(dialog->window), "key_press_event",
			 G_CALLBACK(prefs_search_folder_key_press_cb),
			 new_dialog);

	MANAGE_WINDOW_SIGNALS_CONNECT(dialog->window);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(dialog->notebook), vbox);
	//gtk_container_set_border_width(GTK_CONTAINER(vbox), VBOX_BORDER);

	bool_hbox = gtk_hbox_new(FALSE, 12);
	gtk_widget_show(bool_hbox);
	gtk_box_pack_start(GTK_BOX(vbox), bool_hbox, FALSE, FALSE, 0);

	bool_optmenu = gtk_option_menu_new();
	gtk_widget_show(bool_optmenu);
	gtk_box_pack_start(GTK_BOX(bool_hbox), bool_optmenu, FALSE, FALSE, 0);

	bool_menu = gtk_menu_new();
	MENUITEM_ADD(bool_menu, menuitem, _("Match any of the following"),
		     FLT_OR);
	MENUITEM_ADD(bool_menu, menuitem, _("Match all of the following"),
		     FLT_AND);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(bool_optmenu), bool_menu);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scrolledwin);
	gtk_widget_set_size_request(scrolledwin, -1, 150);
	gtk_box_pack_start(GTK_BOX(vbox), scrolledwin, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);

	cond_edit = prefs_filter_edit_cond_edit_create();
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolledwin),
					      cond_edit->cond_vbox);

	folder_hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show(folder_hbox);
	gtk_box_pack_start(GTK_BOX(vbox), folder_hbox, FALSE, FALSE, 0);

	folder_label = gtk_label_new(_("Folder:"));
	gtk_widget_show(folder_label);
	gtk_box_pack_start(GTK_BOX(folder_hbox), folder_label, FALSE, FALSE, 0);

	folder_entry = gtk_entry_new();
	gtk_widget_show(folder_entry);
	gtk_box_pack_start(GTK_BOX(folder_hbox), folder_entry, TRUE, TRUE, 0);

	folder_btn = gtk_button_new_with_label("...");
	gtk_widget_show(folder_btn);
	gtk_box_pack_start(GTK_BOX(folder_hbox), folder_btn, FALSE, FALSE, 0);

	checkbtn_hbox = gtk_hbox_new(FALSE, 12);
	gtk_widget_show(checkbtn_hbox);
	gtk_box_pack_start(GTK_BOX(vbox), checkbtn_hbox, FALSE, FALSE, 0);

	subfolder_checkbtn =
		gtk_check_button_new_with_label(_("Search subfolders"));
	gtk_widget_show(subfolder_checkbtn);
	gtk_box_pack_start(GTK_BOX(checkbtn_hbox), subfolder_checkbtn,
			   FALSE, FALSE, 0);

	case_checkbtn = gtk_check_button_new_with_label(_("Case sensitive"));
	gtk_widget_show(case_checkbtn);
	gtk_box_pack_start(GTK_BOX(checkbtn_hbox), case_checkbtn,
			   FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(dialog->ok_btn), "clicked",
			 G_CALLBACK(prefs_search_folder_ok_cb), new_dialog);
	g_signal_connect(G_OBJECT(dialog->apply_btn), "clicked",
			 G_CALLBACK(prefs_search_folder_apply_cb), new_dialog);
	g_signal_connect(G_OBJECT(dialog->cancel_btn), "clicked",
			 G_CALLBACK(prefs_search_folder_cancel_cb), new_dialog);

	new_dialog->dialog = dialog;
	new_dialog->item = item;
	new_dialog->bool_optmenu = bool_optmenu;
	new_dialog->cond_edit = cond_edit;

	new_dialog->folder_entry = folder_entry;
	new_dialog->folder_btn = folder_btn;
	new_dialog->subfolder_checkbtn = subfolder_checkbtn;
	new_dialog->case_checkbtn = case_checkbtn;

	return new_dialog;
}

static void prefs_search_folder_set_dialog(PrefsSearchFolderDialog *dialog)
{
	GSList *flist;
	FilterRule *rule;
	gchar *path;
	gchar *rule_file;
	FolderItem *target;
	gint index;

	path = folder_item_get_path(dialog->item);
	rule_file = g_strconcat(path, G_DIR_SEPARATOR_S, "filter.xml", NULL);
	flist = filter_read_file(rule_file);
	g_free(rule_file);
	g_free(path);

	if (!flist) {
		g_warning("filter rule not found\n");
		return;
	}

	rule = (FilterRule *)flist->data;
	target = folder_find_item_from_identifier(rule->target_folder);

	index = menu_find_option_menu_index
		(GTK_OPTION_MENU(dialog->bool_optmenu),
		 GINT_TO_POINTER(rule->bool_op), NULL);
	if (index < 0)
		index = 0;
	gtk_option_menu_set_history(GTK_OPTION_MENU(dialog->bool_optmenu),
				    index);

	prefs_filter_set_header_list(NULL);
	prefs_filter_edit_set_header_list(dialog->cond_edit, rule);
	prefs_filter_edit_add_rule_cond(dialog->cond_edit, rule);

	filter_rule_list_free(flist);
}

static void prefs_search_folder_destroy(PrefsSearchFolderDialog *dialog)
{
	prefs_dialog_destroy(dialog->dialog);
	g_free(dialog->dialog);
	g_free(dialog);

	main_window_popup(main_window_get());
}

static gint prefs_search_folder_delete_cb(GtkWidget *widget, GdkEventAny *event,
					  PrefsSearchFolderDialog *dialog)
{
	prefs_search_folder_destroy(dialog);
	return TRUE;
}

static gint prefs_search_folder_key_press_cb(GtkWidget *widget,
					     GdkEventKey *event,
					     PrefsSearchFolderDialog *dialog)
{
	if (event && event->keyval == GDK_Escape) {
		prefs_search_folder_cancel_cb(widget, dialog);
		return TRUE;
	}
	return FALSE;
}

static void prefs_search_folder_ok_cb(GtkWidget *widget,
				      PrefsSearchFolderDialog *dialog)
{
	prefs_search_folder_apply_cb(widget, dialog);
	prefs_search_folder_destroy(dialog);
}

static void prefs_search_folder_apply_cb(GtkWidget *widget,
					 PrefsSearchFolderDialog *dialog)
{
	// create filter rule
	// delete search cache
}

static void prefs_search_folder_cancel_cb(GtkWidget *widget,
					  PrefsSearchFolderDialog *dialog)
{
	prefs_search_folder_destroy(dialog);
}
