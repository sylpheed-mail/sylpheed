/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2017 Hiroyuki Yamamoto
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
#include <gtk/gtkuimanager.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "query_search.h"
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
#include "statusbar.h"
#include "procmsg.h"
#include "procheader.h"
#include "folder.h"
#include "filter.h"
#include "prefs_common.h"
#include "prefs_filter.h"
#include "prefs_filter_edit.h"
#include "compose.h"
#include "sourcewindow.h"
#include "printing.h"

enum
{
	COL_FOLDER,
	COL_SUBJECT,
	COL_FROM,
	COL_DATE,
	COL_MSGINFO,
	N_COLS
};

static struct QuerySearchWindow {
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

	GtkActionGroup *group;
	GtkUIManager *ui;

	FilterRule *rule;
	gboolean requires_full_headers;

	gboolean exclude_trash;

	gint n_found;

	gboolean on_search;
	gboolean cancelled;
} search_window;

typedef struct {
	GtkWidget *window;

	GtkWidget *folder_entry;
	GtkWidget *name_entry;

	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;

	gboolean cancelled;
	gboolean finished;
} QuerySearchSaveDialog;

static const gchar *ui_def =
	"<ui>"
	"  <popup name='PopUpMenu'>"
	"    <menuitem name='Reply' action='ReplyAction'/>"
	"    <menu name='ReplyTo' action='ReplyToAction'>"
	"      <menuitem name='ReplyAll' action='ReplyAllAction'/>"
	"      <menuitem name='ReplySender' action='ReplySenderAction'/>"
	"      <menuitem name='ReplyList' action='ReplyListAction'/>"
	"    </menu>"
	"    <separator />"
	"    <menuitem name='Forward' action='ForwardAction'/>"
	"    <menuitem name='ForwardAsAttach' action='ForwardAsAttachAction'/>"
	"    <menuitem name='Redirect' action='RedirectAction'/>"
	"    <separator />"
	"    <menuitem name='MoveMsg' action='MoveMsgAction'/>"
	"    <menuitem name='CopyMsg' action='CopyMsgAction'/>"
	"    <separator />"
	"    <menuitem name='AddAddress' action='AddAddressAction'/>"
	"    <menu name='CreateFilter' action='CreateFilterAction'>"
	"      <menuitem name='Auto' action='FilterAutoAction'/>"
	"      <menuitem name='From' action='FilterFromAction'/>"
	"      <menuitem name='To' action='FilterToAction'/>"
	"      <menuitem name='Subject' action='FilterSubjectAction'/>"
	"    </menu>"
	"    <separator />"
	"    <menuitem name='Open' action='OpenAction'/>"
	"    <menuitem name='OpenSource' action='OpenSourceAction'/>"
	"    <separator />"
	"    <menuitem name='Print' action='PrintAction'/>"
	"  </popup>"
	"</ui>";

static void query_search_create	(void);

static FilterRule *query_search_dialog_to_rule	(const gchar	*name,
						 FolderItem    **item);

static void query_search_query			(void);
static void query_search_folder			(FolderItem	*item);

static gboolean query_search_recursive_func	(GNode		*node,
						 gpointer	 data);

static void query_search_append_msg	(MsgInfo	*msginfo);
static void query_search_clear_list	(void);

static void query_search_hbox_added	(CondHBox	*hbox);

static void row_activated		(GtkTreeView		*treeview,
					 GtkTreePath		*path,
					 GtkTreeViewColumn	*column,
					 gpointer		 data);

static gboolean row_selected		(GtkTreeSelection	*selection,
					 GtkTreeModel		*model,
					 GtkTreePath		*path,
					 gboolean		 cur_selected,
					 gpointer		 data);

static gboolean treeview_button_pressed	(GtkWidget	*widget,
					 GdkEventButton	*event,
					 gpointer	 data);

static void query_search_clear		(GtkButton	*button,
					 gpointer	 data);
static void query_select_folder		(GtkButton	*button,
					 gpointer	 data);
static void query_search_clicked	(GtkButton	*button,
					 gpointer	 data);
static void query_search_save		(GtkButton	*button,
					 gpointer	 data);
static void query_search_close		(GtkButton	*button,
					 gpointer	 data);

static void query_search_entry_activated(GtkWidget	*widget,
					 gpointer	 data);

static gint query_search_deleted	(GtkWidget	*widget,
					 GdkEventAny	*event,
					 gpointer	 data);
static gboolean key_pressed		(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gpointer	 data);

static void query_search_open_msg	(gboolean	 new_window);

/* Popup menu callbacks */
static void reply_cb(void);
static void reply_all_cb(void);
static void reply_sender_cb(void);
static void reply_list_cb(void);
static void forward_cb(void);
static void forward_attach_cb(void);
static void redirect_cb(void);
static void move_cb(void);
static void copy_cb(void);
static void add_address_cb(void);
static void create_filter_auto_cb(void);
static void create_filter_from_cb(void);
static void create_filter_to_cb(void);
static void create_filter_subject_cb(void);
static void open_cb			(void);
static void open_source_cb		(void);
static void print_cb			(void);

static gint query_search_cmp_by_folder	(GtkTreeModel	*model,
					 GtkTreeIter	*a,
					 GtkTreeIter	*b,
					 gpointer	 data);
static gint query_search_cmp_by_subject	(GtkTreeModel	*model,
					 GtkTreeIter	*a,
					 GtkTreeIter	*b,
					 gpointer	 data);
static gint query_search_cmp_by_from	(GtkTreeModel	*model,
					 GtkTreeIter	*a,
					 GtkTreeIter	*b,
					 gpointer	 data);
static gint query_search_cmp_by_date	(GtkTreeModel	*model,
					 GtkTreeIter	*a,
					 GtkTreeIter	*b,
					 gpointer	 data);

static GtkActionEntry action_entries[] = {
	{"ReplyAction", NULL, N_("_Reply"), NULL, NULL, reply_cb},
	{"ReplyToAction", NULL, N_("Repl_y to"), NULL, NULL, NULL},
	{"ReplyAllAction", NULL, N_("Reply to _all"), NULL, NULL, reply_all_cb},
	{"ReplySenderAction", NULL, N_("Reply to _sender"), NULL, NULL, reply_sender_cb},
	{"ReplyListAction", NULL, N_("Reply to _list"), NULL, NULL, reply_list_cb},

	{"ForwardAction", NULL, N_("_Forward"), NULL, NULL, forward_cb},
	{"ForwardAsAttachAction", NULL, N_("For_ward as attachment"), NULL, NULL, forward_attach_cb},
	{"RedirectAction", NULL, N_("Redirec_t"), NULL, NULL, redirect_cb},

	{"MoveMsgAction", NULL, N_("M_ove messages to..."), NULL, NULL, move_cb},
	{"CopyMsgAction", NULL, N_("_Copy messages to..."), NULL, NULL, copy_cb},

	{"AddAddressAction", NULL, N_("Add sender to address boo_k..."), NULL, NULL, add_address_cb},
	{"CreateFilterAction", NULL, N_("Create f_ilter rule"), NULL, NULL, NULL},
	{"FilterAutoAction", NULL, N_("_Automatically"), NULL, NULL, create_filter_auto_cb},
	{"FilterFromAction", NULL, N_("by _From"), NULL, NULL, create_filter_from_cb},
	{"FilterToAction", NULL, N_("by _To"), NULL, NULL, create_filter_to_cb},
	{"FilterSubjectAction", NULL, N_("by _Subject"), NULL, NULL, create_filter_subject_cb},

	{"OpenAction", GTK_STOCK_OPEN, N_("Open in _new window"), NULL, NULL, open_cb},
	{"OpenSourceAction", NULL, N_("View mess_age source"), NULL, NULL, open_source_cb},

	{"PrintAction", GTK_STOCK_PRINT, N_("_Print..."), NULL, NULL, print_cb},
};


void query_search(FolderItem *item)
{
	gchar *id;

	if (!search_window.window)
		query_search_create();
	else
		gtk_window_present(GTK_WINDOW(search_window.window));

	if (item && item->stype != F_VIRTUAL) {
		id = folder_item_get_identifier(item);
		gtk_entry_set_text(GTK_ENTRY(search_window.folder_entry), id);
		g_free(id);
	} else
		gtk_entry_set_text(GTK_ENTRY(search_window.folder_entry), "");

	gtk_widget_grab_focus(search_window.search_btn);
	gtk_widget_show(search_window.window);
}

static void query_search_create(void)
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

	GtkActionGroup *group;
	GtkUIManager *ui;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW (window), _("Search messages"));
	gtk_widget_set_size_request(window, 600 * gtkut_get_dpi_multiplier(), -1);
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, TRUE);
	gtk_container_set_border_width(GTK_CONTAINER (window), 8);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(query_search_deleted), NULL);
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
	gtk_option_menu_set_history(GTK_OPTION_MENU(bool_optmenu), FLT_AND);

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
	gtk_widget_set_size_request(scrolledwin, -1, 120 * gtkut_get_dpi_multiplier());

	cond_edit = prefs_filter_edit_cond_edit_create();
	cond_edit->add_hbox = query_search_hbox_added;
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
	gtk_widget_set_size_request(scrolledwin, -1, 150 * gtkut_get_dpi_multiplier());

	store = gtk_list_store_new(N_COLS,
				   G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
				   G_TYPE_STRING, G_TYPE_POINTER);
	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(store);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);
	g_signal_connect(G_OBJECT(treeview), "row-activated",
			 G_CALLBACK(row_activated), NULL);
	g_signal_connect(G_OBJECT(treeview), "button-press-event",
			 G_CALLBACK(treeview_button_pressed), NULL);

	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store), COL_FOLDER,
					query_search_cmp_by_folder, NULL, NULL);
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store), COL_SUBJECT,
					query_search_cmp_by_subject,
					NULL, NULL);
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store), COL_FROM,
					query_search_cmp_by_from, NULL, NULL);
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store), COL_DATE,
					query_search_cmp_by_date, NULL, NULL);

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
	gtk_tree_view_column_set_sort_column_id(column, col);		\
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);	\
}

	APPEND_COLUMN(_("Folder"), COL_FOLDER, 0);
	APPEND_COLUMN(_("Subject"), COL_SUBJECT, 200);
#if GTK_CHECK_VERSION(2, 14, 0)
	gtk_tree_view_column_set_expand(column, TRUE);
#endif
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
			 G_CALLBACK(query_search_clear), NULL);
	g_signal_connect(G_OBJECT(folder_btn), "clicked",
			 G_CALLBACK(query_select_folder), NULL);
	g_signal_connect(G_OBJECT(search_btn), "clicked",
			 G_CALLBACK(query_search_clicked), NULL);
	g_signal_connect(G_OBJECT(save_btn), "clicked",
			 G_CALLBACK(query_search_save), NULL);
	g_signal_connect(G_OBJECT(close_btn), "clicked",
			 G_CALLBACK(query_search_close), NULL);

	group = gtk_action_group_new("main");
	gtk_action_group_set_translation_domain(group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions(group, action_entries,
				     sizeof(action_entries) /
				     sizeof(action_entries[0]), NULL);

	ui = gtk_ui_manager_new();
	gtk_ui_manager_insert_action_group(ui, group, 0);
	gtk_ui_manager_add_ui_from_string(ui, ui_def, -1, NULL);

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

	search_window.group = group;
	search_window.ui = ui;
}

static FilterRule *query_search_dialog_to_rule(const gchar *name,
						 FolderItem **item)
{
	const gchar *id;
	FolderItem *item_;
	FilterBoolOp bool_op;
	gboolean recursive;
	gboolean case_sens;
	GSList *cond_list;
	FilterRule *rule;

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

	cond_list = prefs_filter_edit_cond_edit_to_list(search_window.cond_edit,
							case_sens);
	if (!cond_list)
		return NULL;

	rule = filter_rule_new(name, bool_op, cond_list, NULL);
	rule->target_folder = g_strdup(id);
	rule->recursive = recursive;

	return rule;
}

static void query_search_query(void)
{
	FolderItem *item;
	gchar *msg;

	if (search_window.on_search)
		return;

	search_window.on_search = TRUE;

	search_window.rule = query_search_dialog_to_rule("Query rule", &item);
	if (!search_window.rule) {
		search_window.on_search = FALSE;
		return;
	}
	search_window.requires_full_headers =
		filter_rule_requires_full_headers(search_window.rule);

	if (search_window.rule->recursive) {
		if (item->stype == F_TRASH)
			search_window.exclude_trash = FALSE;
		else
			search_window.exclude_trash = TRUE;
	} else
		search_window.exclude_trash = FALSE;

	search_window.n_found = 0;
	search_window.cancelled = FALSE;

	gtk_widget_set_sensitive(search_window.clear_btn, FALSE);
	gtk_button_set_label(GTK_BUTTON(search_window.search_btn),
			     GTK_STOCK_STOP);
	query_search_clear_list();

	if (search_window.rule->recursive)
		g_node_traverse(item->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
				query_search_recursive_func, NULL);
	else
		query_search_folder(item);

	filter_rule_free(search_window.rule);
	search_window.rule = NULL;
	search_window.requires_full_headers = FALSE;
	search_window.exclude_trash = FALSE;

	gtk_widget_set_sensitive(search_window.clear_btn, TRUE);
	gtk_button_set_label(GTK_BUTTON(search_window.search_btn),
			     GTK_STOCK_FIND);
	if (search_window.n_found == 0)
		msg = g_strdup_printf(_("Message not found."));
	else if (search_window.n_found == 1)
		msg = g_strdup_printf(_("1 message found."));
	else
		msg = g_strdup_printf(_("%d messages found."),
				      search_window.n_found);
	gtk_label_set_text(GTK_LABEL(search_window.status_label), msg);
	g_free(msg);
	statusbar_pop_all();

	if (search_window.cancelled)
		debug_print("* query search cancelled.\n");
	debug_print("query search finished.\n");

	search_window.n_found = 0;
	search_window.on_search = FALSE;
	search_window.cancelled = FALSE;
}

typedef struct _QueryData
{
	FolderItem *item;
	gchar *folder_name;
	gint count;
	gint total;
	gint flag;
	GTimeVal tv_prev;
	GSList *mlist;
#if USE_THREADS
	GAsyncQueue *queue;
	guint timer_tag;
#endif
} QueryData;

static void query_search_folder_show_progress(const gchar *name, gint count,
					      gint total)
{
	gchar *str;

	str = g_strdup_printf(_("Searching %s (%d / %d)..."),
			      name, count, total);
	gtk_label_set_text(GTK_LABEL(search_window.status_label), str);
	g_free(str);
#ifndef USE_THREADS
	ui_update();
#endif
}

#if USE_THREADS
static gboolean query_search_progress_func(gpointer data)
{
	QueryData *qdata = (QueryData *)data;
	MsgInfo *msginfo;

	gdk_threads_enter();
	query_search_folder_show_progress(qdata->folder_name,
					  g_atomic_int_get(&qdata->count),
					  qdata->total);
	while ((msginfo = g_async_queue_try_pop(qdata->queue)))
		query_search_append_msg(msginfo);
	gdk_threads_leave();

	return TRUE;
}
#endif

static gpointer query_search_folder_func(gpointer data)
{
	QueryData *qdata = (QueryData *)data;
	GSList *mlist, *cur;
	FilterInfo fltinfo;
	GTimeVal tv_cur;

	debug_print("query_search_folder_func start\n");

#if USE_THREADS
	g_async_queue_ref(qdata->queue);
#endif

	mlist = qdata->mlist;

	memset(&fltinfo, 0, sizeof(FilterInfo));

	debug_print("requires_full_headers: %d\n",
		    search_window.requires_full_headers);
	debug_print("start query search: %s\n",
		    qdata->item->path ? qdata->item->path : "");

	for (cur = mlist; cur != NULL; cur = cur->next) {
		MsgInfo *msginfo = (MsgInfo *)cur->data;
		GSList *hlist;

		g_atomic_int_add(&qdata->count, 1);

		g_get_current_time(&tv_cur);
		if ((tv_cur.tv_sec - qdata->tv_prev.tv_sec) * G_USEC_PER_SEC +
		    tv_cur.tv_usec - qdata->tv_prev.tv_usec >
		    PROGRESS_UPDATE_INTERVAL * 1000) {
#ifndef USE_THREADS
			query_search_folder_show_progress(qdata->folder_name,
							  qdata->count,
							  qdata->total);
#endif
			qdata->tv_prev = tv_cur;
		}

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
#if USE_THREADS
			g_async_queue_push(qdata->queue, msginfo);
#else
			query_search_append_msg(msginfo);
#endif
			cur->data = NULL;
			search_window.n_found++;
		}

		procheader_header_list_destroy(hlist);
	}

#if USE_THREADS
	g_async_queue_unref(qdata->queue);
#endif

	g_atomic_int_set(&qdata->flag, 1);
	g_main_context_wakeup(NULL);

	debug_print("query_search_folder_func end\n");

	return GINT_TO_POINTER(0);
}

static void query_search_folder(FolderItem *item)
{
	gchar *str;
	QueryData data = {item};
#if USE_THREADS
	GThread *thread;
	MsgInfo *msginfo;
#endif

	if (!item->path || item->stype == F_VIRTUAL)
		return;

	data.folder_name = g_path_get_basename(item->path);
	str = g_strdup_printf(_("Searching %s ..."), data.folder_name);
	gtk_label_set_text(GTK_LABEL(search_window.status_label), str);
	g_free(str);
	g_get_current_time(&data.tv_prev);
#ifndef USE_THREADS
	ui_update();
#endif

	if (search_window.cancelled) {
		g_free(data.folder_name);
		return;
	}

	if (item->opened)
		summary_write_cache(main_window_get()->summaryview);

	procmsg_set_auto_decrypt_message(FALSE);

	data.mlist = folder_item_get_msg_list(item, TRUE);
	data.total = g_slist_length(data.mlist);

#if USE_THREADS
	data.queue = g_async_queue_new();
	data.timer_tag = g_timeout_add(PROGRESS_UPDATE_INTERVAL,
				       query_search_progress_func, &data);
	thread = g_thread_create(query_search_folder_func, &data, TRUE, NULL);

	debug_print("query_search_folder: thread started\n");
	while (g_atomic_int_get(&data.flag) == 0)
		gtk_main_iteration();
	log_window_flush();

	while ((msginfo = g_async_queue_try_pop(data.queue)))
		query_search_append_msg(msginfo);

	g_source_remove(data.timer_tag);
	g_thread_join(thread);
	debug_print("query_search_folder: thread exited\n");

	g_async_queue_unref(data.queue);
#else /* !USE_THREADS */
	query_search_folder_func(&data);
#endif

	procmsg_msg_list_free(data.mlist);
	procmsg_set_auto_decrypt_message(TRUE);
	g_free(data.folder_name);
}

static gboolean query_search_recursive_func(GNode *node, gpointer data)
{
	FolderItem *item;

	g_return_val_if_fail(node->data != NULL, FALSE);

	item = FOLDER_ITEM(node->data);

	if (!item->path)
		return FALSE;
	if (search_window.exclude_trash && item->stype == F_TRASH)
		return FALSE;

	query_search_folder(item);

	if (search_window.cancelled)
		return TRUE;

	return FALSE;
}

static void query_search_append_msg(MsgInfo *msginfo)
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

static void query_search_clear_list(void)
{
	GtkTreeIter iter;
	GtkTreeModel *model = GTK_TREE_MODEL(search_window.store);
	MsgInfo *msginfo;

	gtkut_tree_sortable_unset_sort_column_id
		(GTK_TREE_SORTABLE(search_window.store));

	if (!gtk_tree_model_get_iter_first(model, &iter))
		return;

	do {
		gtk_tree_model_get(model, &iter, COL_MSGINFO, &msginfo, -1);
		procmsg_msginfo_free(msginfo);
	} while (gtk_tree_model_iter_next(model, &iter));

	gtk_list_store_clear(search_window.store);
}

static void query_search_hbox_added(CondHBox *hbox)
{
	g_signal_connect(hbox->key_entry, "activate",
			 G_CALLBACK(query_search_entry_activated), NULL);
}

static void row_activated(GtkTreeView *treeview, GtkTreePath *path,
			  GtkTreeViewColumn *column, gpointer data)
{
	query_search_open_msg(FALSE);
}

static gboolean row_selected(GtkTreeSelection *selection,
			     GtkTreeModel *model, GtkTreePath *path,
			     gboolean cur_selected, gpointer data)
{
	return TRUE;
}

static gboolean treeview_button_pressed(GtkWidget *widget,
					GdkEventButton *event, gpointer data)
{
	GtkTreeIter iter;
	GtkTreeModel *model = GTK_TREE_MODEL(search_window.store);
	GtkWidget *menu;
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GtkTreeViewColumn *column = NULL;
	gboolean is_selected;
	gint px, py;

	debug_print("treeview_button_pressed\n");

	if (!event)
		return FALSE;

	if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget),
					   event->x, event->y, &path, &column,
					   NULL, NULL))
		return FALSE;

	// ??
	gtk_widget_get_pointer(widget, &px, &py);
	if (py == (gint)event->y) {
		gtk_tree_path_free(path);
		return FALSE;
	}

	if (!gtk_tree_model_get_iter(model, &iter, path)) {
		gtk_tree_path_free(path);
		return FALSE;
	}

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
	is_selected = gtk_tree_selection_path_is_selected(selection, path);
	gtk_tree_path_free(path);

	if (event->button == 3) {
		menu = gtk_ui_manager_get_widget(search_window.ui,
						 "/PopUpMenu");
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
			       event->button, event->time);
		if (is_selected)
			return TRUE;
	}

	return FALSE;
}

static void query_search_clear(GtkButton *button, gpointer data)
{
	CondHBox *cond_hbox;

	if (search_window.on_search)
		return;

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

	query_search_clear_list();
}

static void query_select_folder(GtkButton *button, gpointer data)
{
	FolderItem *item;
	gchar *id;

	item = foldersel_folder_sel(NULL, FOLDER_SEL_ALL, NULL);
	if (!item || item->stype == F_VIRTUAL)
		return;

	id = folder_item_get_identifier(item);
	if (id) {
		gtk_entry_set_text(GTK_ENTRY(search_window.folder_entry), id);
		g_free(id);
	}
}

static void query_search_clicked(GtkButton *button, gpointer data)
{
	if (search_window.on_search)
		search_window.cancelled = TRUE;
	else
		query_search_query();
}

static gint query_search_save_dialog_deleted(GtkWidget *widget,
					     GdkEventAny *event, gpointer data)
{
	QuerySearchSaveDialog *dialog = (QuerySearchSaveDialog *)data;

	dialog->cancelled = TRUE;
	dialog->finished = TRUE;
	return TRUE;
}

static gint query_search_save_dialog_key_pressed(GtkWidget *widget,
						   GdkEventKey *event,
						   gpointer data)
{
	QuerySearchSaveDialog *dialog = (QuerySearchSaveDialog *)data;

	if (event && event->keyval == GDK_Escape) {
		dialog->cancelled = TRUE;
		dialog->finished = TRUE;
	}
	return FALSE;
}

static void query_search_save_dialog_select_folder(GtkButton *button,
						   gpointer data)
{
	QuerySearchSaveDialog *dialog = (QuerySearchSaveDialog *)data;
	FolderItem *item;
	gchar *id;

	item = foldersel_folder_sel(NULL, FOLDER_SEL_ALL, NULL);
	if (!item || item->no_sub || item->stype == F_VIRTUAL)
		return;

	id = folder_item_get_identifier(item);
	if (id) {
		gtk_entry_set_text(GTK_ENTRY(dialog->folder_entry), id);
		g_free(id);
	}
}

static void query_search_save_activated(GtkEditable *editable, gpointer data)
{
	QuerySearchSaveDialog *dialog = (QuerySearchSaveDialog *)data;

	gtk_button_clicked(GTK_BUTTON(dialog->ok_btn));
}

static void query_search_save_ok(GtkButton *button, gpointer data)
{
	QuerySearchSaveDialog *dialog = (QuerySearchSaveDialog *)data;

	dialog->finished = TRUE;
}

static void query_search_save_cancel(GtkButton *button, gpointer data)
{
	QuerySearchSaveDialog *dialog = (QuerySearchSaveDialog *)data;

	dialog->cancelled = TRUE;
	dialog->finished = TRUE;
}

static QuerySearchSaveDialog *query_search_save_dialog_create(void)
{
	QuerySearchSaveDialog *dialog;
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

	dialog = g_new0(QuerySearchSaveDialog, 1);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), _("Save as search folder"));
	gtk_widget_set_size_request(window, 400 * gtkut_get_dpi_multiplier(), -1);
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, FALSE);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(query_search_save_dialog_deleted),
			 dialog);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(query_search_save_dialog_key_pressed),
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
			 G_CALLBACK(query_search_save_dialog_select_folder),
			 dialog);

	hbox = gtk_hbox_new(FALSE, 8);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Folder name:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	name_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), name_entry, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(name_entry), "activate",
			 G_CALLBACK(query_search_save_activated), dialog);

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
			 G_CALLBACK(query_search_save_ok), dialog);
	g_signal_connect(G_OBJECT(cancel_btn), "clicked",
			 G_CALLBACK(query_search_save_cancel), dialog);

	gtk_widget_grab_focus(name_entry);

	gtk_widget_show_all(window);

	dialog->window = window;
	dialog->folder_entry = folder_entry;
	dialog->name_entry = name_entry;
	dialog->ok_btn = ok_btn;
	dialog->cancel_btn = cancel_btn;
	dialog->cancelled = FALSE;
	dialog->finished = FALSE;

	return dialog;
}

static void query_search_save_dialog_destroy(QuerySearchSaveDialog *dialog)
{
	gtk_widget_destroy(dialog->window);
	g_free(dialog);
}

static FolderItem *query_search_create_vfolder(FolderItem *parent,
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
		path = g_strconcat(parent->path, "/", name, NULL);
	else
		path = g_strdup(name);

	item = folder_item_new(name, path);
	item->stype = F_VIRTUAL;
	item->no_sub = TRUE;
	folder_item_append(parent, item);

	g_free(path);

	return item;
}

static void query_search_vfolder_update_rule(FolderItem *item)
{
	GSList list;
	FilterRule *rule;
	gchar *file;
	gchar *path;

	rule = query_search_dialog_to_rule(item->name, NULL);
	list.data = rule;
	list.next = NULL;

	path = folder_item_get_path(item);
	file = g_strconcat(path, G_DIR_SEPARATOR_S, FILTER_LIST, NULL);
	filter_write_file(&list, file);
	g_free(file);
	g_free(path);

	filter_rule_free(rule);
}

static void query_search_save(GtkButton *button, gpointer data)
{
	QuerySearchSaveDialog *dialog;
	const gchar *id, *name;
	FolderItem *parent, *item;

	dialog = query_search_save_dialog_create();
	id = gtk_entry_get_text(GTK_ENTRY(search_window.folder_entry));
	if (id && *id)
		gtk_entry_set_text(GTK_ENTRY(dialog->folder_entry), id);

	while (!dialog->finished)
		gtk_main_iteration();

	if (dialog->cancelled) {
		query_search_save_dialog_destroy(dialog);
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
			item = query_search_create_vfolder(parent, name);
			if (item) {
				query_search_vfolder_update_rule(item);
				folderview_append_item(folderview_get(),
						       NULL, item, TRUE);
				folder_write_list();
			}
		}
	}

	query_search_save_dialog_destroy(dialog);
}

static void query_search_close(GtkButton *button, gpointer data)
{
	if (search_window.on_search)
		search_window.cancelled = TRUE;
	gtk_widget_hide(search_window.window);
}

static void query_search_entry_activated(GtkWidget *widget, gpointer data)
{
	gtk_button_clicked(GTK_BUTTON(search_window.search_btn));
}

static gint query_search_deleted(GtkWidget *widget, GdkEventAny *event,
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

static GSList *query_search_get_selected_msg_list(void)
{
	GtkTreeModel *model = GTK_TREE_MODEL(search_window.store);
	GtkTreeSelection *selection;
	GList *list, *cur;
	GtkTreePath *path;
	GtkTreeIter iter;
	GSList *mlist = NULL;
	MsgInfo *msginfo;

	selection = gtk_tree_view_get_selection
		(GTK_TREE_VIEW(search_window.treeview));
	list = gtk_tree_selection_get_selected_rows(selection, NULL);
	if (!list)
		return NULL;

	for (cur = list; cur != NULL; cur = cur->next) {
		path = cur->data;
		if (!gtk_tree_model_get_iter(model, &iter, path))
			break;
		msginfo = NULL;
		gtk_tree_model_get(model, &iter, COL_MSGINFO, &msginfo, -1);
		if (msginfo)
			mlist = g_slist_prepend(mlist, msginfo);
	}
	mlist = g_slist_reverse(mlist);

	g_list_foreach(list, (GFunc)gtk_tree_path_free, NULL);
	g_list_free(list);

	return mlist;
}

static void query_search_open_msg(gboolean new_window)
{
	GSList *mlist;
	MsgInfo *msginfo;
	MessageView *msgview;

	mlist = query_search_get_selected_msg_list();
	if (!mlist)
		return;
	msginfo = (MsgInfo *)mlist->data;
	g_slist_free(mlist);

	if (new_window || !summary_select_by_msginfo(main_window_get()->summaryview, msginfo)) {
		msgview = messageview_create_with_new_window();
		messageview_show(msgview, msginfo, FALSE);
		statusbar_pop_all();
	}
}

static void query_search_reply_forward(ComposeMode mode)
{
	GSList *mlist;
	MsgInfo *msginfo;

	mlist = query_search_get_selected_msg_list();
	if (!mlist)
		return;
	msginfo = (MsgInfo *)mlist->data;

	if (prefs_common.reply_with_quote)
		mode |= COMPOSE_WITH_QUOTE;

	switch (COMPOSE_MODE(mode)) {
	case COMPOSE_REPLY:
	case COMPOSE_REPLY_TO_SENDER:
	case COMPOSE_REPLY_TO_ALL:
	case COMPOSE_REPLY_TO_LIST:
		compose_reply(msginfo, msginfo->folder, mode, NULL);
		break;
	case COMPOSE_FORWARD:
		compose_forward(mlist, NULL, FALSE, NULL);
		break;
	case COMPOSE_FORWARD_AS_ATTACH:
		compose_forward(mlist, NULL, TRUE, NULL);
		break;
	case COMPOSE_REDIRECT:
		compose_redirect(msginfo, msginfo->folder);
		break;
	default:
		g_warning("query_search_reply_forward: invalid mode: %d", mode);
	}

	g_slist_free(mlist);
}

static void query_search_move_copy(gboolean is_copy)
{
	GSList *mlist, *cur;
	FolderItem *dest;
	MsgInfo *msginfo;

	debug_print("query_search_move_copy: is_copy: %d\n", is_copy);

	mlist = query_search_get_selected_msg_list();
	if (!mlist)
		return;

	if (is_copy)
		dest = foldersel_folder_sel_full(NULL, FOLDER_SEL_COPY, NULL,
						 _("Select folder to copy"));
	else
		dest = foldersel_folder_sel_full(NULL, FOLDER_SEL_MOVE, NULL,
						 _("Select folder to move"));
	if (!dest || dest->stype == F_VIRTUAL || !FOLDER_ITEM_CAN_ADD(dest)) {
		g_slist_free(mlist);
		return;
	}
	if (summary_is_locked(main_window_get()->summaryview)) {
		g_slist_free(mlist);
		return;
	}

	for (cur = mlist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;
		if (!msginfo)
			continue;

		if (is_copy) {
			folder_item_copy_msg(dest, msginfo);
		} else {
			folder_item_move_msg(dest, msginfo);
		}
	}

	folderview_update_all_updated(TRUE);
	g_slist_free(mlist);
}

static void query_search_create_filter(FilterCreateType type)
{
	GSList *mlist;
	MsgInfo *msginfo;
	gchar *header = NULL;
	gchar *key = NULL;

	mlist = query_search_get_selected_msg_list();
	if (!mlist)
		return;
	msginfo = (MsgInfo *)mlist->data;
	g_slist_free(mlist);

	filter_get_keyword_from_msg(msginfo, &header, &key, type);
	prefs_filter_open(msginfo, header, key);

	g_free(header);
	g_free(key);
}

static void reply_cb(void)
{
	query_search_reply_forward(COMPOSE_REPLY);
}

static void reply_all_cb(void)
{
	query_search_reply_forward(COMPOSE_REPLY_TO_ALL);
}

static void reply_sender_cb(void)
{
	query_search_reply_forward(COMPOSE_REPLY_TO_SENDER);
}

static void reply_list_cb(void)
{
	query_search_reply_forward(COMPOSE_REPLY_TO_LIST);
}

static void forward_cb(void)
{
	query_search_reply_forward(COMPOSE_FORWARD);
}

static void forward_attach_cb(void)
{
	query_search_reply_forward(COMPOSE_FORWARD_AS_ATTACH);
}

static void redirect_cb(void)
{
	query_search_reply_forward(COMPOSE_REDIRECT);
}

static void move_cb(void)
{
	query_search_move_copy(FALSE);
}

static void copy_cb(void)
{
	query_search_move_copy(TRUE);
}

static void add_address_cb(void)
{
	GSList *mlist;
	MsgInfo *msginfo;
	gchar from[BUFFSIZE];

	mlist = query_search_get_selected_msg_list();
	if (!mlist)
		return;
	msginfo = (MsgInfo *)mlist->data;
	g_slist_free(mlist);

	strncpy2(from, msginfo->from, sizeof(from));
	extract_address(from);
	addressbook_add_contact(msginfo->fromname, from, NULL);
}

static void create_filter_auto_cb(void)
{
	query_search_create_filter(FLT_BY_AUTO);
}

static void create_filter_from_cb(void)
{
	query_search_create_filter(FLT_BY_FROM);
}

static void create_filter_to_cb(void)
{
	query_search_create_filter(FLT_BY_TO);
}

static void create_filter_subject_cb(void)
{
	query_search_create_filter(FLT_BY_SUBJECT);
}

static void open_cb(void)
{
	query_search_open_msg(TRUE);
}

static void open_source_cb(void)
{
	GSList *mlist;
	MsgInfo *msginfo;
	SourceWindow *srcwin;

	mlist = query_search_get_selected_msg_list();
	if (!mlist)
		return;
	msginfo = (MsgInfo *)mlist->data;
	g_slist_free(mlist);

	srcwin = source_window_create();
	source_window_show_msg(srcwin, msginfo);
	source_window_show(srcwin);
}

static void print_cb(void)
{
	GSList *mlist;

	mlist = query_search_get_selected_msg_list();
	if (!mlist)
		return;
	printing_print_messages(mlist, FALSE);
	g_slist_free(mlist);
}

static gint query_search_cmp_by_folder(GtkTreeModel *model,
				       GtkTreeIter *a, GtkTreeIter *b,
				       gpointer data)
{
	gchar *folder_a = NULL, *folder_b = NULL;
	MsgInfo *msginfo_a = NULL, *msginfo_b = NULL;
	gint ret;

	gtk_tree_model_get(model, a, COL_FOLDER, &folder_a, COL_MSGINFO,
			   &msginfo_a, -1);
	gtk_tree_model_get(model, b, COL_FOLDER, &folder_b, COL_MSGINFO,
			   &msginfo_b, -1);

	if (!folder_a || !folder_b || !msginfo_a || !msginfo_b)
		return 0;

	ret = g_ascii_strcasecmp(folder_a, folder_b);
	return (ret != 0) ? ret : (msginfo_a->date_t - msginfo_b->date_t);
}

static gint query_search_cmp_by_subject(GtkTreeModel *model,
					GtkTreeIter *a, GtkTreeIter *b,
					gpointer data)
{
	MsgInfo *msginfo_a = NULL, *msginfo_b = NULL;
	gint ret;

	gtk_tree_model_get(model, a, COL_MSGINFO, &msginfo_a, -1);
	gtk_tree_model_get(model, b, COL_MSGINFO, &msginfo_b, -1);

	if (!msginfo_a || !msginfo_b)
		return 0;

	if (!msginfo_a->subject)
		return -(msginfo_b->subject != NULL);
	if (!msginfo_b->subject)
		return (msginfo_a->subject != NULL);

	ret = subject_compare_for_sort(msginfo_a->subject, msginfo_b->subject);
	return (ret != 0) ? ret : (msginfo_a->date_t - msginfo_b->date_t);
}

static gint query_search_cmp_by_from(GtkTreeModel *model,
				     GtkTreeIter *a, GtkTreeIter *b,
				     gpointer data)
{
	MsgInfo *msginfo_a = NULL, *msginfo_b = NULL;
	gint ret;

	gtk_tree_model_get(model, a, COL_MSGINFO, &msginfo_a, -1);
	gtk_tree_model_get(model, b, COL_MSGINFO, &msginfo_b, -1);

	if (!msginfo_a || !msginfo_b)
		return 0;

	if (!msginfo_a->fromname)
		return -(msginfo_b->fromname != NULL);
	if (!msginfo_b->fromname)
		return (msginfo_a->fromname != NULL);

	ret = g_ascii_strcasecmp(msginfo_a->fromname, msginfo_b->fromname);
	return (ret != 0) ? ret : (msginfo_a->date_t - msginfo_b->date_t);
}

static gint query_search_cmp_by_date(GtkTreeModel *model,
				     GtkTreeIter *a, GtkTreeIter *b,
				     gpointer data)
{
	MsgInfo *msginfo_a = NULL, *msginfo_b = NULL;

	gtk_tree_model_get(model, a, COL_MSGINFO, &msginfo_a, -1);
	gtk_tree_model_get(model, b, COL_MSGINFO, &msginfo_b, -1);

	if (!msginfo_a || !msginfo_b)
		return 0;

	return msginfo_a->date_t - msginfo_b->date_t;
}
