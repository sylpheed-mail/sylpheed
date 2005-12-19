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
#include <gtk/gtktable.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkstock.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkcellrenderertext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "summary_search.h"
#include "prefs_filter_edit.h"
#include "summaryview.h"
#include "messageview.h"
#include "mainwindow.h"
#include "folderview.h"
#include "menu.h"
#include "utils.h"
#include "gtkutils.h"
#include "manage_window.h"
#include "alertpanel.h"
#include "foldersel.h"
#include "procmsg.h"
#include "procheader.h"
#include "folder.h"
#include "filter.h"
#include "prefs_filter.h"
#include "prefs_filter_edit.h"

enum
{
	COL_FOLDER,
	COL_SUBJECT,
	COL_FROM,
	COL_DATE,
	COL_MSGINFO,
	N_COLS
};

static struct SummarySearchWindow {
	GtkWidget *window;

	GtkWidget *bool_optmenu;

	FilterCondEdit *cond_edit;

	GtkWidget *folder_entry;
	GtkWidget *folder_btn;

	GtkWidget *subfolder_checkbtn;
	GtkWidget *case_checkbtn;

	GtkWidget *treeview;
	GtkListStore *store;

	GtkWidget *status_label;

	GtkWidget *clear_btn;
	GtkWidget *search_btn;
	GtkWidget *save_btn;
	GtkWidget *close_btn;

	FilterRule *rule;
	gboolean requires_full_headers;

	gboolean on_search;
	gboolean cancelled;
} search_window;

typedef struct {
	GtkWidget *window;

	GtkWidget *folder_entry;
	GtkWidget *name_entry;

	gboolean cancelled;
	gboolean finished;
} SummarySearchSaveDialog;

static void summary_search_create	(void);

static FilterRule *summary_search_dialog_to_rule
					(const gchar	*name,
					 FolderItem    **item);

static void summary_search_query	(void);
static void summary_search_folder	(FolderItem	*item);

static gboolean summary_search_recursive_func	(GNode		*node,
						 gpointer	 data);

static void summary_search_append_msg	(MsgInfo	*msginfo);
static void summary_search_clear_list	(void);

static void summary_search_hbox_added	(CondHBox	*hbox);

static void row_activated		(GtkTreeView		*treeview,
					 GtkTreePath		*path,
					 GtkTreeViewColumn	*column,
					 gpointer		 data);

static gboolean row_selected		(GtkTreeSelection	*selection,
					 GtkTreeModel		*model,
					 GtkTreePath		*path,
					 gboolean		 cur_selected,
					 gpointer		 data);

static void summary_search_clear	(GtkButton	*button,
					 gpointer	 data);
static void summary_select_folder	(GtkButton	*button,
					 gpointer	 data);
static void summary_search_clicked	(GtkButton	*button,
					 gpointer	 data);
static void summary_search_save		(GtkButton	*button,
					 gpointer	 data);
static void summary_search_close	(GtkButton	*button,
					 gpointer	 data);

static void summary_search_entry_activated	(GtkWidget	*widget,
						 gpointer	 data);

static gint summary_search_deleted	(GtkWidget	*widget,
					 GdkEventAny	*event,
					 gpointer	 data);
static gboolean key_pressed		(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gpointer	 data);


void summary_search(FolderItem *item)
{
	gchar *id;

	if (!search_window.window)
		summary_search_create();
	else
		gtk_widget_hide(search_window.window);

	if (item) {
		id = folder_item_get_identifier(item);
		gtk_entry_set_text(GTK_ENTRY(search_window.folder_entry), id);
		g_free(id);
	} else
		gtk_entry_set_text(GTK_ENTRY(search_window.folder_entry), "");

	gtk_widget_grab_focus(search_window.search_btn);
	gtk_widget_show(search_window.window);
}

static void summary_search_create(void)
{
	GtkWidget *window;
	GtkWidget *vbox1;
	GtkWidget *bool_hbox;
	GtkWidget *bool_optmenu;
	GtkWidget *bool_menu;
	GtkWidget *menuitem;
	GtkWidget *clear_btn;
	GtkWidget *search_btn;

	GtkWidget *scrolledwin;
	FilterCondEdit *cond_edit;
	CondHBox *cond_hbox;

	GtkWidget *folder_hbox;
	GtkWidget *folder_label;
	GtkWidget *folder_entry;
	GtkWidget *folder_btn;

	GtkWidget *checkbtn_hbox;
	GtkWidget *subfolder_checkbtn;
	GtkWidget *case_checkbtn;

	GtkWidget *treeview;
	GtkListStore *store;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeSelection *selection;

	GtkWidget *confirm_area;

	GtkWidget *status_label;

	GtkWidget *btn_hbox;
	GtkWidget *hbbox;
	GtkWidget *save_btn;
	GtkWidget *close_btn;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW (window), _("Search messages"));
	gtk_widget_set_size_request(window, 600, -1);
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, TRUE);
	gtk_container_set_border_width(GTK_CONTAINER (window), 8);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(summary_search_deleted), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);

	vbox1 = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (window), vbox1);

	bool_hbox = gtk_hbox_new(FALSE, 12);
	gtk_widget_show(bool_hbox);
	gtk_box_pack_start(GTK_BOX(vbox1), bool_hbox, FALSE, FALSE, 0);

	bool_optmenu = gtk_option_menu_new();
	gtk_widget_show(bool_optmenu);
	gtk_box_pack_start(GTK_BOX(bool_hbox), bool_optmenu, FALSE, FALSE, 0);

	bool_menu = gtk_menu_new();
	MENUITEM_ADD(bool_menu, menuitem, _("Match any of the following"),
		     FLT_OR);
	MENUITEM_ADD(bool_menu, menuitem, _("Match all of the following"),
		     FLT_AND);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(bool_optmenu), bool_menu);

	hbbox = gtk_hbutton_box_new();
	gtk_widget_show(hbbox);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(hbbox), 6);
	gtk_box_pack_end(GTK_BOX(bool_hbox), hbbox, FALSE, FALSE, 0);

	clear_btn = gtk_button_new_from_stock(GTK_STOCK_CLEAR);
	gtk_widget_show(clear_btn);
	gtk_box_pack_start(GTK_BOX(hbbox), clear_btn, FALSE, FALSE, 0);

	search_btn = gtk_button_new_from_stock(GTK_STOCK_FIND);
	GTK_WIDGET_SET_FLAGS(search_btn, GTK_CAN_DEFAULT);
	gtk_widget_show(search_btn);
	gtk_box_pack_start(GTK_BOX(hbbox), search_btn, FALSE, FALSE, 0);
	gtk_widget_grab_default(search_btn);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scrolledwin);
	gtk_box_pack_start(GTK_BOX(vbox1), scrolledwin, FALSE, FALSE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request(scrolledwin, -1, 120);

	cond_edit = prefs_filter_edit_cond_edit_create();
	cond_edit->add_hbox = summary_search_hbox_added;
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolledwin),
					      cond_edit->cond_vbox);
	prefs_filter_set_header_list(NULL);
	prefs_filter_edit_set_header_list(cond_edit, NULL);
	cond_hbox = prefs_filter_edit_cond_hbox_create(cond_edit);
	prefs_filter_edit_set_cond_hbox_widgets(cond_hbox, PF_COND_HEADER);
	prefs_filter_edit_insert_cond_hbox(cond_edit, cond_hbox, -1);
	if (cond_edit->add_hbox)
		cond_edit->add_hbox(cond_hbox);

	folder_hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (folder_hbox);
	gtk_box_pack_start (GTK_BOX (vbox1), folder_hbox, FALSE, FALSE, 0);

	folder_label = gtk_label_new (_("Folder:"));
	gtk_widget_show (folder_label);
	gtk_box_pack_start (GTK_BOX (folder_hbox), folder_label,
			    FALSE, FALSE, 0);

	folder_entry = gtk_entry_new ();
	gtk_widget_show (folder_entry);
	gtk_box_pack_start (GTK_BOX (folder_hbox), folder_entry, TRUE, TRUE, 0);

	folder_btn = gtk_button_new_with_label("...");
	gtk_widget_show (folder_btn);
	gtk_box_pack_start (GTK_BOX (folder_hbox), folder_btn, FALSE, FALSE, 0);

	checkbtn_hbox = gtk_hbox_new (FALSE, 12);
	gtk_widget_show (checkbtn_hbox);
	gtk_box_pack_start (GTK_BOX (vbox1), checkbtn_hbox, FALSE, FALSE, 0);

	subfolder_checkbtn =
		gtk_check_button_new_with_label (_("Search subfolders"));
	gtk_widget_show (subfolder_checkbtn);
	gtk_box_pack_start (GTK_BOX (checkbtn_hbox), subfolder_checkbtn,
			    FALSE, FALSE, 0);

	case_checkbtn = gtk_check_button_new_with_label (_("Case sensitive"));
	gtk_widget_show (case_checkbtn);
	gtk_box_pack_start (GTK_BOX (checkbtn_hbox), case_checkbtn,
			    FALSE, FALSE, 0);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_box_pack_start(GTK_BOX(vbox1), scrolledwin, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_SHADOW_IN);
	gtk_widget_set_size_request(scrolledwin, -1, 150);

	store = gtk_list_store_new(N_COLS,
				   G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
				   G_TYPE_STRING, G_TYPE_POINTER);
	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(store);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);
	g_signal_connect(G_OBJECT(treeview), "row-activated",
			 G_CALLBACK(row_activated), NULL);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);
	gtk_tree_selection_set_select_function(selection, row_selected,
					       NULL, NULL);

	gtk_container_add(GTK_CONTAINER(scrolledwin), treeview);

#define APPEND_COLUMN(label, col, width)				\
{									\
	renderer = gtk_cell_renderer_text_new();			\
	column = gtk_tree_view_column_new_with_attributes		\
		(label, renderer, "text", col, NULL);			\
	gtk_tree_view_column_set_resizable(column, TRUE);		\
	if (width) {							\
		gtk_tree_view_column_set_sizing				\
			(column, GTK_TREE_VIEW_COLUMN_FIXED);		\
		gtk_tree_view_column_set_fixed_width(column, width);	\
	}								\
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);	\
}

	APPEND_COLUMN(_("Folder"), COL_FOLDER, 0);
	APPEND_COLUMN(_("Subject"), COL_SUBJECT, 200);
	APPEND_COLUMN(_("From"), COL_FROM, 180);
	APPEND_COLUMN(_("Date"), COL_DATE, 0);

	gtk_widget_show_all(scrolledwin);

	confirm_area = gtk_hbox_new(FALSE, 12);
	gtk_widget_show(confirm_area);
	gtk_box_pack_start(GTK_BOX(vbox1), confirm_area, FALSE, FALSE, 0);

	status_label = gtk_label_new("");
	gtk_widget_show(status_label);
	gtk_box_pack_start(GTK_BOX(confirm_area), status_label,
			   FALSE, FALSE, 0);

	btn_hbox = gtk_hbox_new(FALSE, 6);
	gtk_widget_show(btn_hbox);
	gtk_box_pack_end(GTK_BOX(confirm_area), btn_hbox, FALSE, FALSE, 0);

	gtkut_stock_button_set_create(&hbbox, &close_btn, GTK_STOCK_CLOSE,
				      NULL, NULL, NULL, NULL);
	gtk_widget_show(hbbox);
	gtk_box_pack_end(GTK_BOX(btn_hbox), hbbox, FALSE, FALSE, 0);

	save_btn = gtk_button_new_with_mnemonic(_("_Save as search folder"));
	gtk_widget_show(save_btn);
	gtk_box_pack_end(GTK_BOX(btn_hbox), save_btn, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(clear_btn), "clicked",
			 G_CALLBACK(summary_search_clear), NULL);
	g_signal_connect(G_OBJECT(folder_btn), "clicked",
			 G_CALLBACK(summary_select_folder), NULL);
	g_signal_connect(G_OBJECT(search_btn), "clicked",
			 G_CALLBACK(summary_search_clicked), NULL);
	g_signal_connect(G_OBJECT(save_btn), "clicked",
			 G_CALLBACK(summary_search_save), NULL);
	g_signal_connect(G_OBJECT(close_btn), "clicked",
			 G_CALLBACK(summary_search_close), NULL);

	search_window.window = window;
	search_window.bool_optmenu = bool_optmenu;

	search_window.cond_edit = cond_edit;

	search_window.folder_entry = folder_entry;
	search_window.folder_btn = folder_btn;
	search_window.subfolder_checkbtn = subfolder_checkbtn;
	search_window.case_checkbtn = case_checkbtn;

	search_window.treeview = treeview;
	search_window.store = store;

	search_window.status_label = status_label;

	search_window.clear_btn = clear_btn;
	search_window.search_btn = search_btn;
	search_window.save_btn  = save_btn;
	search_window.close_btn = close_btn;
}

static FilterRule *summary_search_dialog_to_rule(const gchar *name,
						 FolderItem **item)
{
	const gchar *id;
	FolderItem *item_;
	FilterBoolOp bool_op = FLT_OR;
	gboolean recursive;
	gboolean case_sens;
	GSList *cond_list = NULL;
	FilterCond *cond;
	FilterRule *rule;
	GSList *cur;

	id = gtk_entry_get_text(GTK_ENTRY(search_window.folder_entry));
	item_ = folder_find_item_from_identifier(id);
	if (!item_)
		return NULL;
	if (item)
		*item = item_;

	bool_op = menu_get_option_menu_active_index
		(GTK_OPTION_MENU(search_window.bool_optmenu));
	recursive = gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON(search_window.subfolder_checkbtn));
	case_sens = gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON(search_window.case_checkbtn));

	for (cur = search_window.cond_edit->cond_hbox_list; cur != NULL;
	     cur = cur->next) {
		CondHBox *hbox = (CondHBox *)cur->data;
		gchar *error_msg;

		cond = prefs_filter_edit_cond_hbox_to_cond(hbox, case_sens,
							   &error_msg);
		if (cond) {
			cond_list = g_slist_append(cond_list, cond);
		} else {
			if (!error_msg)
				error_msg = _("Invalid condition exists.");
			alertpanel_error("%s", error_msg);
			filter_cond_list_free(cond_list);
			return NULL;
		}
	}

	if (!cond_list)
		return NULL;

	rule = filter_rule_new(name, bool_op, cond_list, NULL);
	rule->target_folder = g_strdup(id);
	rule->recursive = recursive;

	return rule;
}

static void summary_search_query(void)
{
	FolderItem *item;

	if (search_window.on_search)
		return;

	search_window.on_search = TRUE;

	search_window.rule = summary_search_dialog_to_rule("Query rule", &item);
	if (!search_window.rule) {
		search_window.on_search = FALSE;
		return;
	}
	search_window.requires_full_headers =
		filter_rule_requires_full_headers(search_window.rule);

	search_window.cancelled = FALSE;

	gtk_button_set_label(GTK_BUTTON(search_window.search_btn),
			     GTK_STOCK_STOP);
	summary_search_clear_list();

	if (search_window.rule->recursive)
		g_node_traverse(item->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
				summary_search_recursive_func, NULL);
	else
		summary_search_folder(item);

	filter_rule_free(search_window.rule);
	search_window.rule = NULL;
	search_window.requires_full_headers = FALSE;

	gtk_button_set_label(GTK_BUTTON(search_window.search_btn),
			     GTK_STOCK_FIND);
	gtk_label_set_text(GTK_LABEL(search_window.status_label), _("Done."));

	if (search_window.cancelled)
		debug_print("* query search cancelled.\n");
	debug_print("query search finished.\n");

	search_window.on_search = FALSE;
	search_window.cancelled = FALSE;
}

static void summary_search_folder(FolderItem *item)
{
	gchar *folder_name, *str;
	GSList *mlist;
	FilterInfo fltinfo;
	GSList *cur;
	gint count = 1, total;
	GTimeVal tv_prev, tv_cur;

	if (!item->path)
		return;

	folder_name = g_path_get_basename(item->path);
	str = g_strdup_printf(_("Searching %s ..."), folder_name);
	gtk_label_set_text(GTK_LABEL(search_window.status_label), str);
	g_free(str);
	g_get_current_time(&tv_prev);
	ui_update();

	if (search_window.cancelled) {
		g_free(folder_name);
		return;
	}

	mlist = folder_item_get_msg_list(item, TRUE);
	total = g_slist_length(mlist);

	memset(&fltinfo, 0, sizeof(FilterInfo));

	debug_print("requires_full_headers: %d\n",
		    search_window.requires_full_headers);
	debug_print("start query search: %s\n", item->path ? item->path : "");

	for (cur = mlist; cur != NULL; cur = cur->next) {
		MsgInfo *msginfo = (MsgInfo *)cur->data;
		GSList *hlist;

		g_get_current_time(&tv_cur);
		if (tv_cur.tv_sec > tv_prev.tv_sec ||
		    tv_cur.tv_usec - tv_prev.tv_usec >
		    PROGRESS_UPDATE_INTERVAL * 1000) {
			str = g_strdup_printf(_("Searching %s (%d / %d)..."),
					      folder_name, count, total);
			gtk_label_set_text
				(GTK_LABEL(search_window.status_label), str);
			g_free(str);
			ui_update();
			tv_prev = tv_cur;
		}
		++count;

		if (search_window.cancelled)
			break;

		fltinfo.flags = msginfo->flags;
		if (search_window.requires_full_headers) {
			gchar *file;

			file = procmsg_get_message_file(msginfo);
			hlist = procheader_get_header_list_from_file(file);
			g_free(file);
		} else
			hlist = procheader_get_header_list_from_msginfo
				(msginfo);
		if (!hlist)
			continue;

		if (filter_match_rule(search_window.rule, msginfo, hlist,
				      &fltinfo)) {
			summary_search_append_msg(msginfo);
			cur->data = NULL;
		}

		procheader_header_list_destroy(hlist);
	}

	procmsg_msg_list_free(mlist);
	g_free(folder_name);
}

static gboolean summary_search_recursive_func(GNode *node, gpointer data)
{
	FolderItem *item;

	g_return_val_if_fail(node->data != NULL, FALSE);

	item = FOLDER_ITEM(node->data);

	if (!item->path)
		return FALSE;

	summary_search_folder(item);

	if (search_window.cancelled)
		return TRUE;

	return FALSE;
}

static void summary_search_append_msg(MsgInfo *msginfo)
{
	GtkListStore *store = search_window.store;
	GtkTreeIter iter;
	gchar *folder;
	gchar date_buf[80];
	const gchar *subject, *from, *date;
	gchar *id;

	id = folder_item_get_identifier(msginfo->folder);
	folder = g_path_get_basename(id);
	g_free(id);
	subject = msginfo->subject ? msginfo->subject : _("(No Subject)");
	from = msginfo->from ? msginfo->from : _("(No From)");
	if (msginfo->date_t) {
		procheader_date_get_localtime(date_buf, sizeof(date_buf),
					      msginfo->date_t);
		date = date_buf;
	} else if (msginfo->date)
		date = msginfo->date;
	else
		date = _("(No Date)");

	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter,
			   COL_FOLDER, folder,
			   COL_SUBJECT, subject,
			   COL_FROM, from,
			   COL_DATE, date,
			   COL_MSGINFO, msginfo,
			   -1);

	g_free(folder);
}

static void summary_search_clear_list(void)
{
	GtkTreeIter iter;
	GtkTreeModel *model = GTK_TREE_MODEL(search_window.store);
	MsgInfo *msginfo;

	if (!gtk_tree_model_get_iter_first(model, &iter))
		return;

	do {
		gtk_tree_model_get(model, &iter, COL_MSGINFO, &msginfo, -1);
		procmsg_msginfo_free(msginfo);
	} while (gtk_tree_model_iter_next(model, &iter));

	gtk_list_store_clear(search_window.store);
}

static void summary_search_hbox_added(CondHBox *hbox)
{
	g_signal_connect(hbox->key_entry, "activate",
			 G_CALLBACK(summary_search_entry_activated), NULL);
}

static void row_activated(GtkTreeView *treeview, GtkTreePath *path,
			  GtkTreeViewColumn *column, gpointer data)
{
	GtkTreeIter iter;
	GtkTreeModel *model = GTK_TREE_MODEL(search_window.store);
	MsgInfo *msginfo;
	MessageView *msgview;

	if (!gtk_tree_model_get_iter(model, &iter, path))
		return;

	gtk_tree_model_get(model, &iter, COL_MSGINFO, &msginfo, -1);
	msgview = messageview_create_with_new_window();
	messageview_show(msgview, msginfo, FALSE);
}

static gboolean row_selected(GtkTreeSelection *selection,
			     GtkTreeModel *model, GtkTreePath *path,
			     gboolean cur_selected, gpointer data)
{
	return TRUE;
}

static void summary_search_clear(GtkButton *button, gpointer data)
{
	CondHBox *cond_hbox;

	prefs_filter_edit_clear_cond_edit(search_window.cond_edit);
	prefs_filter_set_header_list(NULL);
	prefs_filter_edit_set_header_list(search_window.cond_edit, NULL);
	cond_hbox = prefs_filter_edit_cond_hbox_create(search_window.cond_edit);
	prefs_filter_edit_set_cond_hbox_widgets(cond_hbox, PF_COND_HEADER);
	prefs_filter_edit_insert_cond_hbox
		(search_window.cond_edit, cond_hbox, -1);
	if (search_window.cond_edit->add_hbox)
		search_window.cond_edit->add_hbox(cond_hbox);

	gtk_label_set_text(GTK_LABEL(search_window.status_label), "");

	summary_search_clear_list();
}

static void summary_select_folder(GtkButton *button, gpointer data)
{
	FolderItem *item;
	gchar *id;

	item = foldersel_folder_sel(NULL, FOLDER_SEL_ALL, NULL);
	if (!item)
		return;

	id = folder_item_get_identifier(item);
	if (id) {
		gtk_entry_set_text(GTK_ENTRY(search_window.folder_entry), id);
		g_free(id);
	}
}

static void summary_search_clicked(GtkButton *button, gpointer data)
{
	if (search_window.on_search)
		search_window.cancelled = TRUE;
	else
		summary_search_query();
}

static gint summary_search_save_dialog_deleted(GtkWidget *widget,
					       GdkEventAny *event,
					       gpointer data)
{
	SummarySearchSaveDialog *dialog = (SummarySearchSaveDialog *)data;

	dialog->cancelled = TRUE;
	dialog->finished = TRUE;
	return TRUE;
}

static gint summary_search_save_dialog_key_pressed(GtkWidget *widget,
						   GdkEventKey *event,
						   gpointer data)
{
	SummarySearchSaveDialog *dialog = (SummarySearchSaveDialog *)data;

	if (event && event->keyval == GDK_Escape) {
		dialog->cancelled = TRUE;
		dialog->finished = TRUE;
	}
	return FALSE;
}

static void summary_search_save_dialog_select_folder(GtkButton *button,
						     gpointer data)
{
	SummarySearchSaveDialog *dialog = (SummarySearchSaveDialog *)data;
	FolderItem *item;
	gchar *id;

	item = foldersel_folder_sel(NULL, FOLDER_SEL_ALL, NULL);
	if (!item)
		return;

	id = folder_item_get_identifier(item);
	if (id) {
		gtk_entry_set_text(GTK_ENTRY(dialog->folder_entry), id);
		g_free(id);
	}
}

static void summary_search_save_ok(GtkButton *button, gpointer data)
{
	SummarySearchSaveDialog *dialog = (SummarySearchSaveDialog *)data;

	dialog->finished = TRUE;
}

static void summary_search_save_cancel(GtkButton *button, gpointer data)
{
	SummarySearchSaveDialog *dialog = (SummarySearchSaveDialog *)data;

	dialog->cancelled = TRUE;
	dialog->finished = TRUE;
}

static SummarySearchSaveDialog *summary_search_save_dialog_create(void)
{
	SummarySearchSaveDialog *dialog;
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *folder_entry;
	GtkWidget *folder_btn;
	GtkWidget *name_entry;

	GtkWidget *confirm_area;
	GtkWidget *hbbox;
	GtkWidget *cancel_btn;
	GtkWidget *ok_btn;

	dialog = g_new0(SummarySearchSaveDialog, 1);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), _("Save as search folder"));
	gtk_widget_set_size_request(window, 400, -1);
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, FALSE);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(summary_search_save_dialog_deleted),
			 dialog);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(summary_search_save_dialog_key_pressed),
			 dialog);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);
	manage_window_set_transient(GTK_WINDOW(window));

	vbox = gtk_vbox_new(FALSE, 8);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	hbox = gtk_hbox_new(FALSE, 8);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Location:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	folder_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), folder_entry, TRUE, TRUE, 0);

	folder_btn = gtk_button_new_with_label("...");
	gtk_box_pack_start(GTK_BOX(hbox), folder_btn, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(folder_btn), "clicked",
			 G_CALLBACK(summary_search_save_dialog_select_folder),
			 dialog);

	hbox = gtk_hbox_new(FALSE, 8);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Folder name:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	name_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), name_entry, TRUE, TRUE, 0);

	confirm_area = gtk_hbox_new(FALSE, 12);
	gtk_box_pack_end(GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);

	gtkut_stock_button_set_create(&hbbox,
				      &ok_btn, GTK_STOCK_OK,
				      &cancel_btn, GTK_STOCK_CANCEL,
				      NULL, NULL);
	gtk_box_pack_end(GTK_BOX(confirm_area), hbbox, FALSE, FALSE, 0);
	GTK_WIDGET_SET_FLAGS(ok_btn, GTK_CAN_DEFAULT);
	gtk_widget_grab_default(ok_btn);
	g_signal_connect(G_OBJECT(ok_btn), "clicked",
			 G_CALLBACK(summary_search_save_ok), dialog);
	g_signal_connect(G_OBJECT(cancel_btn), "clicked",
			 G_CALLBACK(summary_search_save_cancel), dialog);

	gtk_widget_grab_focus(name_entry);

	gtk_widget_show_all(window);

	dialog->window = window;
	dialog->folder_entry = folder_entry;
	dialog->name_entry = name_entry;
	dialog->cancelled = FALSE;
	dialog->finished = FALSE;

	return dialog;
}

static void summary_search_save_dialog_destroy(SummarySearchSaveDialog *dialog)
{
	gtk_widget_destroy(dialog->window);
	g_free(dialog);
}

static FolderItem *summary_search_create_vfolder(FolderItem *parent,
						 const gchar *name)
{
	gchar *path;
	gchar *fs_name;
	gchar *fullpath;
	FolderItem *item;

	g_return_val_if_fail(parent != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	path = folder_item_get_path(parent);
	fs_name = g_filename_from_utf8(name, -1, NULL, NULL, NULL);
	fullpath = g_strconcat(path, G_DIR_SEPARATOR_S,
			       fs_name ? fs_name : name, NULL);
	g_free(fs_name);
	g_free(path);

	if (make_dir_hier(fullpath) < 0) {
		g_free(fullpath);
		return NULL;
	}

	if (parent->path)
		path = g_strconcat(parent->path, G_DIR_SEPARATOR_S, name, NULL);
	else
		path = g_strdup(name);

	item = folder_item_new(name, path);
	item->stype = F_VIRTUAL;
	folder_item_append(parent, item);

	g_free(path);

	return item;
}

static void summary_search_vfolder_update_rule(FolderItem *item)
{
	GSList list;
	FilterRule *rule;
	gchar *file;
	gchar *path;

	rule = summary_search_dialog_to_rule(item->name, NULL);
	list.data = rule;
	list.next = NULL;

	path = folder_item_get_path(item);
	file = g_strconcat(path, G_DIR_SEPARATOR_S, FILTER_LIST, NULL);
	filter_write_file(&list, file);
	g_free(file);
	g_free(path);

	filter_rule_free(rule);
}

static void summary_search_save(GtkButton *button, gpointer data)
{
	SummarySearchSaveDialog *dialog;
	const gchar *id, *name;
	FolderItem *parent, *item;

	dialog = summary_search_save_dialog_create();
	id = gtk_entry_get_text(GTK_ENTRY(search_window.folder_entry));
	if (id && *id)
		gtk_entry_set_text(GTK_ENTRY(dialog->folder_entry), id);

	while (!dialog->finished)
		gtk_main_iteration();

	if (dialog->cancelled) {
		summary_search_save_dialog_destroy(dialog);
		return;
	}

	id = gtk_entry_get_text(GTK_ENTRY(dialog->folder_entry));
	parent = folder_find_item_from_identifier(id);
	name = gtk_entry_get_text(GTK_ENTRY(dialog->name_entry));
	if (parent && name && *name) {
		if (folder_find_child_item_by_name(parent, name)) {
			alertpanel_error(_("The folder `%s' already exists."),
					 name);
		} else {
			item = summary_search_create_vfolder(parent, name);
			if (item) {
				summary_search_vfolder_update_rule(item);
				folderview_append_item(folderview_get(),
						       NULL, item, TRUE);
				folder_write_list();
			}
		}
	}

	summary_search_save_dialog_destroy(dialog);
}

static void summary_search_close(GtkButton *button, gpointer data)
{
	if (search_window.on_search)
		search_window.cancelled = TRUE;
	gtk_widget_hide(search_window.window);
}

static void summary_search_entry_activated(GtkWidget *widget, gpointer data)
{
	gtk_button_clicked(GTK_BUTTON(search_window.search_btn));
}

static gint summary_search_deleted(GtkWidget *widget, GdkEventAny *event,
				   gpointer data)
{
	gtk_button_clicked(GTK_BUTTON(search_window.close_btn));
	return TRUE;
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event,
			    gpointer data)
{
	if (event && event->keyval == GDK_Escape) {
		if (search_window.on_search)
			gtk_button_clicked
				(GTK_BUTTON(search_window.search_btn));
		else
			gtk_button_clicked(GTK_BUTTON(search_window.close_btn));
		return TRUE;
	}
	return FALSE;
}
