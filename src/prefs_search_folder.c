/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto
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
#include <gtk/gtkstyle.h>
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

	GtkWidget *name_entry;

	GtkWidget *bool_optmenu;

	FilterCondEdit *cond_edit;

	GtkWidget *folder_entry;
	GtkWidget *folder_btn;

	GtkWidget *subfolder_checkbtn;
	GtkWidget *case_checkbtn;

	gboolean finished;
	gboolean updated;
};

static PrefsSearchFolderDialog *prefs_search_folder_create(FolderItem *item);
static void prefs_search_folder_set_dialog(PrefsSearchFolderDialog *dialog);
static void prefs_search_folder_destroy	  (PrefsSearchFolderDialog *dialog);

static gint prefs_search_folder_delete_cb(GtkWidget		  *widget,
					  GdkEventAny		  *event,
					  PrefsSearchFolderDialog *dialog);
static gint prefs_search_folder_key_press_cb
					 (GtkWidget		  *widget,
					  GdkEventKey		  *event,
					  PrefsSearchFolderDialog *dialog);

static void prefs_search_folder_select_folder
					 (GtkWidget		  *widget,
					  PrefsSearchFolderDialog *dialog);

static void prefs_search_folder_ok_cb	 (GtkWidget		  *widget,
					  PrefsSearchFolderDialog *dialog);
static void prefs_search_folder_apply_cb (GtkWidget		  *widget,
					  PrefsSearchFolderDialog *dialog);
static void prefs_search_folder_cancel_cb(GtkWidget		  *widget,
					  PrefsSearchFolderDialog *dialog);


gboolean prefs_search_folder_open(FolderItem *item)
{
	PrefsSearchFolderDialog *dialog;
	gboolean updated;

	g_return_val_if_fail(item != NULL, FALSE);

	dialog = prefs_search_folder_create(item);
	manage_window_set_transient(GTK_WINDOW(dialog->dialog->window));
	prefs_search_folder_set_dialog(dialog);
	gtk_widget_show(dialog->dialog->window);

	while (dialog->finished == FALSE)
		gtk_main_iteration();

	updated = dialog->updated;
	prefs_search_folder_destroy(dialog);

	return updated;
}

static PrefsSearchFolderDialog *prefs_search_folder_create(FolderItem *item)
{
	PrefsSearchFolderDialog *new_dialog;
	PrefsDialog *dialog;
	gchar *title;
	GtkWidget *vbox;

	GtkWidget *name_hbox;
	GtkWidget *name_label;
	GtkWidget *name_entry;
	GtkStyle *style;

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
	gtk_widget_hide(dialog->apply_btn);

	gtk_widget_set_size_request(dialog->window, 600, -1);
	title = g_strdup_printf(_("%s - Edit search condition"), item->name);
	gtk_window_set_title(GTK_WINDOW(dialog->window), title);
	g_free(title);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(dialog->notebook), FALSE);
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(dialog->notebook), FALSE);
	gtk_widget_realize(dialog->window);

	g_signal_connect(G_OBJECT(dialog->window), "delete_event",
			 G_CALLBACK(prefs_search_folder_delete_cb), new_dialog);
	g_signal_connect(G_OBJECT(dialog->window), "key_press_event",
			 G_CALLBACK(prefs_search_folder_key_press_cb),
			 new_dialog);

	MANAGE_WINDOW_SIGNALS_CONNECT(dialog->window);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(dialog->notebook), vbox);

	name_hbox = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(name_hbox);
	gtk_box_pack_start(GTK_BOX(vbox), name_hbox, FALSE, FALSE, 0);

	name_label = gtk_label_new(_("Name:"));
	gtk_widget_show(name_label);
	gtk_box_pack_start(GTK_BOX(name_hbox), name_label, FALSE, FALSE, 0);

	name_entry = gtk_entry_new();
	gtk_widget_show(name_entry);
	gtk_editable_set_editable(GTK_EDITABLE(name_entry), FALSE);
	gtk_box_pack_start(GTK_BOX(name_hbox), name_entry, TRUE, TRUE, 0);

	style = gtk_widget_get_style(dialog->window);
	gtk_widget_modify_base(name_entry, GTK_STATE_NORMAL,
			       &style->bg[GTK_STATE_NORMAL]);
	
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

	folder_hbox = gtk_hbox_new(FALSE, 8);
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

	g_signal_connect(G_OBJECT(folder_btn), "clicked",
			 G_CALLBACK(prefs_search_folder_select_folder),
			 new_dialog);
	g_signal_connect(G_OBJECT(dialog->ok_btn), "clicked",
			 G_CALLBACK(prefs_search_folder_ok_cb), new_dialog);
	g_signal_connect(G_OBJECT(dialog->apply_btn), "clicked",
			 G_CALLBACK(prefs_search_folder_apply_cb), new_dialog);
	g_signal_connect(G_OBJECT(dialog->cancel_btn), "clicked",
			 G_CALLBACK(prefs_search_folder_cancel_cb), new_dialog);

	gtk_widget_grab_focus(dialog->ok_btn);

	new_dialog->dialog = dialog;
	new_dialog->item = item;
	new_dialog->name_entry = name_entry;
	new_dialog->bool_optmenu = bool_optmenu;
	new_dialog->cond_edit = cond_edit;

	new_dialog->folder_entry = folder_entry;
	new_dialog->folder_btn = folder_btn;
	new_dialog->subfolder_checkbtn = subfolder_checkbtn;
	new_dialog->case_checkbtn = case_checkbtn;

	new_dialog->finished = FALSE;
	new_dialog->updated = FALSE;

	return new_dialog;
}

static void prefs_search_folder_set_dialog(PrefsSearchFolderDialog *dialog)
{
	GSList *flist;
	FilterRule *rule;
	GSList *cur;
	gchar *path;
	gchar *rule_file;
	gint index;
	gboolean case_sens = FALSE;

	path = folder_item_get_path(dialog->item);
	rule_file = g_strconcat(path, G_DIR_SEPARATOR_S, "filter.xml", NULL);
	flist = filter_read_file(rule_file);
	g_free(rule_file);
	g_free(path);

	if (!flist) {
		g_warning("filter rule not found\n");
		return;
	}

	gtk_entry_set_text(GTK_ENTRY(dialog->name_entry), dialog->item->name);

	rule = (FilterRule *)flist->data;

	index = menu_find_option_menu_index
		(GTK_OPTION_MENU(dialog->bool_optmenu),
		 GINT_TO_POINTER(rule->bool_op), NULL);
	if (index < 0)
		index = 0;
	gtk_option_menu_set_history(GTK_OPTION_MENU(dialog->bool_optmenu),
				    index);

	gtk_entry_set_text(GTK_ENTRY(dialog->folder_entry),
			   rule->target_folder);
	gtk_toggle_button_set_active
		(GTK_TOGGLE_BUTTON(dialog->subfolder_checkbtn),
		 rule->recursive);
	for (cur = rule->cond_list; cur != NULL; cur = cur->next) {
		FilterCond *cond = (FilterCond *)cur->data;
		if (FLT_IS_CASE_SENS(cond->match_flag)) {
			case_sens = TRUE;
			break;
		}
	}
	gtk_toggle_button_set_active
		(GTK_TOGGLE_BUTTON(dialog->case_checkbtn), case_sens);

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
	dialog->finished = TRUE;
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

static void prefs_search_folder_select_folder(GtkWidget *widget,
					      PrefsSearchFolderDialog *dialog)
{
	FolderItem *item;
	gchar *id;

	item = foldersel_folder_sel(NULL, FOLDER_SEL_ALL, NULL);
	if (!item || item->stype == F_VIRTUAL)
		return;

	id = folder_item_get_identifier(item);
	if (id) {
		gtk_entry_set_text(GTK_ENTRY(dialog->folder_entry), id);
		g_free(id);
	}
}

static void prefs_search_folder_ok_cb(GtkWidget *widget,
				      PrefsSearchFolderDialog *dialog)
{
	prefs_search_folder_apply_cb(widget, dialog);
	dialog->finished = TRUE;
}

static void prefs_search_folder_apply_cb(GtkWidget *widget,
					 PrefsSearchFolderDialog *dialog)
{
	const gchar *id;
	FolderItem *item;
	FilterBoolOp bool_op;
	gboolean recursive;
	gboolean case_sens;
	GSList *cond_list;
	FilterRule *rule;
	GSList list;
	gchar *file;
	gchar *path;

	id = gtk_entry_get_text(GTK_ENTRY(dialog->folder_entry));
	item = folder_find_item_from_identifier(id);
	if (!item)
		return;

	bool_op = menu_get_option_menu_active_index
		(GTK_OPTION_MENU(dialog->bool_optmenu));
	recursive = gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON(dialog->subfolder_checkbtn));
	case_sens = gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON(dialog->case_checkbtn));

	cond_list = prefs_filter_edit_cond_edit_to_list(dialog->cond_edit,
							case_sens);
	if (!cond_list)
		return;

	rule = filter_rule_new(dialog->item->name, bool_op, cond_list, NULL);
	rule->target_folder = g_strdup(id);
	rule->recursive = recursive;
	list.data = rule;
	list.next = NULL;

	path = folder_item_get_path(dialog->item);
	file = g_strconcat(path, G_DIR_SEPARATOR_S, FILTER_LIST, NULL);
	filter_write_file(&list, file);
	g_free(file);
	file = g_strconcat(path, G_DIR_SEPARATOR_S, FILTER_LIST, ".bak", NULL);
	if (is_file_exist(file))
		g_unlink(file);
	g_free(file);
	file = g_strconcat(path, G_DIR_SEPARATOR_S, SEARCH_CACHE, NULL);
	if (is_file_exist(file))
		g_unlink(file);
	g_free(file);
	g_free(path);

	filter_rule_free(rule);

	dialog->updated = TRUE;
}

static void prefs_search_folder_cancel_cb(GtkWidget *widget,
					  PrefsSearchFolderDialog *dialog)
{
	dialog->finished = TRUE;
}
