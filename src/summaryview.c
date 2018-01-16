/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2018 Hiroyuki Yamamoto
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
#include <gtk/gtkversion.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkentry.h>
#include <gtk/gtktooltips.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktreestore.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkcheckmenuitem.h>
#include <gtk/gtkitemfactory.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkhseparator.h>
#include <gtk/gtkarrow.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtkstatusbar.h>
#include <gtk/gtkstock.h>
#include <gtk/gtknotebook.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "main.h"
#include "menu.h"
#include "mainwindow.h"
#include "folderview.h"
#include "summaryview.h"
#include "messageview.h"
#include "foldersel.h"
#include "procmsg.h"
#include "procheader.h"
#include "sourcewindow.h"
#include "prefs_common.h"
#include "prefs_summary_column.h"
#include "prefs_filter.h"
#include "account.h"
#include "compose.h"
#include "utils.h"
#include "gtkutils.h"
#include "stock_pixmap.h"
#include "filesel.h"
#include "alertpanel.h"
#include "inputdialog.h"
#include "statusbar.h"
#include "trayicon.h"
#include "printing.h"
#include "filter.h"
#include "folder.h"
#include "colorlabel.h"
#include "inc.h"
#include "imap.h"
#include "plugin.h"

#define STATUSBAR_PUSH(mainwin, str) \
{ \
	gtk_statusbar_push(GTK_STATUSBAR(mainwin->statusbar), \
			   mainwin->summaryview_cid, str); \
	gtkut_widget_draw_now(mainwin->statusbar); \
}

#define STATUSBAR_POP(mainwin) \
{ \
	gtk_statusbar_pop(GTK_STATUSBAR(mainwin->statusbar), \
			  mainwin->summaryview_cid); \
}

#define GET_MSG_INFO(msginfo, iter__) \
{ \
	gtk_tree_model_get(GTK_TREE_MODEL(summaryview->store), iter__, \
			   S_COL_MSG_INFO, &msginfo, -1); \
}

#define SORT_BLOCK(key)							 \
	if (summaryview->folder_item->sort_key == key) {		 \
		sort_key = key;						 \
		sort_type = summaryview->folder_item->sort_type;	 \
		summary_sort(summaryview, SORT_BY_NONE, SORT_ASCENDING); \
	}

#define SORT_UNBLOCK(key)					\
	if (sort_key == key)					\
		summary_sort(summaryview, sort_key, sort_type);

#define SUMMARY_DISPLAY_TOTAL_NUM(item) \
	(summaryview->on_filter ? summaryview->flt_msg_total : item->total)

#ifdef G_OS_WIN32
#  define SUMMARY_COL_MARK_WIDTH	23
#  define SUMMARY_COL_UNREAD_WIDTH	26
#  define SUMMARY_COL_MIME_WIDTH	21
#else
#  define SUMMARY_COL_MARK_WIDTH	21
#  define SUMMARY_COL_UNREAD_WIDTH	24
#  define SUMMARY_COL_MIME_WIDTH	19
#endif

static GdkPixbuf *mark_pixbuf;
static GdkPixbuf *deleted_pixbuf;

static GdkPixbuf *mail_pixbuf;
static GdkPixbuf *new_pixbuf;
static GdkPixbuf *unread_pixbuf;
static GdkPixbuf *replied_pixbuf;
static GdkPixbuf *forwarded_pixbuf;

static GdkPixbuf *clip_pixbuf;
static GdkPixbuf *html_pixbuf;

static void summary_clear_list_full	(SummaryView		*summaryview,
					 gboolean		 is_refresh);

static GList *summary_get_selected_rows	(SummaryView		*summaryview);
static void summary_selection_list_free	(SummaryView		*summaryview);

static GSList *summary_get_tmp_marked_msg_list
					(SummaryView	*summaryview);
static void summary_restore_tmp_marks	(SummaryView	*summaryview,
					 GSList		*save_mark_mlist);

static void summary_update_msg_list	(SummaryView		*summaryview);

static void summary_msgid_table_create	(SummaryView		*summaryview);
static void summary_msgid_table_destroy	(SummaryView		*summaryview);

static void summary_set_menu_sensitive	(SummaryView		*summaryview);

static void summary_scroll_to_selected	(SummaryView		*summaryview,
					 gboolean		 align_center);

static MsgInfo *summary_get_msginfo	(SummaryView		*summaryview,
					 GtkTreeRowReference	*row);
static guint summary_get_msgnum		(SummaryView		*summaryview,
					 GtkTreeRowReference	*row);

static gboolean summary_find_prev_msg	(SummaryView		*summaryview,
					 GtkTreeIter		*prev,
					 GtkTreeIter		*iter);
static gboolean summary_find_next_msg	(SummaryView		*summaryview,
					 GtkTreeIter		*next,
					 GtkTreeIter		*iter);

static gboolean summary_find_nearest_msg(SummaryView		*summaryview,
					 GtkTreeIter		*target,
					 GtkTreeIter		*iter);

static gboolean summary_find_prev_flagged_msg
					(SummaryView	*summaryview,
					 GtkTreeIter	*prev,
					 GtkTreeIter	*iter,
					 MsgPermFlags	 flags,
					 gboolean	 start_from_prev);
static gboolean summary_find_next_flagged_msg
					(SummaryView	*summaryview,
					 GtkTreeIter	*next,
					 GtkTreeIter	*iter,
					 MsgPermFlags	 flags,
					 gboolean	 start_from_next);

static gboolean summary_find_msg_by_msgnum
					(SummaryView		*summaryview,
					 guint			 msgnum,
					 GtkTreeIter		*found);

static void summary_update_display_state(SummaryView		*summaryview,
					 guint			 disp_msgnum,
					 guint			 sel_msgnum);

static void summary_update_status	(SummaryView		*summaryview);

/* display functions */
static void summary_status_show		(SummaryView		*summaryview);
static void summary_set_row		(SummaryView		*summaryview,
					 GtkTreeIter		*iter,
					 MsgInfo		*msginfo);
static void summary_set_tree_model_from_list
					(SummaryView		*summaryview,
					 GSList			*mlist);
static gboolean summary_row_is_displayed(SummaryView		*summaryview,
					 GtkTreeIter		*iter);
static void summary_display_msg		(SummaryView		*summaryview,
					 GtkTreeIter		*iter);
static void summary_display_msg_full	(SummaryView		*summaryview,
					 GtkTreeIter		*iter,
					 gboolean		 new_window,
					 gboolean		 all_headers,
					 gboolean		 redisplay);

static void summary_activate_selected	(SummaryView		*summaryview);

/* message handling */
static void summary_mark_row		(SummaryView		*summaryview,
					 GtkTreeIter		*iter);
static void summary_mark_row_as_read	(SummaryView		*summaryview,
					 GtkTreeIter		*iter);
static void summary_mark_row_as_unread	(SummaryView		*summaryview,
					 GtkTreeIter		*iter);
static void summary_delete_row		(SummaryView		*summaryview,
					 GtkTreeIter		*iter);
static void summary_unmark_row		(SummaryView		*summaryview,
					 GtkTreeIter		*iter);
static void summary_move_row_to		(SummaryView		*summaryview,
					 GtkTreeIter		*iter,
					 FolderItem		*to_folder);
static void summary_copy_row_to		(SummaryView		*summaryview,
					 GtkTreeIter		*iter,
					 FolderItem		*to_folder);

static void summary_remove_invalid_messages
					(SummaryView		*summaryview);

static gint summary_execute_move	(SummaryView		*summaryview);
static gint summary_execute_copy	(SummaryView		*summaryview);
static gint summary_execute_delete	(SummaryView		*summaryview);

static void summary_modify_threads	(SummaryView		*summaryview);

static void summary_colorlabel_menu_item_activate_cb
					(GtkWidget	*widget,
					 gpointer	 data);
static void summary_colorlabel_menu_item_activate_item_cb
					(GtkMenuItem	*label_menu_item,
					 gpointer	 data);
static void summary_colorlabel_menu_create
					(SummaryView	*summaryview);

static GtkWidget *summary_tree_view_create
					(SummaryView	*summaryview);

/* callback functions */
static gboolean summary_toggle_pressed	(GtkWidget		*eventbox,
					 GdkEventButton		*event,
					 SummaryView		*summaryview);
static gboolean summary_button_pressed	(GtkWidget		*treeview,
					 GdkEventButton		*event,
					 SummaryView		*summaryview);
static gboolean summary_button_released	(GtkWidget		*treeview,
					 GdkEventButton		*event,
					 SummaryView		*summaryview);
static gboolean summary_key_pressed	(GtkWidget		*treeview,
					 GdkEventKey		*event,
					 SummaryView		*summaryview);

static void summary_row_expanded	(GtkTreeView		*treeview,
					 GtkTreeIter		*iter,
					 GtkTreePath		*path,
					 SummaryView		*summaryview);
static void summary_row_collapsed	(GtkTreeView		*treeview,
					 GtkTreeIter		*iter,
					 GtkTreePath		*path,
					 SummaryView		*summaryview);

static void summary_columns_changed	(GtkTreeView		*treeview,
					 SummaryView		*summaryview);

static gboolean summary_select_func	(GtkTreeSelection	*selection,
					 GtkTreeModel		*model,
					 GtkTreePath		*path,
					 gboolean		 cur_selected,
					 gpointer		 data);

static void summary_selection_changed	(GtkTreeSelection	*selection,
					 SummaryView		*summaryview);

static void summary_col_resized		(GtkWidget		*widget,
					 GtkAllocation		*allocation,
					 SummaryView		*summaryview);

static void summary_reply_cb		(SummaryView		*summaryview,
					 guint			 action,
					 GtkWidget		*widget);
static void summary_show_all_header_cb	(SummaryView		*summaryview,
					 guint			 action,
					 GtkWidget		*widget);

static void summary_add_address_cb	(SummaryView		*summaryview,
					 guint			 action,
					 GtkWidget		*widget);
static void summary_create_filter_cb	(SummaryView		*summaryview,
					 guint			 action,
					 GtkWidget		*widget);

static void summary_column_clicked	(GtkWidget		*button,
					 SummaryView		*summaryview);

static gboolean summary_column_drop_func(GtkTreeView		*treeview,
					 GtkTreeViewColumn	*column,
					 GtkTreeViewColumn	*prev_column,
					 GtkTreeViewColumn	*next_column,
					 gpointer		 data);

static void summary_drag_begin		(GtkWidget	  *widget,
					 GdkDragContext	  *drag_context,
					 SummaryView	  *summaryview);
static void summary_drag_end		(GtkWidget	  *widget,
					 GdkDragContext	  *drag_context,
					 SummaryView	  *summaryview);
static void summary_drag_data_get       (GtkWidget        *widget,
					 GdkDragContext   *drag_context,
					 GtkSelectionData *selection_data,
					 guint             info,
					 guint             time,
					 SummaryView      *summaryview);

static void summary_text_adj_value_changed
					(GtkAdjustment		*adj,
					 SummaryView		*summaryview);

/* custom compare functions for sorting */
static gint summary_cmp_by_mark		(GtkTreeModel		*model,
					 GtkTreeIter		*a,
					 GtkTreeIter		*b,
					 gpointer		 data);
static gint summary_cmp_by_unread	(GtkTreeModel		*model,
					 GtkTreeIter		*a,
					 GtkTreeIter		*b,
					 gpointer		 data);
static gint summary_cmp_by_mime		(GtkTreeModel		*model,
					 GtkTreeIter		*a,
					 GtkTreeIter		*b,
					 gpointer		 data);
static gint summary_cmp_by_num		(GtkTreeModel		*model,
					 GtkTreeIter		*a,
					 GtkTreeIter		*b,
					 gpointer		 data);
static gint summary_cmp_by_size		(GtkTreeModel		*model,
					 GtkTreeIter		*a,
					 GtkTreeIter		*b,
					 gpointer		 data);
static gint summary_cmp_by_date		(GtkTreeModel		*model,
					 GtkTreeIter		*a,
					 GtkTreeIter		*b,
					 gpointer		 data);
static gint summary_cmp_by_thread_date	(GtkTreeModel		*model,
					 GtkTreeIter		*a,
					 GtkTreeIter		*b,
					 gpointer		 data);
static gint summary_cmp_by_from		(GtkTreeModel		*model,
					 GtkTreeIter		*a,
					 GtkTreeIter		*b,
					 gpointer		 data);
static gint summary_cmp_by_label	(GtkTreeModel		*model,
					 GtkTreeIter		*a,
					 GtkTreeIter		*b,
					 gpointer		 data);
static gint summary_cmp_by_to		(GtkTreeModel		*model,
					 GtkTreeIter		*a,
					 GtkTreeIter		*b,
					 gpointer		 data);
static gint summary_cmp_by_subject	(GtkTreeModel		*model,
					 GtkTreeIter		*a,
					 GtkTreeIter		*b,
					 gpointer		 data);


/* must be synched with FolderSortKey */
static SummaryColumnType sort_key_to_col[] = {
	-1,
	S_COL_NUMBER,
	S_COL_SIZE,
	S_COL_DATE,
	S_COL_TDATE,
	S_COL_FROM,
	S_COL_SUBJECT,
	-1,
	S_COL_LABEL,
	S_COL_MARK,
	S_COL_UNREAD,
	S_COL_MIME,
	S_COL_TO
};

/* must be synched with SummaryColumnType */
static FolderSortKey col_to_sort_key[] = {
	SORT_BY_MARK,
	SORT_BY_UNREAD,
	SORT_BY_MIME,
	SORT_BY_SUBJECT,
	SORT_BY_FROM,
	SORT_BY_DATE,
	SORT_BY_SIZE,
	SORT_BY_NUMBER,
	SORT_BY_TO
};

enum
{
	DRAG_TYPE_TEXT,
	DRAG_TYPE_RFC822,
	DRAG_TYPE_URI_LIST,

	N_DRAG_TYPES
};

static GtkTargetEntry summary_drag_types[] =
{
	{"text/plain", GTK_TARGET_SAME_APP, DRAG_TYPE_TEXT},
	{"message/rfc822", GTK_TARGET_SAME_APP, DRAG_TYPE_RFC822},
	{"text/uri-list", 0, DRAG_TYPE_URI_LIST}
};

static GtkItemFactoryEntry summary_popup_entries[] =
{
	{N_("/_Reply"),			NULL, summary_reply_cb,	COMPOSE_REPLY, NULL},
	{N_("/Repl_y to"),		NULL, NULL,		0, "<Branch>"},
	{N_("/Repl_y to/_all"),		NULL, summary_reply_cb,	COMPOSE_REPLY_TO_ALL, NULL},
	{N_("/Repl_y to/_sender"),	NULL, summary_reply_cb,	COMPOSE_REPLY_TO_SENDER, NULL},
	{N_("/Repl_y to/mailing _list"),
					NULL, summary_reply_cb,	COMPOSE_REPLY_TO_LIST, NULL},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/_Forward"),		NULL, summary_reply_cb, COMPOSE_FORWARD, NULL},
	{N_("/For_ward as attachment"),	NULL, summary_reply_cb, COMPOSE_FORWARD_AS_ATTACH, NULL},
	{N_("/Redirec_t"),		NULL, summary_reply_cb, COMPOSE_REDIRECT, NULL},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/M_ove..."),		NULL, summary_move_to,	0, NULL},
	{N_("/_Copy..."),		NULL, summary_copy_to,	0, NULL},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/_Mark"),			NULL, NULL,		0, "<Branch>"},
	{N_("/_Mark/Set _flag"),	NULL, summary_mark,	0, NULL},
	{N_("/_Mark/_Unset flag"),	NULL, summary_unmark,	0, NULL},
	{N_("/_Mark/---"),		NULL, NULL,		0, "<Separator>"},
	{N_("/_Mark/Mark as unr_ead"),	NULL, summary_mark_as_unread, 0, NULL},
	{N_("/_Mark/Mark as rea_d"),
					NULL, summary_mark_as_read, 0, NULL},
	{N_("/_Mark/Mark _thread as read"),
					NULL, summary_mark_thread_as_read, 0, NULL},
	{N_("/_Mark/Mark all _read"),	NULL, summary_mark_all_read, 0, NULL},
	{N_("/Color la_bel"),		NULL, NULL,		0, NULL},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/_Delete"),		NULL, summary_delete,	0, NULL},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/Set as _junk mail"),	NULL, summary_junk,	0, NULL},
	{N_("/Set as not j_unk mail"),	NULL, summary_not_junk,	0, NULL},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/Re-_edit"),		NULL, summary_reedit,   0, NULL},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/Add sender to address boo_k..."),
					NULL, summary_add_address_cb, 0, NULL},
	{N_("/Create f_ilter rule"),	NULL, NULL,		0, "<Branch>"},
	{N_("/Create f_ilter rule/_Automatically"),
					NULL, summary_create_filter_cb, FLT_BY_AUTO, NULL},
	{N_("/Create f_ilter rule/by _From"),
					NULL, summary_create_filter_cb, FLT_BY_FROM, NULL},
	{N_("/Create f_ilter rule/by _To"),
					NULL, summary_create_filter_cb, FLT_BY_TO, NULL},
	{N_("/Create f_ilter rule/by _Subject"),
					NULL, summary_create_filter_cb, FLT_BY_SUBJECT, NULL},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/_View"),			NULL, NULL,		0, "<Branch>"},
	{N_("/_View/Open in new _window"),
					NULL, summary_open_msg,	0, NULL},
	{N_("/_View/Mess_age source"),	NULL, summary_view_source, 0, NULL},
	{N_("/_View/All _headers"),	NULL, summary_show_all_header_cb, 0, "<ToggleItem>"},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/_Print..."),		NULL, summary_print,	0, NULL}
};

SummaryView *summary_create(void)
{
	SummaryView *summaryview;
	GtkWidget *vbox;
	GtkWidget *scrolledwin;
	GtkWidget *treeview;
	GtkTreeStore *store;
	GtkTreeSelection *selection;
	GtkWidget *hseparator;
	GtkWidget *hbox;
	GtkWidget *statlabel_folder;
	GtkWidget *statlabel_select;
	GtkWidget *statlabel_msgs;
	GtkWidget *toggle_eventbox;
	GtkWidget *toggle_arrow;
	GtkTooltips *tip;
	GtkWidget *popupmenu;
	GtkItemFactory *popupfactory;
	gint n_entries;
	GList *child;

	debug_print(_("Creating summary view...\n"));
	summaryview = g_new0(SummaryView, 1);

	vbox = gtk_vbox_new(FALSE, 1);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(vbox), scrolledwin, TRUE, TRUE, 0);
	gtk_widget_set_size_request(vbox,
				    prefs_common.summaryview_width,
				    prefs_common.summaryview_height);

	treeview = summary_tree_view_create(summaryview);
	gtk_container_add(GTK_CONTAINER(scrolledwin), treeview);

	store = GTK_TREE_STORE
		(gtk_tree_view_get_model(GTK_TREE_VIEW(treeview)));
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));

	/* create status label */
	hseparator = gtk_hseparator_new();
	gtk_box_pack_end(GTK_BOX(vbox), hseparator, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	statlabel_folder = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(hbox), statlabel_folder, FALSE, FALSE, 2);
	statlabel_select = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(hbox), statlabel_select, FALSE, FALSE, 12);

	/* toggle view button */
	toggle_eventbox = gtk_event_box_new();
	gtk_box_pack_end(GTK_BOX(hbox), toggle_eventbox, FALSE, FALSE, 4);
	toggle_arrow = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_container_add(GTK_CONTAINER(toggle_eventbox), toggle_arrow);
	g_signal_connect(G_OBJECT(toggle_eventbox), "button_press_event",
			 G_CALLBACK(summary_toggle_pressed), summaryview);
	tip = gtk_tooltips_new();
	gtk_tooltips_set_tip(tip, toggle_eventbox, _("Toggle message view"),
			     NULL);

	statlabel_msgs = gtk_label_new("");
	gtk_misc_set_alignment(GTK_MISC(statlabel_msgs), 1, 0.5);
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_label_set_ellipsize(GTK_LABEL(statlabel_msgs),
				PANGO_ELLIPSIZE_START);
#endif
	gtk_box_pack_start(GTK_BOX(hbox), statlabel_msgs, TRUE, TRUE, 2);

	/* create popup menu */
	n_entries = sizeof(summary_popup_entries) /
		sizeof(summary_popup_entries[0]);
	popupmenu = menu_create_items(summary_popup_entries, n_entries,
				      "<SummaryView>", &popupfactory,
				      summaryview);

	summaryview->vbox = vbox;
	summaryview->scrolledwin = scrolledwin;
	summaryview->treeview = treeview;
	summaryview->store = store;
	summaryview->selection = selection;
	summaryview->hseparator = hseparator;
	summaryview->hbox = hbox;
	summaryview->statlabel_folder = statlabel_folder;
	summaryview->statlabel_select = statlabel_select;
	summaryview->statlabel_msgs = statlabel_msgs;
	summaryview->toggle_eventbox = toggle_eventbox;
	summaryview->toggle_arrow = toggle_arrow;
	summaryview->popupmenu = popupmenu;
	summaryview->popupfactory = popupfactory;
	summaryview->lock_count = 0;

	summaryview->reedit_menuitem =
		gtk_item_factory_get_widget(popupfactory, "/Re-edit");
	child = g_list_find(GTK_MENU_SHELL(popupmenu)->children,
			    summaryview->reedit_menuitem);
	summaryview->reedit_separator = GTK_WIDGET(child->next->data);

	summaryview->junk_menuitem =
		gtk_item_factory_get_widget(popupfactory, "/Set as junk mail");
	summaryview->nojunk_menuitem =
		gtk_item_factory_get_widget(popupfactory,
					    "/Set as not junk mail");
	child = g_list_find(GTK_MENU_SHELL(popupmenu)->children,
			    summaryview->nojunk_menuitem);
	summaryview->junk_separator = GTK_WIDGET(child->next->data);

	gtk_widget_show_all(vbox);

	return summaryview;
}

void summary_init(SummaryView *summaryview)
{
	GtkWidget *pixmap;
	PangoFontDescription *font_desc;
	gint size;
	TextView *textview;
	GtkAdjustment *adj;

	stock_pixbuf_gdk(summaryview->treeview, STOCK_PIXMAP_MARK,
			 &mark_pixbuf);
	stock_pixbuf_gdk(summaryview->treeview, STOCK_PIXMAP_DELETED,
			 &deleted_pixbuf);
	stock_pixbuf_gdk(summaryview->treeview, STOCK_PIXMAP_MAIL_SMALL,
			 &mail_pixbuf);
	stock_pixbuf_gdk(summaryview->treeview, STOCK_PIXMAP_NEW,
			 &new_pixbuf);
	stock_pixbuf_gdk(summaryview->treeview, STOCK_PIXMAP_UNREAD,
			 &unread_pixbuf);
	stock_pixbuf_gdk(summaryview->treeview, STOCK_PIXMAP_REPLIED,
			 &replied_pixbuf);
	stock_pixbuf_gdk(summaryview->treeview, STOCK_PIXMAP_FORWARDED,
			 &forwarded_pixbuf);
	stock_pixbuf_gdk(summaryview->treeview, STOCK_PIXMAP_CLIP,
			 &clip_pixbuf);
	stock_pixbuf_gdk(summaryview->treeview, STOCK_PIXMAP_HTML,
			 &html_pixbuf);

	font_desc = pango_font_description_new();
	size = pango_font_description_get_size
		(summaryview->statlabel_folder->style->font_desc);
	pango_font_description_set_size(font_desc, size * PANGO_SCALE_SMALL);
	gtk_widget_modify_font(summaryview->statlabel_folder, font_desc);
	gtk_widget_modify_font(summaryview->statlabel_select, font_desc);
	gtk_widget_modify_font(summaryview->statlabel_msgs, font_desc);
	pango_font_description_free(font_desc);

	pixmap = stock_pixbuf_widget(summaryview->hbox,
				     STOCK_PIXMAP_FOLDER_OPEN);
	gtk_box_pack_start(GTK_BOX(summaryview->hbox), pixmap, FALSE, FALSE, 4);
	gtk_box_reorder_child(GTK_BOX(summaryview->hbox), pixmap, 0);
	gtk_widget_show(pixmap);

	summary_clear_list(summaryview);
	summary_set_column_order(summaryview);
	summary_colorlabel_menu_create(summaryview);
	summary_set_menu_sensitive(summaryview);

	textview = summaryview->messageview->textview;
	adj = GTK_TEXT_VIEW(textview->text)->vadjustment;
	g_signal_connect(adj, "value-changed",
			 G_CALLBACK(summary_text_adj_value_changed),
			 summaryview);
	textview = summaryview->messageview->mimeview->textview;
	adj = GTK_TEXT_VIEW(textview->text)->vadjustment;
	g_signal_connect(adj, "value-changed",
			 G_CALLBACK(summary_text_adj_value_changed),
			 summaryview);
}

static void get_msg_list_func(Folder *folder, FolderItem *item, gpointer data)
{
	SummaryView *summary = (SummaryView *)folder->data;
	gint count = GPOINTER_TO_INT(data);
	static GTimeVal tv_prev = {0, 0};
	GTimeVal tv_cur;

	g_get_current_time(&tv_cur);

	if (tv_prev.tv_sec == 0 ||
	    (tv_cur.tv_sec - tv_prev.tv_sec) * G_USEC_PER_SEC +
	    tv_cur.tv_usec - tv_prev.tv_usec > 100 * 1000) {
		gchar buf[256];

		g_snprintf(buf, sizeof(buf), _("Scanning folder (%s) (%d)..."),
			   item->path, count);
		STATUSBAR_POP(summary->mainwin);
		STATUSBAR_PUSH(summary->mainwin, buf);
		tv_prev = tv_cur;
	}
}

gboolean summary_show(SummaryView *summaryview, FolderItem *item,
		      gboolean update_cache)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(summaryview->treeview);
	GtkTreeIter iter;
	GSList *mlist;
	gchar *buf;
	gboolean is_refresh;
	guint selected_msgnum = 0;
	guint displayed_msgnum = 0;
	gboolean moved;
	gboolean selection_done = FALSE;
	gboolean do_qsearch = FALSE;
	gboolean set_column_order_required = FALSE;
	const gchar *key = NULL;
	gpointer save_data;
	GSList *save_mark_mlist = NULL;

	if (summary_is_locked(summaryview)) return FALSE;

	inc_lock();
	summary_lock(summaryview);

	STATUSBAR_POP(summaryview->mainwin);

	is_refresh = (item == summaryview->folder_item &&
		      update_cache == FALSE) ? TRUE : FALSE;
	selected_msgnum = summary_get_msgnum(summaryview,
					     summaryview->selected);
	displayed_msgnum = summary_get_msgnum(summaryview,
					      summaryview->displayed);
	if (summaryview->folder_item)
		summaryview->folder_item->last_selected = selected_msgnum;
	if (!is_refresh) {
		if (item)
			selected_msgnum = item->last_selected;
		else
			selected_msgnum = 0;
		displayed_msgnum = 0;
	}

	/* process the marks if any */
	if (summaryview->mainwin->lock_count == 0 && !is_refresh &&
	    (summaryview->moved > 0 || summaryview->copied > 0)) {
		AlertValue val;

		val = alertpanel(_("Process mark"),
				 _("Some marks are left. Process it?"),
				 GTK_STOCK_YES, GTK_STOCK_NO, GTK_STOCK_CANCEL);
		if (G_ALERTDEFAULT == val) {
			summary_unlock(summaryview);
			summary_execute(summaryview);
			summary_lock(summaryview);
			GTK_EVENTS_FLUSH();
		} else if (G_ALERTALTERNATE == val) {
			summary_write_cache(summaryview);
			GTK_EVENTS_FLUSH();
		} else {
			summary_unlock(summaryview);
			inc_unlock();
			return FALSE;
		}
	} else {
		/* save temporary move/copy marks */
		if (is_refresh &&
		    (summaryview->moved > 0 || summaryview->copied > 0))
			save_mark_mlist = summary_get_tmp_marked_msg_list(summaryview);

		summary_write_cache(summaryview);
	}

	if (FOLDER_ITEM_IS_SENT_FOLDER(summaryview->folder_item) !=
	    FOLDER_ITEM_IS_SENT_FOLDER(item))
		set_column_order_required = TRUE;

	folderview_set_opened_item(summaryview->folderview, item);

	summary_clear_list_full(summaryview, is_refresh);

	buf = NULL;
	if (!item || !item->path || !item->parent || item->no_select ||
	    (FOLDER_TYPE(item->folder) == F_MH && item->stype != F_VIRTUAL &&
	     ((buf = folder_item_get_path(item)) == NULL ||
	      change_dir(buf) < 0))) {
		g_free(buf);
		debug_print("empty folder\n\n");
		summary_clear_all(summaryview);
		summaryview->folder_item = item;
		if (item)
			item->qsearch_cond_type = QS_ALL;
		if (set_column_order_required)
			summary_set_column_order(summaryview);
		if (save_mark_mlist)
			procmsg_msg_list_free(save_mark_mlist);
		summary_unlock(summaryview);
		inc_unlock();
		return TRUE;
	}
	g_free(buf);

	if (!is_refresh)
		messageview_clear(summaryview->messageview);

	summaryview->folder_item = item;
	if (set_column_order_required)
		summary_set_column_order(summaryview);

	g_signal_handlers_block_matched(G_OBJECT(treeview),
					(GSignalMatchType)G_SIGNAL_MATCH_DATA,
					0, 0, NULL, NULL, summaryview);

	buf = g_strdup_printf(_("Scanning folder (%s)..."), item->path);
	debug_print("%s\n", buf);
	STATUSBAR_PUSH(summaryview->mainwin, buf);
	g_free(buf);

	main_window_cursor_wait(summaryview->mainwin);

	save_data = item->folder->data;
	item->folder->data = summaryview;
	folder_set_ui_func(item->folder, get_msg_list_func, NULL);

	mlist = folder_item_get_msg_list(item, !update_cache);

	folder_set_ui_func(item->folder, NULL, NULL);
	item->folder->data = save_data;

	statusbar_pop_all();
	STATUSBAR_POP(summaryview->mainwin);

	summaryview->all_mlist = mlist;

	/* restore temporary move/copy marks */
	if (save_mark_mlist) {
		summary_restore_tmp_marks(summaryview, save_mark_mlist);
		save_mark_mlist = NULL;
	}

	/* set tree store and hash table from the msginfo list, and
	   create the thread */
	if (prefs_common.show_searchbar &&
	    (prefs_common.persist_qsearch_filter || is_refresh)) {
		if (item->qsearch_cond_type > QS_ALL)
			do_qsearch = TRUE;
		if (is_refresh && summaryview->qsearch->entry_entered) {
			key = gtk_entry_get_text
				(GTK_ENTRY(summaryview->qsearch->entry));
			if (key && *key != '\0')
				do_qsearch = TRUE;
			else
				key = NULL;
		}
	}

	if (do_qsearch) {
		gint index;
		QSearchCondType type = item->qsearch_cond_type;

		index = menu_find_option_menu_index
			(GTK_OPTION_MENU(summaryview->qsearch->optmenu),
			 GINT_TO_POINTER(type), NULL);
		if (index > 0) {
			gtk_option_menu_set_history
                		(GTK_OPTION_MENU(summaryview->qsearch->optmenu),
				 index);
		} else {
			gtk_option_menu_set_history
                		(GTK_OPTION_MENU(summaryview->qsearch->optmenu),
				 0);
			type = QS_ALL;
		}

		if (type > QS_ALL || key) {
			summaryview->flt_mlist =
				quick_search_filter(summaryview->qsearch,
						    type, key);
			summaryview->on_filter = TRUE;
			summary_set_tree_model_from_list
				(summaryview, summaryview->flt_mlist);
			summary_update_status(summaryview);
		} else {
			item->qsearch_cond_type = QS_ALL;
			summary_set_tree_model_from_list(summaryview, mlist);
		}
	} else {
		item->qsearch_cond_type = QS_ALL;
		summary_set_tree_model_from_list(summaryview, mlist);
	}

	if (mlist)
		gtk_widget_grab_focus(GTK_WIDGET(treeview));

	summary_write_cache(summaryview);

	item->opened = TRUE;

	g_signal_handlers_unblock_matched(G_OBJECT(treeview),
					  (GSignalMatchType)G_SIGNAL_MATCH_DATA,
					  0, 0, NULL, NULL, summaryview);

	if (is_refresh) {
		summary_update_display_state(summaryview, displayed_msgnum,
					     selected_msgnum);

		if (!summaryview->selected) {
			/* no selected message - select first unread
			   message, but do not display it */
			if (summary_find_next_flagged_msg
				(summaryview, &iter, NULL, MSG_UNREAD, FALSE)) {
				summary_select_row(summaryview, &iter,
						   FALSE, TRUE);
			} else if (item->sort_type == SORT_ASCENDING &&
				   SUMMARY_DISPLAY_TOTAL_NUM(item) > 1) {
				g_signal_emit_by_name
					(treeview, "move-cursor",
					 GTK_MOVEMENT_BUFFER_ENDS, 1, &moved);
				GTK_EVENTS_FLUSH();
				summary_scroll_to_selected(summaryview, TRUE);
			} else if (gtk_tree_model_get_iter_first
					(GTK_TREE_MODEL(summaryview->store),
					 &iter)) {
				summary_select_row(summaryview, &iter,
						   FALSE, TRUE);
			}
		}
		selection_done = TRUE;
	} else if (prefs_common.remember_last_selected) {
		summary_unlock(summaryview);
		summary_select_by_msgnum(summaryview, selected_msgnum);
		summary_lock(summaryview);

		if (summaryview->selected)
			selection_done = TRUE;
	}

	if (!selection_done) {
		/* select first unread message */
		if (summary_find_next_flagged_msg(summaryview, &iter, NULL,
						  MSG_UNREAD, FALSE)) {
			if (prefs_common.open_unread_on_enter ||
			    prefs_common.always_show_msg) {
				summary_unlock(summaryview);
				summary_select_row(summaryview, &iter,
						   TRUE, TRUE);
				summary_lock(summaryview);
			} else {
				summary_select_row(summaryview, &iter,
						   FALSE, TRUE);
			}
		} else {
			summary_unlock(summaryview);
			if (item->sort_type == SORT_ASCENDING &&
			    SUMMARY_DISPLAY_TOTAL_NUM(item) > 1) {
				g_signal_emit_by_name
					(treeview, "move-cursor",
					 GTK_MOVEMENT_BUFFER_ENDS, 1, &moved);
			} else if (gtk_tree_model_get_iter_first
					(GTK_TREE_MODEL(summaryview->store),
					 &iter)) {
				summary_select_row(summaryview, &iter,
						   FALSE, TRUE);
			}
			summary_lock(summaryview);
			GTK_EVENTS_FLUSH();
			summary_scroll_to_selected(summaryview, TRUE);
		}
	}

	summary_status_show(summaryview);
	summary_set_menu_sensitive(summaryview);
	main_window_set_toolbar_sensitive(summaryview->mainwin);

	debug_print("\n");
	STATUSBAR_PUSH(summaryview->mainwin, _("Done."));

	main_window_cursor_normal(summaryview->mainwin);

	if (prefs_common.online_mode) {
		if (FOLDER_IS_REMOTE(item->folder) &&
		    REMOTE_FOLDER(item->folder)->session == NULL) {
			alertpanel_error(_("Could not establish a connection to the server."));
		}
	}

	summary_unlock(summaryview);
	inc_unlock();

	return TRUE;
}

static void summary_unset_sort_column_id(SummaryView *summaryview)
{
	gint id;
	GtkSortType order;

	if (gtk_tree_sortable_get_sort_column_id
		(GTK_TREE_SORTABLE(summaryview->store), &id, &order) &&
	    id >= 0 && id < N_SUMMARY_VISIBLE_COLS) {
		GtkTreeViewColumn *column = summaryview->columns[id];
		column->sort_column_id = -1;
		gtk_tree_view_column_set_sort_indicator(column, FALSE);
	}

	gtkut_tree_sortable_unset_sort_column_id
		(GTK_TREE_SORTABLE(summaryview->store));
}

void summary_clear_list(SummaryView *summaryview)
{
	summary_clear_list_full(summaryview, FALSE);
}

static void summary_clear_list_full(SummaryView *summaryview,
				    gboolean is_refresh)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(summaryview->treeview);
	GtkAdjustment *adj;

	if (summaryview->folder_item) {
		folder_item_close(summaryview->folder_item);
		summaryview->folder_item = NULL;
	}

	summaryview->display_msg = FALSE;

	summaryview->selected = NULL;
	summaryview->displayed = NULL;

	summary_selection_list_free(summaryview);

	summaryview->total_size = 0;
	summaryview->deleted = summaryview->moved = 0;
	summaryview->copied = 0;

	summary_msgid_table_destroy(summaryview);

	summaryview->tmp_mlist = NULL;
	summaryview->to_folder = NULL;
	if (summaryview->folder_table) {
		g_hash_table_destroy(summaryview->folder_table);
		summaryview->folder_table = NULL;
	}
	summaryview->filtered = 0;
	summaryview->flt_count = 0;
	summaryview->flt_total = 0;

	summaryview->on_button_press = FALSE;
	summaryview->can_toggle_selection = TRUE;
	summaryview->on_drag = FALSE;
	if (summaryview->pressed_path) {
		gtk_tree_path_free(summaryview->pressed_path);
		summaryview->pressed_path = NULL;
	}
	if (summaryview->drag_list) {
		g_free(summaryview->drag_list);
		summaryview->drag_list = NULL;
	}

	if (summaryview->flt_mlist) {
		g_slist_free(summaryview->flt_mlist);
		summaryview->flt_mlist = NULL;
	}
	summaryview->total_flt_msg_size = 0;
	summaryview->flt_msg_total = 0;
	summaryview->flt_deleted = summaryview->flt_moved = 0;
	summaryview->flt_copied = 0;
	summaryview->flt_new = summaryview->flt_unread = 0;
	if (!is_refresh) {
		quick_search_clear_entry(summaryview->qsearch);
		gtk_option_menu_set_history
			(GTK_OPTION_MENU(summaryview->qsearch->optmenu), 0);
	}
	summaryview->on_filter = FALSE;

	procmsg_msg_list_free(summaryview->all_mlist);
	summaryview->all_mlist = NULL;

	gtkut_tree_view_fast_clear(treeview, summaryview->store);

	/* ensure that the "value-changed" signal is always emitted */
	adj = gtk_tree_view_get_vadjustment(treeview);
	adj->value = 0.0;

	summary_unset_sort_column_id(summaryview);
}

void summary_clear_all(SummaryView *summaryview)
{
	messageview_clear(summaryview->messageview);
	summary_clear_list(summaryview);
	summary_set_menu_sensitive(summaryview);
	main_window_set_toolbar_sensitive(summaryview->mainwin);
	summary_status_show(summaryview);
}

void summary_show_queued_msgs(SummaryView *summaryview)
{
	FolderItem *item;
	GSList *qlist, *cur;
	MsgInfo *msginfo;
	GtkTreeStore *store = GTK_TREE_STORE(summaryview->store);
	GtkTreeIter iter;

	if (summary_is_locked(summaryview))
		return;

	item = summaryview->folder_item;
	if (!item || !item->path || !item->cache_queue ||
	    item->stype == F_VIRTUAL)
		return;

	debug_print("summary_show_queued_msgs: appending queued messages to summary (%s)\n", item->path);

	qlist = g_slist_reverse(item->cache_queue);
	item->cache_queue = NULL;
	if (item->mark_queue) {
		procmsg_flaginfo_list_free(item->mark_queue);
		item->mark_queue = NULL;
	}

	for (cur = qlist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;

		debug_print("summary_show_queued_msgs: appending msg %u\n",
			    msginfo->msgnum);
		msginfo->folder = item;
		gtk_tree_store_append(store, &iter, NULL);
		summary_set_row(summaryview, &iter, msginfo);

		if (cur == qlist) {
			GtkTreePath *path;

			path = gtk_tree_model_get_path(GTK_TREE_MODEL(store),
						       &iter);
			gtk_tree_view_scroll_to_cell
				(GTK_TREE_VIEW(summaryview->treeview), path,
				 NULL, FALSE, 0.0, 0.0);
			gtk_tree_path_free(path);
		}

		summaryview->total_size += msginfo->size;
	}

	summaryview->all_mlist = g_slist_concat(summaryview->all_mlist, qlist);

	item->cache_dirty = TRUE;
	summary_selection_list_free(summaryview);

	summary_status_show(summaryview);

	debug_print("summary_show_queued_msgs: done.\n");
}

void summary_lock(SummaryView *summaryview)
{
	summaryview->lock_count++;
	summaryview->write_lock_count++;
	/* g_print("summary_lock: %d\n", summaryview->lock_count); */
}

void summary_unlock(SummaryView *summaryview)
{
	/* g_print("summary_unlock: %d\n", summaryview->lock_count); */
	if (summaryview->lock_count)
		summaryview->lock_count--;
	if (summaryview->write_lock_count)
		summaryview->write_lock_count--;
}

gboolean summary_is_locked(SummaryView *summaryview)
{
	return summaryview->lock_count > 0 || summaryview->write_lock_count > 0;
}

gboolean summary_is_read_locked(SummaryView *summaryview)
{
	return summaryview->lock_count > 0;
}

void summary_write_lock(SummaryView *summaryview)
{
	summaryview->write_lock_count++;
}

void summary_write_unlock(SummaryView *summaryview)
{
	if (summaryview->write_lock_count)
		summaryview->write_lock_count--;
}

gboolean summary_is_write_locked(SummaryView *summaryview)
{
	return summaryview->write_lock_count > 0;
}

FolderItem *summary_get_current_folder(SummaryView *summaryview)
{
	return summaryview->folder_item;
}

SummarySelection summary_get_selection_type(SummaryView *summaryview)
{
	SummarySelection selection;
	GList *rows;

	rows = summary_get_selected_rows(summaryview);

	if (!summaryview->folder_item || summaryview->folder_item->total == 0)
		selection = SUMMARY_NONE;
	else if (!rows)
		selection = SUMMARY_SELECTED_NONE;
	else if (rows && !rows->next)
		selection = SUMMARY_SELECTED_SINGLE;
	else
		selection = SUMMARY_SELECTED_MULTIPLE;

	return selection;
}

static GList *summary_get_selected_rows(SummaryView *summaryview)
{
	if (!summaryview->selection_list)
		summaryview->selection_list =
			gtk_tree_selection_get_selected_rows
				(summaryview->selection, NULL);

	return summaryview->selection_list;
}

static void summary_selection_list_free(SummaryView *summaryview)
{
	if (summaryview->selection_list) {
		g_list_foreach(summaryview->selection_list,
			       (GFunc)gtk_tree_path_free, NULL);
		g_list_free(summaryview->selection_list);
		summaryview->selection_list = NULL;
	}
}

GSList *summary_get_selected_msg_list(SummaryView *summaryview)
{
	GSList *mlist = NULL;
	GList *rows;
	GList *cur;
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter;
	MsgInfo *msginfo;

	rows = summary_get_selected_rows(summaryview);
	for (cur = rows; cur != NULL; cur = cur->next) {
		gtk_tree_model_get_iter(model, &iter, (GtkTreePath *)cur->data);
		gtk_tree_model_get(model, &iter, S_COL_MSG_INFO, &msginfo, -1);
		mlist = g_slist_prepend(mlist, msginfo);
	}

	mlist = g_slist_reverse(mlist);

	return mlist;
}

GSList *summary_get_changed_msg_list(SummaryView *summaryview)
{
	MsgInfo *msginfo;
	GSList *mlist = NULL;
	GSList *cur;

	for (cur = summaryview->all_mlist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;
		if (MSG_IS_FLAG_CHANGED(msginfo->flags))
			mlist = g_slist_prepend(mlist, msginfo);
	}

	return g_slist_reverse(mlist);
}

GSList *summary_get_msg_list(SummaryView *summaryview)
{
	if (summaryview->on_filter)
		return g_slist_copy(summaryview->flt_mlist);
	else
		return g_slist_copy(summaryview->all_mlist);
}

GSList *summary_get_flagged_msg_list(SummaryView *summaryview,
				     MsgPermFlags flags)
{
	MsgInfo *msginfo;
	GSList *list, *cur;
	GSList *mlist = NULL;

	if (summaryview->on_filter)
		list = summaryview->flt_mlist;
	else
		list = summaryview->all_mlist;

	for (cur = list; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;
		if ((msginfo->flags.perm_flags & flags) != 0)
			mlist = g_slist_prepend(mlist, msginfo);
	}

	return g_slist_reverse(mlist);
}

/* return list of copied MsgInfo */
static GSList *summary_get_tmp_marked_msg_list(SummaryView *summaryview)
{
	MsgInfo *msginfo, *markinfo;
	GSList *mlist = NULL;
	GSList *cur;

	for (cur = summaryview->all_mlist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;
		if (MSG_IS_MOVE(msginfo->flags) ||
		    MSG_IS_COPY(msginfo->flags)) {
			markinfo = g_new0(MsgInfo, 1);
			markinfo->msgnum = msginfo->msgnum;
			markinfo->flags = msginfo->flags;
			markinfo->folder = msginfo->folder;
			markinfo->to_folder = msginfo->to_folder;
			mlist = g_slist_prepend(mlist, markinfo);
		}
	}

	return g_slist_reverse(mlist);
}

static void summary_restore_tmp_marks(SummaryView *summaryview,
				      GSList *save_mark_mlist)
{
	GSList *cur, *scur;
	MsgInfo *msginfo, *markinfo;

	debug_print("summary_restore_tmp_marks: restoring temporary marks\n");

	for (cur = summaryview->all_mlist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;
		for (scur = save_mark_mlist; scur != NULL; scur = scur->next) {
			markinfo = (MsgInfo *)scur->data;
			if (msginfo->msgnum == markinfo->msgnum &&
			    msginfo->folder == markinfo->folder) {
				msginfo->flags.tmp_flags |= (markinfo->flags.tmp_flags & (MSG_MOVE|MSG_COPY));
				msginfo->to_folder = markinfo->to_folder;
				save_mark_mlist = g_slist_remove
					(save_mark_mlist, markinfo);
				g_free(markinfo);
				if (!save_mark_mlist)
					return;
				break;
			}
		}
	}

	if (save_mark_mlist)
		procmsg_msg_list_free(save_mark_mlist);
}

static void summary_update_msg_list(SummaryView *summaryview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter;
	GSList *mlist = NULL;
	MsgInfo *msginfo;
	gboolean valid;

	if (summaryview->on_filter)
		return;

	g_slist_free(summaryview->all_mlist);
	summaryview->all_mlist = NULL;

	valid = gtk_tree_model_get_iter_first(model, &iter);

	while (valid) {
		gtk_tree_model_get(model, &iter, S_COL_MSG_INFO, &msginfo, -1);
		mlist = g_slist_prepend(mlist, msginfo);
		valid = gtkut_tree_model_next(model, &iter);
	}

	summaryview->all_mlist = g_slist_reverse(mlist);

}

static gboolean summary_msgid_table_create_func(GtkTreeModel *model,
						GtkTreePath *path,
						GtkTreeIter *iter,
						gpointer data)
{
	GHashTable *msgid_table = (GHashTable *)data;
	MsgInfo *msginfo;
	GtkTreeIter *iter_;

	gtk_tree_model_get(model, iter, S_COL_MSG_INFO, &msginfo, -1);

	if (msginfo && !MSG_IS_INVALID(msginfo->flags) &&
	    !MSG_IS_DELETED(msginfo->flags) &&
	    msginfo->msgid && msginfo->msgid[0] != '\0') {
		iter_ = gtk_tree_iter_copy(iter);
		g_hash_table_replace(msgid_table, msginfo->msgid, iter_);
	}

	return FALSE;
}

static void summary_msgid_table_create(SummaryView *summaryview)
{
	GHashTable *msgid_table;

	g_return_if_fail(summaryview->msgid_table == NULL);

	msgid_table = g_hash_table_new_full(g_str_hash, g_str_equal, NULL,
					    (GDestroyNotify)gtk_tree_iter_free);

	gtk_tree_model_foreach(GTK_TREE_MODEL(summaryview->store),
			       summary_msgid_table_create_func, msgid_table);

	summaryview->msgid_table = msgid_table;
}

static void summary_msgid_table_destroy(SummaryView *summaryview)
{
	if (!summaryview->msgid_table)
		return;

	g_hash_table_destroy(summaryview->msgid_table);
	summaryview->msgid_table = NULL;
}

static void summary_set_menu_sensitive(SummaryView *summaryview)
{
	GtkItemFactory *ifactory = summaryview->popupfactory;
	SummarySelection selection;
	GtkWidget *menuitem;
	gboolean sens;

	selection = summary_get_selection_type(summaryview);
	sens = (selection == SUMMARY_SELECTED_MULTIPLE) ? FALSE : TRUE;

	main_window_set_menu_sensitive(summaryview->mainwin);

	if (FOLDER_ITEM_IS_SENT_FOLDER(summaryview->folder_item)) {
		gtk_widget_show(summaryview->reedit_menuitem);
		gtk_widget_show(summaryview->reedit_separator);
		menu_set_sensitive(ifactory, "/Re-edit", sens);
	} else {
		gtk_widget_hide(summaryview->reedit_menuitem);
		gtk_widget_hide(summaryview->reedit_separator);
		menu_set_sensitive(ifactory, "/Re-edit", FALSE);
	}

	if (selection == SUMMARY_NONE) {
		menu_set_insensitive_all
			(GTK_MENU_SHELL(summaryview->popupmenu));
		return;
	}

	if (summaryview->folder_item &&
	    FOLDER_TYPE(summaryview->folder_item->folder) != F_NEWS) {
		menu_set_sensitive(ifactory, "/Move...", TRUE);
		menu_set_sensitive(ifactory, "/Delete", TRUE);
	} else {
		menu_set_sensitive(ifactory, "/Move...", FALSE);
		menu_set_sensitive(ifactory, "/Delete", FALSE);
	}

	menu_set_sensitive(ifactory, "/Copy...", TRUE);

	menu_set_sensitive(ifactory, "/Mark", TRUE);
	menu_set_sensitive(ifactory, "/Mark/Set flag",   TRUE);
	menu_set_sensitive(ifactory, "/Mark/Unset flag", TRUE);

	menu_set_sensitive(ifactory, "/Mark/Mark as unread", TRUE);
	menu_set_sensitive(ifactory, "/Mark/Mark as read",   TRUE);
	menu_set_sensitive(ifactory, "/Mark/Mark all read",  TRUE);

	if (prefs_common.enable_junk) {
		gtk_widget_show(summaryview->junk_menuitem);
		gtk_widget_show(summaryview->nojunk_menuitem);
		gtk_widget_show(summaryview->junk_separator);
		menu_set_sensitive(ifactory, "/Set as junk mail", TRUE);
		menu_set_sensitive(ifactory, "/Set as not junk mail", TRUE);
	} else {
		gtk_widget_hide(summaryview->junk_menuitem);
		gtk_widget_hide(summaryview->nojunk_menuitem);
		gtk_widget_hide(summaryview->junk_separator);
		menu_set_sensitive(ifactory, "/Set as junk mail", FALSE);
		menu_set_sensitive(ifactory, "/Set as not junk mail", FALSE);
	}

	menu_set_sensitive(ifactory, "/Color label", TRUE);

	menu_set_sensitive(ifactory, "/Reply",			  sens);
	menu_set_sensitive(ifactory, "/Reply to",		  sens);
	menu_set_sensitive(ifactory, "/Reply to/all",		  sens);
	menu_set_sensitive(ifactory, "/Reply to/sender",	  sens);
	menu_set_sensitive(ifactory, "/Reply to/mailing list",	  sens);
	menu_set_sensitive(ifactory, "/Forward",		  TRUE);
	menu_set_sensitive(ifactory, "/Forward as attachment",	  TRUE);
	menu_set_sensitive(ifactory, "/Redirect",		  sens);

	menu_set_sensitive(ifactory, "/Add sender to address book...", sens);
	menu_set_sensitive(ifactory, "/Create filter rule", sens);

	menu_set_sensitive(ifactory, "/View", sens);
	menu_set_sensitive(ifactory, "/View/Open in new window", sens);
	menu_set_sensitive(ifactory, "/View/Message source", sens);
	menu_set_sensitive(ifactory, "/View/All headers", sens);

	menu_set_sensitive(ifactory, "/Print...",   TRUE);

	summary_lock(summaryview);
	menuitem = gtk_item_factory_get_widget(ifactory, "/View/All headers");
	gtk_check_menu_item_set_active
		(GTK_CHECK_MENU_ITEM(menuitem),
		 summaryview->messageview->textview->show_all_headers);
	summary_unlock(summaryview);
}

static void summary_select_prev_flagged(SummaryView *summaryview,
					MsgPermFlags flags,
					const gchar *title,
					const gchar *ask_msg,
					const gchar *notice)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter prev, iter;
	gboolean start_from_prev = FALSE;
	gboolean found;

	if (!gtkut_tree_row_reference_get_iter(model, summaryview->selected,
					       &iter))
		return;

	if (!messageview_is_visible(summaryview->messageview) ||
	    summary_row_is_displayed(summaryview, &iter))
		start_from_prev = TRUE;

	found = summary_find_prev_flagged_msg
		(summaryview, &prev, &iter, flags, start_from_prev);

	if (!found) {
		AlertValue val;

		val = alertpanel(title, ask_msg, GTK_STOCK_YES, GTK_STOCK_NO,
				 NULL);
		if (val != G_ALERTDEFAULT) return;
		found = summary_find_prev_flagged_msg(summaryview, &prev, NULL,
						      flags, start_from_prev);
	}

	if (!found) {
		if (notice)
			alertpanel_notice("%s", notice);
	} else {
		gboolean visible;
		visible = messageview_is_visible(summaryview->messageview);
		summary_select_row(summaryview, &prev, visible, FALSE);
		if (visible)
			summary_mark_displayed_read(summaryview, &prev);
	}
}

static void summary_select_next_flagged(SummaryView *summaryview,
					MsgPermFlags flags,
					const gchar *title,
					const gchar *ask_msg,
					const gchar *notice)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter next, iter;
	gboolean start_from_next = FALSE;
	gboolean found;

	if (!gtkut_tree_row_reference_get_iter(model, summaryview->selected,
					       &iter)) {
		if (!gtk_tree_model_get_iter_first(model, &iter))
			return;
	}

	if (!messageview_is_visible(summaryview->messageview) ||
	    summary_row_is_displayed(summaryview, &iter))
		start_from_next = TRUE;

	found = summary_find_next_flagged_msg
		(summaryview, &next, &iter, flags, start_from_next);

	if (!found) {
		AlertValue val;

		val = alertpanel(title, ask_msg, GTK_STOCK_YES, GTK_STOCK_NO,
				 NULL);
		if (val != G_ALERTDEFAULT) return;
		found = summary_find_next_flagged_msg(summaryview, &next, NULL,
						      flags, start_from_next);
	}

	if (!found) {
		if (notice)
			alertpanel_notice("%s", notice);
	} else {
		gboolean visible;
		visible = messageview_is_visible(summaryview->messageview);
		summary_select_row(summaryview, &next, visible, FALSE);
		if (visible)
			summary_mark_displayed_read(summaryview, &next);
	}
}

static void summary_select_next_flagged_or_folder(SummaryView *summaryview,
						  MsgPermFlags flags,
						  const gchar *title,
						  const gchar *ask_msg,
						  const gchar *notice)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter, next;
	gboolean start_from_next = FALSE;
	gboolean visible;

	if (!gtkut_tree_row_reference_get_iter(model, summaryview->selected,
					       &iter)) {
		if (!gtk_tree_model_get_iter_first(model, &iter))
			return;
	}

	if (!messageview_is_visible(summaryview->messageview) ||
	    summary_row_is_displayed(summaryview, &iter))
		start_from_next = TRUE;

	while (summary_find_next_flagged_msg
		(summaryview, &next, &iter, flags, start_from_next) == FALSE) {
		AlertValue val;

		val = alertpanel(title, ask_msg,
				 GTK_STOCK_YES, GTK_STOCK_NO,
				 _("_Search again"));

		if (val == G_ALERTDEFAULT) {
			folderview_select_next_unread(summaryview->folderview);
			return;
		} else if (val == G_ALERTOTHER) {
			start_from_next = FALSE;
			if (!gtk_tree_model_get_iter_first(model, &iter))
				return;
		} else
			return;
	}

	visible = messageview_is_visible(summaryview->messageview);
	summary_select_row(summaryview, &next, visible, FALSE);
	if (visible)
		summary_mark_displayed_read(summaryview, &next);
}

void summary_select_prev_unread(SummaryView *summaryview)
{
	summary_select_prev_flagged(summaryview, MSG_UNREAD,
				    _("No more unread messages"),
				    _("No unread message found. "
				      "Search from the end?"),
				    _("No unread messages."));
}

void summary_select_next_unread(SummaryView *summaryview)
{
	summary_select_next_flagged_or_folder(summaryview, MSG_UNREAD,
					      _("No more unread messages"),
					      _("No unread message found. "
						"Go to next unread folder?"),
					      NULL);
}

void summary_select_prev_new(SummaryView *summaryview)
{
	summary_select_prev_flagged(summaryview, MSG_NEW,
				    _("No more new messages"),
				    _("No new message found. "
				      "Search from the end?"),
				    _("No new messages."));
}

void summary_select_next_new(SummaryView *summaryview)
{
	summary_select_next_flagged_or_folder(summaryview, MSG_NEW,
					      _("No more new messages"),
					      _("No new message found. "
						"Go to next folder which has new messages?"),
					      NULL);
}

void summary_select_prev_marked(SummaryView *summaryview)
{
	summary_select_prev_flagged(summaryview, MSG_MARKED,
				    _("No more marked messages"),
				    _("No marked message found. "
				      "Search from the end?"),
				    _("No marked messages."));
}

void summary_select_next_marked(SummaryView *summaryview)
{
	summary_select_next_flagged(summaryview, MSG_MARKED,
				    _("No more marked messages"),
				    _("No marked message found. "
				      "Search from the beginning?"),
				    _("No marked messages."));
}

void summary_select_prev_labeled(SummaryView *summaryview)
{
	summary_select_prev_flagged(summaryview, MSG_CLABEL_FLAG_MASK,
				    _("No more labeled messages"),
				    _("No labeled message found. "
				      "Search from the end?"),
				    _("No labeled messages."));
}

void summary_select_next_labeled(SummaryView *summaryview)
{
	summary_select_next_flagged(summaryview, MSG_CLABEL_FLAG_MASK,
				    _("No more labeled messages"),
				    _("No labeled message found. "
				      "Search from the beginning?"),
				    _("No labeled messages."));
}

void summary_select_by_msgnum(SummaryView *summaryview, guint msgnum)
{
	GtkTreeIter iter;

	if (summary_find_msg_by_msgnum(summaryview, msgnum, &iter))
		summary_select_row(summaryview, &iter, FALSE, TRUE);
}

gboolean summary_select_by_msginfo(SummaryView *summaryview, MsgInfo *msginfo)
{
	GtkTreeIter iter;

	if (summaryview->folder_item != msginfo->folder)
		return FALSE;

	if (summary_find_msg_by_msgnum(summaryview, msginfo->msgnum, &iter)) {
		summary_select_row(summaryview, &iter,
			   messageview_is_visible(summaryview->messageview),
			   TRUE);
		return TRUE;
	}

	return FALSE;
}

MsgInfo *summary_get_msginfo_by_msgnum(SummaryView *summaryview, guint msgnum)
{
	GtkTreeIter iter;
	MsgInfo *msginfo = NULL;

	if (summary_find_msg_by_msgnum(summaryview, msgnum, &iter))
		GET_MSG_INFO(msginfo, &iter);

	return msginfo;
}

static gboolean summary_select_true_func(GtkTreeSelection *selection, GtkTreeModel *model, GtkTreePath *path, gboolean cur_selected, gpointer data)
{
	return TRUE;
}

/**
 * summary_select_row:
 * @summaryview: Summary view.
 * @node: Summary tree node.
 * @display_msg: TRUE to display the selected message.
 * @do_refresh: TRUE to refresh the widget.
 *
 * Select @node (bringing it into view by scrolling and expanding its
 * thread, if necessary) and unselect all others.  If @display_msg is
 * TRUE, display the corresponding message in the message view.
 * If @do_refresh is TRUE, the widget is refreshed.
 **/
void summary_select_row(SummaryView *summaryview, GtkTreeIter *iter,
			 gboolean display_msg, gboolean do_refresh)
{
	GtkTreePath *path;

	if (!iter)
		return;

	gtkut_tree_view_expand_parent_all
		(GTK_TREE_VIEW(summaryview->treeview), iter);

	summaryview->display_msg = display_msg;
	path = gtk_tree_model_get_path(GTK_TREE_MODEL(summaryview->store),
				       iter);
	if (!display_msg)
		gtk_tree_selection_set_select_function(summaryview->selection,
						       summary_select_true_func,
						       summaryview, NULL);
	gtk_tree_view_set_cursor(GTK_TREE_VIEW(summaryview->treeview), path,
				 NULL, FALSE);
	if (!display_msg)
		gtk_tree_selection_set_select_function(summaryview->selection,
						       summary_select_func,
						       summaryview, NULL);
	if (do_refresh) {
		GTK_EVENTS_FLUSH();
		gtk_tree_view_scroll_to_cell
			(GTK_TREE_VIEW(summaryview->treeview),
			 path, NULL, TRUE, 0.5, 0.0);
	} else {
		gtkut_tree_view_scroll_to_cell
			(GTK_TREE_VIEW(summaryview->treeview), path,
			 !summaryview->on_button_press);
	}

	gtk_tree_path_free(path);
}

static void summary_scroll_to_selected(SummaryView *summaryview,
				       gboolean align_center)
{
	GtkTreePath *path;

	if (!summaryview->selected)
		return;

	path = gtk_tree_row_reference_get_path(summaryview->selected);
	if (path) {
		if (align_center)
			gtk_tree_view_scroll_to_cell
				(GTK_TREE_VIEW(summaryview->treeview),
				 path, NULL, TRUE, 0.5, 0.0);
		else
			gtkut_tree_view_scroll_to_cell
				(GTK_TREE_VIEW(summaryview->treeview), path,
				 FALSE);
		gtk_tree_path_free(path);
	}
}

static MsgInfo *summary_get_msginfo(SummaryView *summaryview,
				    GtkTreeRowReference *row)
{
	GtkTreeIter iter;
	MsgInfo *msginfo = NULL;

	if (!row)
		return 0;
	if (!gtkut_tree_row_reference_get_iter
		(GTK_TREE_MODEL(summaryview->store), row, &iter))
		return 0;

	gtk_tree_model_get(GTK_TREE_MODEL(summaryview->store), &iter,
			   S_COL_MSG_INFO, &msginfo, -1);

	return msginfo;
}

static guint summary_get_msgnum(SummaryView *summaryview,
				GtkTreeRowReference *row)
{
	MsgInfo *msginfo;

	msginfo = summary_get_msginfo(summaryview, row);
	if (!msginfo)
		return 0;

	return msginfo->msgnum;
}

static gboolean summary_find_prev_msg(SummaryView *summaryview,
				      GtkTreeIter *prev, GtkTreeIter *iter)
{
	GtkTreeIter iter_;
	MsgInfo *msginfo;
	gboolean valid = TRUE;

	if (!iter)
		return FALSE;

	iter_ = *iter;

	while (valid) {
		GET_MSG_INFO(msginfo, &iter_);
		if (msginfo && !MSG_IS_INVALID(msginfo->flags) &&
		    !MSG_IS_DELETED(msginfo->flags)) {
			*prev = iter_;
			return TRUE;
		}
		valid = gtkut_tree_model_prev
			(GTK_TREE_MODEL(summaryview->store), &iter_);
	}

	return FALSE;
}

static gboolean summary_find_next_msg(SummaryView *summaryview,
				      GtkTreeIter *next, GtkTreeIter *iter)
{
	GtkTreeIter iter_;
	MsgInfo *msginfo;
	gboolean valid = TRUE;

	if (!iter)
		return FALSE;

	iter_ = *iter;

	while (valid) {
		GET_MSG_INFO(msginfo, &iter_);
		if (msginfo && !MSG_IS_INVALID(msginfo->flags) &&
		    !MSG_IS_DELETED(msginfo->flags)) {
			*next = iter_;
			return TRUE;
		}
		valid = gtkut_tree_model_next
			(GTK_TREE_MODEL(summaryview->store), &iter_);
	}

	return FALSE;
}

static gboolean summary_find_nearest_msg(SummaryView *summaryview,
					 GtkTreeIter *target, GtkTreeIter *iter)
{
	gboolean valid;

	valid = summary_find_next_msg(summaryview, target, iter);
	if (!valid)
		valid = summary_find_prev_msg(summaryview, target, iter);

	return valid;
}

static gboolean summary_find_prev_flagged_msg(SummaryView *summaryview,
					      GtkTreeIter *prev,
					      GtkTreeIter *iter,
					      MsgPermFlags flags,
					      gboolean start_from_prev)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter_;
	MsgInfo *msginfo;
	gboolean valid = TRUE;

	if (iter) {
		iter_ = *iter;
		if (start_from_prev)
			valid = gtkut_tree_model_prev(model, &iter_);
	} else
		valid = gtkut_tree_model_get_iter_last(model, &iter_);

	for (; valid == TRUE; valid = gtkut_tree_model_prev(model, &iter_)) {
		GET_MSG_INFO(msginfo, &iter_);
		if (msginfo && (msginfo->flags.perm_flags & flags) != 0) {
			*prev = iter_;
			return TRUE;
		}
	}

	return FALSE;
}

static gboolean summary_find_next_flagged_msg(SummaryView *summaryview,
					      GtkTreeIter *next,
					      GtkTreeIter *iter,
					      MsgPermFlags flags,
					      gboolean start_from_next)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter_;
	MsgInfo *msginfo;
	gboolean valid = TRUE;

	if (iter) {
		iter_ = *iter;
		if (start_from_next)
			valid = gtkut_tree_model_next(model, &iter_);
	} else
		valid = gtk_tree_model_get_iter_first(model, &iter_);

	for (; valid == TRUE; valid = gtkut_tree_model_next(model, &iter_)) {
		GET_MSG_INFO(msginfo, &iter_);
		if (msginfo && (msginfo->flags.perm_flags & flags) != 0) {
			*next = iter_;
			return TRUE;
		}
	}

	return FALSE;
}

static gboolean summary_find_msg_by_msgnum(SummaryView *summaryview,
					   guint msgnum, GtkTreeIter *found)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter;
	MsgInfo *msginfo;
	gboolean valid;

	for (valid = gtk_tree_model_get_iter_first(model, &iter);
	     valid == TRUE; valid = gtkut_tree_model_next(model, &iter)) {
		gtk_tree_model_get(model, &iter, S_COL_MSG_INFO, &msginfo, -1);
		if (msginfo && msginfo->msgnum == msgnum) {
			*found = iter;
			return TRUE;
		}
	}

	return FALSE;
}

static void summary_update_display_state(SummaryView *summaryview,
					 guint disp_msgnum, guint sel_msgnum)
{
	GtkTreeIter iter;

	if (summary_find_msg_by_msgnum(summaryview, disp_msgnum, &iter)) {
		GtkTreePath *path;
		GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);

		path = gtk_tree_model_get_path(model, &iter);
		gtk_tree_row_reference_free(summaryview->displayed);
		summaryview->displayed =
			gtk_tree_row_reference_new(model, path);
		gtk_tree_path_free(path);
	} else
		messageview_clear(summaryview->messageview);

	summary_select_by_msgnum(summaryview, sel_msgnum);
}

static guint attract_hash_func(gconstpointer key)
{
	gchar str[BUFFSIZE];
	gchar *p;
	guint h;

	strncpy2(str, (const gchar *)key, sizeof(str));
	trim_subject_for_compare(str);

	p = str;
	h = *p;

	if (h) {
		for (p += 1; *p != '\0'; p++)
			h = (h << 5) - h + *p;
	}

	return h;
}

static gint attract_compare_func(gconstpointer a, gconstpointer b)
{
	return subject_compare((const gchar *)a, (const gchar *)b) == 0;
}

void summary_attract_by_subject(SummaryView *summaryview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter;
	MsgInfo *msginfo, *dest_msginfo;
	GHashTable *subject_table, *order_table;
	GSList *mlist = NULL, *list, *dest, *last = NULL, *next = NULL;
	gboolean valid;
	gint count, i;
	gint *new_order;

	if (!summaryview->folder_item)
		return;
	if (summaryview->folder_item->sort_key != SORT_BY_NONE)
		return;

	valid = gtk_tree_model_get_iter_first(model, &iter);
	if (!valid)
		return;

	debug_print("Attracting messages by subject...");
	STATUSBAR_PUSH(summaryview->mainwin,
		       _("Attracting messages by subject..."));

	main_window_cursor_wait(summaryview->mainwin);

	order_table = g_hash_table_new(NULL, NULL);

	for (count = 1; valid == TRUE; ++count) {
		gtk_tree_model_get(model, &iter, S_COL_MSG_INFO, &msginfo, -1);
		g_hash_table_insert(order_table, msginfo,
				    GINT_TO_POINTER(count));
		mlist = g_slist_prepend(mlist, msginfo);
		valid = gtk_tree_model_iter_next(model, &iter);
	}
	--count;

	mlist = g_slist_reverse(mlist);

	subject_table = g_hash_table_new(attract_hash_func,
					 attract_compare_func);

	for (list = mlist; list != NULL; list = next) {
		msginfo = (MsgInfo *)list->data;

		next = list->next;

		if (!msginfo->subject) {
			last = list;
			continue;
		}

		/* find attracting node */
		dest = g_hash_table_lookup(subject_table, msginfo->subject);

		if (dest) {
			dest_msginfo = (MsgInfo *)dest->data;

			/* if the time difference is more than 30 days,
			   don't attract */
			if (ABS(msginfo->date_t - dest_msginfo->date_t)
			    > 60 * 60 * 24 * 30) {
				last = list;
				continue;
			}

			if (dest->next != list) {
				last->next = list->next;
				list->next = dest->next;
				dest->next = list;
			} else
				last = list;
		} else
			last = list;

		g_hash_table_replace(subject_table, msginfo->subject, list);
	}

	g_hash_table_destroy(subject_table);

	new_order = g_new(gint, count);
	for (list = mlist, i = 0; list != NULL; list = list->next, ++i) {
		gint old_pos;

		msginfo = (MsgInfo *)list->data;

		old_pos = GPOINTER_TO_INT
			(g_hash_table_lookup(order_table, msginfo));
		new_order[i] = old_pos - 1;
	}
	gtk_tree_store_reorder(GTK_TREE_STORE(model), NULL, new_order);
	g_free(new_order);

	g_slist_free(mlist);
	g_hash_table_destroy(order_table);

	summaryview->folder_item->cache_dirty = TRUE;
	summary_selection_list_free(summaryview);
	summary_update_msg_list(summaryview);

	summary_scroll_to_selected(summaryview, TRUE);

	debug_print("done.\n");
	STATUSBAR_POP(summaryview->mainwin);

	main_window_cursor_normal(summaryview->mainwin);
}

static void summary_update_status(SummaryView *summaryview)
{
	GSList *cur;
	MsgInfo *msginfo;
	gint64 total_size = 0;
	gint deleted = 0, moved = 0, copied = 0;
	gint64 flt_total_size = 0;
	gint flt_deleted = 0, flt_moved = 0, flt_copied = 0;
	gint flt_new = 0, flt_unread = 0, flt_total = 0;

	for (cur = summaryview->all_mlist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;

		if (MSG_IS_DELETED(msginfo->flags))
			deleted++;
		if (MSG_IS_MOVE(msginfo->flags))
			moved++;
		if (MSG_IS_COPY(msginfo->flags))
			copied++;
		total_size += msginfo->size;
	}

	for (cur = summaryview->flt_mlist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;

		if (MSG_IS_DELETED(msginfo->flags))
			flt_deleted++;
		if (MSG_IS_MOVE(msginfo->flags))
			flt_moved++;
		if (MSG_IS_COPY(msginfo->flags))
			flt_copied++;
		if (MSG_IS_NEW(msginfo->flags))
			flt_new++;
		if (MSG_IS_UNREAD(msginfo->flags))
			flt_unread++;
		flt_total++;
		flt_total_size += msginfo->size;
	}

	summaryview->total_size = total_size;
	summaryview->deleted = deleted;
	summaryview->moved = moved;
	summaryview->copied = copied;
	summaryview->total_flt_msg_size = flt_total_size;
	summaryview->flt_msg_total = flt_total;
	summaryview->flt_deleted = flt_deleted;
	summaryview->flt_moved = flt_moved;
	summaryview->flt_copied = flt_copied;
	summaryview->flt_new = flt_new;
	summaryview->flt_unread = flt_unread;
}

static void summary_status_show(SummaryView *summaryview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GString *str;
	gchar *name;
	GList *rowlist, *cur;
	guint n_selected = 0;
	gint64 sel_size = 0;
	MsgInfo *msginfo;
	gint deleted, moved, copied;
	gint new, unread, total;
	gint64 total_size;

	if (!summaryview->folder_item) {
		gtk_label_set(GTK_LABEL(summaryview->statlabel_folder), "");
		gtk_label_set(GTK_LABEL(summaryview->statlabel_select), "");
		gtk_label_set(GTK_LABEL(summaryview->statlabel_msgs),   "");
		return;
	}

	rowlist = summary_get_selected_rows(summaryview);
	for (cur = rowlist; cur != NULL; cur = cur->next) {
		GtkTreeIter iter;
		GtkTreePath *path = (GtkTreePath *)cur->data;

		if (gtk_tree_model_get_iter(model, &iter, path)) {
			gtk_tree_model_get(model, &iter,
					   S_COL_MSG_INFO, &msginfo, -1);
			sel_size += msginfo->size;
			n_selected++;
		}
	}

	if (FOLDER_TYPE(summaryview->folder_item->folder) == F_NEWS) {
		gchar *group;
		group = get_abbrev_newsgroup_name
			(g_basename(summaryview->folder_item->path),
			 prefs_common.ng_abbrev_len);
		name = trim_string_before(group, 32);
		g_free(group);
	} else
		name = trim_string_before(summaryview->folder_item->path, 32);
	gtk_label_set(GTK_LABEL(summaryview->statlabel_folder), name);
	g_free(name);

	if (summaryview->on_filter) {
		deleted = summaryview->flt_deleted;
		moved = summaryview->flt_moved;
		copied = summaryview->flt_copied;
	} else {
		deleted = summaryview->deleted;
		moved = summaryview->moved;
		copied = summaryview->copied;
	}

	str = g_string_sized_new(128);

	if (n_selected)
		g_string_append_printf(str, "%d%s (%s)", n_selected,
				       _(" item(s) selected"),
				       to_human_readable(sel_size));
	if (str->len > 0 && (deleted || moved || copied))
		g_string_append(str, "    ");
	if (deleted)
		g_string_append_printf(str, _("%d deleted"), deleted);
	if (moved)
		g_string_append_printf(str, _("%s%d moved"),
				       deleted ? _(", ") : "", moved);
	if (copied)
		g_string_append_printf(str, _("%s%d copied"),
				       deleted || moved ? _(", ") : "", copied);

	gtk_label_set(GTK_LABEL(summaryview->statlabel_select), str->str);
	g_string_truncate(str, 0);

	new = summaryview->folder_item->new;
	unread = summaryview->folder_item->unread;
	total = summaryview->folder_item->total;
	total_size = summaryview->total_size;

	if (summaryview->on_filter) {
		gint f_new, f_unread, f_total;
		gint64 f_total_size;
		gchar f_ts[16], ts[16];

		f_new = summaryview->flt_new;
		f_unread = summaryview->flt_unread;
		f_total = summaryview->flt_msg_total;
		f_total_size = summaryview->total_flt_msg_size;

		g_string_printf(str, _("%d/%d new, %d/%d unread, %d/%d total"),
				f_new, new, f_unread, unread, f_total, total);
		if (FOLDER_IS_LOCAL(summaryview->folder_item->folder)) {
			g_string_append_printf(str, " (%s/%s)",
					       to_human_readable_buf(f_ts, sizeof(f_ts), f_total_size), to_human_readable_buf(ts, sizeof(ts), total_size));
		}
	} else {
		if (FOLDER_IS_LOCAL(summaryview->folder_item->folder)) {
			g_string_printf(str,
					_("%d new, %d unread, %d total (%s)"),
					new, unread, total,
					to_human_readable(total_size));
		} else {
			g_string_printf(str, _("%d new, %d unread, %d total"),
					new, unread, total);
		}
	}

	gtk_label_set(GTK_LABEL(summaryview->statlabel_msgs), str->str);
	g_string_free(str, TRUE);

	folderview_update_opened_msg_num(summaryview->folderview);
}

void summary_sort(SummaryView *summaryview,
		  FolderSortKey sort_key, FolderSortType sort_type)
{
	FolderItem *item = summaryview->folder_item;
	GtkTreeSortable *sortable = GTK_TREE_SORTABLE(summaryview->store);
	SummaryColumnType col_type, prev_col_type;
	GtkTreeViewColumn *column;

	g_return_if_fail(sort_key >= SORT_BY_NONE && sort_key <= SORT_BY_TO);

	if (!item || !item->path || !item->parent || item->no_select) return;

	if (item->sort_key != sort_key || item->sort_type != sort_type)
		item->cache_dirty = TRUE;

	col_type = sort_key_to_col[sort_key];
	prev_col_type = sort_key_to_col[item->sort_key];

	if (col_type == -1) {
		item->sort_key = SORT_BY_NONE;
		item->sort_type = SORT_ASCENDING;
		summary_unset_sort_column_id(summaryview);
		summary_set_menu_sensitive(summaryview);
		return;
	}

	debug_print("Sorting summary by key: %d...\n", sort_key);
	STATUSBAR_PUSH(summaryview->mainwin, _("Sorting summary..."));

	main_window_cursor_wait(summaryview->mainwin);

	item->sort_key = sort_key;
	item->sort_type = sort_type;

	gtk_tree_sortable_set_sort_column_id(sortable, col_type,
					     (GtkSortType)sort_type);

	if (prev_col_type != -1 && col_type != prev_col_type &&
	    prev_col_type < N_SUMMARY_VISIBLE_COLS) {
		column = summaryview->columns[prev_col_type];
		column->sort_column_id = -1;
		gtk_tree_view_column_set_sort_indicator(column, FALSE);
	}
	if (col_type != S_COL_MARK && col_type != S_COL_UNREAD &&
	    col_type != S_COL_MIME && col_type < N_SUMMARY_VISIBLE_COLS) {
		column = summaryview->columns[col_type];
		column->sort_column_id = col_type;
		gtk_tree_view_column_set_sort_indicator(column, TRUE);
		gtk_tree_view_column_set_sort_order(column,
						    (GtkSortType)sort_type);
	}

	summary_selection_list_free(summaryview);
	if (summaryview->all_mlist)
		summary_update_msg_list(summaryview);

	summary_set_menu_sensitive(summaryview);

	summary_scroll_to_selected(summaryview, TRUE);

	debug_print("done.\n");
	STATUSBAR_POP(summaryview->mainwin);

	main_window_cursor_normal(summaryview->mainwin);
}

static gboolean summary_have_unread_children(SummaryView *summaryview,
					     GtkTreeIter *iter)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter_;
	MsgInfo *msginfo;
	gboolean valid;

	gtk_tree_model_get(model, iter, S_COL_MSG_INFO, &msginfo, -1);
	if (MSG_IS_UNREAD(msginfo->flags))
		return TRUE;

	valid = gtk_tree_model_iter_children(model, &iter_, iter);

	while (valid) {
		if (summary_have_unread_children(summaryview, &iter_))
			return TRUE;

		valid = gtk_tree_model_iter_next(model, &iter_);
	}

	return FALSE;
}

static void summary_set_row(SummaryView *summaryview, GtkTreeIter *iter,
			    MsgInfo *msginfo)
{
	GtkTreeStore *store = GTK_TREE_STORE(summaryview->store);
	gchar date_modified[80];
	const gchar *date_s;
	gchar *sw_from_s = NULL;
	gchar *subject_s = NULL;
	gchar *to_s = NULL;
	const gchar *disp_from = NULL;
	GdkPixbuf *mark_pix = NULL;
	GdkPixbuf *unread_pix = NULL;
	GdkPixbuf *mime_pix = NULL;
	GdkColor *foreground = NULL;
	PangoWeight weight = PANGO_WEIGHT_NORMAL;
	MsgFlags flags;
	GdkColor color;
	gint color_val;

	if (!msginfo) {
		GET_MSG_INFO(msginfo, iter);
	}

	if (msginfo->date_t) {
		procheader_date_get_localtime(date_modified,
					      sizeof(date_modified),
					      msginfo->date_t);
		date_s = date_modified;
	} else if (msginfo->date)
		date_s = msginfo->date;
	else
		date_s = _("(No Date)");
	if (prefs_common.swap_from && msginfo->from && msginfo->to) {
		gchar from[BUFFSIZE];

		strncpy2(from, msginfo->from, sizeof(from));
		extract_address(from);
		if (account_address_exist(from)) {
			sw_from_s = g_strconcat("-->", msginfo->to, NULL);
			disp_from = sw_from_s;
		}
	}

	if (!disp_from) {
		/* prevent address-like display-name */
		if (!msginfo->fromname ||
		    strchr(msginfo->fromname, '@') != NULL)
			disp_from = msginfo->from;
		else
			disp_from = msginfo->fromname;
	}

	if (msginfo->subject && *msginfo->subject) {
		if (msginfo->folder && msginfo->folder->trim_summary_subject) {
			subject_s = g_strdup(msginfo->subject);
			trim_subject(subject_s);
		}
	}

	if (msginfo->to)
		to_s = procheader_get_toname(msginfo->to);

	flags = msginfo->flags;

	/* set flag pixbufs */
	if (MSG_IS_DELETED(flags)) {
		mark_pix = deleted_pixbuf;
		foreground = &summaryview->color_dim;
	} else if (MSG_IS_MOVE(flags)) {
		/* mark_pix = move_pixbuf; */
		foreground = &summaryview->color_marked;
	} else if (MSG_IS_COPY(flags)) {
		/* mark_pix = copy_pixbuf; */
		foreground = &summaryview->color_marked;
	} else if (MSG_IS_MARKED(flags))
		mark_pix = mark_pixbuf;

	if (MSG_IS_NEW(flags))
		unread_pix = new_pixbuf;
	else if (MSG_IS_UNREAD(flags))
		unread_pix = unread_pixbuf;
	else if (MSG_IS_REPLIED(flags))
		unread_pix = replied_pixbuf;
	else if (MSG_IS_FORWARDED(flags))
		unread_pix = forwarded_pixbuf;

	if (MSG_IS_MIME(flags)) {
		mime_pix = clip_pixbuf;
	}
	if (MSG_IS_MIME_HTML(flags)) {
		mime_pix = html_pixbuf;
	}

	if (prefs_common.bold_unread) {
		if (MSG_IS_UNREAD(flags))
			weight = PANGO_WEIGHT_BOLD;
		else if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store),
						       iter)) {
			GtkTreePath *path;

			path = gtk_tree_model_get_path
				(GTK_TREE_MODEL(store), iter);
			if (!gtk_tree_view_row_expanded
				(GTK_TREE_VIEW(summaryview->treeview), path) &&
			    summary_have_unread_children(summaryview, iter))
				weight = PANGO_WEIGHT_BOLD;
			gtk_tree_path_free(path);
		}
	}

	color_val = MSG_GET_COLORLABEL_VALUE(flags);
	if (color_val != 0) {
		color = colorlabel_get_color(color_val - 1);
		foreground = &color;
	}

	gtk_tree_store_set(store, iter,
			   S_COL_MARK, mark_pix,
			   S_COL_UNREAD, unread_pix,
			   S_COL_MIME, mime_pix,
			   S_COL_SUBJECT, subject_s ? subject_s :
			   		  msginfo->subject && *msginfo->subject ? msginfo->subject :
					  _("(No Subject)"),
			   S_COL_FROM, disp_from ? disp_from : _("(No From)"),
			   S_COL_DATE, date_s,
			   S_COL_SIZE, to_human_readable(msginfo->size),
			   S_COL_NUMBER, msginfo->msgnum,
			   S_COL_TO, to_s ? to_s : "",

			   S_COL_MSG_INFO, msginfo,

			   S_COL_LABEL, color_val,

			   S_COL_FOREGROUND, foreground,
			   S_COL_BOLD, weight,
			   -1);

	if (to_s)
		g_free(to_s);
	if (subject_s)
		g_free(subject_s);
	if (sw_from_s)
		g_free(sw_from_s);
}

static void summary_insert_gnode(SummaryView *summaryview, GtkTreeStore *store,
				 GtkTreeIter *iter, GtkTreeIter *parent,
				 GtkTreeIter *sibling, GNode *gnode)
{
	MsgInfo *msginfo = (MsgInfo *)gnode->data;

	if (parent && !sibling)
		gtk_tree_store_append(store, iter, parent);
	else
		gtk_tree_store_insert_after(store, iter, parent, sibling);

	summary_set_row(summaryview, iter, msginfo);

	if (!parent) {
		guint tdate;

		tdate = procmsg_get_thread_date(gnode);
		gtk_tree_store_set(store, iter, S_COL_TDATE, tdate, -1);
	}

	for (gnode = gnode->children; gnode != NULL; gnode = gnode->next) {
		GtkTreeIter child;

		summary_insert_gnode(summaryview, store, &child, iter, NULL,
				     gnode);
	}
}

static void summary_insert_gnode_before(SummaryView *summaryview,
					GtkTreeStore *store,
					GtkTreeIter *iter, GtkTreeIter *parent,
					GtkTreeIter *sibling, GNode *gnode)
{
	MsgInfo *msginfo = (MsgInfo *)gnode->data;

	gtk_tree_store_insert_before(store, iter, parent, sibling);

	summary_set_row(summaryview, iter, msginfo);

	if (!parent) {
		guint tdate;

		tdate = procmsg_get_thread_date(gnode);
		gtk_tree_store_set(store, iter, S_COL_TDATE, tdate, -1);
	}

	for (gnode = gnode->children; gnode != NULL; gnode = gnode->next) {
		GtkTreeIter child;

		summary_insert_gnode_before(summaryview, store, &child, iter,
					    NULL, gnode);
	}
}

static void summary_set_tree_model_from_list(SummaryView *summaryview,
					     GSList *mlist)
{
	GtkTreeStore *store = GTK_TREE_STORE(summaryview->store);
	GtkTreeIter iter;
	MsgInfo *msginfo;
	GSList *cur;

	debug_print(_("\tSetting summary from message data..."));
	STATUSBAR_PUSH(summaryview->mainwin,
		       _("Setting summary from message data..."));
	gdk_flush();

	/* temporarily remove the model for speed up */
	gtk_tree_view_set_model(GTK_TREE_VIEW(summaryview->treeview), NULL);

	if (summaryview->folder_item->threaded) {
		GNode *root, *gnode;

		root = procmsg_get_thread_tree(mlist);

		for (gnode = root->children; gnode != NULL;
		     gnode = gnode->next) {
			summary_insert_gnode
				(summaryview, store, &iter, NULL, NULL, gnode);
			if (gnode->children && !prefs_common.expand_thread &&
			    prefs_common.bold_unread &&
			    summary_have_unread_children(summaryview, &iter)) {
				gtk_tree_store_set(store, &iter,
						   S_COL_BOLD,
						   PANGO_WEIGHT_BOLD, -1);
			}
		}

		g_node_destroy(root);

		for (cur = mlist; cur != NULL; cur = cur->next) {
			msginfo = (MsgInfo *)cur->data;

			if (MSG_IS_DELETED(msginfo->flags))
				summaryview->deleted++;
			if (MSG_IS_MOVE(msginfo->flags))
				summaryview->moved++;
			if (MSG_IS_COPY(msginfo->flags))
				summaryview->copied++;
			summaryview->total_size += msginfo->size;
		}
	} else {
		GSList *rev_mlist;
		GtkTreeIter iter;

		rev_mlist = g_slist_reverse(g_slist_copy(mlist));
		for (cur = rev_mlist; cur != NULL; cur = cur->next) {
			msginfo = (MsgInfo *)cur->data;

			gtk_tree_store_prepend(store, &iter, NULL);
			summary_set_row(summaryview, &iter, msginfo);

			if (MSG_IS_DELETED(msginfo->flags))
				summaryview->deleted++;
			if (MSG_IS_MOVE(msginfo->flags))
				summaryview->moved++;
			if (MSG_IS_COPY(msginfo->flags))
				summaryview->copied++;
			summaryview->total_size += msginfo->size;
		}
		g_slist_free(rev_mlist);
	}

	gtk_tree_view_set_model(GTK_TREE_VIEW(summaryview->treeview),
				GTK_TREE_MODEL(store));

	if (summaryview->folder_item->threaded && prefs_common.expand_thread)
		gtk_tree_view_expand_all
			(GTK_TREE_VIEW(summaryview->treeview));

	if (summaryview->folder_item->sort_key != SORT_BY_NONE) {
		summary_sort(summaryview, summaryview->folder_item->sort_key,
			     summaryview->folder_item->sort_type);
	}

	debug_print(_("done.\n"));
	STATUSBAR_POP(summaryview->mainwin);
}

struct wcachefp
{
	FILE *cache_fp;
	FILE *mark_fp;
};

gint summary_write_cache(SummaryView *summaryview)
{
	struct wcachefp fps;
	FolderItem *item;
	gchar *buf;
	GSList *cur;

	item = summaryview->folder_item;
	if (!item || !item->path)
		return -1;
	if (item->mark_queue)
		item->mark_dirty = TRUE;
	if (!item->cache_dirty && !item->mark_dirty)
		return 0;

	if (item->cache_dirty) {
		fps.cache_fp = procmsg_open_cache_file(item, DATA_WRITE);
		if (fps.cache_fp == NULL)
			return -1;
		item->mark_dirty = TRUE;
	} else
		fps.cache_fp = NULL;

	if (item->mark_dirty && item->stype != F_VIRTUAL) {
		fps.mark_fp = procmsg_open_mark_file(item, DATA_WRITE);
		if (fps.mark_fp == NULL) {
			if (fps.cache_fp)
				fclose(fps.cache_fp);
			return -1;
		}
	} else
		fps.mark_fp = NULL;

	if (item->cache_dirty) {
		buf = g_strdup_printf(_("Writing summary cache (%s)..."),
				      item->path);
		debug_print("%s", buf);
		STATUSBAR_PUSH(summaryview->mainwin, buf);
		gdk_flush();
		g_free(buf);
	}

	for (cur = summaryview->all_mlist; cur != NULL; cur = cur->next) {
		MsgInfo *msginfo = (MsgInfo *)cur->data;

		if (msginfo->folder && msginfo->folder->mark_queue != NULL) {
			MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_NEW);
		}
		if (fps.cache_fp)
			procmsg_write_cache(msginfo, fps.cache_fp);
		if (fps.mark_fp)
			procmsg_write_flags(msginfo, fps.mark_fp);
	}

	if (item->cache_queue)
		procmsg_flush_cache_queue(item, fps.cache_fp);
	if (item->mark_queue)
		procmsg_flush_mark_queue(item, fps.mark_fp);

	item->unmarked_num = 0;

	if (fps.cache_fp)
		fclose(fps.cache_fp);
	if (fps.mark_fp)
		fclose(fps.mark_fp);

	if (item->stype == F_VIRTUAL) {
		GSList *mlist;

		mlist = summary_get_changed_msg_list(summaryview);
		if (mlist) {
			procmsg_write_flags_for_multiple_folders(mlist);
			g_slist_free(mlist);
			folderview_update_all_updated(FALSE);
		}
	}

	debug_print(_("done.\n"));

	if (item->cache_dirty) {
		STATUSBAR_POP(summaryview->mainwin);
	}

	item->cache_dirty = item->mark_dirty = FALSE;

	return 0;
}

static gboolean summary_row_is_displayed(SummaryView *summaryview,
					 GtkTreeIter *iter)
{
	GtkTreePath *disp_path, *path;
	gint ret;

	if (!summaryview->displayed || !iter)
		return FALSE;

	disp_path = gtk_tree_row_reference_get_path(summaryview->displayed);
	if (!disp_path)
		return FALSE;

	path = gtk_tree_model_get_path(GTK_TREE_MODEL(summaryview->store),
				       iter);
	if (!path) {
		gtk_tree_path_free(disp_path);
		return FALSE;
	}

	ret = gtk_tree_path_compare(disp_path, path);
	gtk_tree_path_free(path);
	gtk_tree_path_free(disp_path);

	return (ret == 0);
}

static void summary_display_msg(SummaryView *summaryview, GtkTreeIter *iter)
{
	summary_display_msg_full(summaryview, iter, FALSE, FALSE, FALSE);
}

static void summary_display_msg_full(SummaryView *summaryview,
				     GtkTreeIter *iter,
				     gboolean new_window, gboolean all_headers,
				     gboolean redisplay)
{
	GtkTreePath *path;
	MsgInfo *msginfo = NULL;
	gint val;
	gboolean do_mark_read = FALSE;

	g_return_if_fail(iter != NULL);

	if (!new_window && !redisplay &&
	    summary_row_is_displayed(summaryview, iter))
		return;

	if (summary_is_read_locked(summaryview)) return;
	summary_lock(summaryview);

	STATUSBAR_POP(summaryview->mainwin);

	gtk_tree_model_get(GTK_TREE_MODEL(summaryview->store), iter,
			   S_COL_MSG_INFO, &msginfo, -1);

	do_mark_read = prefs_common.always_mark_read_on_show_msg;

	if (new_window) {
		MessageView *msgview;

		msgview = messageview_create_with_new_window();
		val = messageview_show(msgview, msginfo, all_headers);
		do_mark_read = TRUE;
	} else {
		MessageView *msgview = summaryview->messageview;
		gboolean prev_mimeview;

		if (!messageview_is_visible(msgview)) {
			main_window_toggle_message_view(summaryview->mainwin);
			GTK_EVENTS_FLUSH();
		}
		prev_mimeview =
			messageview_get_selected_mime_part(msgview) != NULL;

		val = messageview_show(msgview, msginfo, all_headers);
		if (prev_mimeview &&
		    !messageview_get_selected_mime_part(msgview))
			gtk_widget_grab_focus(summaryview->treeview);
	}

	if (val == 0 && do_mark_read) {
		if (MSG_IS_NEW(msginfo->flags) ||
		    MSG_IS_UNREAD(msginfo->flags)) {
			summary_mark_row_as_read(summaryview, iter);
			if (MSG_IS_IMAP(msginfo->flags))
				imap_msg_unset_perm_flags
					(msginfo, MSG_NEW | MSG_UNREAD);
			summary_set_row(summaryview, iter, msginfo);
			summary_status_show(summaryview);
		}
	}

	path = gtk_tree_model_get_path
		(GTK_TREE_MODEL(summaryview->store), iter);
	if (!new_window) {
		gtk_tree_row_reference_free(summaryview->displayed);
		summaryview->displayed =
			gtk_tree_row_reference_new
				(GTK_TREE_MODEL(summaryview->store), path);
	}
	gtkut_tree_view_scroll_to_cell
		(GTK_TREE_VIEW(summaryview->treeview), path,
		 !summaryview->on_button_press);
	gtk_tree_path_free(path);

	if (summaryview->folder_item->sort_key == SORT_BY_UNREAD)
		summary_selection_list_free(summaryview);

	summary_set_menu_sensitive(summaryview);
	main_window_set_toolbar_sensitive(summaryview->mainwin);

	trayicon_set_tooltip(NULL);
	trayicon_set_notify(FALSE);

	statusbar_pop_all();

	summary_unlock(summaryview);
}

void summary_display_msg_selected(SummaryView *summaryview,
				  gboolean new_window, gboolean all_headers)
{
	GtkTreeIter iter;

	if (summary_is_read_locked(summaryview)) return;

	if (summaryview->selected) {
		if (gtkut_tree_row_reference_get_iter
			(GTK_TREE_MODEL(summaryview->store),
			 summaryview->selected, &iter)) {
			summary_display_msg_full(summaryview, &iter,
						 new_window, all_headers, TRUE);
		}
	}
}

void summary_redisplay_msg(SummaryView *summaryview)
{
	GtkTreeIter iter;

	if (summaryview->displayed) {
		if (gtkut_tree_row_reference_get_iter
			(GTK_TREE_MODEL(summaryview->store),
			 summaryview->displayed, &iter)) {
			summary_display_msg_full(summaryview, &iter,
						 FALSE, FALSE, TRUE);
		}
	}
}

void summary_open_msg(SummaryView *summaryview)
{
	summary_display_msg_selected(summaryview, TRUE, FALSE);
}

static void summary_activate_selected(SummaryView *summaryview)
{
	if (!summaryview->folder_item)
		return;

	if (FOLDER_ITEM_IS_SENT_FOLDER(summaryview->folder_item))
		summary_reedit(summaryview);
	else
		summary_open_msg(summaryview);

	summaryview->display_msg = FALSE;
}

void summary_view_source(SummaryView *summaryview)
{
	GtkTreeIter iter;
	MsgInfo *msginfo;
	SourceWindow *srcwin;

	if (summaryview->selected) {
		if (gtkut_tree_row_reference_get_iter
			(GTK_TREE_MODEL(summaryview->store),
			 summaryview->selected, &iter)) {
			GET_MSG_INFO(msginfo, &iter);

			srcwin = source_window_create();
			source_window_show_msg(srcwin, msginfo);
			source_window_show(srcwin);
		}
	}
}

void summary_reedit(SummaryView *summaryview)
{
	GtkTreeIter iter;
	MsgInfo *msginfo;

	if (!summaryview->selected) return;
	if (!FOLDER_ITEM_IS_SENT_FOLDER(summaryview->folder_item)) return;

	if (gtkut_tree_row_reference_get_iter
		(GTK_TREE_MODEL(summaryview->store),
		 summaryview->selected, &iter)) {
		GET_MSG_INFO(msginfo, &iter);
		compose_reedit(msginfo);
	}
}

gboolean summary_step(SummaryView *summaryview, GtkScrollType type)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter;

	if (summary_is_read_locked(summaryview)) return FALSE;

	if (!gtkut_tree_row_reference_get_iter
		(model, summaryview->selected, &iter))
		return FALSE;

	if (type == GTK_SCROLL_STEP_FORWARD) {
		if (!gtkut_tree_model_next(model, &iter))
			return FALSE;
	} else {
		if (!gtkut_tree_model_prev(model, &iter))
			return FALSE;
	}

	summary_select_row(summaryview, &iter,
			   messageview_is_visible(summaryview->messageview),
			   FALSE);

	return TRUE;
}

void summary_toggle_view(SummaryView *summaryview)
{
	if (!messageview_is_visible(summaryview->messageview) &&
	    summaryview->selected) {
		summary_display_msg_selected(summaryview, FALSE, FALSE);
		summary_mark_displayed_read(summaryview, NULL);
	} else
		main_window_toggle_message_view(summaryview->mainwin);
}

void summary_update_selected_rows(SummaryView *summaryview)
{
	GList *rows, *cur;
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter;
	GtkTreePath *path;

	rows = summary_get_selected_rows(summaryview);
	for (cur = rows; cur != NULL; cur = cur->next) {
		path = (GtkTreePath *)cur->data;
		gtk_tree_model_get_iter(model, &iter, path);
		summary_set_row(summaryview, &iter, NULL);
	}
}

void summary_update_by_msgnum(SummaryView *summaryview, guint msgnum)
{
	GtkTreeIter iter;

	if (summary_find_msg_by_msgnum(summaryview, msgnum, &iter))
		summary_set_row(summaryview, &iter, NULL);
}

static void summary_mark_row(SummaryView *summaryview, GtkTreeIter *iter)
{
	MsgInfo *msginfo = NULL;

	GET_MSG_INFO(msginfo, iter);

	msginfo->to_folder = NULL;
	if (MSG_IS_DELETED(msginfo->flags)) {
		summaryview->deleted--;
		MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_DELETED);
	}
	if (MSG_IS_MOVE(msginfo->flags))
		summaryview->moved--;
	if (MSG_IS_COPY(msginfo->flags))
		summaryview->copied--;
	MSG_UNSET_TMP_FLAGS(msginfo->flags, MSG_MOVE | MSG_COPY);
	MSG_SET_PERM_FLAGS(msginfo->flags, MSG_MARKED);
	MSG_SET_TMP_FLAGS(msginfo->flags, MSG_FLAG_CHANGED);
	summaryview->folder_item->mark_dirty = TRUE;
	summary_set_row(summaryview, iter, msginfo);

	debug_print(_("Message %d is marked\n"), msginfo->msgnum);
}

void summary_mark(SummaryView *summaryview)
{
	GList *rows, *cur;
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter;
	FolderSortKey sort_key = SORT_BY_NONE;
	FolderSortType sort_type = SORT_ASCENDING;

	if (FOLDER_TYPE(summaryview->folder_item->folder) == F_IMAP &&
	    summary_is_read_locked(summaryview))
		return;

	summary_lock(summaryview);
	SORT_BLOCK(SORT_BY_MARK);

	rows = summary_get_selected_rows(summaryview);
	for (cur = rows; cur != NULL; cur = cur->next) {
		GtkTreePath *path = (GtkTreePath *)cur->data;
		gtk_tree_model_get_iter(model, &iter, path);
		summary_mark_row(summaryview, &iter);
	}

	if (FOLDER_TYPE(summaryview->folder_item->folder) == F_IMAP) {
		GSList *msglist;

		msglist = summary_get_selected_msg_list(summaryview);
		imap_msg_list_set_perm_flags(msglist, MSG_MARKED);
		g_slist_free(msglist);
	}

	SORT_UNBLOCK(SORT_BY_MARK);
	summary_unlock(summaryview);

	summary_status_show(summaryview);
}

static void summary_mark_row_as_read(SummaryView *summaryview,
				     GtkTreeIter *iter)
{
	MsgInfo *msginfo = NULL;

	GET_MSG_INFO(msginfo, iter);

	if (MSG_IS_NEW(msginfo->flags)) {
		if (summaryview->folder_item->new > 0)
			summaryview->folder_item->new--;
		if (summaryview->on_filter && summaryview->flt_new > 0)
			summaryview->flt_new--;
		inc_block_notify(TRUE);
	}
	if (MSG_IS_UNREAD(msginfo->flags)) {
		if (summaryview->folder_item->unread > 0)
			summaryview->folder_item->unread--;
		if (summaryview->on_filter && summaryview->flt_unread > 0)
			summaryview->flt_unread--;
	}

	if (summaryview->folder_item->stype == F_VIRTUAL) {
		if (MSG_IS_NEW(msginfo->flags) && msginfo->folder->new > 0)
			msginfo->folder->new--;
		if (MSG_IS_UNREAD(msginfo->flags) &&
		    msginfo->folder->unread > 0)
			msginfo->folder->unread--;
		folderview_update_item(msginfo->folder, FALSE);
	}

	if (MSG_IS_NEW(msginfo->flags) || MSG_IS_UNREAD(msginfo->flags)) {
		MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_NEW | MSG_UNREAD);
		MSG_SET_TMP_FLAGS(msginfo->flags, MSG_FLAG_CHANGED);
		summaryview->folder_item->mark_dirty = TRUE;
		summary_set_row(summaryview, iter, msginfo);
		debug_print(_("Message %d is marked as being read\n"),
			    msginfo->msgnum);
	}
}

void summary_mark_as_read(SummaryView *summaryview)
{
	GList *rows, *cur;
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter;
	FolderSortKey sort_key = SORT_BY_NONE;
	FolderSortType sort_type = SORT_ASCENDING;

	if (FOLDER_TYPE(summaryview->folder_item->folder) == F_IMAP &&
	    summary_is_read_locked(summaryview))
		return;

	summary_lock(summaryview);
	SORT_BLOCK(SORT_BY_UNREAD);

	rows = summary_get_selected_rows(summaryview);

	for (cur = rows; cur != NULL; cur = cur->next) {
		GtkTreePath *path = (GtkTreePath *)cur->data;

		gtk_tree_model_get_iter(model, &iter, path);
		summary_mark_row_as_read(summaryview, &iter);
	}

	if (FOLDER_TYPE(summaryview->folder_item->folder) == F_IMAP) {
		GSList *msglist;

		msglist = summary_get_selected_msg_list(summaryview);
		imap_msg_list_unset_perm_flags(msglist, MSG_NEW | MSG_UNREAD);
		g_slist_free(msglist);
	}

	SORT_UNBLOCK(SORT_BY_UNREAD);
	summary_unlock(summaryview);

	trayicon_set_tooltip(NULL);
	trayicon_set_notify(FALSE);
	summary_status_show(summaryview);
}

static gboolean prepend_thread_rows_func(GtkTreeModel *model, GtkTreePath *path,
					 GtkTreeIter *iter, gpointer data)
{
	GSList **thr_rows = (GSList **)data;
	GtkTreePath *top_path;

	top_path = gtk_tree_path_copy(path);
	*thr_rows = g_slist_prepend(*thr_rows, top_path);
	return FALSE;
}

void summary_mark_thread_as_read(SummaryView *summaryview)
{
	GList *rows, *cur;
	GSList *thr_rows = NULL, *top_rows = NULL, *s_cur;
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeView *treeview = GTK_TREE_VIEW(summaryview->treeview);
	GtkTreeIter iter, parent;
	GtkTreePath *path, *top_path;
	GHashTable *row_table;
	MsgInfo *msginfo;
	GSList *msglist = NULL;
	FolderSortKey sort_key = SORT_BY_NONE;
	FolderSortType sort_type = SORT_ASCENDING;

	if (FOLDER_TYPE(summaryview->folder_item->folder) == F_IMAP &&
	    summary_is_read_locked(summaryview))
		return;

	summary_lock(summaryview);
	SORT_BLOCK(SORT_BY_UNREAD);

	rows = summary_get_selected_rows(summaryview);

	row_table = g_hash_table_new(NULL, NULL);

	for (cur = rows; cur != NULL; cur = cur->next) {
		path = (GtkTreePath *)cur->data;
		gtk_tree_model_get_iter(model, &iter, path);
		while (gtk_tree_model_iter_parent(model, &parent, &iter))
			iter = parent;
		gtk_tree_model_get(model, &iter, S_COL_MSG_INFO, &msginfo, -1);
		if (!g_hash_table_lookup(row_table, msginfo)) {
			g_hash_table_insert(row_table, msginfo,
					    GINT_TO_POINTER(1));
			top_path = gtk_tree_model_get_path(model, &iter);
			top_rows = g_slist_prepend(top_rows, top_path);
			gtkut_tree_model_foreach(model, &iter,
						 prepend_thread_rows_func,
						 &thr_rows);
		}
	}
	top_rows = g_slist_reverse(top_rows);
	thr_rows = g_slist_reverse(thr_rows);

	g_hash_table_destroy(row_table);

	for (s_cur = thr_rows; s_cur != NULL; s_cur = s_cur->next) {
		path = (GtkTreePath *)s_cur->data;
		gtk_tree_model_get_iter(model, &iter, path);
		summary_mark_row_as_read(summaryview, &iter);
		gtk_tree_model_get(model, &iter, S_COL_MSG_INFO, &msginfo, -1);
		msglist = g_slist_prepend(msglist, msginfo);
	}

	if (prefs_common.bold_unread) {
		for (s_cur = top_rows; s_cur != NULL; s_cur = s_cur->next) {
			path = (GtkTreePath *)s_cur->data;
			gtk_tree_model_get_iter(model, &iter, path);
			if (gtk_tree_model_iter_has_child(model, &iter) &&
			    !gtk_tree_view_row_expanded(treeview, path) &&
			    !summary_have_unread_children(summaryview, &iter)) {
				gtk_tree_store_set(GTK_TREE_STORE(model), &iter,
						   S_COL_BOLD,
						   PANGO_WEIGHT_NORMAL, -1);
			}
		}
	}
	msglist = g_slist_reverse(msglist);

	if (FOLDER_TYPE(summaryview->folder_item->folder) == F_IMAP) {
		imap_msg_list_unset_perm_flags(msglist, MSG_NEW | MSG_UNREAD);
	}

	g_slist_free(msglist);
	g_slist_foreach(top_rows, (GFunc)gtk_tree_path_free, NULL);
	g_slist_free(top_rows);
	g_slist_foreach(thr_rows, (GFunc)gtk_tree_path_free, NULL);
	g_slist_free(thr_rows);

	SORT_UNBLOCK(SORT_BY_UNREAD);
	summary_unlock(summaryview);

	trayicon_set_tooltip(NULL);
	trayicon_set_notify(FALSE);
	summary_status_show(summaryview);
}

void summary_mark_all_read(SummaryView *summaryview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter;
	gboolean valid;
	FolderSortKey sort_key = SORT_BY_NONE;
	FolderSortType sort_type = SORT_ASCENDING;

	if (FOLDER_TYPE(summaryview->folder_item->folder) == F_IMAP &&
	    summary_is_read_locked(summaryview))
		return;

	summary_lock(summaryview);
	SORT_BLOCK(SORT_BY_UNREAD);

	if (FOLDER_TYPE(summaryview->folder_item->folder) == F_IMAP) {
		GSList *msglist;
		msglist = summary_get_flagged_msg_list(summaryview,
						       MSG_NEW | MSG_UNREAD);
		imap_msg_list_unset_perm_flags(msglist, MSG_NEW | MSG_UNREAD);
		g_slist_free(msglist);
	}

	valid = gtk_tree_model_get_iter_first(model, &iter);
	while (valid) {
		summary_mark_row_as_read(summaryview, &iter);
		if (prefs_common.bold_unread) {
			if (gtk_tree_model_iter_has_child(model, &iter)) {
				GtkTreePath *path;

				path = gtk_tree_model_get_path(model, &iter);
				if (!gtk_tree_view_row_expanded
					(GTK_TREE_VIEW(summaryview->treeview),
					 path))
					gtk_tree_store_set
						(GTK_TREE_STORE(model), &iter,
						 S_COL_BOLD,
						 PANGO_WEIGHT_NORMAL, -1);
				gtk_tree_path_free(path);
			}
		}
		valid = gtkut_tree_model_next(model, &iter);
	}

	SORT_UNBLOCK(SORT_BY_UNREAD);
	summary_unlock(summaryview);

	trayicon_set_tooltip(NULL);
	trayicon_set_notify(FALSE);
	summary_status_show(summaryview);
}

static void summary_mark_row_as_unread(SummaryView *summaryview,
				       GtkTreeIter *iter)
{
	MsgInfo *msginfo = NULL;

	GET_MSG_INFO(msginfo, iter);

	if (MSG_IS_DELETED(msginfo->flags)) {
		msginfo->to_folder = NULL;
		MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_DELETED);
		summaryview->deleted--;
		summaryview->folder_item->mark_dirty = TRUE;
	}
	MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_REPLIED | MSG_FORWARDED);
	if (!MSG_IS_UNREAD(msginfo->flags)) {
		MSG_SET_PERM_FLAGS(msginfo->flags, MSG_UNREAD);
		summaryview->folder_item->unread++;
		if (summaryview->on_filter)
			summaryview->flt_unread++;
		summaryview->folder_item->mark_dirty = TRUE;
		if (summaryview->folder_item->stype == F_VIRTUAL) {
			msginfo->folder->unread++;
			folderview_update_item(msginfo->folder, FALSE);
		}
		debug_print(_("Message %d is marked as unread\n"),
			    msginfo->msgnum);
	}
	MSG_SET_TMP_FLAGS(msginfo->flags, MSG_FLAG_CHANGED);
	summary_set_row(summaryview, iter, msginfo);
}

void summary_mark_as_unread(SummaryView *summaryview)
{
	GList *rows, *cur;
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter;
	FolderSortKey sort_key = SORT_BY_NONE;
	FolderSortType sort_type = SORT_ASCENDING;

	if (FOLDER_TYPE(summaryview->folder_item->folder) == F_IMAP &&
	    summary_is_read_locked(summaryview))
		return;

	summary_lock(summaryview);
	SORT_BLOCK(SORT_BY_UNREAD);

	rows = summary_get_selected_rows(summaryview);
	for (cur = rows; cur != NULL; cur = cur->next) {
		GtkTreePath *path = (GtkTreePath *)cur->data;

		gtk_tree_model_get_iter(model, &iter, path);
		summary_mark_row_as_unread(summaryview, &iter);
	}

	if (FOLDER_TYPE(summaryview->folder_item->folder) == F_IMAP) {
		GSList *msglist;
		msglist = summary_get_selected_msg_list(summaryview);
		imap_msg_list_unset_perm_flags(msglist, MSG_REPLIED);
		imap_msg_list_set_perm_flags(msglist, MSG_UNREAD);
		g_slist_free(msglist);
	}

	SORT_UNBLOCK(SORT_BY_UNREAD);
	summary_unlock(summaryview);

	summary_status_show(summaryview);
}

static void summary_delete_row(SummaryView *summaryview, GtkTreeIter *iter)
{
	MsgInfo *msginfo = NULL;

	GET_MSG_INFO(msginfo, iter);

	if (MSG_IS_DELETED(msginfo->flags)) return;

	msginfo->to_folder = NULL;
	if (MSG_IS_MOVE(msginfo->flags)) {
		summaryview->moved--;
		if (summaryview->on_filter)
			summaryview->flt_moved--;
	}
	if (MSG_IS_COPY(msginfo->flags)) {
		summaryview->copied--;
		if (summaryview->on_filter)
			summaryview->flt_copied--;
	}
	MSG_UNSET_TMP_FLAGS(msginfo->flags, MSG_MOVE | MSG_COPY);
	MSG_SET_PERM_FLAGS(msginfo->flags, MSG_DELETED);
	MSG_SET_TMP_FLAGS(msginfo->flags, MSG_FLAG_CHANGED);
	summaryview->deleted++;
	if (summaryview->on_filter)
		summaryview->flt_deleted++;
	summaryview->folder_item->mark_dirty = TRUE;

	if (!prefs_common.immediate_exec && summaryview->tmp_flag == 0)
		summary_set_row(summaryview, iter, msginfo);

	debug_print(_("Message %s/%d is set to delete\n"),
		    msginfo->folder->path, msginfo->msgnum);
}

static gboolean summary_delete_foreach_func(GtkTreeModel *model,
					    GtkTreePath *path,
					    GtkTreeIter *iter, gpointer data)
{
	summary_delete_row((SummaryView *)data, iter);
	return FALSE;
}

void summary_delete(SummaryView *summaryview)
{
	FolderItem *item = summaryview->folder_item;
	GList *rows, *cur;
	GtkTreeIter last_sel, next;
	GtkTreeView *treeview = GTK_TREE_VIEW(summaryview->treeview);
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	gboolean is_trash;

	if (!item || FOLDER_TYPE(item->folder) == F_NEWS) return;

	if (summary_is_locked(summaryview)) return;

	/* if current folder is trash, ask for confirmation */
	is_trash = folder_item_is_trash(item);
	if (is_trash) {
		AlertValue aval;

		aval = alertpanel(_("Delete message(s)"),
				  _("Do you really want to delete message(s) from the trash?"),
				  GTK_STOCK_YES, GTK_STOCK_NO, NULL);
		if (aval != G_ALERTDEFAULT) return;
	}

	rows = summary_get_selected_rows(summaryview);
	if (!rows)
		return;

	summaryview->tmp_flag = is_trash ? 1 : 0;

	/* next code sets current row focus right. We need to find a row
	 * that is not deleted. */
	for (cur = rows; cur != NULL; cur = cur->next) {
		GtkTreePath *path = (GtkTreePath *)cur->data;

		gtk_tree_model_get_iter(model, &last_sel, path);
		if (gtk_tree_model_iter_has_child(model, &last_sel) &&
		    !gtk_tree_view_row_expanded(treeview, path)) {
			gtkut_tree_model_foreach
				(model, &last_sel, summary_delete_foreach_func,
				 summaryview);
		} else
			summary_delete_row(summaryview, &last_sel);
	}

	summaryview->tmp_flag = 0;

	if (prefs_common.immediate_exec || is_trash) {
		summary_execute(summaryview);
	} else {
		if (summary_find_nearest_msg(summaryview, &next, &last_sel)) {
			summary_select_row
				(summaryview, &next,
				 messageview_is_visible
					(summaryview->messageview),
				 FALSE);
		}
		summary_status_show(summaryview);
	}
}

static gboolean summary_delete_duplicated_func(GtkTreeModel *model,
					       GtkTreePath *path,
					       GtkTreeIter *iter,
					       gpointer data)
{
	SummaryView *summaryview = (SummaryView *)data;
	MsgInfo *msginfo;
	GtkTreeIter *found;
	GtkTreePath *found_path;

	gtk_tree_model_get(model, iter, S_COL_MSG_INFO, &msginfo, -1);

	if (!msginfo || !msginfo->msgid || !*msginfo->msgid)
		return FALSE;

	found = g_hash_table_lookup(summaryview->msgid_table, msginfo->msgid);

	if (found) {
		found_path = gtk_tree_model_get_path(model, found);
		if (gtk_tree_path_compare(path, found_path) != 0)
			summary_delete_row(summaryview, iter);
		gtk_tree_path_free(found_path);
	}

	return FALSE;
}

void summary_delete_duplicated(SummaryView *summaryview)
{
	if (!summaryview->folder_item ||
	    FOLDER_TYPE(summaryview->folder_item->folder) == F_NEWS) return;
	if (folder_item_is_trash(summaryview->folder_item)) return;

	if (summary_is_locked(summaryview)) return;

	main_window_cursor_wait(summaryview->mainwin);
	debug_print("Deleting duplicated messages...");
	STATUSBAR_PUSH(summaryview->mainwin,
		       _("Deleting duplicated messages..."));

	summary_msgid_table_create(summaryview);

	gtk_tree_model_foreach(GTK_TREE_MODEL(summaryview->store),
			       summary_delete_duplicated_func, summaryview);

	summary_msgid_table_destroy(summaryview);

	if (prefs_common.immediate_exec)
		summary_execute(summaryview);
	else
		summary_status_show(summaryview);

	debug_print("done.\n");
	STATUSBAR_POP(summaryview->mainwin);
	main_window_cursor_normal(summaryview->mainwin);
}

static void summary_unmark_row(SummaryView *summaryview, GtkTreeIter *iter)
{
	MsgInfo *msginfo = NULL;

	GET_MSG_INFO(msginfo, iter);

	msginfo->to_folder = NULL;
	if (MSG_IS_DELETED(msginfo->flags)) {
		summaryview->deleted--;
		if (summaryview->on_filter)
			summaryview->flt_deleted--;
	}
	if (MSG_IS_MOVE(msginfo->flags)) {
		summaryview->moved--;
		if (summaryview->on_filter)
			summaryview->flt_moved--;
	}
	if (MSG_IS_COPY(msginfo->flags)) {
		summaryview->copied--;
		if (summaryview->on_filter)
			summaryview->flt_copied--;
	}
	MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_MARKED | MSG_DELETED);
	MSG_UNSET_TMP_FLAGS(msginfo->flags, MSG_MOVE | MSG_COPY);
	MSG_SET_TMP_FLAGS(msginfo->flags, MSG_FLAG_CHANGED);
	summaryview->folder_item->mark_dirty = TRUE;
	summary_set_row(summaryview, iter, msginfo);

	debug_print(_("Message %s/%d is unmarked\n"),
		    msginfo->folder->path, msginfo->msgnum);
}

void summary_unmark(SummaryView *summaryview)
{
	GList *rows, *cur;
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter;
	FolderSortKey sort_key = SORT_BY_NONE;
	FolderSortType sort_type = SORT_ASCENDING;

	if (FOLDER_TYPE(summaryview->folder_item->folder) == F_IMAP &&
	    summary_is_read_locked(summaryview))
		return;

	summary_lock(summaryview);
	SORT_BLOCK(SORT_BY_MARK);

	rows = summary_get_selected_rows(summaryview);
	for (cur = rows; cur != NULL; cur = cur->next) {
		GtkTreePath *path = (GtkTreePath *)cur->data;
		gtk_tree_model_get_iter(model, &iter, path);
		summary_unmark_row(summaryview, &iter);
	}

	if (FOLDER_TYPE(summaryview->folder_item->folder) == F_IMAP) {
		GSList *msglist;

		msglist = summary_get_selected_msg_list(summaryview);
		imap_msg_list_unset_perm_flags(msglist, MSG_MARKED);
		g_slist_free(msglist);
	}

	SORT_UNBLOCK(SORT_BY_MARK);
	summary_unlock(summaryview);

	summary_status_show(summaryview);
}

static void summary_move_row_to(SummaryView *summaryview, GtkTreeIter *iter,
				FolderItem *to_folder)
{
	MsgInfo *msginfo;

	g_return_if_fail(to_folder != NULL);
	if (to_folder->stype == F_QUEUE || to_folder->stype == F_VIRTUAL)
		return;

	GET_MSG_INFO(msginfo, iter);

	msginfo->to_folder = to_folder;
	if (MSG_IS_DELETED(msginfo->flags)) {
		summaryview->deleted--;
		if (summaryview->on_filter)
			summaryview->flt_deleted--;
		MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_DELETED);
		MSG_SET_TMP_FLAGS(msginfo->flags, MSG_FLAG_CHANGED);
	}
	MSG_UNSET_TMP_FLAGS(msginfo->flags, MSG_COPY);
	if (!MSG_IS_MOVE(msginfo->flags)) {
		MSG_SET_TMP_FLAGS(msginfo->flags, MSG_MOVE);
		summaryview->moved++;
		if (summaryview->on_filter)
			summaryview->flt_moved++;
	}
	summaryview->folder_item->mark_dirty = TRUE;
	if (!prefs_common.immediate_exec)
		summary_set_row(summaryview, iter, msginfo);

	debug_print(_("Message %d is set to move to %s\n"),
		    msginfo->msgnum, to_folder->path);
}

static gboolean summary_move_foreach_func(GtkTreeModel *model,
					  GtkTreePath *path, GtkTreeIter *iter,
					  gpointer data)
{
	SummaryView *summaryview = (SummaryView *)data;

	summary_move_row_to(summaryview, iter, summaryview->to_folder);
	return FALSE;
}

void summary_move_selected_to(SummaryView *summaryview, FolderItem *to_folder)
{
	GList *rows, *cur;
	GtkTreeView *treeview = GTK_TREE_VIEW(summaryview->treeview);
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter;

	if (!to_folder) return;
	if (!summaryview->folder_item ||
	    FOLDER_TYPE(summaryview->folder_item->folder) == F_NEWS)
		return;
	if (summaryview->folder_item->stype == F_QUEUE ||
	    to_folder->stype == F_QUEUE || to_folder->stype == F_VIRTUAL)
		return;

	if (summary_is_locked(summaryview)) return;

	if (summaryview->folder_item == to_folder) {
		alertpanel_warning(_("Destination is same as current folder."));
		return;
	}

	rows = summary_get_selected_rows(summaryview);
	for (cur = rows; cur != NULL; cur = cur->next) {
		GtkTreePath *path = (GtkTreePath *)cur->data;

		gtk_tree_model_get_iter(model, &iter, path);
		if (gtk_tree_model_iter_has_child(model, &iter) &&
		    !gtk_tree_view_row_expanded(treeview, path)) {
			summaryview->to_folder = to_folder;
			gtkut_tree_model_foreach
				(model, &iter, summary_move_foreach_func,
				 summaryview);
			summaryview->to_folder = NULL;
		} else
			summary_move_row_to(summaryview, &iter, to_folder);
	}

	if (prefs_common.immediate_exec)
		summary_execute(summaryview);
	else {
		summary_step(summaryview, GTK_SCROLL_STEP_FORWARD);
		summary_status_show(summaryview);
	}
}

void summary_move_to(SummaryView *summaryview)
{
	FolderItem *to_folder;

	if (!summaryview->folder_item ||
	    FOLDER_TYPE(summaryview->folder_item->folder) == F_NEWS) return;

	to_folder = foldersel_folder_sel_full(summaryview->folder_item->folder,
					      FOLDER_SEL_MOVE, NULL,
					      _("Select folder to move"));
	summary_move_selected_to(summaryview, to_folder);
}

static void summary_copy_row_to(SummaryView *summaryview, GtkTreeIter *iter,
				FolderItem *to_folder)
{
	MsgInfo *msginfo;

	g_return_if_fail(to_folder != NULL);
	if (to_folder->stype == F_QUEUE || to_folder->stype == F_VIRTUAL)
		return;

	GET_MSG_INFO(msginfo, iter);

	msginfo->to_folder = to_folder;
	if (MSG_IS_DELETED(msginfo->flags)) {
		summaryview->deleted--;
		if (summaryview->on_filter)
			summaryview->flt_deleted--;
		MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_DELETED);
		MSG_SET_TMP_FLAGS(msginfo->flags, MSG_FLAG_CHANGED);
	}
	MSG_UNSET_TMP_FLAGS(msginfo->flags, MSG_MOVE);
	if (!MSG_IS_COPY(msginfo->flags)) {
		MSG_SET_TMP_FLAGS(msginfo->flags, MSG_COPY);
		summaryview->copied++;
		if (summaryview->on_filter)
			summaryview->flt_copied++;
	}
	summaryview->folder_item->mark_dirty = TRUE;
	if (!prefs_common.immediate_exec)
		summary_set_row(summaryview, iter, msginfo);

	debug_print(_("Message %d is set to copy to %s\n"),
		    msginfo->msgnum, to_folder->path);
}

static gboolean summary_copy_foreach_func(GtkTreeModel *model,
					  GtkTreePath *path, GtkTreeIter *iter,
					  gpointer data)
{
	SummaryView *summaryview = (SummaryView *)data;

	summary_copy_row_to(summaryview, iter, summaryview->to_folder);
	return FALSE;
}

void summary_copy_selected_to(SummaryView *summaryview, FolderItem *to_folder)
{
	GList *rows, *cur;
	GtkTreeView *treeview = GTK_TREE_VIEW(summaryview->treeview);
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter;

	if (!to_folder) return;
	if (!summaryview->folder_item) return;
	if (summaryview->folder_item->stype == F_QUEUE ||
	    to_folder->stype == F_QUEUE || to_folder->stype == F_VIRTUAL)
		return;

	if (summary_is_locked(summaryview)) return;

	if (summaryview->folder_item == to_folder) {
		alertpanel_warning
			(_("Destination for copy is same as current folder."));
		return;
	}

	rows = summary_get_selected_rows(summaryview);
	for (cur = rows; cur != NULL; cur = cur->next) {
		GtkTreePath *path = (GtkTreePath *)cur->data;

		gtk_tree_model_get_iter(model, &iter, path);
		if (gtk_tree_model_iter_has_child(model, &iter) &&
		    !gtk_tree_view_row_expanded(treeview, path)) {
			summaryview->to_folder = to_folder;
			gtkut_tree_model_foreach
				 (model, &iter, summary_copy_foreach_func,
				  summaryview);
			summaryview->to_folder = NULL;
		} else
			summary_copy_row_to(summaryview, &iter, to_folder);
	}

	if (prefs_common.immediate_exec)
		summary_execute(summaryview);
	else {
		summary_step(summaryview, GTK_SCROLL_STEP_FORWARD);
		summary_status_show(summaryview);
	}
}

void summary_copy_to(SummaryView *summaryview)
{
	FolderItem *to_folder;

	if (!summaryview->folder_item) return;

	to_folder = foldersel_folder_sel_full(summaryview->folder_item->folder,
					      FOLDER_SEL_COPY, NULL,
					      _("Select folder to copy"));
	summary_copy_selected_to(summaryview, to_folder);
}

void summary_add_address(SummaryView *summaryview)
{
	GtkTreeIter iter;
	MsgInfo *msginfo = NULL;
	gchar from[BUFFSIZE];

	if (!summaryview->selected) return;

	if (!gtkut_tree_row_reference_get_iter
		(GTK_TREE_MODEL(summaryview->store),
		 summaryview->selected, &iter))
		return;

	GET_MSG_INFO(msginfo, &iter);
	strncpy2(from, msginfo->from, sizeof(from));
	eliminate_address_comment(from);
	extract_address(from);
	addressbook_add_contact(msginfo->fromname, from, NULL);
}

void summary_select_all(SummaryView *summaryview)
{
	gtk_tree_selection_select_all(summaryview->selection);
}

void summary_unselect_all(SummaryView *summaryview)
{
	gtk_tree_selection_unselect_all(summaryview->selection);
}

void summary_select_thread(SummaryView *summaryview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter, parent, child, next;
	GtkTreePath *start_path, *end_path;
	gboolean valid;

	valid = gtkut_tree_row_reference_get_iter(model, summaryview->selected,
						  &iter);
	if (!valid)
		return;

	while (gtk_tree_model_iter_parent(model, &parent, &iter))
		iter = parent;

	if (!gtk_tree_model_iter_children(model, &child, &iter))
		return;

	start_path = gtk_tree_model_get_path(model, &iter);

	for (;;) {
		next = iter = child;
		while (gtk_tree_model_iter_next(model, &next))
			iter = next;
		if (!gtk_tree_model_iter_children(model, &child, &iter))
			break;
	}

	end_path = gtk_tree_model_get_path(model, &iter);

	gtk_tree_view_expand_row(GTK_TREE_VIEW(summaryview->treeview),
				 start_path, TRUE);
	gtk_tree_selection_select_range(summaryview->selection,
					start_path, end_path);

	gtk_tree_path_free(end_path);
	gtk_tree_path_free(start_path);
}

void summary_save_as(SummaryView *summaryview)
{
	GtkTreeIter iter;
	MsgInfo *msginfo = NULL;
	gchar *filename;
	gchar *src, *dest;
	FileselFileType types[4] = {{NULL, NULL}};
	gint selected_type = 0;
	gint result;
	gboolean all_headers;

	if (!summaryview->selected) return;
	if (!gtkut_tree_row_reference_get_iter
		(GTK_TREE_MODEL(summaryview->store),
		 summaryview->selected, &iter))
		return;

	GET_MSG_INFO(msginfo, &iter);
	if (!msginfo) return;

	if (msginfo->subject && *msginfo->subject) {
		filename = g_strdup_printf("%s.eml", msginfo->subject);
	} else {
		filename = g_strdup_printf("%u.eml", msginfo->msgnum);
	}
	subst_for_filename(filename);

	types[0].type = _("Original (EML/RFC 822)");
	types[0].ext = "eml";
	types[1].type = _("Text");
	types[1].ext = "txt";
	types[2].type = _("Text (UTF-8)");
	types[2].ext = "txt";
	
	dest = filesel_save_as_type(filename, types, prefs_common.save_file_type, &selected_type);

	g_free(filename);
	if (!dest)
		return;

	debug_print("summary_save_as: selected_type: %d\n", selected_type);

	all_headers = summaryview->messageview->textview->show_all_headers;

	if (selected_type == 1) {
		result = procmsg_save_message_as_text(msginfo, dest, conv_get_locale_charset_str(), all_headers);
	} else if (selected_type == 2) {
		result = procmsg_save_message_as_text(msginfo, dest, NULL, all_headers);
	} else {
		src = procmsg_get_message_file(msginfo);
		result = copy_file(src, dest, TRUE);
		g_free(src);
	}

	if (result < 0) {
		gchar *utf8_dest;

		utf8_dest = conv_filename_to_utf8(dest);
		alertpanel_error(_("Can't save the file `%s'."),
				 g_basename(utf8_dest));
		g_free(utf8_dest);
	}

	g_free(dest);

	prefs_common.save_file_type = selected_type;
}

void summary_print(SummaryView *summaryview)
{
	GSList *mlist;
	gboolean all_headers;

	all_headers = summaryview->messageview->textview->show_all_headers;
	mlist = summary_get_selected_msg_list(summaryview);
	if (!mlist)
		return;
	printing_print_messages(mlist, all_headers);
	g_slist_free(mlist);
}

gboolean summary_execute(SummaryView *summaryview)
{
	gint val = 0;

	if (!summaryview->folder_item) return FALSE;

	if (summary_is_locked(summaryview)) return FALSE;
	summary_lock(summaryview);

	val |= summary_execute_move(summaryview);
	val |= summary_execute_copy(summaryview);
	val |= summary_execute_delete(summaryview);

	summary_unlock(summaryview);

	summary_remove_invalid_messages(summaryview);

	statusbar_pop_all();
	STATUSBAR_POP(summaryview->mainwin);

	if (val != 0) {
		alertpanel_error(_("Error occurred while processing messages."));
	}

	return TRUE;
}

static void summary_remove_invalid_messages(SummaryView *summaryview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	MsgInfo *disp_msginfo = NULL, *msginfo;
	FolderItem *item = summaryview->folder_item;
	GtkTreeIter iter, next;
	GtkTreePath *path;
	gboolean valid;

	/* get currently displayed message */
	if (summaryview->displayed) {
		GtkTreeIter displayed;

		valid = gtkut_tree_row_reference_get_iter
			(model, summaryview->displayed, &displayed);
		if (valid) {
			gtk_tree_model_get(model, &displayed,
					   S_COL_MSG_INFO, &disp_msginfo, -1);
			if (MSG_IS_INVALID(disp_msginfo->flags)) {
				valid = FALSE;
				disp_msginfo = NULL;
			}
		}
		if (!valid) {
			/* g_print("displayed became invalid before removing\n"); */
			messageview_clear(summaryview->messageview);
			gtk_tree_row_reference_free(summaryview->displayed);
			summaryview->displayed = NULL;
		}
	}

	if (item->threaded)
		summary_modify_threads(summaryview);

	/* update selection */
	valid = gtkut_tree_row_reference_get_iter(model, summaryview->selected,
						  &iter);
	if (valid) {
		valid = summary_find_nearest_msg(summaryview, &next, &iter);
		if (valid) {
			gboolean display;

			gtk_tree_model_get(model, &next,
					   S_COL_MSG_INFO, &msginfo, -1);
			if (disp_msginfo && disp_msginfo == msginfo) {
				/* g_print("replace displayed\n"); */
				path = gtk_tree_model_get_path(model, &next);
				gtk_tree_row_reference_free
					(summaryview->displayed);
				summaryview->displayed =
					gtk_tree_row_reference_new(model, path);
				gtk_tree_path_free(path);
			}
			display = prefs_common.immediate_exec &&
				messageview_is_visible
					(summaryview->messageview);
			summary_select_row
				(summaryview, &next, display, FALSE);
			if (display)
				summary_mark_displayed_read(summaryview, &next);
		}
	}
	if (!valid)
		summary_unselect_all(summaryview);

	for (valid = gtk_tree_model_get_iter_first(model, &iter);
	     valid == TRUE; iter = next) {
		next = iter;
		valid = gtkut_tree_model_next(model, &next);

		gtk_tree_model_get(model, &iter, S_COL_MSG_INFO, &msginfo, -1);
		if (!MSG_IS_INVALID(msginfo->flags))
			continue;

		if (gtk_tree_model_iter_has_child(model, &iter)) {
			g_warning("summary_remove_invalid_messages(): "
				  "tried to remove row which has child\n");
			continue;
		}

		gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
		summaryview->all_mlist = g_slist_remove(summaryview->all_mlist,
							msginfo);
		if (summaryview->flt_mlist)
			summaryview->flt_mlist =
				g_slist_remove(summaryview->flt_mlist, msginfo);
		procmsg_msginfo_free(msginfo);

		item->cache_dirty = TRUE;
	}

	/* selection list becomes invalid if modified */
	if (item->cache_dirty)
		summary_selection_list_free(summaryview);

	if (summaryview->displayed &&
	    !gtk_tree_row_reference_valid(summaryview->displayed)) {
		/* g_print("displayed became invalid after removing. searching disp_msginfo...\n"); */
		if (disp_msginfo &&
		    gtkut_tree_model_find_by_column_data
			(model, &iter, NULL, S_COL_MSG_INFO, disp_msginfo)) {
			/* g_print("replace displayed\n"); */
			path = gtk_tree_model_get_path(model, &iter);
			gtk_tree_row_reference_free(summaryview->displayed);
			summaryview->displayed =
				gtk_tree_row_reference_new(model, path);
			gtk_tree_path_free(path);
		} else {
			messageview_clear(summaryview->messageview);
			gtk_tree_row_reference_free(summaryview->displayed);
			summaryview->displayed = NULL;
		}
	}

	if (gtk_tree_model_get_iter_first(model, &iter))
		gtk_widget_grab_focus(summaryview->treeview);
	else {
		menu_set_insensitive_all
			(GTK_MENU_SHELL(summaryview->popupmenu));
		gtk_widget_grab_focus(summaryview->folderview->treeview);
	}

	summary_write_cache(summaryview);

	summary_update_status(summaryview);
	summary_status_show(summaryview);
}

static gboolean summary_execute_move_func(GtkTreeModel *model,
					  GtkTreePath *path, GtkTreeIter *iter,
					  gpointer data)
{
	SummaryView *summaryview = data;
	MsgInfo *msginfo;

	gtk_tree_model_get(model, iter, S_COL_MSG_INFO, &msginfo, -1);

	if (msginfo && MSG_IS_MOVE(msginfo->flags) && msginfo->to_folder) {
		g_hash_table_insert(summaryview->folder_table,
				    msginfo->to_folder, GINT_TO_POINTER(1));

		summaryview->tmp_mlist =
			g_slist_prepend(summaryview->tmp_mlist, msginfo);

		MSG_UNSET_TMP_FLAGS(msginfo->flags, MSG_MOVE);
		summary_set_row(summaryview, iter, msginfo);
	}

	return FALSE;
}

static gint summary_execute_move(SummaryView *summaryview)
{
	gint val = 0;

	summaryview->folder_table = g_hash_table_new(NULL, NULL);

	/* search moving messages and execute */
	gtk_tree_model_foreach(GTK_TREE_MODEL(summaryview->store), 
			       summary_execute_move_func, summaryview);

	if (summaryview->tmp_mlist) {
		summaryview->tmp_mlist =
			g_slist_reverse(summaryview->tmp_mlist);
		val = procmsg_move_messages(summaryview->tmp_mlist);

		folderview_update_item_foreach(summaryview->folder_table,
					       FALSE);

		g_slist_free(summaryview->tmp_mlist);
		summaryview->tmp_mlist = NULL;
	}

	g_hash_table_destroy(summaryview->folder_table);
	summaryview->folder_table = NULL;

	return val;
}

static gboolean summary_execute_copy_func(GtkTreeModel *model,
					  GtkTreePath *path, GtkTreeIter *iter,
					  gpointer data)
{
	SummaryView *summaryview = data;
	MsgInfo *msginfo;

	gtk_tree_model_get(model, iter, S_COL_MSG_INFO, &msginfo, -1);

	if (msginfo && MSG_IS_COPY(msginfo->flags) && msginfo->to_folder) {
		g_hash_table_insert(summaryview->folder_table,
				    msginfo->to_folder, GINT_TO_POINTER(1));

		summaryview->tmp_mlist =
			g_slist_prepend(summaryview->tmp_mlist, msginfo);

		MSG_UNSET_TMP_FLAGS(msginfo->flags, MSG_COPY);
		summary_set_row(summaryview, iter, msginfo);
	}

	return FALSE;
}

static gint summary_execute_copy(SummaryView *summaryview)
{
	gint val = 0;

	summaryview->folder_table = g_hash_table_new(NULL, NULL);

	/* search copying messages and execute */
	gtk_tree_model_foreach(GTK_TREE_MODEL(summaryview->store), 
			       summary_execute_copy_func, summaryview);

	if (summaryview->tmp_mlist) {
		summaryview->tmp_mlist =
			g_slist_reverse(summaryview->tmp_mlist);
		val = procmsg_copy_messages(summaryview->tmp_mlist);

		folderview_update_item_foreach(summaryview->folder_table,
					       FALSE);

		g_slist_free(summaryview->tmp_mlist);
		summaryview->tmp_mlist = NULL;
	}

	g_hash_table_destroy(summaryview->folder_table);
	summaryview->folder_table = NULL;

	return val;
}

static gboolean summary_execute_delete_func(GtkTreeModel *model,
					    GtkTreePath *path,
					    GtkTreeIter *iter, gpointer data)
{
	SummaryView *summaryview = data;
	MsgInfo *msginfo;

	gtk_tree_model_get(model, iter, S_COL_MSG_INFO, &msginfo, -1);

	if (msginfo && MSG_IS_DELETED(msginfo->flags)) {
		summaryview->tmp_mlist =
			g_slist_prepend(summaryview->tmp_mlist, msginfo);
	}

	return FALSE;
}

static gint summary_execute_delete(SummaryView *summaryview)
{
	FolderItem *trash = NULL;
	PrefsAccount *ac;
	gint val = 0;

	ac = account_find_from_item_property(summaryview->folder_item);
	if (ac && ac->set_trash_folder && ac->trash_folder)
		trash = folder_find_item_from_identifier(ac->trash_folder);
	if (!trash)
		trash = summaryview->folder_item->folder->trash;
	if (!trash)
		folder_get_default_trash();

	if (FOLDER_TYPE(summaryview->folder_item->folder) == F_MH) {
		g_return_val_if_fail(trash != NULL, 0);
	}

	/* search deleting messages and execute */
	gtk_tree_model_foreach(GTK_TREE_MODEL(summaryview->store), 
			       summary_execute_delete_func, summaryview);

	if (!summaryview->tmp_mlist) return 0;

	summaryview->tmp_mlist = g_slist_reverse(summaryview->tmp_mlist);

	if (summaryview->folder_item != trash)
		val = folder_item_move_msgs(trash, summaryview->tmp_mlist);
	else
		val = folder_item_remove_msgs(trash, summaryview->tmp_mlist);

	g_slist_free(summaryview->tmp_mlist);
	summaryview->tmp_mlist = NULL;

	if (summaryview->folder_item != trash) {
		folder_item_scan(trash);
		folderview_update_item(trash, FALSE);
	}

	return val == -1 ? -1 : 0;
}

/* thread functions */

void summary_thread_build(SummaryView *summaryview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeStore *store = summaryview->store;
	GtkTreeIter iter, next;
	GtkTreePath *path;
	GSList *mlist;
	GNode *root, *node;
	GHashTable *node_table;
	MsgInfo *msginfo;
	gboolean valid;
	FolderSortKey sort_key = SORT_BY_NONE;
	FolderSortType sort_type = SORT_ASCENDING;
	MsgInfo *selected_msg, *displayed_msg;

	if (!summaryview->folder_item)
		return;

	summary_lock(summaryview);

	debug_print(_("Building threads..."));
	STATUSBAR_PUSH(summaryview->mainwin, _("Building threads..."));
	main_window_cursor_wait(summaryview->mainwin);

	g_signal_handlers_block_matched(G_OBJECT(summaryview->treeview),
					(GSignalMatchType)G_SIGNAL_MATCH_DATA,
					0, 0, NULL, NULL, summaryview);

	selected_msg = summary_get_msginfo(summaryview, summaryview->selected);
	displayed_msg = summary_get_msginfo
		(summaryview, summaryview->displayed);

	summaryview->folder_item->threaded = TRUE;

	mlist = summary_get_msg_list(summaryview);
	root = procmsg_get_thread_tree(mlist);
	node_table = g_hash_table_new(NULL, NULL);
	for (node = root->children; node != NULL; node = node->next) {
		g_hash_table_insert(node_table, node->data, node);
	}

	if (summaryview->folder_item->sort_key != SORT_BY_NONE) {
		sort_key = summaryview->folder_item->sort_key;
		sort_type = summaryview->folder_item->sort_type;
		summary_sort(summaryview, SORT_BY_NONE, SORT_ASCENDING);
	}

	valid = gtk_tree_model_get_iter_first(model, &next);
	while (valid) {
		iter = next;
		valid = gtk_tree_model_iter_next(model, &next);

		gtk_tree_model_get(model, &iter, S_COL_MSG_INFO, &msginfo, -1);
		node = g_hash_table_lookup(node_table, msginfo);
		if (node) {
			GNode *cur;
			GtkTreeIter child;
			guint tdate;

			for (cur = node->children; cur != NULL;
			     cur = cur->next) {
				summary_insert_gnode(summaryview, store, &child,
						     &iter, NULL, cur);
			}

			tdate = procmsg_get_thread_date(node);
			gtk_tree_store_set(store, &iter, S_COL_TDATE, tdate, -1);
		} else
			gtk_tree_store_remove(store, &iter);
	}

	if (sort_key != SORT_BY_NONE)
		summary_sort(summaryview, sort_key, sort_type);

	g_hash_table_destroy(node_table);
	g_node_destroy(root);
	g_slist_free(mlist);

	if (prefs_common.expand_thread)
		gtk_tree_view_expand_all(GTK_TREE_VIEW(summaryview->treeview));

	if (!summaryview->selected ||
	    (summaryview->selected &&
	     !gtk_tree_row_reference_valid(summaryview->selected))) {
		if (selected_msg &&
		    gtkut_tree_model_find_by_column_data
			(model, &iter, NULL, S_COL_MSG_INFO, selected_msg)) {
			summary_select_row(summaryview, &iter, FALSE, TRUE);
		}
	} else
		summary_scroll_to_selected(summaryview, TRUE);

	if (summaryview->displayed &&
	    !gtk_tree_row_reference_valid(summaryview->displayed)) {
		if (displayed_msg &&
		    gtkut_tree_model_find_by_column_data
			(model, &iter, NULL, S_COL_MSG_INFO, displayed_msg)) {
			path = gtk_tree_model_get_path(model, &iter);
			gtk_tree_row_reference_free(summaryview->displayed);
			summaryview->displayed =
				gtk_tree_row_reference_new(model, path);
			gtk_tree_path_free(path);
		} else {
			messageview_clear(summaryview->messageview);
			gtk_tree_row_reference_free(summaryview->displayed);
			summaryview->displayed = NULL;
		}
	}

	g_signal_handlers_unblock_matched(G_OBJECT(summaryview->treeview),
					  (GSignalMatchType)G_SIGNAL_MATCH_DATA,
					  0, 0, NULL, NULL, summaryview);

	debug_print(_("done.\n"));
	STATUSBAR_POP(summaryview->mainwin);
	main_window_cursor_normal(summaryview->mainwin);

	summary_unlock(summaryview);
}

static void summary_unthread_node_recursive(SummaryView *summaryview,
					    GtkTreeIter *iter,
					    GtkTreeIter *sibling)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter_, child;
	MsgInfo *msginfo;
	gboolean valid;

	gtk_tree_model_get(model, iter, S_COL_MSG_INFO, &msginfo, -1);
	gtk_tree_store_insert_after(GTK_TREE_STORE(model), &iter_,
				    NULL, sibling);
	summary_set_row(summaryview, &iter_, msginfo);
	*sibling = iter_;

	valid = gtk_tree_model_iter_children(model, &child, iter);
	while (valid) {
		summary_unthread_node_recursive(summaryview, &child, sibling);
		valid = gtk_tree_model_iter_next(model, &child);
	}
}

static void summary_unthread_node(SummaryView *summaryview, GtkTreeIter *iter)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter child, sibling, next;
	gboolean valid;

	sibling = *iter;

	valid = gtk_tree_model_iter_children(model, &next, iter);
	while (valid) {
		child = next;
		valid = gtk_tree_model_iter_next(model, &next);
		summary_unthread_node_recursive(summaryview, &child, &sibling);
		gtk_tree_store_remove(GTK_TREE_STORE(model), &child);
	}
}

void summary_unthread(SummaryView *summaryview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter, next;
	GtkTreePath *path;
	gboolean valid;
	FolderSortKey sort_key = SORT_BY_NONE;
	FolderSortType sort_type = SORT_ASCENDING;
	MsgInfo *selected_msg, *displayed_msg;

	if (!summaryview->folder_item)
		return;

	summary_lock(summaryview);

	debug_print(_("Unthreading..."));
	STATUSBAR_PUSH(summaryview->mainwin, _("Unthreading..."));
	main_window_cursor_wait(summaryview->mainwin);

	g_signal_handlers_block_matched(G_OBJECT(summaryview->treeview),
					(GSignalMatchType)G_SIGNAL_MATCH_DATA,
					0, 0, NULL, NULL, summaryview);

	selected_msg = summary_get_msginfo(summaryview, summaryview->selected);
	displayed_msg = summary_get_msginfo
		(summaryview, summaryview->displayed);

	summaryview->folder_item->threaded = FALSE;

	if (summaryview->folder_item->sort_key != SORT_BY_NONE) {
		sort_key = summaryview->folder_item->sort_key;
		sort_type = summaryview->folder_item->sort_type;
		summary_sort(summaryview, SORT_BY_NONE, SORT_ASCENDING);
	}

	valid = gtk_tree_model_get_iter_first(model, &next);
	while (valid) {
		iter = next;
		valid = gtk_tree_model_iter_next(model, &next);
		gtk_tree_store_set(GTK_TREE_STORE(model), &iter,
				   S_COL_TDATE, 0, -1);
	}

	valid = gtk_tree_model_get_iter_first(model, &next);
	while (valid) {
		iter = next;
		valid = gtk_tree_model_iter_next(model, &next);
		summary_unthread_node(summaryview, &iter);
	}

	if (sort_key != SORT_BY_NONE)
		summary_sort(summaryview, sort_key, sort_type);

	if (!summaryview->selected ||
	    (summaryview->selected &&
	     !gtk_tree_row_reference_valid(summaryview->selected))) {
		if (selected_msg &&
		    gtkut_tree_model_find_by_column_data
			(model, &iter, NULL, S_COL_MSG_INFO, selected_msg)) {
			summary_select_row(summaryview, &iter, FALSE, TRUE);
		}
	} else
		summary_scroll_to_selected(summaryview, TRUE);

	if (summaryview->displayed &&
	    !gtk_tree_row_reference_valid(summaryview->displayed)) {
		if (displayed_msg &&
		    gtkut_tree_model_find_by_column_data
			(model, &iter, NULL, S_COL_MSG_INFO, displayed_msg)) {
			path = gtk_tree_model_get_path(model, &iter);
			gtk_tree_row_reference_free(summaryview->displayed);
			summaryview->displayed =
				gtk_tree_row_reference_new(model, path);
			gtk_tree_path_free(path);
		} else {
			messageview_clear(summaryview->messageview);
			gtk_tree_row_reference_free(summaryview->displayed);
			summaryview->displayed = NULL;
		}
	}

	g_signal_handlers_unblock_matched(G_OBJECT(summaryview->treeview),
					  (GSignalMatchType)G_SIGNAL_MATCH_DATA,
					  0, 0, NULL, NULL, summaryview);

	debug_print(_("done.\n"));
	STATUSBAR_POP(summaryview->mainwin);
	main_window_cursor_normal(summaryview->mainwin);

	summary_unlock(summaryview);
}

static gboolean summary_has_invalid_node(GtkTreeModel *model, GtkTreeIter *iter)
{
	GtkTreeIter child;
	MsgInfo *msginfo;
	gboolean valid;

	gtk_tree_model_get(model, iter, S_COL_MSG_INFO, &msginfo, -1);
	if (MSG_IS_INVALID(msginfo->flags))
		return TRUE;

	valid = gtk_tree_model_iter_children(model, &child, iter);
	while (valid) {
		if (summary_has_invalid_node(model, &child))
			return TRUE;
		valid = gtk_tree_model_iter_next(model, &child);
	}

	return FALSE;
}

static GNode *summary_get_modified_node(SummaryView *summaryview,
					GtkTreeIter *iter,
					GNode *parent, GNode *sibling)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GNode *node = NULL, *new_sibling;
	GtkTreeIter child;
	MsgInfo *msginfo;
	gboolean valid;

	gtk_tree_model_get(model, iter, S_COL_MSG_INFO, &msginfo, -1);

	if (!MSG_IS_INVALID(msginfo->flags)) {
		node = g_node_new(msginfo);
		g_node_insert_after(parent, sibling, node);
		parent = node;
		sibling = NULL;
	} else {
		summaryview->all_mlist = g_slist_remove(summaryview->all_mlist,
							msginfo);
		if (summaryview->flt_mlist)
			summaryview->flt_mlist =
				g_slist_remove(summaryview->flt_mlist, msginfo);
		procmsg_msginfo_free(msginfo);
	}

	valid = gtk_tree_model_iter_children(model, &child, iter);

	while (valid) {
		new_sibling = summary_get_modified_node(summaryview, &child,
							parent, sibling);
		if (new_sibling) {
			sibling = new_sibling;
			if (!node)
				node = sibling;
		}
		valid = gtk_tree_model_iter_next(model, &child);
	}

	return node;
}

#if 0
static gboolean traverse(GNode *node, gpointer data)
{
	gint i;

	if (!node->data)
		return FALSE;
	for (i = 0; i < g_node_depth(node); i++)
		g_print(" ");
	g_print("%s\n", ((MsgInfo *)node->data)->subject);
	return FALSE;
}
#endif

static void summary_modify_node(SummaryView *summaryview, GtkTreeIter *iter,
				GtkTreeIter *selected)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	MsgInfo *msginfo, *sel_msginfo = NULL;
	GNode *root, *cur;
	GtkTreeIter iter_, sibling;
	GtkTreeIter *sibling_p = NULL;
	GtkTreePath *path, *sel_path;
	gboolean found = FALSE;

	if (!gtk_tree_model_iter_has_child(model, iter))
		return;
	if (!summary_has_invalid_node(model, iter))
		return;

	gtk_tree_model_get(model, iter, S_COL_MSG_INFO, &msginfo, -1);

	if (selected) {
		path = gtk_tree_model_get_path(model, iter);
		sel_path = gtk_tree_model_get_path(model, selected);
		if (gtk_tree_path_compare(path, sel_path) == 0 ||
		    gtk_tree_path_is_ancestor(path, sel_path))
			gtk_tree_model_get(model, selected,
					   S_COL_MSG_INFO, &sel_msginfo, -1);
		gtk_tree_path_free(sel_path);
		gtk_tree_path_free(path);
	}

	root = g_node_new(NULL);
	summary_get_modified_node(summaryview, iter, root, NULL);

	/* g_node_traverse(root, G_PRE_ORDER, G_TRAVERSE_ALL, -1, traverse, NULL); */

	sibling = *iter;
	if (gtk_tree_model_iter_next(model, &sibling))
		sibling_p = &sibling;

	gtk_tree_store_remove(GTK_TREE_STORE(model), iter);

	for (cur = root->children; cur != NULL; cur = cur->next) {
		summary_insert_gnode_before(summaryview, GTK_TREE_STORE(model),
					    &iter_, NULL, sibling_p, cur);
		if (summaryview->folder_item->threaded &&
		    prefs_common.expand_thread) {
			path = gtk_tree_model_get_path(model, &iter_);
			gtk_tree_view_expand_row
				(GTK_TREE_VIEW(summaryview->treeview),
				 path, TRUE);
			gtk_tree_path_free(path);
		}
		if (sel_msginfo && !found) {
			found = gtkut_tree_model_find_by_column_data
				(model, selected, &iter_,
				 S_COL_MSG_INFO, sel_msginfo);
		}
	}

	g_node_destroy(root);

	summaryview->folder_item->cache_dirty = TRUE;
}

static void summary_modify_threads(SummaryView *summaryview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter, next, selected, new_selected;
	GtkTreeIter *selected_p = NULL;
	GtkTreePath *prev_path = NULL;
	gboolean valid, has_selection;

	summary_lock(summaryview);

	debug_print("Modifying threads for execution...");

	g_signal_handlers_block_by_func(summaryview->selection,
					summary_selection_changed, summaryview);

	has_selection = gtkut_tree_row_reference_get_iter
		(model, summaryview->selected, &selected);
	if (has_selection) {
		prev_path = gtk_tree_row_reference_get_path
			(summaryview->selected);
		if (summary_find_nearest_msg(summaryview, &next, &selected)) {
			selected = next;
			selected_p = &selected;
		} else
			has_selection = FALSE;
	}

	valid = gtk_tree_model_get_iter_first(model, &next);
	while (valid) {
		iter = next;
		valid = gtk_tree_model_iter_next(model, &next);
		summary_modify_node(summaryview, &iter, selected_p);
	}

	g_signal_handlers_unblock_by_func(summaryview->selection,
					  summary_selection_changed,
					  summaryview);

	if (summaryview->folder_item->cache_dirty)
		summary_selection_list_free(summaryview);

	if (has_selection &&
	    !gtk_tree_row_reference_valid(summaryview->selected)) {
		if (prev_path &&
		    gtk_tree_model_get_iter(model, &new_selected, prev_path))
			selected = new_selected;
		summary_select_row(summaryview, &selected, FALSE, FALSE);
	}
	gtk_tree_path_free(prev_path);

	debug_print("done.\n");

	summary_unlock(summaryview);
}

void summary_expand_threads(SummaryView *summaryview)
{
	gtk_tree_view_expand_all(GTK_TREE_VIEW(summaryview->treeview));
}

void summary_collapse_threads(SummaryView *summaryview)
{
	gtk_tree_view_collapse_all(GTK_TREE_VIEW(summaryview->treeview));
}

static gboolean summary_filter_func(GtkTreeModel *model, GtkTreePath *path,
				    GtkTreeIter *iter, gpointer data)
{
	SummaryView *summaryview = (SummaryView *)data;
	MsgInfo *msginfo;
	FilterInfo *fltinfo;

	gtk_tree_model_get(model, iter, S_COL_MSG_INFO, &msginfo, -1);

	summaryview->flt_count++;
	{
		gchar msg[1024];
		g_snprintf(msg, sizeof(msg), _("Filtering (%d / %d)..."),
			   summaryview->flt_count, summaryview->flt_total);
		STATUSBAR_POP(summaryview->mainwin);
		STATUSBAR_PUSH(summaryview->mainwin, msg);
		if ((summaryview->flt_count % 100) == 0) {
			GTK_EVENTS_FLUSH();
		}
	}

	fltinfo = filter_info_new();
	fltinfo->flags = msginfo->flags;
	filter_apply_msginfo(prefs_common.fltlist, msginfo, fltinfo);
	if (fltinfo->actions[FLT_ACTION_MOVE] ||
	    fltinfo->actions[FLT_ACTION_COPY] ||
	    fltinfo->actions[FLT_ACTION_DELETE] ||
	    fltinfo->actions[FLT_ACTION_EXEC] ||
	    fltinfo->actions[FLT_ACTION_EXEC_ASYNC] ||
	    fltinfo->actions[FLT_ACTION_MARK] ||
	    fltinfo->actions[FLT_ACTION_COLOR_LABEL] ||
	    fltinfo->actions[FLT_ACTION_MARK_READ] ||
	    fltinfo->actions[FLT_ACTION_FORWARD] ||
	    fltinfo->actions[FLT_ACTION_FORWARD_AS_ATTACHMENT] ||
	    fltinfo->actions[FLT_ACTION_REDIRECT])
		summaryview->filtered++;

	if (msginfo->flags.perm_flags != fltinfo->flags.perm_flags) {
		msginfo->flags = fltinfo->flags;
		MSG_SET_TMP_FLAGS(msginfo->flags, MSG_FLAG_CHANGED);
		summaryview->folder_item->mark_dirty = TRUE;
		summary_set_row(summaryview, iter, msginfo);
		if (MSG_IS_IMAP(msginfo->flags)) {
			if (fltinfo->actions[FLT_ACTION_MARK])
				imap_msg_set_perm_flags(msginfo, MSG_MARKED);
			if (fltinfo->actions[FLT_ACTION_MARK_READ])
				imap_msg_unset_perm_flags(msginfo,
							  MSG_NEW|MSG_UNREAD);
		}
	}

	if (fltinfo->actions[FLT_ACTION_MOVE] && fltinfo->move_dest)
		summary_move_row_to(summaryview, iter, fltinfo->move_dest);
	else if (fltinfo->actions[FLT_ACTION_DELETE])
		summary_delete_row(summaryview, iter);

	filter_info_free(fltinfo);

	return FALSE;
}

static gboolean summary_filter_junk_func(GtkTreeModel *model, GtkTreePath *path,
					 GtkTreeIter *iter, gpointer data)
{
	SummaryView *summaryview = (SummaryView *)data;
	MsgInfo *msginfo;
	FilterInfo *fltinfo;

	gtk_tree_model_get(model, iter, S_COL_MSG_INFO, &msginfo, -1);

	summaryview->flt_count++;
	{
		gchar msg[1024];
		g_snprintf(msg, sizeof(msg), _("Filtering (%d / %d)..."),
			   summaryview->flt_count, summaryview->flt_total);
		STATUSBAR_POP(summaryview->mainwin);
		STATUSBAR_PUSH(summaryview->mainwin, msg);
		if ((summaryview->flt_count % 100) == 0) {
			GTK_EVENTS_FLUSH();
		}
	}

	fltinfo = filter_info_new();
	fltinfo->flags = msginfo->flags;
	filter_apply_msginfo(summaryview->junk_fltlist, msginfo, fltinfo);

	if (fltinfo->actions[FLT_ACTION_MOVE] ||
	    fltinfo->actions[FLT_ACTION_COPY] ||
	    fltinfo->actions[FLT_ACTION_DELETE] ||
	    fltinfo->actions[FLT_ACTION_MARK_READ])
		summaryview->filtered++;
	else if (fltinfo->error == FLT_ERROR_EXEC_FAILED ||
		 fltinfo->last_exec_exit_status >= 3) {
		if (summaryview->flt_count == 1) {
			g_warning("summary_filter_junk_func: junk filter command returned %d", fltinfo->last_exec_exit_status);
			alertpanel_error
				(_("Execution of the junk filter command failed.\n"
				   "Please check the junk mail control setting."));
		}
		return TRUE;
	}

	if (msginfo->flags.perm_flags != fltinfo->flags.perm_flags) {
		msginfo->flags = fltinfo->flags;
		MSG_SET_TMP_FLAGS(msginfo->flags, MSG_FLAG_CHANGED);
		summaryview->folder_item->mark_dirty = TRUE;
		summary_set_row(summaryview, iter, msginfo);
		if (MSG_IS_IMAP(msginfo->flags)) {
			if (fltinfo->actions[FLT_ACTION_MARK_READ])
				imap_msg_unset_perm_flags(msginfo,
							  MSG_NEW|MSG_UNREAD);
		}
	}

	if (fltinfo->actions[FLT_ACTION_MOVE] && fltinfo->move_dest)
		summary_move_row_to(summaryview, iter, fltinfo->move_dest);
	else if (fltinfo->actions[FLT_ACTION_DELETE])
		summary_delete_row(summaryview, iter);

	filter_info_free(fltinfo);

	return FALSE;
}

static void summary_filter_real(SummaryView *summaryview,
				GtkTreeModelForeachFunc func,
				gboolean selected_only)
{
	GList *rows;
	FolderSortKey sort_key;
	FolderSortType sort_type;

	if (!summaryview->folder_item) return;

	if (summary_is_locked(summaryview)) return;
	summary_lock(summaryview);

	STATUSBAR_POP(summaryview->mainwin);

	debug_print(_("filtering..."));
	STATUSBAR_PUSH(summaryview->mainwin, _("Filtering..."));
	main_window_cursor_wait(summaryview->mainwin);
	GTK_EVENTS_FLUSH();

	sort_key = summaryview->folder_item->sort_key;
	sort_type = summaryview->folder_item->sort_type;
	if (sort_key != SORT_BY_NONE)
		summary_sort(summaryview, SORT_BY_NONE, SORT_ASCENDING);

	summaryview->filtered = 0;
	summaryview->flt_count = 0;

	if (selected_only) {
		rows = summary_get_selected_rows(summaryview);
		summaryview->flt_total = g_list_length(rows);

		gtk_tree_selection_selected_foreach
			(summaryview->selection,
			 (GtkTreeSelectionForeachFunc)func,
			 summaryview);
	} else {
		summaryview->flt_total = summaryview->folder_item->total;
		gtk_tree_model_foreach(GTK_TREE_MODEL(summaryview->store),
				       func, summaryview);
	}

	if (sort_key != SORT_BY_NONE)
		summary_sort(summaryview, sort_key, sort_type);

	summary_unlock(summaryview);

	if (prefs_common.immediate_exec)
		summary_execute(summaryview);
	else
		summary_status_show(summaryview);

	folderview_update_all_updated(FALSE);

	debug_print(_("done.\n"));
	STATUSBAR_POP(summaryview->mainwin);
	main_window_cursor_normal(summaryview->mainwin);

	if (summaryview->filtered > 0) {
		gchar result_msg[BUFFSIZE];
		g_snprintf(result_msg, sizeof(result_msg),
			   _("%d message(s) have been filtered."),
			   summaryview->filtered);
		STATUSBAR_PUSH(summaryview->mainwin, result_msg);
	}
	summaryview->filtered = 0;
	summaryview->flt_count = 0;
	summaryview->flt_total = 0;
}

void summary_filter(SummaryView *summaryview, gboolean selected_only)
{
	if (prefs_common.fltlist)
		summary_filter_real(summaryview, summary_filter_func,
				    selected_only);
}

void summary_filter_junk(SummaryView *summaryview, gboolean selected_only)
{
	FilterRule *rule;
	GSList junk_fltlist = {NULL, NULL};
	FolderItem *item = summaryview->folder_item;
	FolderItem *junk = NULL;

	if (!item)
		return;

	if (item->folder)
		junk = folder_get_junk(item->folder);
	rule = filter_junk_rule_create(NULL, junk, TRUE);
	if (rule) {
		junk_fltlist.data = rule;
		summaryview->junk_fltlist = &junk_fltlist;
		summary_filter_real(summaryview, summary_filter_junk_func,
				    selected_only);
		summaryview->junk_fltlist = NULL;
		filter_rule_free(rule);
	}
}

void summary_filter_open(SummaryView *summaryview, FilterCreateType type)
{
	GtkTreeIter iter;
	MsgInfo *msginfo = NULL;
	gchar *header = NULL;
	gchar *key = NULL;

	if (!summaryview->selected) return;
	if (!gtkut_tree_row_reference_get_iter
		(GTK_TREE_MODEL(summaryview->store),
		 summaryview->selected, &iter))
		return;

	GET_MSG_INFO(msginfo, &iter);
	if (!msginfo) return;

	filter_get_keyword_from_msg(msginfo, &header, &key, type);
	prefs_filter_open(msginfo, header, key);

	g_free(header);
	g_free(key);
}

static void summary_junk_func(GtkTreeModel *model, GtkTreePath *path,
			      GtkTreeIter *iter, gpointer data)
{
	FilterRule rule = {NULL, FLT_OR, NULL, NULL, FLT_TIMING_ANY, TRUE};
	FilterAction action1 = {FLT_ACTION_EXEC, NULL, 0};
	FilterAction action2 = {FLT_ACTION_MOVE, NULL, 0};
	FilterAction action3 = {FLT_ACTION_MARK_READ, NULL, 0};
	SummaryView *summaryview = (SummaryView *)data;
	MsgInfo *msginfo;
	FilterInfo *fltinfo;
	gchar *file;
	gchar *junk_id = NULL;
	gint ret;

	gtk_tree_model_get(model, iter, S_COL_MSG_INFO, &msginfo, -1);
	file = procmsg_get_message_file(msginfo);
	g_return_if_fail(file != NULL);

	if (summaryview->to_folder)
		junk_id = folder_item_get_identifier(summaryview->to_folder);

	action1.str_value = prefs_common.junk_learncmd;
	action2.str_value = junk_id;

	rule.action_list = g_slist_append(rule.action_list, &action1);
	if (junk_id)
		rule.action_list = g_slist_append(rule.action_list, &action2);
	if (prefs_common.mark_junk_as_read)
		rule.action_list = g_slist_append(rule.action_list, &action3);

	fltinfo = filter_info_new();
	fltinfo->flags = msginfo->flags;

	ret = filter_action_exec(&rule, msginfo, file, fltinfo);

	if (ret < 0 || fltinfo->last_exec_exit_status != 0) {
		g_warning("summary_junk_func: junk filter command returned %d",
			  fltinfo->last_exec_exit_status);
		alertpanel_error
			(_("Execution of the junk filter command failed.\n"
			   "Please check the junk mail control setting."));
	} else {
		if (ret == 0 &&
		    msginfo->flags.perm_flags != fltinfo->flags.perm_flags) {
			msginfo->flags = fltinfo->flags;
			summary_set_row(summaryview, iter, msginfo);
			if (MSG_IS_IMAP(msginfo->flags)) {
				if (fltinfo->actions[FLT_ACTION_MARK_READ])
					imap_msg_unset_perm_flags
						(msginfo, MSG_NEW | MSG_UNREAD);
			}
		}
		if (ret == 0 && fltinfo->actions[FLT_ACTION_MOVE] &&
		    fltinfo->move_dest)
			summary_move_row_to(summaryview, iter,
					    fltinfo->move_dest);
	}

	filter_info_free(fltinfo);
	g_slist_free(rule.action_list);
	g_free(junk_id);
	g_free(file);
}

static void summary_not_junk_func(GtkTreeModel *model, GtkTreePath *path,
				  GtkTreeIter *iter, gpointer data)
{
	FilterRule rule = {NULL, FLT_OR, NULL, NULL, FLT_TIMING_ANY, TRUE};
	FilterAction action = {FLT_ACTION_EXEC, NULL, 0};
	MsgInfo *msginfo;
	FilterInfo *fltinfo;
	gchar *file;
	gint ret;

	gtk_tree_model_get(model, iter, S_COL_MSG_INFO, &msginfo, -1);
	file = procmsg_get_message_file(msginfo);
	g_return_if_fail(file != NULL);

	action.str_value = prefs_common.nojunk_learncmd;

	rule.action_list = g_slist_append(rule.action_list, &action);

	fltinfo = filter_info_new();

	ret = filter_action_exec(&rule, msginfo, file, fltinfo);

	if (ret < 0 || fltinfo->last_exec_exit_status != 0) {
		g_warning("summary_not_junk_func: junk filter command returned %d",
			  fltinfo->last_exec_exit_status);
		alertpanel_error
			(_("Execution of the junk filter command failed.\n"
			   "Please check the junk mail control setting."));
	}

	filter_info_free(fltinfo);
	g_slist_free(rule.action_list);
	g_free(file);
}

void summary_junk(SummaryView *summaryview)
{
	FolderItem *junk = NULL;

	if (!prefs_common.enable_junk)
		return;
	if (!prefs_common.junk_learncmd)
		return;

	debug_print("summary_junk: setting selected mails as junk\n");

	if (prefs_common.junk_folder)
		junk = folder_find_item_from_identifier
			(prefs_common.junk_folder);
	if (!junk &&
	    summaryview->folder_item && summaryview->folder_item->folder)
		junk = folder_get_junk(summaryview->folder_item->folder);
	if (!junk)
		junk = folder_get_default_junk();
	if (!junk) {
		g_warning("summary_junk: junk folder not found");
		return;
	}

	summary_lock(summaryview);

	summaryview->to_folder = junk;
	gtk_tree_selection_selected_foreach(summaryview->selection,
					    summary_junk_func, summaryview);
	summaryview->to_folder = NULL;

	summary_unlock(summaryview);

	if (junk && prefs_common.immediate_exec)
		summary_execute(summaryview);
	else {
		summary_step(summaryview, GTK_SCROLL_STEP_FORWARD);
		summary_status_show(summaryview);
	}

	folderview_update_all_updated(FALSE);
}

void summary_not_junk(SummaryView *summaryview)
{
	if (!prefs_common.enable_junk)
		return;
	if (!prefs_common.nojunk_learncmd)
		return;

	summary_lock(summaryview);

	debug_print("Set mail as not junk\n");

	gtk_tree_selection_selected_foreach(summaryview->selection,
					    summary_not_junk_func, summaryview);

	summary_unlock(summaryview);
}

void summary_reply(SummaryView *summaryview, ComposeMode mode)
{
	GSList *mlist;
	MsgInfo *msginfo;
	MsgInfo *displayed_msginfo = NULL;
	gchar *text = NULL;

	mlist = summary_get_selected_msg_list(summaryview);
	if (!mlist) return;
	msginfo = (MsgInfo *)mlist->data;

	if (summaryview->displayed) {
		GtkTreeIter iter;

		if (gtkut_tree_row_reference_get_iter
			(GTK_TREE_MODEL(summaryview->store),
			 summaryview->displayed, &iter)) {
			GET_MSG_INFO(displayed_msginfo, &iter);
		}
	}

	/* use selection only if the displayed message is selected */
	if (!mlist->next && msginfo == displayed_msginfo) {
		TextView *textview;

		textview = messageview_get_current_textview
			(summaryview->messageview);
		if (textview) {
			text = gtkut_text_view_get_selection
				(GTK_TEXT_VIEW(textview->text));
			if (text && *text == '\0') {
				g_free(text);
				text = NULL;
			}
		}
	}

	if (prefs_common.reply_with_quote)
		mode |= COMPOSE_WITH_QUOTE;

	switch (COMPOSE_MODE(mode)) {
	case COMPOSE_REPLY:
	case COMPOSE_REPLY_TO_SENDER:
	case COMPOSE_REPLY_TO_ALL:
	case COMPOSE_REPLY_TO_LIST:
		compose_reply(msginfo, summaryview->folder_item, mode, text);
		break;
	case COMPOSE_FORWARD:
		compose_forward(mlist, summaryview->folder_item, FALSE, text);
		break;
	case COMPOSE_FORWARD_AS_ATTACH:
		compose_forward(mlist, summaryview->folder_item, TRUE, NULL);
		break;
	case COMPOSE_REDIRECT:
		compose_redirect(msginfo, summaryview->folder_item);
		break;
	default:
		g_warning("summary_reply(): invalid mode: %d\n", mode);
	}

	summary_update_selected_rows(summaryview);
	g_free(text);
	g_slist_free(mlist);
}

/* color label */

#define N_COLOR_LABELS colorlabel_get_color_count()

static void summary_colorlabel_menu_item_activate_cb(GtkWidget *widget,
						     gpointer data)
{
	guint color = GPOINTER_TO_UINT(data);
	SummaryView *summaryview;

	summaryview = g_object_get_data(G_OBJECT(widget), "summaryview");
	g_return_if_fail(summaryview != NULL);

	/* "dont_toggle" state set? */
	if (g_object_get_data(G_OBJECT(summaryview->colorlabel_menu),
			      "dont_toggle"))
		return;

	summary_set_colorlabel(summaryview, color, NULL);
}

/* summary_set_colorlabel() - labelcolor parameter is the color *flag*
 * for the messsage; not the color index */
void summary_set_colorlabel(SummaryView *summaryview, guint labelcolor,
			    GtkWidget *widget)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GList *rows, *cur;
	GtkTreeIter iter;
	MsgInfo *msginfo;
	FolderSortKey sort_key = SORT_BY_NONE;
	FolderSortType sort_type = SORT_ASCENDING;

	if (FOLDER_TYPE(summaryview->folder_item->folder) == F_IMAP &&
	    summary_is_read_locked(summaryview))
		return;

	summary_lock(summaryview);
	SORT_BLOCK(SORT_BY_LABEL);

	rows = summary_get_selected_rows(summaryview);
	for (cur = rows; cur != NULL; cur = cur->next) {
		gtk_tree_model_get_iter(model, &iter, (GtkTreePath *)cur->data);
		gtk_tree_model_get(model, &iter, S_COL_MSG_INFO, &msginfo, -1);

		MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_CLABEL_FLAG_MASK);
		MSG_SET_COLORLABEL_VALUE(msginfo->flags, labelcolor);
		MSG_SET_TMP_FLAGS(msginfo->flags, MSG_FLAG_CHANGED);
		summary_set_row(summaryview, &iter, msginfo);
	}

	if (FOLDER_TYPE(summaryview->folder_item->folder) == F_IMAP) {
		GSList *msglist;

		msglist = summary_get_selected_msg_list(summaryview);
		imap_msg_list_set_colorlabel_flags(msglist, labelcolor);
		g_slist_free(msglist);
	}

	if (rows)
		summaryview->folder_item->mark_dirty = TRUE;

	SORT_UNBLOCK(SORT_BY_LABEL);
	summary_unlock(summaryview);
}

static void summary_colorlabel_menu_item_activate_item_cb(GtkMenuItem *menuitem,
							  gpointer data)
{
	SummaryView *summaryview;
	GtkMenuShell *menu;
	GtkCheckMenuItem **items;
	gint n;
	GList *menu_cur;
	GSList *mlist, *cur;

	summaryview = (SummaryView *)data;
	g_return_if_fail(summaryview != NULL);

	menu = GTK_MENU_SHELL(summaryview->colorlabel_menu);
	g_return_if_fail(menu != NULL);

	mlist = summary_get_selected_msg_list(summaryview);
	if (!mlist) return;

	items = (GtkCheckMenuItem **)g_new(GtkWidget *, N_COLOR_LABELS + 1);

	/* NOTE: don't return prematurely because we set the "dont_toggle"
	 * state for check menu items */
	g_object_set_data(G_OBJECT(menu), "dont_toggle", GINT_TO_POINTER(1));

	/* clear items. get item pointers. */
	for (n = 0, menu_cur = menu->children; menu_cur != NULL;
	     menu_cur = menu_cur->next) {
		if (GTK_IS_CHECK_MENU_ITEM(menu_cur->data)) {
			gtk_check_menu_item_set_active
				(GTK_CHECK_MENU_ITEM(menu_cur->data), FALSE);
			items[n] = GTK_CHECK_MENU_ITEM(menu_cur->data);
			n++;
		}
	}

	if (n == (N_COLOR_LABELS + 1)) {
		/* iterate all messages and set the state of the appropriate
		 * items */
		for (cur = mlist; cur != NULL; cur = cur->next) {
			MsgInfo *msginfo = (MsgInfo *)cur->data;
			gint clabel;

			if (msginfo) {
				clabel = MSG_GET_COLORLABEL_VALUE
					(msginfo->flags);
				if (!gtk_check_menu_item_get_active
					(items[clabel]))
					gtk_check_menu_item_set_active
						(items[clabel], TRUE);
			}
		}
	} else
		g_warning("invalid number of color elements (%d)\n", n);

	/* reset "dont_toggle" state */
	g_object_set_data(G_OBJECT(menu), "dont_toggle", GINT_TO_POINTER(0));

	g_free(items);
}

static void summary_colorlabel_menu_create(SummaryView *summaryview)
{
	GtkWidget *label_menuitem;
	GtkWidget *menu;
	GtkWidget *item;
	gint i;

	label_menuitem = gtk_item_factory_get_item(summaryview->popupfactory,
						   "/Color label");
	g_signal_connect(G_OBJECT(label_menuitem), "activate",
			 G_CALLBACK(summary_colorlabel_menu_item_activate_item_cb),
			 summaryview);
	gtk_widget_show(label_menuitem);

	menu = gtk_menu_new();

	/* create sub items. for the menu item activation callback we pass the
	 * color flag value as data parameter. Also we attach a data pointer
	 * so we can always get back the SummaryView pointer. */

	item = gtk_check_menu_item_new_with_label(_("None"));
	gtk_menu_append(GTK_MENU(menu), item);
	g_signal_connect(G_OBJECT(item), "activate",
			 G_CALLBACK(summary_colorlabel_menu_item_activate_cb),
			 GUINT_TO_POINTER(0));
	g_object_set_data(G_OBJECT(item), "summaryview", summaryview);
	gtk_widget_show(item);

	item = gtk_menu_item_new();
	gtk_menu_append(GTK_MENU(menu), item);
	gtk_widget_show(item);

	/* create pixmap/label menu items */
	for (i = 0; i < N_COLOR_LABELS; i++) {
		item = colorlabel_create_check_color_menu_item(i);
		gtk_menu_append(GTK_MENU(menu), item);
		g_signal_connect(G_OBJECT(item), "activate",
				 G_CALLBACK(summary_colorlabel_menu_item_activate_cb),
				 GUINT_TO_POINTER(i + 1));
		g_object_set_data(G_OBJECT(item), "summaryview", summaryview);
		gtk_widget_show(item);
	}

	gtk_widget_show(menu);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(label_menuitem), menu);
	summaryview->colorlabel_menu = menu;
}

static GtkWidget *summary_tree_view_create(SummaryView *summaryview)
{
	GtkWidget *treeview;
	GtkTreeStore *store;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkWidget *image;
	SummaryColumnType type;

	for (type = 0; type < N_SUMMARY_VISIBLE_COLS; type++)
		summaryview->columns[type] = NULL;

	store = gtk_tree_store_new(N_SUMMARY_COLS,
				   GDK_TYPE_PIXBUF,
				   GDK_TYPE_PIXBUF,
				   GDK_TYPE_PIXBUF,
				   G_TYPE_STRING,
				   G_TYPE_STRING,
				   G_TYPE_STRING,
				   G_TYPE_STRING,
				   G_TYPE_UINT,
				   G_TYPE_STRING,

				   G_TYPE_POINTER,

				   G_TYPE_INT,
				   G_TYPE_UINT,

				   GDK_TYPE_COLOR,
				   G_TYPE_INT);

#define SET_SORT(col, func)						\
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store),	\
					col, func, summaryview, NULL)

	SET_SORT(S_COL_MARK, summary_cmp_by_mark);
	SET_SORT(S_COL_UNREAD, summary_cmp_by_unread);
	SET_SORT(S_COL_MIME, summary_cmp_by_mime);
	SET_SORT(S_COL_SUBJECT, summary_cmp_by_subject);
	SET_SORT(S_COL_FROM, summary_cmp_by_from);
	SET_SORT(S_COL_DATE, summary_cmp_by_date);
	SET_SORT(S_COL_SIZE, summary_cmp_by_size);
	SET_SORT(S_COL_NUMBER, summary_cmp_by_num);
	SET_SORT(S_COL_TO, summary_cmp_by_to);
	SET_SORT(S_COL_LABEL, summary_cmp_by_label);
	SET_SORT(S_COL_TDATE, summary_cmp_by_thread_date);

#undef SET_SORT

	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), TRUE);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview),
				     prefs_common.enable_rules_hint);
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(treeview), FALSE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(treeview), S_COL_SUBJECT);
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(treeview), FALSE);
	g_object_set(treeview, "fixed-height-mode", TRUE, NULL);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);
	gtk_tree_selection_set_select_function(selection, summary_select_func,
					       summaryview, NULL);

#define ADD_COLUMN(title, type, col, text_attr, width, align)		\
{									\
	renderer = gtk_cell_renderer_ ## type ## _new();		\
	g_object_set(renderer, "xalign", align, "ypad", 0, NULL);	\
	column = gtk_tree_view_column_new_with_attributes		\
		(title, renderer, # type , col, NULL);			\
	g_object_set_data(G_OBJECT(column), "column_id",		\
			  GINT_TO_POINTER(col));			\
	summaryview->columns[col] = column;				\
	if (text_attr) {						\
		gtk_tree_view_column_set_attributes			\
			(column, renderer,				\
			 # type, col,					\
			 "foreground-gdk", S_COL_FOREGROUND,		\
			 "weight", S_COL_BOLD,				\
			 NULL);						\
		gtk_tree_view_column_set_resizable(column, TRUE);	\
	}								\
	gtk_tree_view_column_set_alignment(column, align);		\
	gtk_tree_view_column_set_sizing					\
		(column, GTK_TREE_VIEW_COLUMN_FIXED);			\
	gtk_tree_view_column_set_fixed_width(column, width);		\
	gtk_tree_view_column_set_min_width(column, 8);			\
	gtk_tree_view_column_set_clickable(column, TRUE);		\
	/* gtk_tree_view_column_set_sort_column_id(column, col); */	\
	gtk_tree_view_column_set_reorderable(column, TRUE);		\
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);	\
	g_signal_connect(G_OBJECT(column->button), "clicked",		\
			 G_CALLBACK(summary_column_clicked),		\
			 summaryview);					\
	g_signal_connect(G_OBJECT(column->button), "size-allocate",	\
			 G_CALLBACK(summary_col_resized), summaryview);	\
}

	ADD_COLUMN(NULL, pixbuf, S_COL_MARK, FALSE,
		   SUMMARY_COL_MARK_WIDTH, 0.5);
	image = stock_pixbuf_widget(treeview, STOCK_PIXMAP_MARK);
	gtk_widget_show(image);
	gtk_tree_view_column_set_widget(column, image);
	ADD_COLUMN(NULL, pixbuf, S_COL_UNREAD, FALSE,
		   SUMMARY_COL_UNREAD_WIDTH, 0.5);
	image = stock_pixbuf_widget(treeview, STOCK_PIXMAP_MAIL_SMALL);
	gtk_widget_show(image);
	gtk_tree_view_column_set_widget(column, image);
	ADD_COLUMN(NULL, pixbuf, S_COL_MIME, FALSE,
		   SUMMARY_COL_MIME_WIDTH, 0.5);
	image = stock_pixbuf_widget(treeview, STOCK_PIXMAP_CLIP);
	gtk_widget_show(image);
	gtk_tree_view_column_set_widget(column, image);

	ADD_COLUMN(_("Subject"), text, S_COL_SUBJECT, TRUE,
		   prefs_common.summary_col_size[S_COL_SUBJECT], 0.0);
#if GTK_CHECK_VERSION(2, 14, 0)
	gtk_tree_view_column_set_expand(column, TRUE);
#endif
	gtk_tree_view_set_expander_column(GTK_TREE_VIEW(treeview), column);
	ADD_COLUMN(_("From"), text, S_COL_FROM, TRUE,
		   prefs_common.summary_col_size[S_COL_FROM], 0.0);
	ADD_COLUMN(_("Date"), text, S_COL_DATE, TRUE,
		   prefs_common.summary_col_size[S_COL_DATE], 0.0);
	ADD_COLUMN(_("Size"), text, S_COL_SIZE, TRUE,
		   prefs_common.summary_col_size[S_COL_SIZE], 1.0);
	ADD_COLUMN(_("No."), text, S_COL_NUMBER, TRUE,
		   prefs_common.summary_col_size[S_COL_NUMBER], 1.0);
	ADD_COLUMN(_("To"), text, S_COL_TO, TRUE,
		   prefs_common.summary_col_size[S_COL_TO], 0.0);

#undef ADD_COLUMN

	/* add rightmost empty column */
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_min_width(column, 0);
	gtk_tree_view_column_set_max_width(column, 0);
	gtk_tree_view_column_set_clickable(column, FALSE);
	gtk_tree_view_column_set_reorderable(column, FALSE);
	gtk_tree_view_set_column_drag_function(GTK_TREE_VIEW(treeview),
					       summary_column_drop_func,
					       summaryview, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	g_object_set_data(G_OBJECT(treeview), "user_data", summaryview);

	g_signal_connect(G_OBJECT(treeview), "button_press_event",
			 G_CALLBACK(summary_button_pressed), summaryview);
	g_signal_connect(G_OBJECT(treeview), "button_release_event",
			 G_CALLBACK(summary_button_released), summaryview);
	g_signal_connect(G_OBJECT(treeview), "key_press_event",
			 G_CALLBACK(summary_key_pressed), summaryview);

	g_signal_connect(G_OBJECT(selection), "changed",
			 G_CALLBACK(summary_selection_changed), summaryview);

	g_signal_connect(G_OBJECT(treeview), "row-expanded",
			 G_CALLBACK(summary_row_expanded), summaryview);
	g_signal_connect(G_OBJECT(treeview), "row-collapsed",
			 G_CALLBACK(summary_row_collapsed), summaryview);

	g_signal_connect(G_OBJECT(treeview), "columns-changed",
			 G_CALLBACK(summary_columns_changed), summaryview);

	gtk_tree_view_enable_model_drag_source
		(GTK_TREE_VIEW(treeview),
		 GDK_BUTTON1_MASK, summary_drag_types, N_DRAG_TYPES,
		 GDK_ACTION_MOVE | GDK_ACTION_COPY);

	g_signal_connect_after(G_OBJECT(treeview), "drag-begin",
			 G_CALLBACK(summary_drag_begin), summaryview);
	g_signal_connect_after(G_OBJECT(treeview), "drag-end",
			 G_CALLBACK(summary_drag_end), summaryview);
	g_signal_connect(G_OBJECT(treeview), "drag-data-get",
			 G_CALLBACK(summary_drag_data_get), summaryview);

	return treeview;
}

void summary_set_column_order(SummaryView *summaryview)
{
	const SummaryColumnState *col_state;
	SummaryColumnType type;
	GtkTreeViewColumn *column, *last_column = NULL;
	gint pos;

	g_signal_handlers_block_by_func(summaryview->treeview,
					summary_columns_changed, summaryview);

	col_state = prefs_summary_column_get_config
		(FOLDER_ITEM_IS_SENT_FOLDER(summaryview->folder_item));

	for (pos = 0; pos < N_SUMMARY_VISIBLE_COLS; pos++) {
		summaryview->col_state[pos] = col_state[pos];
		type = col_state[pos].type;
		summaryview->col_pos[type] = pos;
		column = summaryview->columns[type];

		gtk_tree_view_move_column_after
			(GTK_TREE_VIEW(summaryview->treeview),
			 column, last_column);
		gtk_tree_view_column_set_visible
			(column, col_state[pos].visible);
		last_column = column;

		debug_print("summary_set_column_order: "
			    "pos %d : type %d, vislble %d\n",
			    pos, type, summaryview->col_state[pos].visible);
	}

	g_signal_handlers_unblock_by_func(summaryview->treeview,
					  summary_columns_changed, summaryview);
}

void summary_get_column_order(SummaryView *summaryview)
{
	GtkTreeViewColumn *column;
	GList *columns, *cur;
	gint pos = 0;
	SummaryColumnType type;
	gboolean visible;

	columns = gtk_tree_view_get_columns
		(GTK_TREE_VIEW(summaryview->treeview));

	for (cur = columns; cur != NULL && pos < N_SUMMARY_VISIBLE_COLS;
	     cur = cur->next, pos++) {
		column = (GtkTreeViewColumn *)cur->data;
		type = GPOINTER_TO_INT
			(g_object_get_data(G_OBJECT(column), "column_id"));
		if (type < 0 || type >= N_SUMMARY_VISIBLE_COLS) {
			g_warning("summary_get_column_order: "
				  "invalid type: %d\n", type);
			break;
		}
		visible = gtk_tree_view_column_get_visible(column);
		summaryview->col_state[pos].type = type;
		summaryview->col_state[pos].visible = visible;
		summaryview->col_pos[type] = pos;
		debug_print("summary_get_column_order: "
			    "pos: %d, type: %d, visible: %d\n",
			    pos, type, visible);
	}

	prefs_summary_column_set_config
		(summaryview->col_state,
		 FOLDER_ITEM_IS_SENT_FOLDER(summaryview->folder_item));

	g_list_free(columns);
}

void summary_qsearch_reset(SummaryView *summaryview)
{
	guint selected_msgnum = 0;
	guint displayed_msgnum = 0;

	if (!summaryview->on_filter)
		return;
	if (!summaryview->folder_item)
		return;

	if (summary_is_read_locked(summaryview)) return;

	g_signal_handlers_block_matched(G_OBJECT(summaryview->treeview),
					(GSignalMatchType)G_SIGNAL_MATCH_DATA,
					0, 0, NULL, NULL, summaryview);

	quick_search_clear_entry(summaryview->qsearch);
	gtk_option_menu_set_history
		(GTK_OPTION_MENU(summaryview->qsearch->optmenu), 0);
	summaryview->folder_item->qsearch_cond_type = QS_ALL;

	selected_msgnum = summary_get_msgnum(summaryview,
					     summaryview->selected);
	displayed_msgnum = summary_get_msgnum(summaryview,
					      summaryview->displayed);

	summaryview->on_filter = FALSE;
	g_slist_free(summaryview->flt_mlist);
	summaryview->flt_mlist = NULL;
	summaryview->total_flt_msg_size = 0;
	summaryview->flt_msg_total = 0;
	summaryview->flt_deleted = 0;
	summaryview->flt_moved = 0;
	summaryview->flt_copied = 0;
	summaryview->flt_new = 0;
	summaryview->flt_unread = 0;

	main_window_cursor_wait(summaryview->mainwin);
	summary_lock(summaryview);

	gtkut_tree_view_fast_clear(GTK_TREE_VIEW(summaryview->treeview),
				   summaryview->store);
	summary_unset_sort_column_id(summaryview);
	summaryview->total_size = 0;
	summary_set_tree_model_from_list(summaryview, summaryview->all_mlist);
	summary_selection_list_free(summaryview);

	summary_unlock(summaryview);
	main_window_cursor_normal(summaryview->mainwin);

	g_signal_handlers_unblock_matched(G_OBJECT(summaryview->treeview),
					  (GSignalMatchType)G_SIGNAL_MATCH_DATA,
					  0, 0, NULL, NULL, summaryview);

	summary_update_display_state(summaryview, displayed_msgnum,
				     selected_msgnum);

	summary_update_status(summaryview);
	summary_status_show(summaryview);
	summary_set_menu_sensitive(summaryview);
	main_window_set_toolbar_sensitive(summaryview->mainwin);
}

void summary_qsearch_clear_entry(SummaryView *summaryview)
{
	if (summary_is_read_locked(summaryview))
		return;
	quick_search_clear_entry(summaryview->qsearch);
	summary_qsearch(summaryview);
}

void summary_qsearch(SummaryView *summaryview)
{
	QSearchCondType type;
	GtkWidget *menuitem;
	const gchar *key = NULL;
	GSList *flt_mlist;
	guint selected_msgnum = 0;
	guint displayed_msgnum = 0;

	if (!summaryview->folder_item)
		return;

	if (summary_is_read_locked(summaryview)) return;

	menuitem = gtk_menu_get_active(GTK_MENU(summaryview->qsearch->menu));
	type = GPOINTER_TO_INT
		(g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID));
	summaryview->folder_item->qsearch_cond_type = type;

	if (!summaryview->all_mlist)
		return;

	if (summaryview->qsearch->entry_entered)
		key = gtk_entry_get_text
			(GTK_ENTRY(summaryview->qsearch->entry));
	if (type == QS_ALL && (!key || *key == '\0')) {
		summary_qsearch_reset(summaryview);
		return;
	}

	selected_msgnum = summary_get_msgnum(summaryview,
					     summaryview->selected);
	displayed_msgnum = summary_get_msgnum(summaryview,
					      summaryview->displayed);

	summaryview->on_filter = FALSE;
	g_slist_free(summaryview->flt_mlist);
	summaryview->flt_mlist = NULL;
	summaryview->total_flt_msg_size = 0;
	summaryview->flt_msg_total = 0;
	summaryview->flt_deleted = 0;
	summaryview->flt_moved = 0;
	summaryview->flt_copied = 0;
	summaryview->flt_new = 0;
	summaryview->flt_unread = 0;

	main_window_cursor_wait(summaryview->mainwin);
	summary_lock(summaryview);

	flt_mlist = quick_search_filter(summaryview->qsearch, type, key);
	summaryview->on_filter = TRUE;
	summaryview->flt_mlist = flt_mlist;

	g_signal_handlers_block_matched(G_OBJECT(summaryview->treeview),
					(GSignalMatchType)G_SIGNAL_MATCH_DATA,
					0, 0, NULL, NULL, summaryview);

	gtkut_tree_view_fast_clear(GTK_TREE_VIEW(summaryview->treeview),
				   summaryview->store);
	summary_unset_sort_column_id(summaryview);
	summaryview->total_size = 0;
	summary_set_tree_model_from_list(summaryview, flt_mlist);
	summary_selection_list_free(summaryview);

	g_signal_handlers_unblock_matched(G_OBJECT(summaryview->treeview),
					  (GSignalMatchType)G_SIGNAL_MATCH_DATA,
					  0, 0, NULL, NULL, summaryview);

	summary_unlock(summaryview);
	summary_update_display_state(summaryview, displayed_msgnum,
				     selected_msgnum);

	main_window_cursor_normal(summaryview->mainwin);
	summary_update_status(summaryview);
	summary_status_show(summaryview);
	summary_set_menu_sensitive(summaryview);
	main_window_set_toolbar_sensitive(summaryview->mainwin);
}

void summary_mark_displayed_read(SummaryView *summaryview, GtkTreeIter *iter)
{
	MsgInfo *msginfo = NULL;
	GtkTreeIter iter_;

	if (summary_is_read_locked(summaryview)) return;

	if (prefs_common.mark_as_read_on_new_window)
		return;

	if (!iter) {
		if (!gtkut_tree_row_reference_get_iter
			(GTK_TREE_MODEL(summaryview->store),
			 summaryview->displayed, &iter_))
			return;
		iter = &iter_;
	}

	GET_MSG_INFO(msginfo, iter);
	if (!msginfo)
		return;

	summary_lock(summaryview);

	if (MSG_IS_NEW(msginfo->flags) ||
	    MSG_IS_UNREAD(msginfo->flags)) {
		summary_mark_row_as_read(summaryview, iter);
		if (MSG_IS_IMAP(msginfo->flags))
			imap_msg_unset_perm_flags
				(msginfo, MSG_NEW | MSG_UNREAD);
		summary_set_row(summaryview, iter, msginfo);
		summary_status_show(summaryview);

		/* sort order can be changed here */
		if (summaryview->folder_item->sort_key == SORT_BY_UNREAD)
			summary_selection_list_free(summaryview);
	}

	summary_unlock(summaryview);
}


/* callback functions */

static gboolean summary_toggle_pressed(GtkWidget *eventbox,
				       GdkEventButton *event,
				       SummaryView *summaryview)
{
	if (!event) return FALSE;

	summary_toggle_view(summaryview);
	return FALSE;
}

static gboolean summary_button_pressed(GtkWidget *treeview,
				       GdkEventButton *event,
				       SummaryView *summaryview)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeViewColumn *column = NULL;
	gboolean is_selected;
	gboolean mod_pressed;
	gint px, py;

	if (!event) return FALSE;

	if (summaryview->folder_item && summaryview->folder_item->folder &&
	    FOLDER_TYPE(summaryview->folder_item->folder) == F_IMAP &&
	    summary_is_locked(summaryview))
		return TRUE;

	if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(treeview),
					   event->x, event->y, &path, &column,
					   NULL, NULL))
		return FALSE;

	/* pass through if the border of column titles is clicked */
	gtk_widget_get_pointer(treeview, &px, &py);
	if (py == (gint)event->y)
		return FALSE;

	if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(summaryview->store),
				     &iter, path))
		return FALSE;

	is_selected = gtk_tree_selection_path_is_selected
		(summaryview->selection, path);
	mod_pressed = ((event->state &
		        (GDK_SHIFT_MASK|GDK_MOD1_MASK|GDK_CONTROL_MASK)) != 0);

	if ((event->button == 1 || event->button == 2)) {
		MsgInfo *msginfo;
		FolderSortKey sort_key = SORT_BY_NONE;
		FolderSortType sort_type = SORT_ASCENDING;

		GET_MSG_INFO(msginfo, &iter);

		if (column == summaryview->columns[S_COL_MARK]) {
			if (MSG_IS_IMAP(msginfo->flags) &&
			    summary_is_locked(summaryview)) {
				gtk_tree_path_free(path);
				return TRUE;
			}

			SORT_BLOCK(SORT_BY_MARK);

			if (!MSG_IS_DELETED(msginfo->flags) &&
			    !MSG_IS_MOVE(msginfo->flags) &&
			    !MSG_IS_COPY(msginfo->flags)) {
				if (MSG_IS_MARKED(msginfo->flags)) {
					summary_unmark_row(summaryview, &iter);
					if (MSG_IS_IMAP(msginfo->flags)) {
						summary_lock(summaryview);
						imap_msg_unset_perm_flags
							(msginfo, MSG_MARKED);
						summary_unlock(summaryview);
					}
				} else {
					summary_mark_row(summaryview, &iter);
					if (MSG_IS_IMAP(msginfo->flags)) {
						summary_lock(summaryview);
						imap_msg_set_perm_flags
							(msginfo, MSG_MARKED);
						summary_unlock(summaryview);
					}
				}
			}
			gtk_tree_path_free(path);

			SORT_UNBLOCK(SORT_BY_MARK);

			return TRUE;
		} else if (column == summaryview->columns[S_COL_UNREAD]) {
			if (MSG_IS_IMAP(msginfo->flags) &&
			    summary_is_locked(summaryview)) {
				gtk_tree_path_free(path);
				return TRUE;
			}

			SORT_BLOCK(SORT_BY_UNREAD);

			if (MSG_IS_UNREAD(msginfo->flags)) {
				summary_mark_row_as_read(summaryview, &iter);
				if (MSG_IS_IMAP(msginfo->flags)) {
					summary_lock(summaryview);
					imap_msg_unset_perm_flags
						(msginfo, MSG_NEW | MSG_UNREAD);
					summary_unlock(summaryview);
				}
				trayicon_set_tooltip(NULL);
				trayicon_set_notify(FALSE);
				summary_status_show(summaryview);
			} else if (!MSG_IS_REPLIED(msginfo->flags) &&
				   !MSG_IS_FORWARDED(msginfo->flags)) {
				summary_mark_row_as_unread(summaryview, &iter);
				if (MSG_IS_IMAP(msginfo->flags)) {
					summary_lock(summaryview);
					imap_msg_set_perm_flags
						(msginfo, MSG_UNREAD);
					summary_unlock(summaryview);
				}
				summary_status_show(summaryview);
			}
			gtk_tree_path_free(path);

			SORT_UNBLOCK(SORT_BY_UNREAD);

			return TRUE;
		}
	}

	if (event->button == 1) {
		summaryview->on_button_press = TRUE;

		if (summary_get_selection_type(summaryview) ==
			SUMMARY_SELECTED_MULTIPLE && is_selected &&
			!mod_pressed) {
			summaryview->can_toggle_selection = FALSE;
			summaryview->pressed_path = gtk_tree_path_copy(path);
		} else {
			if (event->type == GDK_2BUTTON_PRESS && is_selected)
				summary_activate_selected(summaryview);
			else if (summary_get_selection_type(summaryview) ==
				 SUMMARY_SELECTED_SINGLE && is_selected &&
				 !mod_pressed &&
				 summary_row_is_displayed(summaryview, &iter)) {
				summary_mark_displayed_read(summaryview, &iter);
			} else {
				summaryview->can_toggle_selection = TRUE;
				if (!mod_pressed &&
				    messageview_is_visible(summaryview->messageview))
					summaryview->display_msg = TRUE;
			}
		}

		if (summaryview->on_button_press == FALSE) {
			/* button released within sub event loop */
			gtk_tree_path_free(path);
			return TRUE;
		}
	} else if (event->button == 2) {
		summary_select_row(summaryview, &iter, TRUE, FALSE);
		summary_mark_displayed_read(summaryview, &iter);
		gtk_tree_path_free(path);
		return TRUE;
	} else if (event->button == 3) {
		/* right clicked */
		syl_plugin_signal_emit("summaryview-menu-popup",
				       summaryview->popupfactory);
		gtk_menu_popup(GTK_MENU(summaryview->popupmenu), NULL, NULL,
			       NULL, NULL, event->button, event->time);
		if (is_selected) {
			gtk_tree_path_free(path);
			return TRUE;
		}
	}

	gtk_tree_path_free(path);

	return FALSE;
}

static gboolean summary_button_released(GtkWidget *treeview,
					GdkEventButton *event,
					SummaryView *summaryview)
{
	if (!summaryview->can_toggle_selection && !summaryview->on_drag &&
	    summaryview->pressed_path) {
		summaryview->can_toggle_selection = TRUE;
		summaryview->display_msg =
			messageview_is_visible(summaryview->messageview);
		gtk_tree_view_set_cursor(GTK_TREE_VIEW(treeview),
					 summaryview->pressed_path,
					 NULL, FALSE);
	}

	summaryview->on_button_press = FALSE;
	summaryview->can_toggle_selection = TRUE;
	summaryview->on_drag = FALSE;
	if (summaryview->pressed_path) {
		gtk_tree_path_free(summaryview->pressed_path);
		summaryview->pressed_path = NULL;
	}

	return FALSE;
}

void summary_pass_key_press_event(SummaryView *summaryview, GdkEventKey *event)
{
	summary_key_pressed(summaryview->treeview, event, summaryview);
}

#define BREAK_ON_MODIFIER_KEY() \
	if ((event->state & (GDK_MOD1_MASK|GDK_CONTROL_MASK)) != 0) break

static gboolean summary_key_pressed(GtkWidget *widget, GdkEventKey *event,
				    SummaryView *summaryview)
{
	MessageView *messageview;
	TextView *textview;
	GtkAdjustment *adj;
	gboolean mod_pressed;
	gboolean scrolled;

	if (!event) return FALSE;

	if (summary_is_read_locked(summaryview)) {
		switch (event->keyval) {
		case GDK_Home:
		case GDK_End:
		case GDK_Up:
		case GDK_Down:
		case GDK_Page_Up:
		case GDK_Page_Down:
			return TRUE;
		default:
			break;
		}
		return FALSE;
	}

	switch (event->keyval) {
	case GDK_Left:		/* Move focus */
	case GDK_KP_Left:
		adj = gtk_scrolled_window_get_hadjustment
			(GTK_SCROLLED_WINDOW(summaryview->scrolledwin));
		if (adj->lower != adj->value)
			return FALSE;
		/* FALLTHROUGH */
	case GDK_Escape:
		gtk_widget_grab_focus(summaryview->folderview->treeview);
		return TRUE;
	default:
		break;
	}

	if (!summaryview->selected)
		return FALSE;

	messageview = summaryview->messageview;
	if (messageview->type == MVIEW_MIME &&
	    gtk_notebook_get_current_page
		(GTK_NOTEBOOK(messageview->notebook)) == 1)
		textview = messageview->mimeview->textview;
	else
		textview = messageview->textview;

	mod_pressed =
		((event->state & (GDK_SHIFT_MASK|GDK_MOD1_MASK)) != 0);

	switch (event->keyval) {
	case GDK_space:		/* Page down or go to the next */
	case GDK_KP_Space:
		if (summaryview->selected &&
		    !gtkut_tree_row_reference_equal(summaryview->displayed,
						    summaryview->selected)) {
			summary_display_msg_selected(summaryview, FALSE, FALSE);
			summary_mark_displayed_read(summaryview, NULL);
		} else if (mod_pressed) {
			scrolled = textview_scroll_page(textview, TRUE);
			if (!scrolled)
				summary_select_prev_unread(summaryview);
		} else {
			scrolled = textview_scroll_page(textview, FALSE);
			summary_mark_displayed_read(summaryview, NULL);
			if (!scrolled)
				summary_select_next_unread(summaryview);
		}
		return TRUE;
	case GDK_BackSpace:	/* Page up */
		textview_scroll_page(textview, TRUE);
		return TRUE;
	case GDK_Return:	/* Scroll up/down one line */
	case GDK_KP_Enter:
		if (summaryview->selected &&
		    !gtkut_tree_row_reference_equal(summaryview->displayed,
						    summaryview->selected))
			summary_display_msg_selected(summaryview, FALSE, FALSE);
		else
			textview_scroll_one_line(textview, mod_pressed);
		summary_mark_displayed_read(summaryview, NULL);
		return TRUE;
	case GDK_Delete:
	case GDK_KP_Delete:
		BREAK_ON_MODIFIER_KEY();
		summary_delete(summaryview);
		return TRUE;
	case GDK_F10:
	case GDK_Menu:
		if (event->keyval == GDK_F10 &&
		    (event->state & GDK_SHIFT_MASK) == 0)
			break;
		syl_plugin_signal_emit("summaryview-menu-popup",
				       summaryview->popupfactory);
		gtk_menu_popup(GTK_MENU(summaryview->popupmenu), NULL, NULL,
			       menu_widget_position, summaryview->treeview,
			       0, GDK_CURRENT_TIME);
		return TRUE;
	default:
		break;
	}

	return FALSE;
}

static void summary_set_bold_recursive(SummaryView *summaryview,
				       GtkTreeIter *iter)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter child;
	MsgInfo *msginfo;
	gboolean valid;

	if (!gtk_tree_model_iter_has_child(model, iter))
		return;

	GET_MSG_INFO(msginfo, iter);
	if (!MSG_IS_UNREAD(msginfo->flags)) {
		gtk_tree_store_set(summaryview->store, iter,
				   S_COL_BOLD, PANGO_WEIGHT_NORMAL, -1);
	}

	valid = gtk_tree_model_iter_children(model, &child, iter);
	while (valid) {
		summary_set_bold_recursive(summaryview, &child);
		valid = gtk_tree_model_iter_next(model, &child);
	}
}

static void summary_row_expanded(GtkTreeView *treeview, GtkTreeIter *iter,
				 GtkTreePath *path, SummaryView *summaryview)
{
	gtk_tree_view_expand_row(treeview, path, TRUE);

	if (prefs_common.bold_unread)
		summary_set_bold_recursive(summaryview, iter);

	/* workaround for last row expand problem */
#if GTK_CHECK_VERSION(2, 8, 0)
	gtk_widget_queue_resize(GTK_WIDGET(treeview));
#else
	g_object_set(treeview, "fixed-height-mode", FALSE, NULL);
	gtk_widget_queue_resize(GTK_WIDGET(treeview));
	g_object_set(treeview, "fixed-height-mode", TRUE, NULL);
#endif
}

static void summary_row_collapsed(GtkTreeView *treeview, GtkTreeIter *iter,
				  GtkTreePath *path, SummaryView *summaryview)
{
	if (prefs_common.bold_unread &&
	    summary_have_unread_children(summaryview, iter)) {
		gtk_tree_store_set(summaryview->store, iter,
				   S_COL_BOLD, PANGO_WEIGHT_BOLD, -1);
	}
}

static void summary_columns_changed(GtkTreeView *treeview,
				    SummaryView *summaryview)
{
	summary_get_column_order(summaryview);
}

static gboolean summary_select_func(GtkTreeSelection *selection,
				    GtkTreeModel *model, GtkTreePath *path,
				    gboolean cur_selected, gpointer data)
{
	SummaryView *summaryview = (SummaryView *)data;

	return summaryview->can_toggle_selection &&
	       !summary_is_read_locked(summaryview);
}

static gboolean summary_display_msg_idle_func(gpointer data)
{
	SummaryView *summaryview = (SummaryView *)data;
	GtkTreePath *path;
	GtkTreeIter iter;

	gdk_threads_enter();
	path = gtk_tree_row_reference_get_path(summaryview->selected);
	if (path) {
		gtk_tree_model_get_iter(GTK_TREE_MODEL(summaryview->store),
					&iter, path);
		gtk_tree_path_free(path);
		summary_display_msg(summaryview, &iter);
		summary_mark_displayed_read(summaryview, &iter);
	}
	gdk_threads_leave();

	return FALSE;
}

static void summary_selection_changed(GtkTreeSelection *selection,
				      SummaryView *summaryview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter;
	GtkTreePath *path;
	GList *list;
	gboolean single_selection = FALSE;

	summary_selection_list_free(summaryview);
	list = summary_get_selected_rows(summaryview);

	summary_status_show(summaryview);

	gtk_tree_row_reference_free(summaryview->selected);
	if (list) {
		if (list->next == NULL)
			single_selection = TRUE;
		path = (GtkTreePath *)list->data;
		gtk_tree_model_get_iter(model, &iter, path);
		summaryview->selected =
			gtk_tree_row_reference_new(model, path);
	} else
		summaryview->selected = NULL;

	if (!single_selection) {
		summaryview->display_msg = FALSE;
#if 0
		if (summaryview->displayed && prefs_common.always_show_msg) {
			messageview_clear(summaryview->messageview);
			gtk_tree_row_reference_free(summaryview->displayed);
			summaryview->displayed = NULL;
		}
#endif
		summary_set_menu_sensitive(summaryview);
		main_window_set_toolbar_sensitive(summaryview->mainwin);
		return;
	}

	if (summaryview->display_msg ||
	    (prefs_common.always_show_msg &&
	     messageview_is_visible(summaryview->messageview))) {
		summaryview->display_msg = FALSE;
		if (!gtkut_tree_row_reference_equal(summaryview->displayed,
						    summaryview->selected)) {
			if (summaryview->on_button_press)
				g_idle_add(summary_display_msg_idle_func,
					   summaryview);
			else
				summary_display_msg(summaryview, &iter);
			return;
		}
	}

	summary_set_menu_sensitive(summaryview);
	main_window_set_toolbar_sensitive(summaryview->mainwin);
}

static void summary_col_resized(GtkWidget *widget, GtkAllocation *allocation,
				SummaryView *summaryview)
{
	SummaryColumnType type;

	for (type = 0; type < N_SUMMARY_VISIBLE_COLS; type++) {
		if (summaryview->columns[type]->button == widget) {
			prefs_common.summary_col_size[type] = allocation->width;
			break;
		}
	}
}

static void summary_reply_cb(SummaryView *summaryview, guint action,
			     GtkWidget *widget)
{
	summary_reply(summaryview, (ComposeMode)action);
}

static void summary_show_all_header_cb(SummaryView *summaryview,
				       guint action, GtkWidget *widget)
{
	summary_display_msg_selected(summaryview, FALSE,
				     GTK_CHECK_MENU_ITEM(widget)->active);
}

static void summary_add_address_cb(SummaryView *summaryview,
				   guint action, GtkWidget *widget)
{
	summary_add_address(summaryview);
}

static void summary_create_filter_cb(SummaryView *summaryview,
				     guint action, GtkWidget *widget)
{
	summary_filter_open(summaryview, (FilterCreateType)action);
}

static void summary_sort_by_column_click(SummaryView *summaryview,
					 SummaryColumnType type)
{
	FolderItem *item = summaryview->folder_item;

	if (!item) return;

	if (item->sort_key == col_to_sort_key[type])
		summary_sort(summaryview, col_to_sort_key[type],
			     item->sort_type == SORT_ASCENDING
			     ? SORT_DESCENDING : SORT_ASCENDING);
	else
		summary_sort(summaryview, col_to_sort_key[type],
			     SORT_ASCENDING);
}

static void summary_column_clicked(GtkWidget *button, SummaryView *summaryview)
{
	SummaryColumnType type;

	for (type = 0; type < N_SUMMARY_VISIBLE_COLS; type++) {
		if (summaryview->columns[type]->button == button) {
			summary_sort_by_column_click(summaryview, type);
			break;
		}
	}
}

static gboolean summary_column_drop_func(GtkTreeView *treeview,
					 GtkTreeViewColumn *column,
					 GtkTreeViewColumn *prev_column,
					 GtkTreeViewColumn *next_column,
					 gpointer data)
{
	if (next_column == NULL)
		return FALSE;
	else
		return TRUE;
}

static void summary_drag_begin(GtkWidget *widget, GdkDragContext *drag_context,
			       SummaryView *summaryview)
{
	if (!summaryview->on_button_press)
		g_warning("summary_drag_begin: drag began without button press");
	summaryview->on_drag = TRUE;
	gtk_drag_set_icon_default(drag_context);
}

static void summary_drag_end(GtkWidget *widget, GdkDragContext *drag_context,
			     SummaryView *summaryview)
{
	if (summaryview->drag_list) {
		g_free(summaryview->drag_list);
		summaryview->drag_list = NULL;
	}
}

static void summary_drag_data_get(GtkWidget        *widget,
				  GdkDragContext   *drag_context,
				  GtkSelectionData *selection_data,
				  guint             info,
				  guint             time,
				  SummaryView      *summaryview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GList *rows, *cur;
	gchar *mail_list = NULL;
	gchar *file, *filename, *fs_filename, *tmp;
	gint suffix = 0;
	MsgInfo *msginfo;
	GtkTreeIter iter;

	if (info == DRAG_TYPE_TEXT) {
		gtk_selection_data_set(selection_data, selection_data->target,
				       8, (guchar *)"drag-from-summary", 17);
		return;
	}

	if (!summaryview->drag_list) {
		rows = summary_get_selected_rows(summaryview);

		for (cur = rows; cur != NULL; cur = cur->next) {
			gtk_tree_model_get_iter(model, &iter,
						(GtkTreePath *)cur->data);
			gtk_tree_model_get(model, &iter, S_COL_MSG_INFO,
					   &msginfo, -1);
			file = procmsg_get_message_file(msginfo);
			if (!file) continue;

			if (msginfo->subject && *msginfo->subject != '\0') {
				filename = g_strdup(msginfo->subject);
				subst_for_filename(filename);
			} else
				filename = g_strdup(g_basename(file));
			fs_filename = conv_filename_from_utf8(filename);

			suffix = 0;
			do {
				if (suffix == 0)
					tmp = g_strdup_printf
						("%s%c%s.eml", get_tmp_dir(),
						 G_DIR_SEPARATOR,
						 fs_filename);
				else
					tmp = g_strdup_printf
						("%s%c%s_(%d).eml",
						 get_tmp_dir(),
						 G_DIR_SEPARATOR, fs_filename,
						 suffix);

				if (is_file_exist(tmp)) {
					suffix++;
					g_free(tmp);
				} else
					break;
			} while (1);

			if (copy_file(file, tmp, FALSE) < 0) {
				g_warning("Can't copy '%s'\n", file);
			} else {
				gchar *uri;

				uri = encode_uri(tmp);

				if (!mail_list) {
					mail_list = uri;
				} else {
					gchar *list_tmp;

					list_tmp = g_strconcat
						(mail_list, "\n", uri, NULL);
					g_free(mail_list);
					g_free(uri);
					mail_list = list_tmp;
				}
			}

			g_free(tmp);
			g_free(fs_filename);
			g_free(filename);
			g_free(file);
		}

		summaryview->drag_list = mail_list;
	}

	if (summaryview->drag_list) {
		gtk_selection_data_set(selection_data,
				       selection_data->target, 8,
				       (guchar *)summaryview->drag_list,
				       strlen(summaryview->drag_list));
	}
}

static void summary_text_adj_value_changed(GtkAdjustment *adj,
					   SummaryView *summaryview)
{
	static gdouble prev_vadj = 0.0;

	if (summaryview->displayed && adj->value > prev_vadj &&
	    prefs_common.always_show_msg)
		summary_mark_displayed_read(summaryview, NULL);

	prev_vadj = adj->value;
}


/* custom compare functions for sorting */

#define CMP_FUNC_DEF(func_name, val)					\
static gint func_name(GtkTreeModel *model,				\
		      GtkTreeIter *a, GtkTreeIter *b, gpointer data)	\
{									\
	MsgInfo *msginfo_a = NULL, *msginfo_b = NULL;			\
	gint ret;							\
									\
	gtk_tree_model_get(model, a, S_COL_MSG_INFO, &msginfo_a, -1);	\
	gtk_tree_model_get(model, b, S_COL_MSG_INFO, &msginfo_b, -1);	\
									\
	if (!msginfo_a || !msginfo_b)					\
		return 0;						\
									\
	ret = (val);							\
	return (ret != 0) ? ret :					\
		(msginfo_a->date_t - msginfo_b->date_t);		\
}

CMP_FUNC_DEF(summary_cmp_by_mark,
	     MSG_IS_MARKED(msginfo_a->flags) - MSG_IS_MARKED(msginfo_b->flags))
CMP_FUNC_DEF(summary_cmp_by_unread,
	     MSG_IS_UNREAD(msginfo_a->flags) - MSG_IS_UNREAD(msginfo_b->flags))
CMP_FUNC_DEF(summary_cmp_by_mime,
	     MSG_IS_MIME(msginfo_a->flags) - MSG_IS_MIME(msginfo_b->flags))
CMP_FUNC_DEF(summary_cmp_by_label,
	     MSG_GET_COLORLABEL(msginfo_a->flags) -
	     MSG_GET_COLORLABEL(msginfo_b->flags))
CMP_FUNC_DEF(summary_cmp_by_size, msginfo_a->size - msginfo_b->size)

#undef CMP_FUNC_DEF
#define CMP_FUNC_DEF(func_name, val)					\
static gint func_name(GtkTreeModel *model,				\
		      GtkTreeIter *a, GtkTreeIter *b, gpointer data)	\
{									\
	MsgInfo *msginfo_a = NULL, *msginfo_b = NULL;			\
									\
	gtk_tree_model_get(model, a, S_COL_MSG_INFO, &msginfo_a, -1);	\
	gtk_tree_model_get(model, b, S_COL_MSG_INFO, &msginfo_b, -1);	\
									\
	if (!msginfo_a || !msginfo_b)					\
		return 0;						\
									\
	return (val);							\
}

CMP_FUNC_DEF(summary_cmp_by_num, msginfo_a->msgnum - msginfo_b->msgnum)
CMP_FUNC_DEF(summary_cmp_by_date, msginfo_a->date_t - msginfo_b->date_t)

#undef CMP_FUNC_DEF

static gint summary_cmp_by_thread_date(GtkTreeModel *model,
				       GtkTreeIter *a, GtkTreeIter *b,
				       gpointer data)
{
	MsgInfo *msginfo_a = NULL, *msginfo_b = NULL;
	guint tdate_a, tdate_b;

	gtk_tree_model_get(model, a, S_COL_MSG_INFO, &msginfo_a, S_COL_TDATE,
			   &tdate_a, -1);
	gtk_tree_model_get(model, b, S_COL_MSG_INFO, &msginfo_b, S_COL_TDATE,
			   &tdate_b, -1);

	if (!msginfo_a || !msginfo_b)
		return 0;

	if (tdate_a == 0 && tdate_b == 0)
		return msginfo_a->date_t - msginfo_b->date_t;
	else
		return tdate_a - tdate_b;
}

#define CMP_FUNC_DEF(func_name, var_name)				\
static gint func_name(GtkTreeModel *model,				\
		      GtkTreeIter *a, GtkTreeIter *b, gpointer data)	\
{									\
	MsgInfo *msginfo_a = NULL, *msginfo_b = NULL;			\
	gint ret;							\
									\
	gtk_tree_model_get(model, a, S_COL_MSG_INFO, &msginfo_a, -1);	\
	gtk_tree_model_get(model, b, S_COL_MSG_INFO, &msginfo_b, -1);	\
									\
	if (!msginfo_a || !msginfo_b)					\
		return 0;						\
									\
	if (msginfo_a->var_name == NULL)				\
		return -(msginfo_b->var_name != NULL);			\
	if (msginfo_b->var_name == NULL)				\
		return (msginfo_a->var_name != NULL);			\
									\
	ret = g_ascii_strcasecmp					\
		(msginfo_a->var_name, msginfo_b->var_name);		\
									\
	return (ret != 0) ? ret :					\
		(msginfo_a->date_t - msginfo_b->date_t);		\
}

CMP_FUNC_DEF(summary_cmp_by_from, fromname)

#undef CMP_FUNC_DEF

static gint summary_cmp_by_to(GtkTreeModel *model,
				   GtkTreeIter *a, GtkTreeIter *b,
				   gpointer data)
{
	MsgInfo *msginfo_a = NULL, *msginfo_b = NULL;
	gchar *to_a = NULL, *to_b = NULL;
	gint ret;

	gtk_tree_model_get(model, a, S_COL_MSG_INFO, &msginfo_a,
			   S_COL_TO, &to_a, -1);
	gtk_tree_model_get(model, b, S_COL_MSG_INFO, &msginfo_b,
			   S_COL_TO, &to_b, -1);

	if (!msginfo_a || !msginfo_b) {
		g_free(to_b);
		g_free(to_a);
		return 0;
	}

	ret = g_ascii_strcasecmp(to_a ? to_a : "", to_b ? to_b : "");

	g_free(to_b);
	g_free(to_a);

	return (ret != 0) ? ret :
		(msginfo_a->date_t - msginfo_b->date_t);
}

static gint summary_cmp_by_subject(GtkTreeModel *model,
				   GtkTreeIter *a, GtkTreeIter *b,
				   gpointer data)
{
	MsgInfo *msginfo_a = NULL, *msginfo_b = NULL;
	gint ret;

	gtk_tree_model_get(model, a, S_COL_MSG_INFO, &msginfo_a, -1);
	gtk_tree_model_get(model, b, S_COL_MSG_INFO, &msginfo_b, -1);

	if (!msginfo_a || !msginfo_b)
		return 0;

	if (msginfo_a->subject == NULL)
		return -(msginfo_b->subject != NULL);
	if (msginfo_b->subject == NULL)
		return (msginfo_a->subject != NULL);

	ret = subject_compare_for_sort(msginfo_a->subject, msginfo_b->subject);

	return (ret != 0) ? ret :
		(msginfo_a->date_t - msginfo_b->date_t);
}
