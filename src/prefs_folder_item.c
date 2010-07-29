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
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "folder.h"
#include "prefs.h"
#include "prefs_ui.h"
#include "prefs_folder_item.h"
#include "prefs_account.h"
#include "account.h"
#include "manage_window.h"
#include "folderview.h"
#include "mainwindow.h"
#include "inc.h"
#include "menu.h"

typedef struct _PrefsFolderItemDialog	PrefsFolderItemDialog;

struct _PrefsFolderItemDialog
{
	PrefsDialog *dialog;
	FolderItem *item;

	/* General */
	GtkWidget *name_entry;
	GtkWidget *id_label;
	GtkWidget *path_label;
	GtkWidget *type_optmenu;

	GtkWidget *trim_summary_subj_chkbtn;
	GtkWidget *trim_compose_subj_chkbtn;

	/* Compose */
	GtkWidget *account_optmenu;
	GtkWidget *ac_apply_sub_chkbtn;
	GtkWidget *to_entry;
	GtkWidget *on_reply_chkbtn;
	GtkWidget *cc_entry;
	GtkWidget *bcc_entry;
	GtkWidget *replyto_entry;
};

static PrefsFolderItemDialog *prefs_folder_item_create
					(FolderItem		*item);
static void prefs_folder_item_general_create
					(PrefsFolderItemDialog	*dialog);
static void prefs_folder_item_compose_create
					(PrefsFolderItemDialog	*dialog);
static void prefs_folder_item_set_dialog(PrefsFolderItemDialog	*dialog);

static void prefs_folder_item_ok_cb	(GtkWidget		*widget, 
					 PrefsFolderItemDialog	*dialog);
static void prefs_folder_item_apply_cb	(GtkWidget		*widget, 
					 PrefsFolderItemDialog	*dialog);
static void prefs_folder_item_cancel_cb	(GtkWidget		*widget, 
					 PrefsFolderItemDialog	*dialog);
static gint prefs_folder_item_delete_cb	(GtkWidget		*widget,
					 GdkEventAny		*event, 
					 PrefsFolderItemDialog	*dialog);
static gboolean prefs_folder_item_key_press_cb
					(GtkWidget		*widget,
					 GdkEventKey		*event,
					 PrefsFolderItemDialog	*dialog);

void prefs_folder_item_open(FolderItem *item)
{
	PrefsFolderItemDialog *dialog;

	g_return_if_fail(item != NULL);

	inc_lock();

	dialog = prefs_folder_item_create(item);

	manage_window_set_transient(GTK_WINDOW(dialog->dialog->window));

	prefs_folder_item_set_dialog(dialog);

	gtk_widget_show_all(dialog->dialog->window);
}

PrefsFolderItemDialog *prefs_folder_item_create(FolderItem *item)
{
	PrefsFolderItemDialog *new_dialog;
	PrefsDialog *dialog;

	new_dialog = g_new0(PrefsFolderItemDialog, 1);

	dialog = g_new0(PrefsDialog, 1);
	prefs_dialog_create(dialog);

	gtk_window_set_title(GTK_WINDOW(dialog->window), _("Folder properties"));
	gtk_widget_realize(dialog->window);
	g_signal_connect(G_OBJECT(dialog->window), "delete_event",
			 G_CALLBACK(prefs_folder_item_delete_cb), new_dialog);
	g_signal_connect(G_OBJECT(dialog->window), "key_press_event",
			 G_CALLBACK(prefs_folder_item_key_press_cb), new_dialog);
	MANAGE_WINDOW_SIGNALS_CONNECT(dialog->window);

	g_signal_connect(G_OBJECT(dialog->ok_btn), "clicked",
			 G_CALLBACK(prefs_folder_item_ok_cb), new_dialog);
	g_signal_connect(G_OBJECT(dialog->apply_btn), "clicked",
			 G_CALLBACK(prefs_folder_item_apply_cb), new_dialog);
	g_signal_connect(G_OBJECT(dialog->cancel_btn), "clicked",
			 G_CALLBACK(prefs_folder_item_cancel_cb), new_dialog);

	new_dialog->dialog = dialog;
	new_dialog->item = item;

	prefs_folder_item_general_create(new_dialog);
	prefs_folder_item_compose_create(new_dialog);

	SET_NOTEBOOK_LABEL(dialog->notebook, _("General"), 0);
	SET_NOTEBOOK_LABEL(dialog->notebook, _("Compose"), 1);

	return new_dialog;
}

static void prefs_folder_item_general_create(PrefsFolderItemDialog *dialog)
{
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *name_entry;
	GtkWidget *id_label;
	GtkWidget *path_label;
	GtkWidget *optmenu;
	GtkWidget *optmenu_menu;
	GtkWidget *menuitem;
	GtkWidget *vbox2;
	GtkWidget *trim_summary_subj_chkbtn;
	GtkWidget *trim_compose_subj_chkbtn;
	GtkStyle *style;

	vbox = gtk_vbox_new(FALSE, VSPACING);
	gtk_container_add(GTK_CONTAINER(dialog->dialog->notebook), vbox);
	gtk_container_set_border_width(GTK_CONTAINER (vbox), VBOX_BORDER);

	table = gtk_table_new(4, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	gtk_table_set_row_spacings(GTK_TABLE(table), 8);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	label = gtk_label_new(_("Name"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
			 GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);

	name_entry = gtk_entry_new();
	gtk_editable_set_editable(GTK_EDITABLE(name_entry), FALSE);
	gtk_widget_set_size_request(name_entry, 200, -1);
	style = gtk_widget_get_style(dialog->dialog->window);
	gtk_widget_modify_base(name_entry, GTK_STATE_NORMAL,
			       &style->bg[GTK_STATE_NORMAL]);
	gtk_table_attach(GTK_TABLE(table), name_entry, 1, 2, 0, 1,
			 GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			 GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);

	label = gtk_label_new(_("Identifier"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
			 GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);

	id_label = gtk_label_new("");
	gtk_label_set_selectable(GTK_LABEL(id_label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(id_label), 0, 0.5);
	gtk_label_set_justify(GTK_LABEL(id_label), GTK_JUSTIFY_LEFT);
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_label_set_ellipsize(GTK_LABEL(id_label), PANGO_ELLIPSIZE_MIDDLE);
#endif
	gtk_table_attach(GTK_TABLE(table), id_label, 1, 2, 1, 2,
			 GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			 GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);

	label = gtk_label_new(_("Path"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3,
			 GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);

	path_label = gtk_label_new("");
	gtk_label_set_selectable(GTK_LABEL(path_label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(path_label), 0, 0.5);
	gtk_label_set_justify(GTK_LABEL(path_label), GTK_JUSTIFY_LEFT);
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_label_set_ellipsize(GTK_LABEL(path_label), PANGO_ELLIPSIZE_MIDDLE);
#endif
	gtk_table_attach(GTK_TABLE(table), path_label, 1, 2, 2, 3,
			 GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			 GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);

	label = gtk_label_new(_("Type"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4,
			 GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);

	hbox = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(hbox);
	gtk_table_attach(GTK_TABLE(table), hbox, 1, 2, 3, 4,
			 GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			 GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);

	optmenu = gtk_option_menu_new();
	gtk_box_pack_start(GTK_BOX(hbox), optmenu, FALSE, FALSE, 0);

	optmenu_menu = gtk_menu_new();

	MENUITEM_ADD(optmenu_menu, menuitem, _("Normal"), F_NORMAL);
	MENUITEM_ADD(optmenu_menu, menuitem, _("Inbox") , F_INBOX);
	MENUITEM_ADD(optmenu_menu, menuitem, _("Sent")  , F_OUTBOX);
	MENUITEM_ADD(optmenu_menu, menuitem, _("Drafts"), F_DRAFT);
	MENUITEM_ADD(optmenu_menu, menuitem, _("Queue") , F_QUEUE);
	MENUITEM_ADD(optmenu_menu, menuitem, _("Trash") , F_TRASH);
	MENUITEM_ADD(optmenu_menu, menuitem, _("Junk")  , F_JUNK);

	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), optmenu_menu);

	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), vbox2, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON(vbox2, trim_summary_subj_chkbtn,
			  _("Don't display [...] or (...) at the beginning of subject in summary"));
	PACK_CHECK_BUTTON(vbox2, trim_compose_subj_chkbtn,
			  _("Delete [...] or (...) at the beginning of subject on reply"));

	if (!dialog->item->parent) {
		gtk_widget_set_sensitive(optmenu, FALSE);
		gtk_widget_set_sensitive(vbox2, FALSE);
	}
	if (dialog->item->stype == F_VIRTUAL)
		gtk_widget_set_sensitive(optmenu, FALSE);

	dialog->name_entry = name_entry;
	dialog->id_label = id_label;
	dialog->path_label = path_label;
	dialog->type_optmenu = optmenu;
	dialog->trim_summary_subj_chkbtn = trim_summary_subj_chkbtn;
	dialog->trim_compose_subj_chkbtn = trim_compose_subj_chkbtn;
}

static void prefs_folder_item_compose_create(PrefsFolderItemDialog *dialog)
{
	GtkWidget *vbox;
	GtkWidget *frame;
	GtkWidget *account_vbox;
	GtkWidget *table;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *optmenu;
	GtkWidget *optmenu_menu;
	GtkWidget *menuitem;
	GtkWidget *ac_apply_sub_chkbtn;
	GtkWidget *to_entry;
	GtkWidget *on_reply_chkbtn;
	GtkWidget *cc_entry;
	GtkWidget *bcc_entry;
	GtkWidget *replyto_entry;
	GList *list;

	vbox = gtk_vbox_new(FALSE, VSPACING);
	gtk_container_add(GTK_CONTAINER(dialog->dialog->notebook), vbox);
	gtk_container_set_border_width(GTK_CONTAINER (vbox), VBOX_BORDER);

	PACK_FRAME(vbox, frame, _("Account"));

	account_vbox = gtk_vbox_new(FALSE, VSPACING_NARROW);
	gtk_container_add(GTK_CONTAINER(frame), account_vbox);
	gtk_container_set_border_width (GTK_CONTAINER (account_vbox), 8);

	table = gtk_table_new(1, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(account_vbox), table, FALSE, FALSE, 0);
	gtk_table_set_row_spacings(GTK_TABLE(table), VSPACING_NARROW);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	label = gtk_label_new(_("Account"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
			 GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);

	hbox = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(hbox);
	gtk_table_attach(GTK_TABLE(table), hbox, 1, 2, 0, 1,
			 GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			 GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);

	optmenu = gtk_option_menu_new();
	gtk_box_pack_start(GTK_BOX(hbox), optmenu, FALSE, FALSE, 0);

	optmenu_menu = gtk_menu_new();

	MENUITEM_ADD(optmenu_menu, menuitem, _("None"), -1);

	for (list = account_get_list(); list != NULL; list = list->next) {
		gchar *text;
		PrefsAccount *ac = list->data;

		text = g_strdup_printf("%s: %s", ac->account_name, ac->address);
		MENUITEM_ADD(optmenu_menu, menuitem, text, ac->account_id);
	}

	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), optmenu_menu);

	PACK_CHECK_BUTTON(account_vbox, ac_apply_sub_chkbtn,
			  _("Apply to subfolders"));

	PACK_FRAME(vbox, frame, _("Automatically set the following addresses"));

	table = gtk_table_new(4, 2, FALSE);
	gtk_container_add(GTK_CONTAINER(frame), table);
	gtk_container_set_border_width (GTK_CONTAINER (table), 8);
	gtk_table_set_row_spacings(GTK_TABLE(table), VSPACING_NARROW);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	label = gtk_label_new(_("To:"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
			 GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);

	hbox = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(hbox);
	gtk_table_attach(GTK_TABLE(table), hbox, 1, 2, 0, 1,
			 GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			 GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);

	to_entry = gtk_entry_new();
	gtk_widget_set_size_request(to_entry, 200, -1);
	gtk_box_pack_start(GTK_BOX(hbox), to_entry, TRUE, TRUE, 0);

	PACK_CHECK_BUTTON(hbox, on_reply_chkbtn, _("use also on reply"));

	label = gtk_label_new(_("Cc:"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
			 GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);

	cc_entry = gtk_entry_new();
	gtk_widget_set_size_request(cc_entry, 200, -1);
	gtk_table_attach(GTK_TABLE(table), cc_entry, 1, 2, 1, 2,
			 GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			 GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);

	label = gtk_label_new(_("Bcc:"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3,
			 GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);

	bcc_entry = gtk_entry_new();
	gtk_widget_set_size_request(bcc_entry, 200, -1);
	gtk_table_attach(GTK_TABLE(table), bcc_entry, 1, 2, 2, 3,
			 GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			 GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);

	label = gtk_label_new(_("Reply-To:"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4,
			 GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);

	replyto_entry = gtk_entry_new();
	gtk_widget_set_size_request(replyto_entry, 200, -1);
	gtk_table_attach(GTK_TABLE(table), replyto_entry, 1, 2, 3, 4,
			 GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			 GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);

	if (!dialog->item->parent) {
		gtk_widget_set_sensitive(frame, FALSE);
		gtk_widget_set_sensitive(ac_apply_sub_chkbtn, FALSE);
	}

	dialog->account_optmenu     = optmenu;
	dialog->ac_apply_sub_chkbtn = ac_apply_sub_chkbtn;
	dialog->to_entry            = to_entry;
	dialog->on_reply_chkbtn     = on_reply_chkbtn;
	dialog->cc_entry            = cc_entry;
	dialog->bcc_entry           = bcc_entry;
	dialog->replyto_entry       = replyto_entry;
}

#define SET_ENTRY(entry, str) \
	gtk_entry_set_text(GTK_ENTRY(dialog->entry), \
			   dialog->item->str ? dialog->item->str : "")

static void prefs_folder_item_set_dialog(PrefsFolderItemDialog *dialog)
{
	GtkWidget *menu;
	GtkWidget *menuitem;
	GtkOptionMenu *optmenu;
	gchar *id;
	gchar *path;
	gchar *utf8_path;
	GList *cur;
	SpecialFolderItemType type;
	gint n;
	guint index = 0;

	/* General */

	SET_ENTRY(name_entry, name);

	id = folder_item_get_identifier(dialog->item);
	gtk_label_set_text(GTK_LABEL(dialog->id_label), id);
	g_free(id);

	path = folder_item_get_path(dialog->item);
	utf8_path = conv_filename_to_utf8(path);
	gtk_label_set_text(GTK_LABEL(dialog->path_label), utf8_path);
	g_free(utf8_path);
	g_free(path);

	optmenu = GTK_OPTION_MENU(dialog->type_optmenu);
	menu = gtk_option_menu_get_menu(optmenu);
	for (cur = GTK_MENU_SHELL(menu)->children, n = 0;
	     cur != NULL; cur = cur->next, n++) {
		menuitem = GTK_WIDGET(cur->data);
		type = (SpecialFolderItemType)
			g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID);
		if (type != F_NORMAL &&
		    FOLDER_TYPE(dialog->item->folder) == F_NEWS)
			gtk_widget_set_sensitive(menuitem, FALSE);
		if (dialog->item->stype == type)
			index = n;
	}

	gtk_option_menu_set_history(optmenu, index);

	gtk_toggle_button_set_active
		(GTK_TOGGLE_BUTTON(dialog->trim_summary_subj_chkbtn),
		 dialog->item->trim_summary_subject);
	gtk_toggle_button_set_active
		(GTK_TOGGLE_BUTTON(dialog->trim_compose_subj_chkbtn),
		 dialog->item->trim_compose_subject);

	/* Compose */

	index = 0;
	optmenu = GTK_OPTION_MENU(dialog->account_optmenu);
	if (dialog->item->account) {
		index = menu_find_option_menu_index
			(optmenu,
			 GINT_TO_POINTER(dialog->item->account->account_id),
			 NULL);
		if (index < 0)
			index = 0;
	}

	gtk_option_menu_set_history(optmenu, index);

	gtk_toggle_button_set_active
		(GTK_TOGGLE_BUTTON(dialog->ac_apply_sub_chkbtn),
		 dialog->item->ac_apply_sub);

	SET_ENTRY(to_entry, auto_to);
	gtk_toggle_button_set_active
		(GTK_TOGGLE_BUTTON(dialog->on_reply_chkbtn),
		 dialog->item->use_auto_to_on_reply);

	SET_ENTRY(cc_entry, auto_cc);
	SET_ENTRY(bcc_entry, auto_bcc);
	SET_ENTRY(replyto_entry, auto_replyto);
}

#undef SET_ENTRY

void prefs_folder_item_destroy(PrefsFolderItemDialog *dialog) 
{
	prefs_dialog_destroy(dialog->dialog);
	g_free(dialog->dialog);
	g_free(dialog);

	main_window_popup(main_window_get());
	inc_unlock();
}

static void prefs_folder_item_ok_cb(GtkWidget *widget,
				    PrefsFolderItemDialog *dialog)
{
	prefs_folder_item_apply_cb(widget, dialog);
	prefs_folder_item_destroy(dialog);
}

#define SET_DATA_FROM_ENTRY(entry, str) \
{ \
	entry_str = gtk_entry_get_text(GTK_ENTRY(dialog->entry)); \
	g_free(item->str); \
	item->str = (entry_str && *entry_str) ? g_strdup(entry_str) : NULL; \
}

static void prefs_folder_item_apply_cb(GtkWidget *widget,
				       PrefsFolderItemDialog *dialog)
{
	GtkWidget *menu;
	GtkWidget *menuitem;
	GtkOptionMenu *optmenu;
	SpecialFolderItemType type;
	FolderItem *item = dialog->item;
	Folder *folder = item->folder;
	FolderItem *prev_item = NULL;
	gint account_id;
	const gchar *entry_str;

	optmenu = GTK_OPTION_MENU(dialog->type_optmenu);
	menu = gtk_option_menu_get_menu(optmenu);
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	type = (SpecialFolderItemType)
		g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID);

	if (item->stype != type && item->stype != F_VIRTUAL) {
		switch (type) {
		case F_NORMAL:
			break;
		case F_INBOX:
			if (folder->inbox)
				folder->inbox->stype = F_NORMAL;
			prev_item = folder->inbox;
			folder->inbox = item;
			break;
		case F_OUTBOX:
			if (folder->outbox)
				folder->outbox->stype = F_NORMAL;
			prev_item = folder->outbox;
			folder->outbox = item;
			break;
		case F_DRAFT:
			if (folder->draft)
				folder->draft->stype = F_NORMAL;
			prev_item = folder->draft;
			folder->draft = item;
			break;
		case F_QUEUE:
			if (folder->queue)
				folder->queue->stype = F_NORMAL;
			prev_item = folder->queue;
			folder->queue = item;
			break;
		case F_TRASH:
			if (folder->trash)
				folder->trash->stype = F_NORMAL;
			prev_item = folder->trash;
			folder->trash = item;
			break;
		case F_JUNK:
			prev_item = folder_get_junk(folder);
			if (prev_item)
				prev_item->stype = F_NORMAL;
			folder_set_junk(folder, item);
			break;
		default:
			type = item->stype;
			break;
		}

		item->stype = type;

		if (prev_item)
			folderview_update_item(prev_item, FALSE);
		folderview_update_item(item, FALSE);
	}

	item->trim_summary_subject = gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON(dialog->trim_summary_subj_chkbtn));
	item->trim_compose_subject = gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON(dialog->trim_compose_subj_chkbtn));

	/* account menu */
	optmenu = GTK_OPTION_MENU(dialog->account_optmenu);
	menu = gtk_option_menu_get_menu(optmenu);
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	account_id = GPOINTER_TO_INT
		(g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID));
	if (account_id >= 0)
		item->account = account_find_from_id(account_id);
	else
		item->account = NULL;

	if (!item->parent && item->account)
		item->ac_apply_sub = TRUE;
	else if (item->account)
		item->ac_apply_sub = gtk_toggle_button_get_active
			(GTK_TOGGLE_BUTTON(dialog->ac_apply_sub_chkbtn));
	else
		item->ac_apply_sub = FALSE;

	SET_DATA_FROM_ENTRY(to_entry, auto_to);
	item->use_auto_to_on_reply = gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON(dialog->on_reply_chkbtn));

	SET_DATA_FROM_ENTRY(cc_entry, auto_cc);
	SET_DATA_FROM_ENTRY(bcc_entry, auto_bcc);
	SET_DATA_FROM_ENTRY(replyto_entry, auto_replyto);
}

#undef SET_DATA_FROM_ENTRY

static void prefs_folder_item_cancel_cb(GtkWidget *widget,
					PrefsFolderItemDialog *dialog)
{
	prefs_folder_item_destroy(dialog);
}

static gint prefs_folder_item_delete_cb(GtkWidget *widget, GdkEventAny *event,
					PrefsFolderItemDialog *dialog)
{
	prefs_folder_item_destroy(dialog);
	return TRUE;
}

static gboolean prefs_folder_item_key_press_cb(GtkWidget *widget,
					   GdkEventKey *event,
					   PrefsFolderItemDialog *dialog)
{
	if (event && event->keyval == GDK_Escape)
		prefs_folder_item_cancel_cb(widget, dialog);
	return FALSE;
}
