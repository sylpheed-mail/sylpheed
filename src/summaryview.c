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
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkctree.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktext.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkitemfactory.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkstyle.h>
#include <gtk/gtkarrow.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtkstatusbar.h>

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
#include "filter.h"
#include "folder.h"
#include "colorlabel.h"
#include "inc.h"
#include "imap.h"

#define STATUSBAR_PUSH(mainwin, str) \
{ \
	gtk_statusbar_push(GTK_STATUSBAR(mainwin->statusbar), \
			   mainwin->summaryview_cid, str); \
	gtkut_widget_wait_for_draw(mainwin->statusbar); \
}

#define STATUSBAR_POP(mainwin) \
{ \
	gtk_statusbar_pop(GTK_STATUSBAR(mainwin->statusbar), \
			  mainwin->summaryview_cid); \
}

#define SUMMARY_COL_MARK_WIDTH		10
#define SUMMARY_COL_UNREAD_WIDTH	13
#define SUMMARY_COL_MIME_WIDTH		10

static GtkStyle *bold_style;
static GtkStyle *bold_marked_style;
static GtkStyle *bold_deleted_style;

static GdkPixmap *markxpm;
static GdkBitmap *markxpmmask;
static GdkPixmap *deletedxpm;
static GdkBitmap *deletedxpmmask;

static GdkPixmap *mailxpm;
static GdkBitmap *mailxpmmask;
static GdkPixmap *newxpm;
static GdkBitmap *newxpmmask;
static GdkPixmap *unreadxpm;
static GdkBitmap *unreadxpmmask;
static GdkPixmap *repliedxpm;
static GdkBitmap *repliedxpmmask;
static GdkPixmap *forwardedxpm;
static GdkBitmap *forwardedxpmmask;

static GdkPixmap *clipxpm;
static GdkBitmap *clipxpmmask;

static void summary_free_msginfo_func	(GtkCTree		*ctree,
					 GtkCTreeNode		*node,
					 gpointer		 data);
static void summary_set_marks_func	(GtkCTree		*ctree,
					 GtkCTreeNode		*node,
					 gpointer		 data);
static void summary_write_cache_func	(GtkCTree		*ctree,
					 GtkCTreeNode		*node,
					 gpointer		 data);

static void summary_set_menu_sensitive	(SummaryView		*summaryview);

static guint summary_get_msgnum		(SummaryView		*summaryview,
					 GtkCTreeNode		*node);

static GtkCTreeNode *summary_find_prev_msg
					(SummaryView		*summaryview,
					 GtkCTreeNode		*current_node);
static GtkCTreeNode *summary_find_next_msg
					(SummaryView		*summaryview,
					 GtkCTreeNode		*current_node);

static GtkCTreeNode *summary_find_prev_flagged_msg
					(SummaryView	*summaryview,
					 GtkCTreeNode	*current_node,
					 MsgPermFlags	 flags,
					 gboolean	 start_from_prev);
static GtkCTreeNode *summary_find_next_flagged_msg
					(SummaryView	*summaryview,
					 GtkCTreeNode	*current_node,
					 MsgPermFlags	 flags,
					 gboolean	 start_from_next);

static GtkCTreeNode *summary_find_msg_by_msgnum
					(SummaryView		*summaryview,
					 guint			 msgnum);

static void summary_update_status	(SummaryView		*summaryview);

/* display functions */
static void summary_status_show		(SummaryView		*summaryview);
static void summary_set_column_titles	(SummaryView		*summaryview);
static void summary_set_ctree_from_list	(SummaryView		*summaryview,
					 GSList			*mlist);
static void summary_set_header		(SummaryView		*summaryview,
					 gchar			*text[],
					 MsgInfo		*msginfo);
static void summary_display_msg		(SummaryView		*summaryview,
					 GtkCTreeNode		*row);
static void summary_display_msg_full	(SummaryView		*summaryview,
					 GtkCTreeNode		*row,
					 gboolean		 new_window,
					 gboolean		 all_headers);
static void summary_set_row_marks	(SummaryView		*summaryview,
					 GtkCTreeNode		*row);

/* message handling */
static void summary_mark_row		(SummaryView		*summaryview,
					 GtkCTreeNode		*row);
static void summary_mark_row_as_read	(SummaryView		*summaryview,
					 GtkCTreeNode		*row);
static void summary_mark_row_as_unread	(SummaryView		*summaryview,
					 GtkCTreeNode		*row);
static void summary_delete_row		(SummaryView		*summaryview,
					 GtkCTreeNode		*row);
static void summary_unmark_row		(SummaryView		*summaryview,
					 GtkCTreeNode		*row);
static void summary_move_row_to		(SummaryView		*summaryview,
					 GtkCTreeNode		*row,
					 FolderItem		*to_folder);
static void summary_copy_row_to		(SummaryView		*summaryview,
					 GtkCTreeNode		*row,
					 FolderItem		*to_folder);

static void summary_delete_duplicated_func
					(GtkCTree		*ctree,
					 GtkCTreeNode		*node,
					 SummaryView		*summaryview);

static void summary_remove_invalid_messages
					(SummaryView		*summaryview);

static gint summary_execute_move	(SummaryView		*summaryview);
static void summary_execute_move_func	(GtkCTree		*ctree,
					 GtkCTreeNode		*node,
					 gpointer		 data);
static gint summary_execute_copy	(SummaryView		*summaryview);
static void summary_execute_copy_func	(GtkCTree		*ctree,
					 GtkCTreeNode		*node,
					 gpointer		 data);
static gint summary_execute_delete	(SummaryView		*summaryview);
static void summary_execute_delete_func	(GtkCTree		*ctree,
					 GtkCTreeNode		*node,
					 gpointer		 data);

static void summary_thread_init		(SummaryView		*summaryview);

static void summary_unthread_for_exec		(SummaryView	*summaryview);
static void summary_unthread_for_exec_func	(GtkCTree	*ctree,
						 GtkCTreeNode	*node,
						 gpointer	 data);

static void summary_filter_func		(GtkCTree		*ctree,
					 GtkCTreeNode		*node,
					 gpointer		 data);

static void summary_colorlabel_menu_item_activate_cb
					  (GtkWidget	*widget,
					   gpointer	 data);
static void summary_colorlabel_menu_item_activate_item_cb
					  (GtkMenuItem	*label_menu_item,
					   gpointer	 data);
static void summary_colorlabel_menu_create(SummaryView	*summaryview);

static GtkWidget *summary_ctree_create	(SummaryView	*summaryview);

/* callback functions */
static gboolean summary_toggle_pressed	(GtkWidget		*eventbox,
					 GdkEventButton		*event,
					 SummaryView		*summaryview);
static gboolean summary_button_pressed	(GtkWidget		*ctree,
					 GdkEventButton		*event,
					 SummaryView		*summaryview);
static gboolean summary_button_released	(GtkWidget		*ctree,
					 GdkEventButton		*event,
					 SummaryView		*summaryview);
static gboolean summary_key_pressed	(GtkWidget		*ctree,
					 GdkEventKey		*event,
					 SummaryView		*summaryview);
static void summary_open_row		(GtkSCTree		*sctree,
					 SummaryView		*summaryview);
static void summary_tree_expanded	(GtkCTree		*ctree,
					 GtkCTreeNode		*node,
					 SummaryView		*summaryview);
static void summary_tree_collapsed	(GtkCTree		*ctree,
					 GtkCTreeNode		*node,
					 SummaryView		*summaryview);
static void summary_selected		(GtkCTree		*ctree,
					 GtkCTreeNode		*row,
					 gint			 column,
					 SummaryView		*summaryview);
static void summary_col_resized		(GtkCList		*clist,
					 gint			 column,
					 gint			 width,
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

static void summary_mark_clicked	(GtkWidget		*button,
					 SummaryView		*summaryview);
static void summary_unread_clicked	(GtkWidget		*button,
					 SummaryView		*summaryview);
static void summary_mime_clicked	(GtkWidget		*button,
					 SummaryView		*summaryview);
static void summary_num_clicked		(GtkWidget		*button,
					 SummaryView		*summaryview);
static void summary_size_clicked	(GtkWidget		*button,
					 SummaryView		*summaryview);
static void summary_date_clicked	(GtkWidget		*button,
					 SummaryView		*summaryview);
static void summary_from_clicked	(GtkWidget		*button,
					 SummaryView		*summaryview);
static void summary_subject_clicked	(GtkWidget		*button,
					 SummaryView		*summaryview);

static void summary_start_drag		(GtkWidget        *widget, 
					 int button,
					 GdkEvent *event,
					 SummaryView      *summaryview);
static void summary_drag_data_get       (GtkWidget        *widget,
					 GdkDragContext   *drag_context,
					 GtkSelectionData *selection_data,
					 guint             info,
					 guint             time,
					 SummaryView      *summaryview);

/* custom compare functions for sorting */

static gint summary_cmp_by_mark		(GtkCList		*clist,
					 gconstpointer		 ptr1,
					 gconstpointer		 ptr2);
static gint summary_cmp_by_unread	(GtkCList		*clist,
					 gconstpointer		 ptr1,
					 gconstpointer		 ptr2);
static gint summary_cmp_by_mime		(GtkCList		*clist,
					 gconstpointer		 ptr1,
					 gconstpointer		 ptr2);
static gint summary_cmp_by_num		(GtkCList		*clist,
					 gconstpointer		 ptr1,
					 gconstpointer		 ptr2);
static gint summary_cmp_by_size		(GtkCList		*clist,
					 gconstpointer		 ptr1,
					 gconstpointer		 ptr2);
static gint summary_cmp_by_date		(GtkCList		*clist,
					 gconstpointer		 ptr1,
					 gconstpointer		 ptr2);
static gint summary_cmp_by_from		(GtkCList		*clist,
					 gconstpointer		 ptr1,
					 gconstpointer		 ptr2);
static gint summary_cmp_by_label	(GtkCList		*clist,
					 gconstpointer		 ptr1,
					 gconstpointer		 ptr2);
static gint summary_cmp_by_to		(GtkCList		*clist,
					 gconstpointer		 ptr1,
					 gconstpointer		 ptr2);
static gint summary_cmp_by_subject	(GtkCList		*clist,
					 gconstpointer		 ptr1,
					 gconstpointer		 ptr2);

GtkTargetEntry summary_drag_types[1] =
{
	{"text/plain", GTK_TARGET_SAME_APP, TARGET_DUMMY}
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
	{N_("/_Delete"),		NULL, summary_delete,	0, NULL},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/_Mark"),			NULL, NULL,		0, "<Branch>"},
	{N_("/_Mark/_Mark"),		NULL, summary_mark,	0, NULL},
	{N_("/_Mark/_Unmark"),		NULL, summary_unmark,	0, NULL},
	{N_("/_Mark/---"),		NULL, NULL,		0, "<Separator>"},
	{N_("/_Mark/Mark as unr_ead"),	NULL, summary_mark_as_unread, 0, NULL},
	{N_("/_Mark/Mark as rea_d"),
					NULL, summary_mark_as_read, 0, NULL},
	{N_("/_Mark/Mark all _read"),	NULL, summary_mark_all_read, 0, NULL},
	{N_("/Color la_bel"),		NULL, NULL,		0, NULL},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/Re-_edit"),		NULL, summary_reedit,   0, NULL},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/Add sender to address boo_k"),
					NULL, summary_add_address_cb, 0, NULL},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/_View"),			NULL, NULL,		0, "<Branch>"},
	{N_("/_View/Open in new _window"),
					NULL, summary_open_msg,	0, NULL},
	{N_("/_View/_Source"),		NULL, summary_view_source, 0, NULL},
	{N_("/_View/All _header"),	NULL, summary_show_all_header_cb, 0, "<ToggleItem>"},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/_Print..."),		NULL, summary_print,	0, NULL}
};

static const gchar *const col_label[N_SUMMARY_COLS] = {
	N_("M"),	/* S_COL_MARK    */
	N_("U"),	/* S_COL_UNREAD  */
	"",		/* S_COL_MIME    */
	N_("Subject"),	/* S_COL_SUBJECT */
	N_("From"),	/* S_COL_FROM    */
	N_("Date"),	/* S_COL_DATE    */
	N_("Size"),	/* S_COL_SIZE    */
	N_("No."),	/* S_COL_NUMBER  */
};

SummaryView *summary_create(void)
{
	SummaryView *summaryview;
	GtkWidget *vbox;
	GtkWidget *scrolledwin;
	GtkWidget *ctree;
	GtkWidget *hbox;
	GtkWidget *hbox_l;
	GtkWidget *statlabel_folder;
	GtkWidget *statlabel_select;
	GtkWidget *statlabel_msgs;
	GtkWidget *hbox_spc;
	GtkWidget *toggle_eventbox;
	GtkWidget *toggle_arrow;
	GtkWidget *popupmenu;
	GtkItemFactory *popupfactory;
	gint n_entries;
	GList *child;

	debug_print(_("Creating summary view...\n"));
	summaryview = g_new0(SummaryView, 1);

	vbox = gtk_vbox_new(FALSE, 2);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_ALWAYS);
	gtk_box_pack_start(GTK_BOX(vbox), scrolledwin, TRUE, TRUE, 0);
	gtk_widget_set_size_request(vbox,
				    prefs_common.summaryview_width,
				    prefs_common.summaryview_height);

	ctree = summary_ctree_create(summaryview);

	gtk_scrolled_window_set_hadjustment(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_CLIST(ctree)->hadjustment);
	gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_CLIST(ctree)->vadjustment);
	gtk_container_add(GTK_CONTAINER(scrolledwin), ctree);

	/* create status label */
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	hbox_l = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), hbox_l, TRUE, TRUE, 0);

	statlabel_folder = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(hbox_l), statlabel_folder, FALSE, FALSE, 2);
	statlabel_select = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(hbox_l), statlabel_select, FALSE, FALSE, 12);

	/* toggle view button */
	toggle_eventbox = gtk_event_box_new();
	gtk_box_pack_end(GTK_BOX(hbox), toggle_eventbox, FALSE, FALSE, 4);
	toggle_arrow = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_container_add(GTK_CONTAINER(toggle_eventbox), toggle_arrow);
	g_signal_connect(G_OBJECT(toggle_eventbox), "button_press_event",
			 G_CALLBACK(summary_toggle_pressed), summaryview);

	statlabel_msgs = gtk_label_new("");
	gtk_box_pack_end(GTK_BOX(hbox), statlabel_msgs, FALSE, FALSE, 4);

	hbox_spc = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), hbox_spc, FALSE, FALSE, 6);

	/* create popup menu */
	n_entries = sizeof(summary_popup_entries) /
		sizeof(summary_popup_entries[0]);
	popupmenu = menu_create_items(summary_popup_entries, n_entries,
				      "<SummaryView>", &popupfactory,
				      summaryview);

	summaryview->vbox = vbox;
	summaryview->scrolledwin = scrolledwin;
	summaryview->ctree = ctree;
	summaryview->hbox = hbox;
	summaryview->hbox_l = hbox_l;
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

	gtk_widget_show_all(vbox);

	return summaryview;
}

void summary_init(SummaryView *summaryview)
{
	GtkWidget *pixmap;
	PangoFontDescription *font_desc;
	gint size;

	gtk_widget_realize(summaryview->ctree);
	stock_pixmap_gdk(summaryview->ctree, STOCK_PIXMAP_MARK,
			 &markxpm, &markxpmmask);
	stock_pixmap_gdk(summaryview->ctree, STOCK_PIXMAP_DELETED,
			 &deletedxpm, &deletedxpmmask);
	stock_pixmap_gdk(summaryview->ctree, STOCK_PIXMAP_MAIL_SMALL,
			 &mailxpm, &mailxpmmask);
	stock_pixmap_gdk(summaryview->ctree, STOCK_PIXMAP_NEW,
			 &newxpm, &newxpmmask);
	stock_pixmap_gdk(summaryview->ctree, STOCK_PIXMAP_UNREAD,
			 &unreadxpm, &unreadxpmmask);
	stock_pixmap_gdk(summaryview->ctree, STOCK_PIXMAP_REPLIED,
			 &repliedxpm, &repliedxpmmask);
	stock_pixmap_gdk(summaryview->ctree, STOCK_PIXMAP_FORWARDED,
			 &forwardedxpm, &forwardedxpmmask);
	stock_pixmap_gdk(summaryview->ctree, STOCK_PIXMAP_CLIP,
			 &clipxpm, &clipxpmmask);

	if (!bold_style) {
		bold_style = gtk_style_copy
			(gtk_widget_get_style(summaryview->ctree));
		pango_font_description_set_weight
			(bold_style->font_desc, PANGO_WEIGHT_BOLD);
		bold_marked_style = gtk_style_copy(bold_style);
		bold_marked_style->fg[GTK_STATE_NORMAL] =
			summaryview->color_marked;
		bold_deleted_style = gtk_style_copy(bold_style);
		bold_deleted_style->fg[GTK_STATE_NORMAL] =
			summaryview->color_dim;
	}

	font_desc = pango_font_description_new();
	size = pango_font_description_get_size
		(summaryview->statlabel_folder->style->font_desc);
	pango_font_description_set_size(font_desc, size * PANGO_SCALE_SMALL);
	gtk_widget_modify_font(summaryview->statlabel_folder, font_desc);
	gtk_widget_modify_font(summaryview->statlabel_select, font_desc);
	gtk_widget_modify_font(summaryview->statlabel_msgs, font_desc);
	pango_font_description_free(font_desc);

	pixmap = stock_pixmap_widget(summaryview->hbox_l, STOCK_PIXMAP_DIR_OPEN);
	gtk_box_pack_start(GTK_BOX(summaryview->hbox_l), pixmap, FALSE, FALSE, 4);
	gtk_box_reorder_child(GTK_BOX(summaryview->hbox_l), pixmap, 0);
	gtk_widget_show(pixmap);

	summary_clear_list(summaryview);
	summary_set_column_titles(summaryview);
	summary_colorlabel_menu_create(summaryview);
	summary_set_menu_sensitive(summaryview);
}

gboolean summary_show(SummaryView *summaryview, FolderItem *item,
		      gboolean update_cache)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node;
	GSList *mlist = NULL;
	gchar *buf;
	gboolean is_refresh;
	guint selected_msgnum = 0;
	guint displayed_msgnum = 0;
	GtkCTreeNode *selected_node = summaryview->folderview->selected;

	if (summary_is_locked(summaryview)) return FALSE;

	inc_lock();
	summary_lock(summaryview);

	STATUSBAR_POP(summaryview->mainwin);

	is_refresh = (item == summaryview->folder_item &&
		      update_cache == FALSE) ? TRUE : FALSE;
	if (is_refresh) {
		selected_msgnum = summary_get_msgnum(summaryview,
						     summaryview->selected);
		displayed_msgnum = summary_get_msgnum(summaryview,
						      summaryview->displayed);
	}

	/* process the marks if any */
	if (summaryview->mainwin->lock_count == 0 &&
	    (summaryview->moved > 0 || summaryview->copied > 0)) {
		AlertValue val;

		val = alertpanel(_("Process mark"),
				 _("Some marks are left. Process it?"),
				 GTK_STOCK_YES, GTK_STOCK_NO, GTK_STOCK_CANCEL);
		if (G_ALERTDEFAULT == val) {
			summary_unlock(summaryview);
			summary_execute(summaryview);
			summary_lock(summaryview);
		} else if (G_ALERTALTERNATE == val)
			summary_write_cache(summaryview);
		else {
			summary_unlock(summaryview);
			inc_unlock();
			return FALSE;
		}
	} else
		summary_write_cache(summaryview);

	summaryview->folderview->opened = selected_node;

	gtk_clist_freeze(GTK_CLIST(ctree));

	summary_clear_list(summaryview);
	summary_set_column_titles(summaryview);

	buf = NULL;
	if (!item || !item->path || !item->parent || item->no_select ||
	    (FOLDER_TYPE(item->folder) == F_MH &&
	     ((buf = folder_item_get_path(item)) == NULL ||
	      change_dir(buf) < 0))) {
		g_free(buf);
		debug_print("empty folder\n\n");
		summary_clear_all(summaryview);
		summaryview->folder_item = item;
		gtk_clist_thaw(GTK_CLIST(ctree));
		summary_unlock(summaryview);
		inc_unlock();
		return TRUE;
	}
	g_free(buf);

	if (!is_refresh)
		messageview_clear(summaryview->messageview);

	summaryview->folder_item = item;

	g_signal_handlers_block_matched(G_OBJECT(ctree),
					(GSignalMatchType)G_SIGNAL_MATCH_DATA,
					0, 0, NULL, NULL, summaryview);

	buf = g_strdup_printf(_("Scanning folder (%s)..."), item->path);
	debug_print("%s\n", buf);
	STATUSBAR_PUSH(summaryview->mainwin, buf);
	g_free(buf);

	main_window_cursor_wait(summaryview->mainwin);

	mlist = folder_item_get_msg_list(item, !update_cache);

	statusbar_pop_all();
	STATUSBAR_POP(summaryview->mainwin);

	/* set ctree and hash table from the msginfo list, and
	   create the thread */
	summary_set_ctree_from_list(summaryview, mlist);

	g_slist_free(mlist);

	summary_write_cache(summaryview);

	item->opened = TRUE;

	g_signal_handlers_unblock_matched(G_OBJECT(ctree),
					  (GSignalMatchType)G_SIGNAL_MATCH_DATA,
					  0, 0, NULL, NULL, summaryview);

	gtk_clist_thaw(GTK_CLIST(ctree));

	if (is_refresh) {
		summaryview->displayed =
			summary_find_msg_by_msgnum(summaryview,
						   displayed_msgnum);
		if (!summaryview->displayed)
			messageview_clear(summaryview->messageview);
		summary_select_by_msgnum(summaryview, selected_msgnum);
		if (!summaryview->selected) {
			/* no selected message - select first unread
			   message, but do not display it */
			node = summary_find_next_flagged_msg(summaryview, NULL,
							     MSG_UNREAD, FALSE);
			if (node == NULL && GTK_CLIST(ctree)->row_list != NULL)
				node = gtk_ctree_node_nth
					(ctree,
					 item->sort_type == SORT_DESCENDING
					 ? 0 : GTK_CLIST(ctree)->rows - 1);
			summary_select_node(summaryview, node, FALSE, TRUE);
		}
	} else {
		/* select first unread message */
		node = summary_find_next_flagged_msg(summaryview, NULL,
						     MSG_UNREAD, FALSE);
		if (node == NULL && GTK_CLIST(ctree)->row_list != NULL) {
			node = gtk_ctree_node_nth
				(ctree,
				 item->sort_type == SORT_DESCENDING
				 ? 0 : GTK_CLIST(ctree)->rows - 1);
		}
		if (prefs_common.open_unread_on_enter ||
		    prefs_common.always_show_msg) {
			summary_unlock(summaryview);
			summary_select_node(summaryview, node, TRUE, TRUE);
			summary_lock(summaryview);
		} else
			summary_select_node(summaryview, node, FALSE, TRUE);
	}

	summary_set_column_titles(summaryview);
	summary_status_show(summaryview);
	summary_set_menu_sensitive(summaryview);
	main_window_set_toolbar_sensitive(summaryview->mainwin);

	debug_print("\n");
	STATUSBAR_PUSH(summaryview->mainwin, _("Done."));

	main_window_cursor_normal(summaryview->mainwin);
	summary_unlock(summaryview);
	inc_unlock();

	return TRUE;
}

void summary_clear_list(SummaryView *summaryview)
{
	GtkCList *clist = GTK_CLIST(summaryview->ctree);
	gint optimal_width;

	gtk_clist_freeze(clist);

	gtk_ctree_pre_recursive(GTK_CTREE(summaryview->ctree),
				NULL, summary_free_msginfo_func, NULL);

	if (summaryview->folder_item) {
		folder_item_close(summaryview->folder_item);
		summaryview->folder_item = NULL;
	}

	summaryview->display_msg = FALSE;

	summaryview->selected = NULL;
	summaryview->displayed = NULL;
	summaryview->total_size = 0;
	summaryview->deleted = summaryview->moved = 0;
	summaryview->copied = 0;
	if (summaryview->msgid_table) {
		g_hash_table_destroy(summaryview->msgid_table);
		summaryview->msgid_table = NULL;
	}
	summaryview->mlist = NULL;
	if (summaryview->folder_table) {
		g_hash_table_destroy(summaryview->folder_table);
		summaryview->folder_table = NULL;
	}

	gtk_clist_clear(clist);
	if (summaryview->col_pos[S_COL_SUBJECT] == N_SUMMARY_COLS - 1) {
		optimal_width = gtk_clist_optimal_column_width
			(clist, summaryview->col_pos[S_COL_SUBJECT]);
		gtk_clist_set_column_width
			(clist, summaryview->col_pos[S_COL_SUBJECT],
			 optimal_width);
	}

	gtk_clist_thaw(clist);
}

void summary_clear_all(SummaryView *summaryview)
{
	messageview_clear(summaryview->messageview);
	summary_clear_list(summaryview);
	summary_set_menu_sensitive(summaryview);
	main_window_set_toolbar_sensitive(summaryview->mainwin);
	summary_status_show(summaryview);
}

void summary_lock(SummaryView *summaryview)
{
	summaryview->lock_count++;
}

void summary_unlock(SummaryView *summaryview)
{
	if (summaryview->lock_count)
		summaryview->lock_count--;
}

gboolean summary_is_locked(SummaryView *summaryview)
{
	return summaryview->lock_count > 0;
}

SummarySelection summary_get_selection_type(SummaryView *summaryview)
{
	GtkCList *clist = GTK_CLIST(summaryview->ctree);
	SummarySelection selection;

	if (!clist->row_list)
		selection = SUMMARY_NONE;
	else if (!clist->selection)
		selection = SUMMARY_SELECTED_NONE;
	else if (!clist->selection->next)
		selection = SUMMARY_SELECTED_SINGLE;
	else
		selection = SUMMARY_SELECTED_MULTIPLE;

	return selection;
}

GSList *summary_get_selected_msg_list(SummaryView *summaryview)
{
	GSList *mlist = NULL;
	GList *cur;
	MsgInfo *msginfo;

	for (cur = GTK_CLIST(summaryview->ctree)->selection; cur != NULL;
	     cur = cur->next) {
		msginfo = GTKUT_CTREE_NODE_GET_ROW_DATA(cur->data);
		mlist = g_slist_prepend(mlist, msginfo);
	}

	mlist = g_slist_reverse(mlist);

	return mlist;
}

GSList *summary_get_msg_list(SummaryView *summaryview)
{
	GSList *mlist = NULL;
	GtkCTree *ctree;
	GtkCTreeNode *node;
	MsgInfo *msginfo;

	ctree = GTK_CTREE(summaryview->ctree);

	for (node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);
	     node != NULL; node = gtkut_ctree_node_next(ctree, node)) {
		msginfo = GTKUT_CTREE_NODE_GET_ROW_DATA(node);
		mlist = g_slist_prepend(mlist, msginfo);
	}

	mlist = g_slist_reverse(mlist);

	return mlist;
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

	if (summaryview->folder_item &&
	    (summaryview->folder_item->stype == F_OUTBOX ||
	     summaryview->folder_item->stype == F_DRAFT  ||
	     summaryview->folder_item->stype == F_QUEUE)) {
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

	if (FOLDER_TYPE(summaryview->folder_item->folder) != F_NEWS) {
		menu_set_sensitive(ifactory, "/Move...", TRUE);
		menu_set_sensitive(ifactory, "/Delete", TRUE);
	} else {
		menu_set_sensitive(ifactory, "/Move...", FALSE);
		menu_set_sensitive(ifactory, "/Delete", FALSE);
	}

	menu_set_sensitive(ifactory, "/Copy...", TRUE);

	menu_set_sensitive(ifactory, "/Mark", TRUE);
	menu_set_sensitive(ifactory, "/Mark/Mark",   TRUE);
	menu_set_sensitive(ifactory, "/Mark/Unmark", TRUE);

	menu_set_sensitive(ifactory, "/Mark/Mark as unread", TRUE);
	menu_set_sensitive(ifactory, "/Mark/Mark as read",   TRUE);
	menu_set_sensitive(ifactory, "/Mark/Mark all read",  TRUE);

	menu_set_sensitive(ifactory, "/Color label", TRUE);

	menu_set_sensitive(ifactory, "/Reply",			  sens);
	menu_set_sensitive(ifactory, "/Reply to",		  sens);
	menu_set_sensitive(ifactory, "/Reply to/all",		  sens);
	menu_set_sensitive(ifactory, "/Reply to/sender",	  sens);
	menu_set_sensitive(ifactory, "/Reply to/mailing list",	  sens);
	menu_set_sensitive(ifactory, "/Forward",		  TRUE);
	menu_set_sensitive(ifactory, "/Forward as attachment",	  TRUE);
	menu_set_sensitive(ifactory, "/Redirect",		  sens);

	menu_set_sensitive(ifactory, "/Add sender to address book", sens);

	menu_set_sensitive(ifactory, "/View", sens);
	menu_set_sensitive(ifactory, "/View/Open in new window", sens);
	menu_set_sensitive(ifactory, "/View/Source", sens);
	menu_set_sensitive(ifactory, "/View/All header", sens);

	menu_set_sensitive(ifactory, "/Print...",   TRUE);

	summary_lock(summaryview);
	menuitem = gtk_item_factory_get_widget(ifactory, "/View/All header");
	gtk_check_menu_item_set_active
		(GTK_CHECK_MENU_ITEM(menuitem),
		 summaryview->messageview->textview->show_all_headers);
	summary_unlock(summaryview);
}

void summary_select_prev_unread(SummaryView *summaryview)
{
	GtkCTreeNode *node;

	node = summary_find_prev_flagged_msg
		(summaryview, summaryview->selected, MSG_UNREAD, FALSE);

	if (!node) {
		AlertValue val;

		val = alertpanel(_("No more unread messages"),
				 _("No unread message found. "
				   "Search from the end?"),
				 GTK_STOCK_YES, GTK_STOCK_NO, NULL);
		if (val != G_ALERTDEFAULT) return;
		node = summary_find_prev_flagged_msg(summaryview, NULL,
						     MSG_UNREAD, FALSE);
	}

	if (!node)
		alertpanel_notice(_("No unread messages."));
	else
		summary_select_node(summaryview, node, TRUE, FALSE);
}

void summary_select_next_unread(SummaryView *summaryview)
{
	GtkCTreeNode *node = summaryview->selected;
	//GtkCTree *ctree = GTK_CTREE(summaryview->ctree);

	while ((node = summary_find_next_flagged_msg
		(summaryview, node, MSG_UNREAD, FALSE)) == NULL) {
		AlertValue val;

		val = alertpanel(_("No more unread messages"),
				 _("No unread message found. "
				   "Go to next folder?"),
				 GTK_STOCK_YES, _("Search again"),
				 GTK_STOCK_NO);
		if (val == G_ALERTDEFAULT) {
#if 0
			if (gtk_signal_n_emissions_by_name
				(G_OBJECT(ctree), "key_press_event") > 0)
					gtk_signal_emit_stop_by_name
						(G_OBJECT(ctree),
						 "key_press_event");
#endif
			folderview_select_next_unread(summaryview->folderview);
			return;
		} else if (val == G_ALERTALTERNATE)
			node = NULL;
		else
			return;
	}

	if (node)
		summary_select_node(summaryview, node, TRUE, FALSE);
}

void summary_select_prev_new(SummaryView *summaryview)
{
	GtkCTreeNode *node;

	node = summary_find_prev_flagged_msg
		(summaryview, summaryview->selected, MSG_NEW, FALSE);

	if (!node) {
		AlertValue val;

		val = alertpanel(_("No more new messages"),
				 _("No new message found. "
				   "Search from the end?"),
				 GTK_STOCK_YES, GTK_STOCK_NO, NULL);
		if (val != G_ALERTDEFAULT) return;
		node = summary_find_prev_flagged_msg(summaryview, NULL,
						     MSG_NEW, FALSE);
	}

	if (!node)
		alertpanel_notice(_("No new messages."));
	else
		summary_select_node(summaryview, node, TRUE, FALSE);
}

void summary_select_next_new(SummaryView *summaryview)
{
	GtkCTreeNode *node = summaryview->selected;
	//GtkCTree *ctree = GTK_CTREE(summaryview->ctree);

	while ((node = summary_find_next_flagged_msg
		(summaryview, node, MSG_NEW, FALSE)) == NULL) {
		AlertValue val;

		val = alertpanel(_("No more new messages"),
				 _("No new message found. "
				   "Go to next folder?"),
				 GTK_STOCK_YES, _("Search again"),
				 GTK_STOCK_NO);
		if (val == G_ALERTDEFAULT) {
#if 0
			if (gtk_signal_n_emissions_by_name
				(G_OBJECT(ctree), "key_press_event") > 0)
					gtk_signal_emit_stop_by_name
						(G_OBJECT(ctree),
						 "key_press_event");
#endif
			folderview_select_next_unread(summaryview->folderview);
			return;
		} else if (val == G_ALERTALTERNATE)
			node = NULL;
		else
			return;
	}

	if (node)
		summary_select_node(summaryview, node, TRUE, FALSE);
}

void summary_select_prev_marked(SummaryView *summaryview)
{
	GtkCTreeNode *node;

	node = summary_find_prev_flagged_msg
		(summaryview, summaryview->selected, MSG_MARKED, TRUE);

	if (!node) {
		AlertValue val;

		val = alertpanel(_("No more marked messages"),
				 _("No marked message found. "
				   "Search from the end?"),
				 GTK_STOCK_YES, GTK_STOCK_NO, NULL);
		if (val != G_ALERTDEFAULT) return;
		node = summary_find_prev_flagged_msg(summaryview, NULL,
						     MSG_MARKED, TRUE);
	}

	if (!node)
		alertpanel_notice(_("No marked messages."));
	else
		summary_select_node(summaryview, node, TRUE, FALSE);
}

void summary_select_next_marked(SummaryView *summaryview)
{
	GtkCTreeNode *node;

	node = summary_find_next_flagged_msg
		(summaryview, summaryview->selected, MSG_MARKED, TRUE);

	if (!node) {
		AlertValue val;

		val = alertpanel(_("No more marked messages"),
				 _("No marked message found. "
				   "Search from the beginning?"),
				 GTK_STOCK_YES, GTK_STOCK_NO, NULL);
		if (val != G_ALERTDEFAULT) return;
		node = summary_find_next_flagged_msg(summaryview, NULL,
						     MSG_MARKED, TRUE);
	}

	if (!node)
		alertpanel_notice(_("No marked messages."));
	else
		summary_select_node(summaryview, node, TRUE, FALSE);
}

void summary_select_prev_labeled(SummaryView *summaryview)
{
	GtkCTreeNode *node;

	node = summary_find_prev_flagged_msg
		(summaryview, summaryview->selected, MSG_CLABEL_FLAG_MASK, TRUE);

	if (!node) {
		AlertValue val;

		val = alertpanel(_("No more labeled messages"),
				 _("No labeled message found. "
				   "Search from the end?"),
				 GTK_STOCK_YES, GTK_STOCK_NO, NULL);
		if (val != G_ALERTDEFAULT) return;
		node = summary_find_prev_flagged_msg(summaryview, NULL,
						     MSG_CLABEL_FLAG_MASK, TRUE);
	}

	if (!node)
		alertpanel_notice(_("No labeled messages."));
	else
		summary_select_node(summaryview, node, TRUE, FALSE);
}

void summary_select_next_labeled(SummaryView *summaryview)
{
	GtkCTreeNode *node;

	node = summary_find_next_flagged_msg
		(summaryview, summaryview->selected, MSG_CLABEL_FLAG_MASK, TRUE);

	if (!node) {
		AlertValue val;

		val = alertpanel(_("No more labeled messages"),
				 _("No labeled message found. "
				   "Search from the beginning?"),
				 GTK_STOCK_YES, GTK_STOCK_NO, NULL);
		if (val != G_ALERTDEFAULT) return;
		node = summary_find_next_flagged_msg(summaryview, NULL,
						     MSG_CLABEL_FLAG_MASK, TRUE);
	}

	if (!node)
		alertpanel_notice(_("No labeled messages."));
	else
		summary_select_node(summaryview, node, TRUE, FALSE);
}

void summary_select_by_msgnum(SummaryView *summaryview, guint msgnum)
{
	GtkCTreeNode *node;

	node = summary_find_msg_by_msgnum(summaryview, msgnum);
	summary_select_node(summaryview, node, FALSE, TRUE);
}

/**
 * summary_select_node:
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
void summary_select_node(SummaryView *summaryview, GtkCTreeNode *node,
			 gboolean display_msg, gboolean do_refresh)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);

	if (node) {
		gtkut_ctree_expand_parent_all(ctree, node);
		if (do_refresh) {
			GTK_EVENTS_FLUSH();
			gtk_widget_grab_focus(GTK_WIDGET(ctree));
			gtk_ctree_node_moveto(ctree, node, -1, 0.5, 0);
		}
		gtk_sctree_unselect_all(GTK_SCTREE(ctree));
		if (display_msg && summaryview->displayed == node)
			summaryview->displayed = NULL;
		summaryview->display_msg = display_msg;
		gtk_sctree_select(GTK_SCTREE(ctree), node);
	}
}

static guint summary_get_msgnum(SummaryView *summaryview, GtkCTreeNode *node)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;

	if (!node)
		return 0;
	msginfo = gtk_ctree_node_get_row_data(ctree, node);
	return msginfo->msgnum;
}

static GtkCTreeNode *summary_find_prev_msg(SummaryView *summaryview,
					   GtkCTreeNode *current_node)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node;
	MsgInfo *msginfo;

	if (current_node)
		node = current_node;
	else
		node = gtk_ctree_node_nth(ctree, GTK_CLIST(ctree)->rows - 1);

	for (; node != NULL; node = GTK_CTREE_NODE_PREV(node)) {
		msginfo = gtk_ctree_node_get_row_data(ctree, node);
		if (msginfo && !MSG_IS_INVALID(msginfo->flags) &&
		    !MSG_IS_DELETED(msginfo->flags)) break;
	}

	return node;
}

static GtkCTreeNode *summary_find_next_msg(SummaryView *summaryview,
					   GtkCTreeNode *current_node)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node;
	MsgInfo *msginfo;

	if (current_node)
		node = current_node;
	else
		node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);

	for (; node != NULL; node = gtkut_ctree_node_next(ctree, node)) {
		msginfo = gtk_ctree_node_get_row_data(ctree, node);
		if (msginfo && !MSG_IS_INVALID(msginfo->flags) &&
		    !MSG_IS_DELETED(msginfo->flags)) break;
	}

	return node;
}

static GtkCTreeNode *summary_find_prev_flagged_msg(SummaryView *summaryview,
						   GtkCTreeNode *current_node,
						   MsgPermFlags flags,
						   gboolean start_from_prev)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node;
	MsgInfo *msginfo;

	if (current_node) {
		if (start_from_prev)
			node = GTK_CTREE_NODE_PREV(current_node);
		else
			node = current_node;
	} else
		node = gtk_ctree_node_nth(ctree, GTK_CLIST(ctree)->rows - 1);

	for (; node != NULL; node = GTK_CTREE_NODE_PREV(node)) {
		msginfo = gtk_ctree_node_get_row_data(ctree, node);
		if (msginfo && (msginfo->flags.perm_flags & flags) != 0) break;
	}

	return node;
}

static GtkCTreeNode *summary_find_next_flagged_msg(SummaryView *summaryview,
						   GtkCTreeNode *current_node,
						   MsgPermFlags flags,
						   gboolean start_from_next)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node;
	MsgInfo *msginfo;

	if (current_node) {
		if (start_from_next)
			node = gtkut_ctree_node_next(ctree, current_node);
		else
			node = current_node;
	} else
		node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);

	for (; node != NULL; node = gtkut_ctree_node_next(ctree, node)) {
		msginfo = gtk_ctree_node_get_row_data(ctree, node);
		if (msginfo && (msginfo->flags.perm_flags & flags) != 0) break;
	}

	return node;
}

static GtkCTreeNode *summary_find_msg_by_msgnum(SummaryView *summaryview,
						guint msgnum)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node;
	MsgInfo *msginfo;

	node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);

	for (; node != NULL; node = gtkut_ctree_node_next(ctree, node)) {
		msginfo = gtk_ctree_node_get_row_data(ctree, node);
		if (msginfo && msginfo->msgnum == msgnum) break;
	}

	return node;
}

static guint attract_hash_func(gconstpointer key)
{
	gchar *str;
	gchar *p;
	guint h;

	Xstrdup_a(str, (const gchar *)key, return 0);
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
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCList *clist = GTK_CLIST(ctree);
	GtkCTreeNode *src_node;
	GtkCTreeNode *dst_node, *sibling;
	GtkCTreeNode *tmp;
	MsgInfo *src_msginfo, *dst_msginfo;
	GHashTable *subject_table;

	debug_print(_("Attracting messages by subject..."));
	STATUSBAR_PUSH(summaryview->mainwin,
		       _("Attracting messages by subject..."));

	main_window_cursor_wait(summaryview->mainwin);
	gtk_clist_freeze(clist);

	subject_table = g_hash_table_new(attract_hash_func,
					 attract_compare_func);

	for (src_node = GTK_CTREE_NODE(clist->row_list);
	     src_node != NULL;
	     src_node = tmp) {
		tmp = GTK_CTREE_ROW(src_node)->sibling;
		src_msginfo = GTKUT_CTREE_NODE_GET_ROW_DATA(src_node);
		if (!src_msginfo) continue;
		if (!src_msginfo->subject) continue;

		/* find attracting node */
		dst_node = g_hash_table_lookup(subject_table,
					       src_msginfo->subject);

		if (dst_node) {
			dst_msginfo = GTKUT_CTREE_NODE_GET_ROW_DATA(dst_node);

			/* if the time difference is more than 20 days,
			   don't attract */
			if (ABS(src_msginfo->date_t - dst_msginfo->date_t)
			    > 60 * 60 * 24 * 20)
				continue;

			sibling = GTK_CTREE_ROW(dst_node)->sibling;
			if (src_node != sibling)
				gtk_ctree_move(ctree, src_node, NULL, sibling);
		}

		g_hash_table_insert(subject_table,
				    src_msginfo->subject, src_node);
	}

	g_hash_table_destroy(subject_table);

	gtk_ctree_node_moveto(ctree, summaryview->selected, -1, 0.5, 0);

	gtk_clist_thaw(clist);

	debug_print(_("done.\n"));
	STATUSBAR_POP(summaryview->mainwin);

	main_window_cursor_normal(summaryview->mainwin);
}

static void summary_free_msginfo_func(GtkCTree *ctree, GtkCTreeNode *node,
				      gpointer data)
{
	MsgInfo *msginfo = gtk_ctree_node_get_row_data(ctree, node);

	if (msginfo)
		procmsg_msginfo_free(msginfo);
}

static void summary_set_marks_func(GtkCTree *ctree, GtkCTreeNode *node,
				   gpointer data)
{
	SummaryView *summaryview = data;
	MsgInfo *msginfo;

	msginfo = gtk_ctree_node_get_row_data(ctree, node);

	if (MSG_IS_DELETED(msginfo->flags))
		summaryview->deleted++;

	summaryview->total_size += msginfo->size;

	summary_set_row_marks(summaryview, node);
}

static void summary_update_status(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node;
	MsgInfo *msginfo;

	summaryview->total_size =
	summaryview->deleted = summaryview->moved = summaryview->copied = 0;

	for (node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);
	     node != NULL; node = gtkut_ctree_node_next(ctree, node)) {
		msginfo = GTKUT_CTREE_NODE_GET_ROW_DATA(node);

		if (MSG_IS_DELETED(msginfo->flags))
			summaryview->deleted++;
		if (MSG_IS_MOVE(msginfo->flags))
			summaryview->moved++;
		if (MSG_IS_COPY(msginfo->flags))
			summaryview->copied++;
		summaryview->total_size += msginfo->size;
	}
}

static void summary_status_show(SummaryView *summaryview)
{
	gchar *str;
	gchar *del, *mv, *cp;
	gchar *sel;
	gchar *spc;
	GList *rowlist, *cur;
	guint n_selected = 0;
	off_t sel_size = 0;
	MsgInfo *msginfo;

	if (!summaryview->folder_item) {
		gtk_label_set(GTK_LABEL(summaryview->statlabel_folder), "");
		gtk_label_set(GTK_LABEL(summaryview->statlabel_select), "");
		gtk_label_set(GTK_LABEL(summaryview->statlabel_msgs),   "");
		return;
	}

	rowlist = GTK_CLIST(summaryview->ctree)->selection;
	for (cur = rowlist; cur != NULL; cur = cur->next) {
		msginfo = gtk_ctree_node_get_row_data
			(GTK_CTREE(summaryview->ctree),
			 GTK_CTREE_NODE(cur->data));
		if (!msginfo)
			g_warning("summary_status_show(): msginfo == NULL\n");
		else {
			sel_size += msginfo->size;
			n_selected++;
		}
	}

	if (FOLDER_TYPE(summaryview->folder_item->folder) == F_NEWS) {
		gchar *group;
		group = get_abbrev_newsgroup_name
			(g_basename(summaryview->folder_item->path),
			 prefs_common.ng_abbrev_len);
		str = trim_string_before(group, 32);
		g_free(group);
	} else
		str = trim_string_before(summaryview->folder_item->path, 32);
	gtk_label_set(GTK_LABEL(summaryview->statlabel_folder), str);
	g_free(str);

	if (summaryview->deleted)
		del = g_strdup_printf(_("%d deleted"), summaryview->deleted);
	else
		del = g_strdup("");
	if (summaryview->moved)
		mv = g_strdup_printf(_("%s%d moved"),
				     summaryview->deleted ? _(", ") : "",
				     summaryview->moved);
	else
		mv = g_strdup("");
	if (summaryview->copied)
		cp = g_strdup_printf(_("%s%d copied"),
				     summaryview->deleted ||
				     summaryview->moved ? _(", ") : "",
				     summaryview->copied);
	else
		cp = g_strdup("");

	if (summaryview->deleted || summaryview->moved || summaryview->copied)
		spc = "    ";
	else
		spc = "";

	if (n_selected)
		sel = g_strdup_printf(" (%s)", to_human_readable(sel_size));
	else
		sel = g_strdup("");
	str = g_strconcat(n_selected ? itos(n_selected) : "",
			  n_selected ? _(" item(s) selected") : "",
			  sel, spc, del, mv, cp, NULL);
	gtk_label_set(GTK_LABEL(summaryview->statlabel_select), str);
	g_free(str);
	g_free(sel);
	g_free(del);
	g_free(mv);
	g_free(cp);

	if (FOLDER_IS_LOCAL(summaryview->folder_item->folder)) {
		str = g_strdup_printf(_("%d new, %d unread, %d total (%s)"),
				      summaryview->folder_item->new,
				      summaryview->folder_item->unread,
				      summaryview->folder_item->total,
				      to_human_readable(summaryview->total_size));
	} else {
		str = g_strdup_printf(_("%d new, %d unread, %d total"),
				      summaryview->folder_item->new,
				      summaryview->folder_item->unread,
				      summaryview->folder_item->total);
	}
	gtk_label_set(GTK_LABEL(summaryview->statlabel_msgs), str);
	g_free(str);

	folderview_update_msg_num(summaryview->folderview,
				  summaryview->folderview->opened);
}

static void summary_set_column_titles(SummaryView *summaryview)
{
	GtkCList *clist = GTK_CLIST(summaryview->ctree);
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *arrow;
	gint pos;
	const gchar *title;
	SummaryColumnType type;
	GtkJustification justify;
	FolderItem *item = summaryview->folder_item;

	static FolderSortKey sort_by[N_SUMMARY_COLS] = {
		SORT_BY_MARK,
		SORT_BY_UNREAD,
		SORT_BY_MIME,
		SORT_BY_SUBJECT,
		SORT_BY_FROM,
		SORT_BY_DATE,
		SORT_BY_SIZE,
		SORT_BY_NUMBER
	};

	for (pos = 0; pos < N_SUMMARY_COLS; pos++) {
		type = summaryview->col_state[pos].type;

		justify = (type == S_COL_NUMBER || type == S_COL_SIZE)
			? GTK_JUSTIFY_RIGHT : GTK_JUSTIFY_LEFT;

		switch (type) {
		case S_COL_SUBJECT:
		case S_COL_FROM:
		case S_COL_DATE:
		case S_COL_NUMBER:
			if (prefs_common.trans_hdr)
				title = gettext(col_label[type]);
			else
				title = col_label[type];
			break;
		default:
			title = gettext(col_label[type]);
		}

		if (type == S_COL_MARK) {
			label = gtk_pixmap_new(markxpm, markxpmmask);
			gtk_widget_show(label);
			gtk_clist_set_column_widget(clist, pos, label);
			continue;
		} else if (type == S_COL_UNREAD) {
			label = gtk_pixmap_new(mailxpm, mailxpmmask);
			gtk_widget_show(label);
			gtk_clist_set_column_widget(clist, pos, label);
			continue;
		} else if (type == S_COL_MIME) {
			label = gtk_pixmap_new(clipxpm, clipxpmmask);
			gtk_widget_show(label);
			gtk_clist_set_column_widget(clist, pos, label);
			continue;
		}

		hbox = gtk_hbox_new(FALSE, 4);
		label = gtk_label_new(title);
		if (justify == GTK_JUSTIFY_RIGHT)
			gtk_box_pack_end(GTK_BOX(hbox), label,
					 FALSE, FALSE, 0);
		else
			gtk_box_pack_start(GTK_BOX(hbox), label,
					   FALSE, FALSE, 0);

		if (item && item->sort_key == sort_by[type]) {
			arrow = gtk_arrow_new
				(item->sort_type == SORT_ASCENDING
				 ? GTK_ARROW_UP : GTK_ARROW_DOWN,
				 GTK_SHADOW_IN);
			if (justify == GTK_JUSTIFY_RIGHT)
				gtk_box_pack_start(GTK_BOX(hbox), arrow,
						   FALSE, FALSE, 0);
			else
				gtk_box_pack_end(GTK_BOX(hbox), arrow,
						 FALSE, FALSE, 0);
		}

		gtk_widget_show_all(hbox);
		gtk_clist_set_column_widget(clist, pos, hbox);
	}
}

void summary_sort(SummaryView *summaryview,
		  FolderSortKey sort_key, FolderSortType sort_type)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCList *clist = GTK_CLIST(summaryview->ctree);
	GtkCListCompareFunc cmp_func;
	FolderItem *item = summaryview->folder_item;

	if (!item || !item->path || !item->parent || item->no_select) return;

	switch (sort_key) {
	case SORT_BY_MARK:
		cmp_func = (GtkCListCompareFunc)summary_cmp_by_mark;
		break;
	case SORT_BY_UNREAD:
		cmp_func = (GtkCListCompareFunc)summary_cmp_by_unread;
		break;
	case SORT_BY_MIME:
		cmp_func = (GtkCListCompareFunc)summary_cmp_by_mime;
		break;
	case SORT_BY_NUMBER:
		cmp_func = (GtkCListCompareFunc)summary_cmp_by_num;
		break;
	case SORT_BY_SIZE:
		cmp_func = (GtkCListCompareFunc)summary_cmp_by_size;
		break;
	case SORT_BY_DATE:
		cmp_func = (GtkCListCompareFunc)summary_cmp_by_date;
		break;
	case SORT_BY_FROM:
		cmp_func = (GtkCListCompareFunc)summary_cmp_by_from;
		break;
	case SORT_BY_SUBJECT:
		cmp_func = (GtkCListCompareFunc)summary_cmp_by_subject;
		break;
	case SORT_BY_LABEL:
		cmp_func = (GtkCListCompareFunc)summary_cmp_by_label;
		break;
	case SORT_BY_TO:
		cmp_func = (GtkCListCompareFunc)summary_cmp_by_to;
		break;
	case SORT_BY_NONE:
		item->sort_key = sort_key;
		item->sort_type = SORT_ASCENDING;
		summary_set_column_titles(summaryview);
		summary_set_menu_sensitive(summaryview);
		return;
	default:
		return;
	}

	debug_print(_("Sorting summary..."));
	STATUSBAR_PUSH(summaryview->mainwin, _("Sorting summary..."));

	main_window_cursor_wait(summaryview->mainwin);

	gtk_clist_set_compare_func(clist, cmp_func);

	gtk_clist_set_sort_type(clist, (GtkSortType)sort_type);
	item->sort_key = sort_key;
	item->sort_type = sort_type;

	summary_set_column_titles(summaryview);
	summary_set_menu_sensitive(summaryview);

	gtk_ctree_sort_recursive(ctree, NULL);

	gtk_ctree_node_moveto(ctree, summaryview->selected, -1, 0.5, 0);

	debug_print(_("done.\n"));
	STATUSBAR_POP(summaryview->mainwin);

	main_window_cursor_normal(summaryview->mainwin);
}

gboolean summary_insert_gnode_func(GtkCTree *ctree, guint depth, GNode *gnode,
				   GtkCTreeNode *cnode, gpointer data)
{
	SummaryView *summaryview = (SummaryView *)data;
	MsgInfo *msginfo = (MsgInfo *)gnode->data;
	gchar *text[N_SUMMARY_COLS];
	gint *col_pos = summaryview->col_pos;
	const gchar *msgid = msginfo->msgid;
	GHashTable *msgid_table = summaryview->msgid_table;

	summary_set_header(summaryview, text, msginfo);

	gtk_ctree_set_node_info(ctree, cnode, text[col_pos[S_COL_SUBJECT]], 2,
				NULL, NULL, NULL, NULL, FALSE,
				gnode->parent->parent ? TRUE : FALSE);
#define SET_TEXT(col) \
	gtk_ctree_node_set_text(ctree, cnode, col_pos[col], \
				text[col_pos[col]])

	SET_TEXT(S_COL_NUMBER);
	SET_TEXT(S_COL_SIZE);
	SET_TEXT(S_COL_DATE);
	SET_TEXT(S_COL_FROM);
	SET_TEXT(S_COL_SUBJECT);

#undef SET_TEXT

	GTKUT_CTREE_NODE_SET_ROW_DATA(cnode, msginfo);
	summary_set_marks_func(ctree, cnode, summaryview);

	if (msgid && msgid[0] != '\0')
		g_hash_table_insert(msgid_table, (gchar *)msgid, cnode);

	return TRUE;
}

static void summary_set_ctree_from_list(SummaryView *summaryview,
					GSList *mlist)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;
	GtkCTreeNode *node = NULL;
	GHashTable *msgid_table;

	if (!mlist) return;

	debug_print(_("\tSetting summary from message data..."));
	STATUSBAR_PUSH(summaryview->mainwin,
		       _("Setting summary from message data..."));
	gdk_flush();

	msgid_table = g_hash_table_new(g_str_hash, g_str_equal);
	summaryview->msgid_table = msgid_table;

	if (summaryview->folder_item->threaded) {
		GNode *root, *gnode;

		root = procmsg_get_thread_tree(mlist);

		for (gnode = root->children; gnode != NULL;
		     gnode = gnode->next) {
			node = gtk_ctree_insert_gnode
				(ctree, NULL, node, gnode,
				 summary_insert_gnode_func, summaryview);
		}

		g_node_destroy(root);

		summary_thread_init(summaryview);
	} else {
		gchar *text[N_SUMMARY_COLS];

		mlist = g_slist_reverse(mlist);
		for (; mlist != NULL; mlist = mlist->next) {
			msginfo = (MsgInfo *)mlist->data;

			summary_set_header(summaryview, text, msginfo);

			node = gtk_ctree_insert_node
				(ctree, NULL, node, text, 2,
				 NULL, NULL, NULL, NULL, FALSE, FALSE);
			GTKUT_CTREE_NODE_SET_ROW_DATA(node, msginfo);
			summary_set_marks_func(ctree, node, summaryview);

			if (msginfo->msgid && msginfo->msgid[0] != '\0')
				g_hash_table_insert(msgid_table,
						    msginfo->msgid, node);
		}
		mlist = g_slist_reverse(mlist);
	}

	if (prefs_common.enable_hscrollbar &&
	    summaryview->col_pos[S_COL_SUBJECT] == N_SUMMARY_COLS - 1) {
		gint optimal_width;

		optimal_width = gtk_clist_optimal_column_width
			(GTK_CLIST(ctree), summaryview->col_pos[S_COL_SUBJECT]);
		gtk_clist_set_column_width(GTK_CLIST(ctree),
					   summaryview->col_pos[S_COL_SUBJECT],
					   optimal_width);
	}

	debug_print(_("done.\n"));
	STATUSBAR_POP(summaryview->mainwin);
	if (debug_mode)
		debug_print("\tmsgid hash table size = %d\n",
			    g_hash_table_size(msgid_table));
}

struct wcachefp
{
	FILE *cache_fp;
	FILE *mark_fp;
};

gint summary_write_cache(SummaryView *summaryview)
{
	struct wcachefp fps;
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	FolderItem *item;
	gchar *buf;

	item = summaryview->folder_item;
	if (!item || !item->path)
		return -1;

	fps.cache_fp = procmsg_open_cache_file(item, DATA_WRITE);
	if (fps.cache_fp == NULL)
		return -1;
	fps.mark_fp = procmsg_open_mark_file(item, DATA_WRITE);
	if (fps.mark_fp == NULL) {
		fclose(fps.cache_fp);
		return -1;
	}

	buf = g_strdup_printf(_("Writing summary cache (%s)..."), item->path);
	debug_print(buf);
	STATUSBAR_PUSH(summaryview->mainwin, buf);
	g_free(buf);

	gtk_ctree_pre_recursive(ctree, NULL, summary_write_cache_func, &fps);

	procmsg_flush_mark_queue(item, fps.mark_fp);
	item->unmarked_num = 0;

	fclose(fps.cache_fp);
	fclose(fps.mark_fp);

	debug_print(_("done.\n"));
	STATUSBAR_POP(summaryview->mainwin);

	return 0;
}

static void summary_write_cache_func(GtkCTree *ctree, GtkCTreeNode *node,
				     gpointer data)
{
	struct wcachefp *fps = data;
	MsgInfo *msginfo = gtk_ctree_node_get_row_data(ctree, node);

	if (msginfo == NULL) return;

	if (msginfo->folder->mark_queue != NULL) {
		MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_NEW);
	}

	procmsg_write_cache(msginfo, fps->cache_fp);
	procmsg_write_flags(msginfo, fps->mark_fp);
}

static void summary_set_header(SummaryView *summaryview, gchar *text[],
			       MsgInfo *msginfo)
{
	static gchar date_modified[80];
	static gchar *to = NULL;
	static gchar *subject = NULL;
	gint *col_pos = summaryview->col_pos;

	text[col_pos[S_COL_MARK]]   = NULL;
	text[col_pos[S_COL_UNREAD]] = NULL;
	text[col_pos[S_COL_MIME]]   = NULL;
	text[col_pos[S_COL_NUMBER]] = itos(msginfo->msgnum);
	text[col_pos[S_COL_SIZE]]   = to_human_readable(msginfo->size);

	if (msginfo->date_t) {
		procheader_date_get_localtime(date_modified,
					      sizeof(date_modified),
					      msginfo->date_t);
		text[col_pos[S_COL_DATE]] = date_modified;
	} else if (msginfo->date)
		text[col_pos[S_COL_DATE]] = msginfo->date;
	else
		text[col_pos[S_COL_DATE]] = _("(No Date)");

	text[col_pos[S_COL_FROM]] = msginfo->fromname ? msginfo->fromname :
		_("(No From)");
	if (prefs_common.swap_from && msginfo->from && msginfo->to &&
	    cur_account && cur_account->address) {
		gchar *from;

		Xstrdup_a(from, msginfo->from, return);
		extract_address(from);
		if (!strcmp(from, cur_account->address)) {
			g_free(to);
			to = g_strconcat("-->", msginfo->to, NULL);
			text[col_pos[S_COL_FROM]] = to;
		}
	}

	if (msginfo->subject) {
		if (msginfo->folder && msginfo->folder->trim_summary_subject) {
			g_free(subject);
			subject = g_strdup(msginfo->subject);
			trim_subject(subject);
			text[col_pos[S_COL_SUBJECT]] = subject;
		} else
			text[col_pos[S_COL_SUBJECT]] = msginfo->subject;
	} else
		text[col_pos[S_COL_SUBJECT]] = _("(No Subject)");
}

static void summary_display_msg(SummaryView *summaryview, GtkCTreeNode *row)
{
	summary_display_msg_full(summaryview, row, FALSE, FALSE);
}

static void summary_display_msg_full(SummaryView *summaryview,
				     GtkCTreeNode *row,
				     gboolean new_window, gboolean all_headers)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;
	gint val;

	if (!new_window && summaryview->displayed == row) return;
	g_return_if_fail(row != NULL);

	if (summary_is_locked(summaryview)) return;
	summary_lock(summaryview);

	STATUSBAR_POP(summaryview->mainwin);
	GTK_EVENTS_FLUSH();

	msginfo = gtk_ctree_node_get_row_data(ctree, row);

	if (new_window) {
		MessageView *msgview;

		msgview = messageview_create_with_new_window();
		val = messageview_show(msgview, msginfo, all_headers);
	} else {
		MessageView *msgview;

		msgview = summaryview->messageview;

		summaryview->displayed = row;
		if (!messageview_is_visible(msgview))
			main_window_toggle_message_view(summaryview->mainwin);
		val = messageview_show(msgview, msginfo, all_headers);
		if (msgview->type == MVIEW_TEXT ||
		    (msgview->type == MVIEW_MIME &&
		     (GTK_CLIST(msgview->mimeview->ctree)->row_list == NULL ||
		      gtk_notebook_get_current_page
			(GTK_NOTEBOOK(msgview->mimeview->notebook)) == 0)))
			gtk_widget_grab_focus(summaryview->ctree);
		GTK_EVENTS_FLUSH();
		gtkut_ctree_node_move_if_on_the_edge(ctree, row);
	}

	if (val == 0 &&
	    (new_window || !prefs_common.mark_as_read_on_new_window)) {
		if (MSG_IS_NEW(msginfo->flags))
			summaryview->folder_item->new--;
		if (MSG_IS_UNREAD(msginfo->flags))
			summaryview->folder_item->unread--;
		if (MSG_IS_NEW(msginfo->flags) || MSG_IS_UNREAD(msginfo->flags)) {
			MSG_UNSET_PERM_FLAGS
				(msginfo->flags, MSG_NEW | MSG_UNREAD);
			if (MSG_IS_IMAP(msginfo->flags))
				imap_msg_unset_perm_flags
					(msginfo, MSG_NEW | MSG_UNREAD);
			summary_set_row_marks(summaryview, row);
			gtk_clist_thaw(GTK_CLIST(ctree));
			summary_status_show(summaryview);
		}
	}

	summary_set_menu_sensitive(summaryview);
	main_window_set_toolbar_sensitive(summaryview->mainwin);

	statusbar_pop_all();

	summary_unlock(summaryview);
}

void summary_display_msg_selected(SummaryView *summaryview,
				  gboolean all_headers)
{
	if (summary_is_locked(summaryview)) return;
	summaryview->displayed = NULL;
	summary_display_msg_full(summaryview, summaryview->selected, FALSE,
				 all_headers);
}

void summary_redisplay_msg(SummaryView *summaryview)
{
	GtkCTreeNode *node;

	if (summaryview->displayed) {
		node = summaryview->displayed;
		summaryview->displayed = NULL;
		summary_display_msg(summaryview, node);
	}
}

void summary_open_msg(SummaryView *summaryview)
{
	if (!summaryview->selected) return;

	summary_display_msg_full(summaryview, summaryview->selected,
				 TRUE, FALSE);
}

void summary_view_source(SummaryView * summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;
	SourceWindow *srcwin;

	if (!summaryview->selected) return;

	srcwin = source_window_create();
	msginfo = gtk_ctree_node_get_row_data(ctree, summaryview->selected);
	source_window_show_msg(srcwin, msginfo);
	source_window_show(srcwin);
}

void summary_reedit(SummaryView *summaryview)
{
	MsgInfo *msginfo;

	if (!summaryview->selected) return;
	if (!summaryview->folder_item) return;
	if (summaryview->folder_item->stype != F_OUTBOX &&
	    summaryview->folder_item->stype != F_DRAFT  &&
	    summaryview->folder_item->stype != F_QUEUE) return;

	msginfo = gtk_ctree_node_get_row_data(GTK_CTREE(summaryview->ctree),
					      summaryview->selected);
	if (!msginfo) return;

	compose_reedit(msginfo);
}

gboolean summary_step(SummaryView *summaryview, GtkScrollType type)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node;

	if (summary_is_locked(summaryview)) return FALSE;

	if (type == GTK_SCROLL_STEP_FORWARD) {
		node = gtkut_ctree_node_next(ctree, summaryview->selected);
		if (node)
			gtkut_ctree_expand_parent_all(ctree, node);
		else
			return FALSE;
	} else {
		if (summaryview->selected) {
			node = GTK_CTREE_NODE_PREV(summaryview->selected);
			if (!node) return FALSE;
		}
	}

	if (messageview_is_visible(summaryview->messageview))
		summaryview->display_msg = TRUE;

	g_signal_emit_by_name(G_OBJECT(ctree), "scroll_vertical", type, 0.0);

	if (GTK_CLIST(ctree)->selection)
		gtk_sctree_set_anchor_row
			(GTK_SCTREE(ctree),
			 GTK_CTREE_NODE(GTK_CLIST(ctree)->selection->data));

	return TRUE;
}

void summary_toggle_view(SummaryView *summaryview)
{
	if (!messageview_is_visible(summaryview->messageview) &&
	    summaryview->selected)
		summary_display_msg(summaryview,
				    summaryview->selected);
	else
		main_window_toggle_message_view(summaryview->mainwin);
}

static gboolean summary_search_unread_recursive(GtkCTree *ctree,
						GtkCTreeNode *node)
{
	MsgInfo *msginfo;

	if (node) {
		msginfo = gtk_ctree_node_get_row_data(ctree, node);
		if (msginfo && MSG_IS_UNREAD(msginfo->flags))
			return TRUE;
		node = GTK_CTREE_ROW(node)->children;
	} else
		node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);

	while (node) {
		if (summary_search_unread_recursive(ctree, node) == TRUE)
			return TRUE;
		node = GTK_CTREE_ROW(node)->sibling;
	}

	return FALSE;
}

static gboolean summary_have_unread_children(SummaryView *summaryview,
					     GtkCTreeNode *node)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);

	if (!node) return FALSE;

	node = GTK_CTREE_ROW(node)->children;

	while (node) {
		if (summary_search_unread_recursive(ctree, node) == TRUE)
			return TRUE;
		node = GTK_CTREE_ROW(node)->sibling;
	}

	return FALSE;
}

static void summary_set_row_marks(SummaryView *summaryview, GtkCTreeNode *row)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkStyle *style = NULL;
	MsgInfo *msginfo;
	MsgFlags flags;
	gint *col_pos = summaryview->col_pos;

	msginfo = gtk_ctree_node_get_row_data(ctree, row);
	if (!msginfo) return;

	flags = msginfo->flags;

	gtk_ctree_node_set_foreground(ctree, row, NULL);

	/* set new/unread column */
	if (MSG_IS_NEW(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, col_pos[S_COL_UNREAD],
					  newxpm, newxpmmask);
	} else if (MSG_IS_UNREAD(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, col_pos[S_COL_UNREAD],
					  unreadxpm, unreadxpmmask);
	} else if (MSG_IS_REPLIED(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, col_pos[S_COL_UNREAD],
					  repliedxpm, repliedxpmmask);
	} else if (MSG_IS_FORWARDED(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, col_pos[S_COL_UNREAD],
					  forwardedxpm, forwardedxpmmask);
	} else {
		gtk_ctree_node_set_text(ctree, row, col_pos[S_COL_UNREAD],
					NULL);
	}

	if (prefs_common.bold_unread &&
	    (MSG_IS_UNREAD(flags) ||
	     (!GTK_CTREE_ROW(row)->expanded &&
	      GTK_CTREE_ROW(row)->children &&
	      summary_have_unread_children(summaryview, row))))
		style = bold_style;

	/* set mark column */
	if (MSG_IS_DELETED(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, col_pos[S_COL_MARK],
					  deletedxpm, deletedxpmmask);
		if (style)
			style = bold_deleted_style;
		else
			gtk_ctree_node_set_foreground
				(ctree, row, &summaryview->color_dim);
	} else if (MSG_IS_MOVE(flags)) {
		gtk_ctree_node_set_text(ctree, row, col_pos[S_COL_MARK], "o");
		if (style)
			style = bold_marked_style;
		else
			gtk_ctree_node_set_foreground
				(ctree, row, &summaryview->color_marked);
	} else if (MSG_IS_COPY(flags)) {
		gtk_ctree_node_set_text(ctree, row, col_pos[S_COL_MARK], "O");
		if (style)
			style = bold_marked_style;
		else
			gtk_ctree_node_set_foreground
				(ctree, row, &summaryview->color_marked);
	} else if (MSG_IS_MARKED(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, col_pos[S_COL_MARK],
					  markxpm, markxpmmask);
	} else {
		gtk_ctree_node_set_text(ctree, row, col_pos[S_COL_MARK], NULL);
	}

	if (MSG_IS_MIME(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, col_pos[S_COL_MIME],
					  clipxpm, clipxpmmask);
	} else {
		gtk_ctree_node_set_text(ctree, row, col_pos[S_COL_MIME], NULL);
	}

	gtk_ctree_node_set_row_style(ctree, row, style);

	if (MSG_GET_COLORLABEL(flags))
		summary_set_colorlabel_color(ctree, row,
					     MSG_GET_COLORLABEL_VALUE(flags));
}

void summary_set_marks_selected(SummaryView *summaryview)
{
	GList *cur;

	for (cur = GTK_CLIST(summaryview->ctree)->selection; cur != NULL;
	     cur = cur->next)
		summary_set_row_marks(summaryview, GTK_CTREE_NODE(cur->data));
}

static void summary_mark_row(SummaryView *summaryview, GtkCTreeNode *row)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;

	msginfo = gtk_ctree_node_get_row_data(ctree, row);
	msginfo->to_folder = NULL;
	if (MSG_IS_DELETED(msginfo->flags))
		summaryview->deleted--;
	if (MSG_IS_MOVE(msginfo->flags))
		summaryview->moved--;
	if (MSG_IS_COPY(msginfo->flags))
		summaryview->copied--;
	MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_DELETED);
	MSG_UNSET_TMP_FLAGS(msginfo->flags, MSG_MOVE | MSG_COPY);
	MSG_SET_PERM_FLAGS(msginfo->flags, MSG_MARKED);
	summary_set_row_marks(summaryview, row);
	debug_print(_("Message %d is marked\n"), msginfo->msgnum);
}

void summary_mark(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GList *cur;

	for (cur = GTK_CLIST(ctree)->selection; cur != NULL; cur = cur->next)
		summary_mark_row(summaryview, GTK_CTREE_NODE(cur->data));
	if (FOLDER_TYPE(summaryview->folder_item->folder) == F_IMAP) {
		GSList *msglist;
		msglist = summary_get_selected_msg_list(summaryview);
		imap_msg_list_set_perm_flags(msglist, MSG_MARKED);
		g_slist_free(msglist);
	}

	/* summary_step(summaryview, GTK_SCROLL_STEP_FORWARD); */
}

static void summary_mark_row_as_read(SummaryView *summaryview,
				     GtkCTreeNode *row)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;

	msginfo = gtk_ctree_node_get_row_data(ctree, row);
	if (MSG_IS_NEW(msginfo->flags))
		summaryview->folder_item->new--;
	if (MSG_IS_UNREAD(msginfo->flags))
		summaryview->folder_item->unread--;
	if (MSG_IS_NEW(msginfo->flags) ||
	    MSG_IS_UNREAD(msginfo->flags)) {
		MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_NEW | MSG_UNREAD);
		summary_set_row_marks(summaryview, row);
		debug_print(_("Message %d is marked as being read\n"),
			    msginfo->msgnum);
	}
}

void summary_mark_as_read(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GList *cur;

	for (cur = GTK_CLIST(ctree)->selection; cur != NULL; cur = cur->next)
		summary_mark_row_as_read(summaryview,
					 GTK_CTREE_NODE(cur->data));
	if (FOLDER_TYPE(summaryview->folder_item->folder) == F_IMAP) {
		GSList *msglist;
		msglist = summary_get_selected_msg_list(summaryview);
		imap_msg_list_unset_perm_flags(msglist, MSG_NEW | MSG_UNREAD);
		g_slist_free(msglist);
	}

	summary_status_show(summaryview);
}

void summary_mark_all_read(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCList *clist = GTK_CLIST(summaryview->ctree);
	GtkCTreeNode *node;

	gtk_clist_freeze(clist);
	for (node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list); node != NULL;
	     node = gtkut_ctree_node_next(ctree, node))
		summary_mark_row_as_read(summaryview, node);
	for (node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list); node != NULL;
	     node = gtkut_ctree_node_next(ctree, node)) {
		if (!GTK_CTREE_ROW(node)->expanded)
			summary_set_row_marks(summaryview, node);
	}
	if (FOLDER_TYPE(summaryview->folder_item->folder) == F_IMAP) {
		GSList *msglist;
		msglist = summary_get_msg_list(summaryview);
		imap_msg_list_unset_perm_flags(msglist, MSG_NEW | MSG_UNREAD);
		g_slist_free(msglist);
	}
	gtk_clist_thaw(clist);

	summary_status_show(summaryview);
}

static void summary_mark_row_as_unread(SummaryView *summaryview,
				       GtkCTreeNode *row)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;

	msginfo = gtk_ctree_node_get_row_data(ctree, row);
	if (MSG_IS_DELETED(msginfo->flags)) {
		msginfo->to_folder = NULL;
		MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_DELETED);
		summaryview->deleted--;
	}
	MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_REPLIED | MSG_FORWARDED);
	if (!MSG_IS_UNREAD(msginfo->flags)) {
		MSG_SET_PERM_FLAGS(msginfo->flags, MSG_UNREAD);
		summaryview->folder_item->unread++;
		debug_print(_("Message %d is marked as unread\n"),
			    msginfo->msgnum);
	}
	summary_set_row_marks(summaryview, row);
}

void summary_mark_as_unread(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GList *cur;

	for (cur = GTK_CLIST(ctree)->selection; cur != NULL; cur = cur->next)
		summary_mark_row_as_unread(summaryview,
					   GTK_CTREE_NODE(cur->data));
	if (FOLDER_TYPE(summaryview->folder_item->folder) == F_IMAP) {
		GSList *msglist;
		msglist = summary_get_selected_msg_list(summaryview);
		imap_msg_list_unset_perm_flags(msglist, MSG_REPLIED);
		imap_msg_list_set_perm_flags(msglist, MSG_UNREAD);
		g_slist_free(msglist);
	}

	summary_status_show(summaryview);
}

static void summary_delete_row(SummaryView *summaryview, GtkCTreeNode *row)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;

	msginfo = gtk_ctree_node_get_row_data(ctree, row);

	if (MSG_IS_DELETED(msginfo->flags)) return;

	msginfo->to_folder = NULL;
	if (MSG_IS_MOVE(msginfo->flags))
		summaryview->moved--;
	if (MSG_IS_COPY(msginfo->flags))
		summaryview->copied--;
	MSG_UNSET_TMP_FLAGS(msginfo->flags, MSG_MOVE | MSG_COPY);
	MSG_SET_PERM_FLAGS(msginfo->flags, MSG_DELETED);
	summaryview->deleted++;

	if (!prefs_common.immediate_exec && 
	    summaryview->folder_item->stype != F_TRASH)
		summary_set_row_marks(summaryview, row);

	debug_print(_("Message %s/%d is set to delete\n"),
		    msginfo->folder->path, msginfo->msgnum);
}

void summary_delete(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	FolderItem *item = summaryview->folder_item;
	GList *cur;
	GtkCTreeNode *sel_last = NULL;
	GtkCTreeNode *node;

	if (!item || FOLDER_TYPE(item->folder) == F_NEWS) return;

	if (summary_is_locked(summaryview)) return;

	/* if current folder is trash, ask for confirmation */
	if (item->stype == F_TRASH) {
		AlertValue aval;

		aval = alertpanel(_("Delete message(s)"),
				  _("Do you really want to delete message(s) from the trash?"),
				  GTK_STOCK_YES, GTK_STOCK_NO, NULL);
		if (aval != G_ALERTDEFAULT) return;
	}

	/* next code sets current row focus right. We need to find a row
	 * that is not deleted. */
	for (cur = GTK_CLIST(ctree)->selection; cur != NULL; cur = cur->next) {
		sel_last = GTK_CTREE_NODE(cur->data);
		summary_delete_row(summaryview, sel_last);
	}

	node = summary_find_next_msg(summaryview, sel_last);
	if (!node)
		node = summary_find_prev_msg(summaryview, sel_last);

	if (node) {
		if (sel_last && node == gtkut_ctree_node_next(ctree, sel_last))
			summary_step(summaryview, GTK_SCROLL_STEP_FORWARD);
		else if (sel_last && node == GTK_CTREE_NODE_PREV(sel_last))
			summary_step(summaryview, GTK_SCROLL_STEP_BACKWARD);
		else
			summary_select_node
				(summaryview, node,
				 messageview_is_visible(summaryview->messageview),
				 FALSE);
	}

	if (prefs_common.immediate_exec || item->stype == F_TRASH)
		summary_execute(summaryview);
	else
		summary_status_show(summaryview);
}

void summary_delete_duplicated(SummaryView *summaryview)
{
	if (!summaryview->folder_item ||
	    FOLDER_TYPE(summaryview->folder_item->folder) == F_NEWS) return;
	if (summaryview->folder_item->stype == F_TRASH) return;

	main_window_cursor_wait(summaryview->mainwin);
	debug_print(_("Deleting duplicated messages..."));
	STATUSBAR_PUSH(summaryview->mainwin,
		       _("Deleting duplicated messages..."));

	gtk_ctree_pre_recursive(GTK_CTREE(summaryview->ctree), NULL,
				GTK_CTREE_FUNC(summary_delete_duplicated_func),
				summaryview);

	if (prefs_common.immediate_exec)
		summary_execute(summaryview);
	else
		summary_status_show(summaryview);

	debug_print(_("done.\n"));
	STATUSBAR_POP(summaryview->mainwin);
	main_window_cursor_normal(summaryview->mainwin);
}

static void summary_delete_duplicated_func(GtkCTree *ctree, GtkCTreeNode *node,
					   SummaryView *summaryview)
{
	GtkCTreeNode *found;
	MsgInfo *msginfo;

	msginfo = GTKUT_CTREE_NODE_GET_ROW_DATA(node);

	if (!msginfo || !msginfo->msgid || !*msginfo->msgid) return;

	found = g_hash_table_lookup(summaryview->msgid_table, msginfo->msgid);

	if (found && found != node)
		summary_delete_row(summaryview, node);
}

static void summary_unmark_row(SummaryView *summaryview, GtkCTreeNode *row)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;

	msginfo = gtk_ctree_node_get_row_data(ctree, row);
	msginfo->to_folder = NULL;
	if (MSG_IS_DELETED(msginfo->flags))
		summaryview->deleted--;
	if (MSG_IS_MOVE(msginfo->flags))
		summaryview->moved--;
	if (MSG_IS_COPY(msginfo->flags))
		summaryview->copied--;
	MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_MARKED | MSG_DELETED);
	MSG_UNSET_TMP_FLAGS(msginfo->flags, MSG_MOVE | MSG_COPY);
	summary_set_row_marks(summaryview, row);

	debug_print(_("Message %s/%d is unmarked\n"),
		    msginfo->folder->path, msginfo->msgnum);
}

void summary_unmark(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GList *cur;

	for (cur = GTK_CLIST(ctree)->selection; cur != NULL; cur = cur->next)
		summary_unmark_row(summaryview, GTK_CTREE_NODE(cur->data));
	if (FOLDER_TYPE(summaryview->folder_item->folder) == F_IMAP) {
		GSList *msglist;
		msglist = summary_get_selected_msg_list(summaryview);
		imap_msg_list_unset_perm_flags(msglist, MSG_MARKED);
		g_slist_free(msglist);
	}

	summary_status_show(summaryview);
}

static void summary_move_row_to(SummaryView *summaryview, GtkCTreeNode *row,
				FolderItem *to_folder)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;

	g_return_if_fail(to_folder != NULL);

	msginfo = gtk_ctree_node_get_row_data(ctree, row);
	msginfo->to_folder = to_folder;
	if (MSG_IS_DELETED(msginfo->flags))
		summaryview->deleted--;
	MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_DELETED);
	MSG_UNSET_TMP_FLAGS(msginfo->flags, MSG_COPY);
	if (!MSG_IS_MOVE(msginfo->flags)) {
		MSG_SET_TMP_FLAGS(msginfo->flags, MSG_MOVE);
		summaryview->moved++;
	}
	if (!prefs_common.immediate_exec)
		summary_set_row_marks(summaryview, row);

	debug_print(_("Message %d is set to move to %s\n"),
		    msginfo->msgnum, to_folder->path);
}

void summary_move_selected_to(SummaryView *summaryview, FolderItem *to_folder)
{
	GList *cur;

	if (!to_folder) return;
	if (!summaryview->folder_item ||
	    FOLDER_TYPE(summaryview->folder_item->folder) == F_NEWS) return;

	if (summary_is_locked(summaryview)) return;

	if (summaryview->folder_item == to_folder) {
		alertpanel_warning(_("Destination is same as current folder."));
		return;
	}

	for (cur = GTK_CLIST(summaryview->ctree)->selection;
	     cur != NULL; cur = cur->next)
		summary_move_row_to
			(summaryview, GTK_CTREE_NODE(cur->data), to_folder);

	summary_step(summaryview, GTK_SCROLL_STEP_FORWARD);

	if (prefs_common.immediate_exec)
		summary_execute(summaryview);
	else
		summary_status_show(summaryview);
}

void summary_move_to(SummaryView *summaryview)
{
	FolderItem *to_folder;

	if (!summaryview->folder_item ||
	    FOLDER_TYPE(summaryview->folder_item->folder) == F_NEWS) return;

	to_folder = foldersel_folder_sel(summaryview->folder_item->folder,
					 FOLDER_SEL_MOVE, NULL);
	summary_move_selected_to(summaryview, to_folder);
}

static void summary_copy_row_to(SummaryView *summaryview, GtkCTreeNode *row,
				FolderItem *to_folder)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;

	g_return_if_fail(to_folder != NULL);

	msginfo = gtk_ctree_node_get_row_data(ctree, row);
	msginfo->to_folder = to_folder;
	if (MSG_IS_DELETED(msginfo->flags))
		summaryview->deleted--;
	MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_DELETED);
	MSG_UNSET_TMP_FLAGS(msginfo->flags, MSG_MOVE);
	if (!MSG_IS_COPY(msginfo->flags)) {
		MSG_SET_TMP_FLAGS(msginfo->flags, MSG_COPY);
		summaryview->copied++;
	}
	if (!prefs_common.immediate_exec)
		summary_set_row_marks(summaryview, row);

	debug_print(_("Message %d is set to copy to %s\n"),
		    msginfo->msgnum, to_folder->path);
}

void summary_copy_selected_to(SummaryView *summaryview, FolderItem *to_folder)
{
	GList *cur;

	if (!to_folder) return;
	if (!summaryview->folder_item) return;

	if (summary_is_locked(summaryview)) return;

	if (summaryview->folder_item == to_folder) {
		alertpanel_warning
			(_("Destination for copy is same as current folder."));
		return;
	}

	for (cur = GTK_CLIST(summaryview->ctree)->selection;
	     cur != NULL; cur = cur->next)
		summary_copy_row_to
			(summaryview, GTK_CTREE_NODE(cur->data), to_folder);

	summary_step(summaryview, GTK_SCROLL_STEP_FORWARD);

	if (prefs_common.immediate_exec)
		summary_execute(summaryview);
	else
		summary_status_show(summaryview);
}

void summary_copy_to(SummaryView *summaryview)
{
	FolderItem *to_folder;

	if (!summaryview->folder_item) return;

	to_folder = foldersel_folder_sel(summaryview->folder_item->folder,
					 FOLDER_SEL_COPY, NULL);
	summary_copy_selected_to(summaryview, to_folder);
}

void summary_add_address(SummaryView *summaryview)
{
	MsgInfo *msginfo;
	gchar *from;

	msginfo = gtk_ctree_node_get_row_data(GTK_CTREE(summaryview->ctree),
					      summaryview->selected);
	if (!msginfo) return;

	Xstrdup_a(from, msginfo->from, return);
	eliminate_address_comment(from);
	extract_address(from);
	addressbook_add_contact(msginfo->fromname, from, NULL);
}

void summary_select_all(SummaryView *summaryview)
{
	if (!summaryview->folder_item) return;

	if (summaryview->folder_item->total >= 500) {
		STATUSBAR_PUSH(summaryview->mainwin,
			       _("Selecting all messages..."));
		main_window_cursor_wait(summaryview->mainwin);
	}

	gtk_clist_select_all(GTK_CLIST(summaryview->ctree));

	if (summaryview->folder_item->total >= 500) {
		STATUSBAR_POP(summaryview->mainwin);
		main_window_cursor_normal(summaryview->mainwin);
	}
}

void summary_unselect_all(SummaryView *summaryview)
{
	gtk_sctree_unselect_all(GTK_SCTREE(summaryview->ctree));
}

void summary_select_thread(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node = summaryview->selected;

	if (!node) return;

	while (GTK_CTREE_ROW(node)->parent != NULL)
		node = GTK_CTREE_ROW(node)->parent;

	if (node != summaryview->selected)
		summary_select_node
			(summaryview, node,
			 messageview_is_visible(summaryview->messageview),
			 FALSE);

	gtk_ctree_select_recursive(ctree, node);

	summary_status_show(summaryview);
}

void summary_save_as(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;
	gchar *filename = NULL;
	gchar *src, *dest;

	if (!summaryview->selected) return;
	msginfo = gtk_ctree_node_get_row_data(ctree, summaryview->selected);
	if (!msginfo) return;

	if (msginfo->subject) {
		Xstrdup_a(filename, msginfo->subject, return);
		subst_for_filename(filename);
	}

	dest = filesel_save_as(filename);
	if (!dest) return;

	src = procmsg_get_message_file(msginfo);
	if (copy_file(src, dest, TRUE) < 0) {
		gchar *utf8_dest;

		utf8_dest = conv_filename_to_utf8(dest);
		alertpanel_error(_("Can't save the file `%s'."),
				 g_basename(utf8_dest));
		g_free(utf8_dest);
	}
	g_free(src);

	g_free(dest);
}

void summary_print(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCList *clist = GTK_CLIST(summaryview->ctree);
	MsgInfo *msginfo;
	GList *cur;
	gchar *cmdline;
	gchar *p;

	if (clist->selection == NULL) return;

	cmdline = input_dialog(_("Print"),
			       _("Enter the print command line:\n"
				 "(`%s' will be replaced with file name)"),
			       prefs_common.print_cmd);
	if (!cmdline) return;
	if (!(p = strchr(cmdline, '%')) || *(p + 1) != 's' ||
	    strchr(p + 2, '%')) {
		alertpanel_error(_("Print command line is invalid:\n`%s'"),
				 cmdline);
		g_free(cmdline);
		return;
	}

	for (cur = clist->selection; cur != NULL; cur = cur->next) {
		msginfo = gtk_ctree_node_get_row_data
			(ctree, GTK_CTREE_NODE(cur->data));
		if (msginfo) procmsg_print_message(msginfo, cmdline);
	}

	g_free(cmdline);
}

gboolean summary_execute(SummaryView *summaryview)
{
	GtkCList *clist = GTK_CLIST(summaryview->ctree);
	gint val = 0;

	if (!summaryview->folder_item) return FALSE;

	if (summary_is_locked(summaryview)) return FALSE;
	summary_lock(summaryview);

	gtk_clist_freeze(clist);

	val |= summary_execute_move(summaryview);
	val |= summary_execute_copy(summaryview);
	val |= summary_execute_delete(summaryview);

	statusbar_pop_all();
	STATUSBAR_POP(summaryview->mainwin);

	summary_remove_invalid_messages(summaryview);

	gtk_clist_thaw(clist);

	summary_unlock(summaryview);

	if (val != 0) {
		alertpanel_error(_("Error occurred while processing messages."));
	}

	return TRUE;
}

static void summary_remove_invalid_messages(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCList *clist = GTK_CLIST(summaryview->ctree);
	GtkCTreeNode *node, *next;
	GtkCTreeNode *new_selected = NULL;

	gtk_clist_freeze(clist);

	if (summaryview->folder_item->threaded)
		summary_unthread_for_exec(summaryview);

	node = GTK_CTREE_NODE(clist->row_list);
	for (; node != NULL; node = next) {
		MsgInfo *msginfo;

		next = gtkut_ctree_node_next(ctree, node);

		msginfo = GTKUT_CTREE_NODE_GET_ROW_DATA(node);
		if (!msginfo || !MSG_IS_INVALID(msginfo->flags))
			continue;

		if (node == summaryview->displayed) {
			messageview_clear(summaryview->messageview);
			summaryview->displayed = NULL;
		}
		if (GTK_CTREE_ROW(node)->children != NULL) {
			g_warning("summary_execute(): children != NULL\n");
			continue;
		}

		if (!new_selected &&
		    gtkut_ctree_node_is_selected(ctree, node)) {
			gtk_sctree_unselect_all(GTK_SCTREE(ctree));
			new_selected = summary_find_next_msg(summaryview, node);
			if (!new_selected)
				new_selected = summary_find_prev_msg
					(summaryview, node);
		}

		if (msginfo->msgid && *msginfo->msgid &&
		    node == g_hash_table_lookup(summaryview->msgid_table,
						msginfo->msgid))
			g_hash_table_remove(summaryview->msgid_table,
					    msginfo->msgid);

		gtk_ctree_remove_node(ctree, node);
		procmsg_msginfo_free(msginfo);
	}

	if (new_selected) {
		gtk_sctree_select
			(GTK_SCTREE(ctree),
			 summaryview->displayed ? summaryview->displayed
			 : new_selected);
	}

	if (summaryview->folder_item->threaded)
		summary_thread_build(summaryview);

	summaryview->selected = clist->selection ?
		GTK_CTREE_NODE(clist->selection->data) : NULL;

	if (!GTK_CLIST(summaryview->ctree)->row_list) {
		menu_set_insensitive_all
			(GTK_MENU_SHELL(summaryview->popupmenu));
		gtk_widget_grab_focus(summaryview->folderview->ctree);
	} else
		gtk_widget_grab_focus(summaryview->ctree);

	summary_write_cache(summaryview);

	summary_update_status(summaryview);
	summary_status_show(summaryview);

	gtk_ctree_node_moveto(ctree, summaryview->selected, -1, 0.5, 0);

	gtk_clist_thaw(clist);
}

static gint summary_execute_move(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	gint val = 0;

	summaryview->folder_table = g_hash_table_new(NULL, NULL);

	/* search moving messages and execute */
	gtk_ctree_pre_recursive(ctree, NULL, summary_execute_move_func,
				summaryview);

	if (summaryview->mlist) {
		summaryview->mlist = g_slist_reverse(summaryview->mlist);
		val = procmsg_move_messages(summaryview->mlist);

		folder_item_scan_foreach(summaryview->folder_table);
		folderview_update_item_foreach(summaryview->folder_table,
					       FALSE);

		g_slist_free(summaryview->mlist);
		summaryview->mlist = NULL;
	}

	g_hash_table_destroy(summaryview->folder_table);
	summaryview->folder_table = NULL;

	return val;
}

static void summary_execute_move_func(GtkCTree *ctree, GtkCTreeNode *node,
				      gpointer data)
{
	SummaryView *summaryview = data;
	MsgInfo *msginfo;

	msginfo = GTKUT_CTREE_NODE_GET_ROW_DATA(node);

	if (msginfo && MSG_IS_MOVE(msginfo->flags) && msginfo->to_folder) {
		g_hash_table_insert(summaryview->folder_table,
				    msginfo->to_folder, GINT_TO_POINTER(1));

		summaryview->mlist =
			g_slist_prepend(summaryview->mlist, msginfo);

		MSG_UNSET_TMP_FLAGS(msginfo->flags, MSG_MOVE);
		summary_set_row_marks(summaryview, node);
	}
}

static gint summary_execute_copy(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	gint val = 0;

	summaryview->folder_table = g_hash_table_new(NULL, NULL);

	/* search copying messages and execute */
	gtk_ctree_pre_recursive(ctree, NULL, summary_execute_copy_func,
				summaryview);

	if (summaryview->mlist) {
		summaryview->mlist = g_slist_reverse(summaryview->mlist);
		val = procmsg_copy_messages(summaryview->mlist);

		folder_item_scan_foreach(summaryview->folder_table);
		folderview_update_item_foreach(summaryview->folder_table,
					       FALSE);

		g_slist_free(summaryview->mlist);
		summaryview->mlist = NULL;
	}

	g_hash_table_destroy(summaryview->folder_table);
	summaryview->folder_table = NULL;

	return val;
}

static void summary_execute_copy_func(GtkCTree *ctree, GtkCTreeNode *node,
				      gpointer data)
{
	SummaryView *summaryview = data;
	MsgInfo *msginfo;

	msginfo = GTKUT_CTREE_NODE_GET_ROW_DATA(node);

	if (msginfo && MSG_IS_COPY(msginfo->flags) && msginfo->to_folder) {
		g_hash_table_insert(summaryview->folder_table,
				    msginfo->to_folder, GINT_TO_POINTER(1));

		summaryview->mlist =
			g_slist_prepend(summaryview->mlist, msginfo);

		MSG_UNSET_TMP_FLAGS(msginfo->flags, MSG_COPY);
		summary_set_row_marks(summaryview, node);
	}
}

static gint summary_execute_delete(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	FolderItem *trash;
	gint val;

	trash = summaryview->folder_item->folder->trash;
	if (FOLDER_TYPE(summaryview->folder_item->folder) == F_MH) {
		g_return_val_if_fail(trash != NULL, 0);
	}

	/* search deleting messages and execute */
	gtk_ctree_pre_recursive
		(ctree, NULL, summary_execute_delete_func, summaryview);

	if (!summaryview->mlist) return 0;

	summaryview->mlist = g_slist_reverse(summaryview->mlist);

	if (summaryview->folder_item != trash)
		val = folder_item_move_msgs(trash, summaryview->mlist);
	else
		val = folder_item_remove_msgs(trash, summaryview->mlist);

	g_slist_free(summaryview->mlist);
	summaryview->mlist = NULL;

	if (summaryview->folder_item != trash) {
		folder_item_scan(trash);
		folderview_update_item(trash, FALSE);
	}

	return val == -1 ? -1 : 0;
}

static void summary_execute_delete_func(GtkCTree *ctree, GtkCTreeNode *node,
					gpointer data)
{
	SummaryView *summaryview = data;
	MsgInfo *msginfo;

	msginfo = GTKUT_CTREE_NODE_GET_ROW_DATA(node);

	if (msginfo && MSG_IS_DELETED(msginfo->flags)) {
		summaryview->mlist =
			g_slist_prepend(summaryview->mlist, msginfo);
	}
}

/* thread functions */

void summary_thread_build(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node;
	GtkCTreeNode *next;
	GtkCTreeNode *parent;
	MsgInfo *msginfo;

	summary_lock(summaryview);

	debug_print(_("Building threads..."));
	STATUSBAR_PUSH(summaryview->mainwin, _("Building threads..."));
	main_window_cursor_wait(summaryview->mainwin);

	g_signal_handlers_block_by_func(G_OBJECT(ctree),
					G_CALLBACK(summary_tree_expanded),
					summaryview);
	gtk_clist_freeze(GTK_CLIST(ctree));

	node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);
	while (node) {
		next = GTK_CTREE_ROW(node)->sibling;

		msginfo = GTKUT_CTREE_NODE_GET_ROW_DATA(node);
		if (msginfo && msginfo->inreplyto) {
			parent = g_hash_table_lookup(summaryview->msgid_table,
						     msginfo->inreplyto);
			if (parent && parent != node) {
				gtk_ctree_move(ctree, node, parent, NULL);
				gtk_ctree_expand(ctree, node);
			}
		}

		node = next;
	}

	node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);

	while (node) {
		next = GTK_CTREE_NODE_NEXT(node);
		if (prefs_common.expand_thread)
			gtk_ctree_expand(ctree, node);
		if (prefs_common.bold_unread &&
		    GTK_CTREE_ROW(node)->children)
			summary_set_row_marks(summaryview, node);
		node = next;
	}

	gtk_clist_thaw(GTK_CLIST(ctree));
	g_signal_handlers_unblock_by_func(G_OBJECT(ctree),
					  G_CALLBACK(summary_tree_expanded),
					  summaryview);

	debug_print(_("done.\n"));
	STATUSBAR_POP(summaryview->mainwin);
	main_window_cursor_normal(summaryview->mainwin);

	summary_unlock(summaryview);
}

static void summary_thread_init(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);
	GtkCTreeNode *next;

	if (prefs_common.expand_thread) {
		while (node) {
			next = GTK_CTREE_ROW(node)->sibling;
			if (GTK_CTREE_ROW(node)->children)
				gtk_ctree_expand(ctree, node);
			node = next;
		}
	} else if (prefs_common.bold_unread) {
		while (node) {
			next = GTK_CTREE_ROW(node)->sibling;
			if (GTK_CTREE_ROW(node)->children)
				summary_set_row_marks(summaryview, node);
			node = next;
		}
	}
}

void summary_unthread(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node;
	GtkCTreeNode *child;
	GtkCTreeNode *sibling;
	GtkCTreeNode *next_child;

	summary_lock(summaryview);

	debug_print(_("Unthreading..."));
	STATUSBAR_PUSH(summaryview->mainwin, _("Unthreading..."));
	main_window_cursor_wait(summaryview->mainwin);

	g_signal_handlers_block_by_func(G_OBJECT(ctree),
					G_CALLBACK(summary_tree_collapsed),
					summaryview);
	gtk_clist_freeze(GTK_CLIST(ctree));

	for (node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);
	     node != NULL; node = GTK_CTREE_NODE_NEXT(node)) {
		child = GTK_CTREE_ROW(node)->children;
		sibling = GTK_CTREE_ROW(node)->sibling;

		while (child != NULL) {
			next_child = GTK_CTREE_ROW(child)->sibling;
			gtk_ctree_move(ctree, child, NULL, sibling);
			child = next_child;
		}
	}

	gtk_clist_thaw(GTK_CLIST(ctree));
	g_signal_handlers_unblock_by_func(G_OBJECT(ctree),
					  G_CALLBACK(summary_tree_collapsed),
					  summaryview);

	debug_print(_("done.\n"));
	STATUSBAR_POP(summaryview->mainwin);
	main_window_cursor_normal(summaryview->mainwin);

	summary_unlock(summaryview);
}

static void summary_unthread_for_exec(SummaryView *summaryview)
{
	GtkCTreeNode *node;
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);

	summary_lock(summaryview);

	debug_print(_("Unthreading for execution..."));

	gtk_clist_freeze(GTK_CLIST(ctree));

	for (node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);
	     node != NULL; node = gtkut_ctree_node_next(ctree, node)) {
		summary_unthread_for_exec_func(ctree, node, NULL);
	}

	gtk_clist_thaw(GTK_CLIST(ctree));

	debug_print(_("done.\n"));

	summary_unlock(summaryview);
}

static void summary_unthread_for_exec_func(GtkCTree *ctree, GtkCTreeNode *node,
					   gpointer data)
{
	MsgInfo *msginfo;
	GtkCTreeNode *top_parent;
	GtkCTreeNode *child;
	GtkCTreeNode *sibling;

	msginfo = GTKUT_CTREE_NODE_GET_ROW_DATA(node);

	if (!msginfo || !MSG_IS_INVALID(msginfo->flags))
		return;
	child = GTK_CTREE_ROW(node)->children;
	if (!child) return;

	for (top_parent = node;
	     GTK_CTREE_ROW(top_parent)->parent != NULL;
	     top_parent = GTK_CTREE_ROW(top_parent)->parent)
		;
	sibling = GTK_CTREE_ROW(top_parent)->sibling;

	while (child != NULL) {
		GtkCTreeNode *next_child;

		next_child = GTK_CTREE_ROW(child)->sibling;
		gtk_ctree_move(ctree, child, NULL, sibling);
		child = next_child;
	}
}

void summary_expand_threads(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);

	gtk_clist_freeze(GTK_CLIST(ctree));

	while (node) {
		if (GTK_CTREE_ROW(node)->children)
			gtk_ctree_expand(ctree, node);
		node = GTK_CTREE_NODE_NEXT(node);
	}

	gtk_clist_thaw(GTK_CLIST(ctree));

	gtk_ctree_node_moveto(ctree, summaryview->selected, -1, 0.5, 0);
}

void summary_collapse_threads(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);

	gtk_clist_freeze(GTK_CLIST(ctree));

	while (node) {
		if (GTK_CTREE_ROW(node)->children)
			gtk_ctree_collapse(ctree, node);
		node = GTK_CTREE_ROW(node)->sibling;
	}

	gtk_clist_thaw(GTK_CLIST(ctree));

	gtk_ctree_node_moveto(ctree, summaryview->selected, -1, 0.5, 0);
}

void summary_filter(SummaryView *summaryview, gboolean selected_only)
{
	if (!prefs_common.fltlist) return;

	summary_lock(summaryview);

	STATUSBAR_POP(summaryview->mainwin);

	debug_print(_("filtering..."));
	STATUSBAR_PUSH(summaryview->mainwin, _("Filtering..."));
	main_window_cursor_wait(summaryview->mainwin);

	gtk_clist_freeze(GTK_CLIST(summaryview->ctree));

	summaryview->filtered = 0;

	if (selected_only) {
		GList *cur;

		for (cur = GTK_CLIST(summaryview->ctree)->selection;
		     cur != NULL; cur = cur->next) {
			summary_filter_func(GTK_CTREE(summaryview->ctree),
					    GTK_CTREE_NODE(cur->data),
					    summaryview);
		}
	} else {
		gtk_ctree_pre_recursive(GTK_CTREE(summaryview->ctree), NULL,
					GTK_CTREE_FUNC(summary_filter_func),
					summaryview);
	}

	summary_unlock(summaryview);

	if (prefs_common.immediate_exec)
		summary_execute(summaryview);
	else
		summary_status_show(summaryview);

	folderview_update_all_updated(FALSE);

	gtk_clist_thaw(GTK_CLIST(summaryview->ctree));

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
}

static void summary_filter_func(GtkCTree *ctree, GtkCTreeNode *node,
				gpointer data)
{
	MsgInfo *msginfo = GTKUT_CTREE_NODE_GET_ROW_DATA(node);
	SummaryView *summaryview = (SummaryView *)data;
	FilterInfo *fltinfo;

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

	if ((fltinfo->actions[FLT_ACTION_MARK] ||
	     fltinfo->actions[FLT_ACTION_COLOR_LABEL] ||
	     fltinfo->actions[FLT_ACTION_MARK_READ])) {
		msginfo->flags = fltinfo->flags;
		summary_set_row_marks(summaryview, node);
	}

	if (fltinfo->actions[FLT_ACTION_MOVE] && fltinfo->move_dest)
		summary_move_row_to(summaryview, node, fltinfo->move_dest);
	else if (fltinfo->actions[FLT_ACTION_DELETE])
		summary_delete_row(summaryview, node);

	filter_info_free(fltinfo);
}

void summary_filter_open(SummaryView *summaryview, PrefsFilterType type)
{
	MsgInfo *msginfo;
	gchar *header = NULL;
	gchar *key = NULL;

	if (!summaryview->selected) return;

	msginfo = gtk_ctree_node_get_row_data(GTK_CTREE(summaryview->ctree),
					      summaryview->selected);
	if (!msginfo) return;

	procmsg_get_filter_keyword(msginfo, &header, &key, type);
	prefs_filter_open(msginfo, header);

	g_free(header);
	g_free(key);
}

void summary_reply(SummaryView *summaryview, ComposeMode mode)
{
	GList *sel = GTK_CLIST(summaryview->ctree)->selection;
	GSList *mlist = NULL;
	MsgInfo *msginfo;
	MsgInfo *displayed_msginfo = NULL;
	gchar *text = NULL;

	for (; sel != NULL; sel = sel->next) {
		mlist = g_slist_append(mlist,
				       gtk_ctree_node_get_row_data
					(GTK_CTREE(summaryview->ctree),
					 GTK_CTREE_NODE(sel->data)));
	}
	if (!mlist) return;
	msginfo = (MsgInfo *)mlist->data;

	if (summaryview->displayed) {
		displayed_msginfo = gtk_ctree_node_get_row_data
			(GTK_CTREE(summaryview->ctree), summaryview->displayed);
	}

	/* use selection only if the displayed message is selected */
	if (!mlist->next && msginfo == displayed_msginfo) {
		text = gtkut_text_view_get_selection
			(GTK_TEXT_VIEW(summaryview->messageview->textview->text));
		if (text && *text == '\0') {
			g_free(text);
			text = NULL;
		}
	}

	if (!COMPOSE_QUOTE_MODE(mode))
		mode |= prefs_common.reply_with_quote
			? COMPOSE_WITH_QUOTE : COMPOSE_WITHOUT_QUOTE;

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

	summary_set_marks_selected(summaryview);
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

/* summary_set_colorlabel_color() - labelcolor parameter is the color *flag*
 * for the messsage; not the color index */
void summary_set_colorlabel_color(GtkCTree *ctree, GtkCTreeNode *node,
				  guint labelcolor)
{
	GdkColor color;
	GtkStyle *style, *prev_style, *ctree_style;
	MsgInfo *msginfo;
	gint color_index;

	msginfo = gtk_ctree_node_get_row_data(ctree, node);
	MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_CLABEL_FLAG_MASK);
	MSG_SET_COLORLABEL_VALUE(msginfo->flags, labelcolor);

	color_index = labelcolor == 0 ? -1 : (gint)labelcolor - 1;
	ctree_style = gtk_widget_get_style(GTK_WIDGET(ctree));
	prev_style = gtk_ctree_node_get_row_style(ctree, node);

	if (color_index < 0 || color_index >= N_COLOR_LABELS) {
		if (!prev_style) return;
		style = gtk_style_copy(prev_style);
		color = ctree_style->fg[GTK_STATE_NORMAL];
		style->fg[GTK_STATE_NORMAL] = color;
		color = ctree_style->fg[GTK_STATE_SELECTED];
		style->fg[GTK_STATE_SELECTED] = color;
	} else {
		if (prev_style)
			style = gtk_style_copy(prev_style);
		else
			style = gtk_style_copy(ctree_style);
		color = colorlabel_get_color(color_index);
		style->fg[GTK_STATE_NORMAL] = color;
		/* get the average of label color and selected fg color
		   for visibility */
		style->fg[GTK_STATE_SELECTED].red   = (color.red   + ctree_style->fg[GTK_STATE_SELECTED].red  ) / 2;
		style->fg[GTK_STATE_SELECTED].green = (color.green + ctree_style->fg[GTK_STATE_SELECTED].green) / 2;
		style->fg[GTK_STATE_SELECTED].blue  = (color.blue  + ctree_style->fg[GTK_STATE_SELECTED].blue ) / 2;
	}

	gtk_ctree_node_set_row_style(ctree, node, style);
}

void summary_set_colorlabel(SummaryView *summaryview, guint labelcolor,
			    GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCList *clist = GTK_CLIST(summaryview->ctree);
	GList *cur;

	for (cur = clist->selection; cur != NULL; cur = cur->next)
		summary_set_colorlabel_color(ctree, GTK_CTREE_NODE(cur->data),
					     labelcolor);
}

static void summary_colorlabel_menu_item_activate_item_cb(GtkMenuItem *menuitem,
							  gpointer data)
{
	SummaryView *summaryview;
	GtkMenuShell *menu;
	GtkCheckMenuItem **items;
	gint n;
	GList *cur, *sel;

	summaryview = (SummaryView *)data;
	g_return_if_fail(summaryview != NULL);

	sel = GTK_CLIST(summaryview->ctree)->selection;
	if (!sel) return;

	menu = GTK_MENU_SHELL(summaryview->colorlabel_menu);
	g_return_if_fail(menu != NULL);

	Xalloca(items, (N_COLOR_LABELS + 1) * sizeof(GtkWidget *), return);

	/* NOTE: don't return prematurely because we set the "dont_toggle"
	 * state for check menu items */
	g_object_set_data(G_OBJECT(menu), "dont_toggle", GINT_TO_POINTER(1));

	/* clear items. get item pointers. */
	for (n = 0, cur = menu->children; cur != NULL; cur = cur->next) {
		if (GTK_IS_CHECK_MENU_ITEM(cur->data)) {
			gtk_check_menu_item_set_state
				(GTK_CHECK_MENU_ITEM(cur->data), FALSE);
			items[n] = GTK_CHECK_MENU_ITEM(cur->data);
			n++;
		}
	}

	if (n == (N_COLOR_LABELS + 1)) {
		/* iterate all messages and set the state of the appropriate
		 * items */
		for (; sel != NULL; sel = sel->next) {
			MsgInfo *msginfo;
			gint clabel;

			msginfo = gtk_ctree_node_get_row_data
				(GTK_CTREE(summaryview->ctree),
				 GTK_CTREE_NODE(sel->data));
			if (msginfo) {
				clabel = MSG_GET_COLORLABEL_VALUE(msginfo->flags);
				if (!items[clabel]->active)
					gtk_check_menu_item_set_state
						(items[clabel], TRUE);
			}
		}
	} else
		g_warning("invalid number of color elements (%d)\n", n);

	/* reset "dont_toggle" state */
	g_object_set_data(G_OBJECT(menu), "dont_toggle", GINT_TO_POINTER(0));
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
	 * index of label_colors[] as data parameter. for the None color we
	 * pass an invalid (high) value. also we attach a data pointer so we
	 * can always get back the SummaryView pointer. */

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

static GtkWidget *summary_ctree_create(SummaryView *summaryview)
{
	GtkWidget *ctree;
	gint *col_pos = summaryview->col_pos;
	SummaryColumnState *col_state;
	gchar *titles[N_SUMMARY_COLS];
	SummaryColumnType type;
	gint pos;

	memset(titles, 0, sizeof(titles));

	col_state = prefs_summary_column_get_config();
	for (pos = 0; pos < N_SUMMARY_COLS; pos++) {
		summaryview->col_state[pos] = col_state[pos];
		type = col_state[pos].type;
		col_pos[type] = pos;
	}
	col_state = summaryview->col_state;

	ctree = gtk_sctree_new_with_titles
		(N_SUMMARY_COLS, col_pos[S_COL_SUBJECT], titles);

	gtk_clist_set_selection_mode(GTK_CLIST(ctree), GTK_SELECTION_EXTENDED);
	gtk_clist_set_column_justification(GTK_CLIST(ctree), col_pos[S_COL_MARK],
					   GTK_JUSTIFY_CENTER);
	gtk_clist_set_column_justification(GTK_CLIST(ctree), col_pos[S_COL_UNREAD],
					   GTK_JUSTIFY_CENTER);
	gtk_clist_set_column_justification(GTK_CLIST(ctree), col_pos[S_COL_MIME],
					   GTK_JUSTIFY_CENTER);
	gtk_clist_set_column_justification(GTK_CLIST(ctree), col_pos[S_COL_SIZE],
					   GTK_JUSTIFY_RIGHT);
	gtk_clist_set_column_justification(GTK_CLIST(ctree), col_pos[S_COL_NUMBER],
					   GTK_JUSTIFY_RIGHT);
	gtk_clist_set_column_width(GTK_CLIST(ctree), col_pos[S_COL_MARK],
				   SUMMARY_COL_MARK_WIDTH);
	gtk_clist_set_column_width(GTK_CLIST(ctree), col_pos[S_COL_UNREAD],
				   SUMMARY_COL_UNREAD_WIDTH);
	gtk_clist_set_column_width(GTK_CLIST(ctree), col_pos[S_COL_MIME],
				   SUMMARY_COL_MIME_WIDTH);
	gtk_clist_set_column_width(GTK_CLIST(ctree), col_pos[S_COL_SUBJECT],
				   prefs_common.summary_col_size[S_COL_SUBJECT]);
	gtk_clist_set_column_width(GTK_CLIST(ctree), col_pos[S_COL_FROM],
				   prefs_common.summary_col_size[S_COL_FROM]);
	gtk_clist_set_column_width(GTK_CLIST(ctree), col_pos[S_COL_DATE],
				   prefs_common.summary_col_size[S_COL_DATE]);
	gtk_clist_set_column_width(GTK_CLIST(ctree), col_pos[S_COL_SIZE],
				   prefs_common.summary_col_size[S_COL_SIZE]);
	gtk_clist_set_column_width(GTK_CLIST(ctree), col_pos[S_COL_NUMBER],
				   prefs_common.summary_col_size[S_COL_NUMBER]);
	gtk_ctree_set_line_style(GTK_CTREE(ctree), GTK_CTREE_LINES_DOTTED);
	gtk_ctree_set_expander_style(GTK_CTREE(ctree),
				     GTK_CTREE_EXPANDER_SQUARE);
#if 0
	gtk_ctree_set_line_style(GTK_CTREE(ctree), GTK_CTREE_LINES_NONE);
	gtk_ctree_set_expander_style(GTK_CTREE(ctree),
				     GTK_CTREE_EXPANDER_TRIANGLE);
#endif
	gtk_ctree_set_indent(GTK_CTREE(ctree), 12);
	g_object_set_data(G_OBJECT(ctree), "user_data", summaryview);

	for (pos = 0; pos < N_SUMMARY_COLS; pos++) {
		GTK_WIDGET_UNSET_FLAGS(GTK_CLIST(ctree)->column[pos].button,
				       GTK_CAN_FOCUS);
		gtk_clist_set_column_visibility
			(GTK_CLIST(ctree), pos, col_state[pos].visible);
	}

	/* connect signal to the buttons for sorting */
#define CLIST_BUTTON_SIGNAL_CONNECT(col, func) \
	g_signal_connect \
		(G_OBJECT(GTK_CLIST(ctree)->column[col_pos[col]].button), \
		 "clicked", \
		 G_CALLBACK(func), \
		 summaryview)

	CLIST_BUTTON_SIGNAL_CONNECT(S_COL_MARK   , summary_mark_clicked);
	CLIST_BUTTON_SIGNAL_CONNECT(S_COL_UNREAD , summary_unread_clicked);
	CLIST_BUTTON_SIGNAL_CONNECT(S_COL_MIME   , summary_mime_clicked);
	CLIST_BUTTON_SIGNAL_CONNECT(S_COL_NUMBER , summary_num_clicked);
	CLIST_BUTTON_SIGNAL_CONNECT(S_COL_SIZE   , summary_size_clicked);
	CLIST_BUTTON_SIGNAL_CONNECT(S_COL_DATE   , summary_date_clicked);
	CLIST_BUTTON_SIGNAL_CONNECT(S_COL_FROM   , summary_from_clicked);
	CLIST_BUTTON_SIGNAL_CONNECT(S_COL_SUBJECT, summary_subject_clicked);

#undef CLIST_BUTTON_SIGNAL_CONNECT

	g_signal_connect(G_OBJECT(ctree), "tree_select_row",
			 G_CALLBACK(summary_selected), summaryview);
	g_signal_connect(G_OBJECT(ctree), "button_press_event",
			 G_CALLBACK(summary_button_pressed), summaryview);
	g_signal_connect(G_OBJECT(ctree), "button_release_event",
			 G_CALLBACK(summary_button_released), summaryview);
	g_signal_connect(G_OBJECT(ctree), "key_press_event",
			 G_CALLBACK(summary_key_pressed), summaryview);
	g_signal_connect(G_OBJECT(ctree), "resize_column",
			 G_CALLBACK(summary_col_resized), summaryview);
        g_signal_connect(G_OBJECT(ctree), "open_row",
			 G_CALLBACK(summary_open_row), summaryview);

	g_signal_connect_after(G_OBJECT(ctree), "tree_expand",
			       G_CALLBACK(summary_tree_expanded),
			       summaryview);
	g_signal_connect_after(G_OBJECT(ctree), "tree_collapse",
			       G_CALLBACK(summary_tree_collapsed),
			       summaryview);

	g_signal_connect(G_OBJECT(ctree), "start_drag",
			 G_CALLBACK(summary_start_drag), summaryview);
	g_signal_connect(G_OBJECT(ctree), "drag_data_get",
			 G_CALLBACK(summary_drag_data_get), summaryview);

	return ctree;
}

void summary_set_column_order(SummaryView *summaryview)
{
	GtkWidget *ctree;
	GtkWidget *scrolledwin = summaryview->scrolledwin;
	GtkWidget *pixmap;
	FolderItem *item;

	item = summaryview->folder_item;
	summary_clear_all(summaryview);
	gtk_widget_destroy(summaryview->ctree);

	summaryview->ctree = ctree = summary_ctree_create(summaryview);
	pixmap = gtk_pixmap_new(clipxpm, clipxpmmask);
	gtk_clist_set_column_widget(GTK_CLIST(ctree),
				    summaryview->col_pos[S_COL_MIME], pixmap);
	gtk_widget_show(pixmap);
	gtk_scrolled_window_set_hadjustment(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_CLIST(ctree)->hadjustment);
	gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_CLIST(ctree)->vadjustment);
	gtk_container_add(GTK_CONTAINER(scrolledwin), ctree);
	gtk_widget_show(ctree);

	summary_show(summaryview, item, FALSE);
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

static gboolean summary_button_pressed(GtkWidget *ctree, GdkEventButton *event,
				       SummaryView *summaryview)
{
	if (!event) return FALSE;

	if (event->button == 3) {
		/* right clicked */
		gtk_menu_popup(GTK_MENU(summaryview->popupmenu), NULL, NULL,
			       NULL, NULL, event->button, event->time);
	} else if (event->button == 2) {
		summaryview->display_msg = TRUE;
	} else if (event->button == 1) {
		if (!prefs_common.emulate_emacs &&
		    messageview_is_visible(summaryview->messageview))
			summaryview->display_msg = TRUE;
	}

	return FALSE;
}

static gboolean summary_button_released(GtkWidget *ctree, GdkEventButton *event,
					SummaryView *summaryview)
{
	return FALSE;
}

void summary_pass_key_press_event(SummaryView *summaryview, GdkEventKey *event)
{
	summary_key_pressed(summaryview->ctree, event, summaryview);
}

#define BREAK_ON_MODIFIER_KEY() \
	if ((event->state & (GDK_MOD1_MASK|GDK_CONTROL_MASK)) != 0) break

static gboolean summary_key_pressed(GtkWidget *widget, GdkEventKey *event,
				    SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(widget);
	GtkCTreeNode *node;
	MessageView *messageview;
	TextView *textview;
	GtkAdjustment *adj;
	gboolean mod_pressed;

	if (summary_is_locked(summaryview)) return FALSE;
	if (!event) return FALSE;

	switch (event->keyval) {
	case GDK_Left:		/* Move focus */
		adj = gtk_scrolled_window_get_hadjustment
			(GTK_SCROLLED_WINDOW(summaryview->scrolledwin));
		if (adj->lower != adj->value)
			break;
		/* FALLTHROUGH */
	case GDK_Escape:
		gtk_widget_grab_focus(summaryview->folderview->ctree);
		return FALSE;
	default:
		break;
	}

	if (!summaryview->selected) {
		node = gtk_ctree_node_nth(ctree, 0);
		if (node)
			gtk_sctree_select(GTK_SCTREE(ctree), node);
		else
			return FALSE;
	}

	messageview = summaryview->messageview;
	if (messageview->type == MVIEW_MIME &&
	    gtk_notebook_get_current_page
		(GTK_NOTEBOOK(messageview->mimeview->notebook)) == 1)
		textview = messageview->mimeview->textview;
	else
		textview = messageview->textview;

	mod_pressed =
		((event->state & (GDK_SHIFT_MASK|GDK_MOD1_MASK)) != 0);

	switch (event->keyval) {
	case GDK_space:		/* Page down or go to the next */
		if (summaryview->displayed != summaryview->selected) {
			summary_display_msg(summaryview,
					    summaryview->selected);
			break;
		}
		if (mod_pressed) {
			if (!textview_scroll_page(textview, TRUE))
				summary_select_prev_unread(summaryview);
		} else {
			if (!textview_scroll_page(textview, FALSE))
				summary_select_next_unread(summaryview);
		}
		break;
	case GDK_BackSpace:	/* Page up */
		textview_scroll_page(textview, TRUE);
		break;
	case GDK_Return:	/* Scroll up/down one line */
		if (summaryview->displayed != summaryview->selected) {
			summary_display_msg(summaryview,
					    summaryview->selected);
			break;
		}
		textview_scroll_one_line(textview, mod_pressed);
		break;
	case GDK_Delete:
		BREAK_ON_MODIFIER_KEY();
		summary_delete(summaryview);
		break;
	default:
		break;
	}

	return FALSE;
}

static void summary_open_row(GtkSCTree *sctree, SummaryView *summaryview)
{
	if (summaryview->folder_item->stype == F_OUTBOX ||
	    summaryview->folder_item->stype == F_DRAFT  ||
	    summaryview->folder_item->stype == F_QUEUE)
		summary_reedit(summaryview);
	else
		summary_open_msg(summaryview);

	summaryview->display_msg = FALSE;
}

static void summary_tree_expanded(GtkCTree *ctree, GtkCTreeNode *node,
				  SummaryView *summaryview)
{
	summary_set_row_marks(summaryview, node);
}

static void summary_tree_collapsed(GtkCTree *ctree, GtkCTreeNode *node,
				   SummaryView *summaryview)
{
	summary_set_row_marks(summaryview, node);
}

static void summary_selected(GtkCTree *ctree, GtkCTreeNode *row,
			     gint column, SummaryView *summaryview)
{
	MsgInfo *msginfo;

	summary_status_show(summaryview);

	if (GTK_CLIST(ctree)->selection &&
	    GTK_CLIST(ctree)->selection->next) {
		summaryview->display_msg = FALSE;
		summary_set_menu_sensitive(summaryview);
		main_window_set_toolbar_sensitive(summaryview->mainwin);
		return;
	}

	summaryview->selected = row;

	msginfo = gtk_ctree_node_get_row_data(ctree, row);
	g_return_if_fail(msginfo != NULL);

	switch (column < 0 ? column : summaryview->col_state[column].type) {
	case S_COL_MARK:
		if (!MSG_IS_DELETED(msginfo->flags) &&
		    !MSG_IS_MOVE(msginfo->flags) &&
		    !MSG_IS_COPY(msginfo->flags)) {
			if (MSG_IS_MARKED(msginfo->flags)) {
				summary_unmark_row(summaryview, row);
				if (MSG_IS_IMAP(msginfo->flags))
					imap_msg_unset_perm_flags(msginfo,
								  MSG_MARKED);
			} else {
				summary_mark_row(summaryview, row);
				if (MSG_IS_IMAP(msginfo->flags))
					imap_msg_set_perm_flags(msginfo,
								MSG_MARKED);
			}
		}
		break;
	case S_COL_UNREAD:
		if (MSG_IS_UNREAD(msginfo->flags)) {
			summary_mark_row_as_read(summaryview, row);
			if (MSG_IS_IMAP(msginfo->flags))
				imap_msg_unset_perm_flags
					(msginfo, MSG_NEW | MSG_UNREAD);
			summary_status_show(summaryview);
		} else if (!MSG_IS_REPLIED(msginfo->flags) &&
			 !MSG_IS_FORWARDED(msginfo->flags)) {
			summary_mark_row_as_unread(summaryview, row);
			if (MSG_IS_IMAP(msginfo->flags))
				imap_msg_set_perm_flags(msginfo, MSG_UNREAD);
			summary_status_show(summaryview);
		}
		break;
	default:
		break;
	}

	if (summaryview->display_msg ||
	    (prefs_common.always_show_msg &&
	     messageview_is_visible(summaryview->messageview))) {
		summaryview->display_msg = FALSE;
		if (summaryview->displayed != row) {
			summary_display_msg(summaryview, row);
			return;
		}
	}

	summary_set_menu_sensitive(summaryview);
	main_window_set_toolbar_sensitive(summaryview->mainwin);
}

static void summary_col_resized(GtkCList *clist, gint column, gint width,
				SummaryView *summaryview)
{
	SummaryColumnType type = summaryview->col_state[column].type;

	prefs_common.summary_col_size[type] = width;
}

static void summary_reply_cb(SummaryView *summaryview, guint action,
			     GtkWidget *widget)
{
	summary_reply(summaryview, (ComposeMode)action);
}

static void summary_show_all_header_cb(SummaryView *summaryview,
				       guint action, GtkWidget *widget)
{
	summary_display_msg_selected(summaryview,
				     GTK_CHECK_MENU_ITEM(widget)->active);
}

static void summary_add_address_cb(SummaryView *summaryview,
				   guint action, GtkWidget *widget)
{
	summary_add_address(summaryview);
}

static void summary_sort_by_column_click(SummaryView *summaryview,
					 FolderSortKey sort_key)
{
	FolderItem *item = summaryview->folder_item;

	if (!item) return;

	if (item->sort_key == sort_key)
		summary_sort(summaryview, sort_key,
			     item->sort_type == SORT_ASCENDING
			     ? SORT_DESCENDING : SORT_ASCENDING);
	else
		summary_sort(summaryview, sort_key, SORT_ASCENDING);
}

static void summary_mark_clicked(GtkWidget *button, SummaryView *summaryview)
{
	summary_sort_by_column_click(summaryview, SORT_BY_MARK);
}

static void summary_unread_clicked(GtkWidget *button, SummaryView *summaryview)
{
	summary_sort_by_column_click(summaryview, SORT_BY_UNREAD);
}

static void summary_mime_clicked(GtkWidget *button, SummaryView *summaryview)
{
	summary_sort_by_column_click(summaryview, SORT_BY_MIME);
}

static void summary_num_clicked(GtkWidget *button, SummaryView *summaryview)
{
	summary_sort_by_column_click(summaryview, SORT_BY_NUMBER);
}

static void summary_size_clicked(GtkWidget *button, SummaryView *summaryview)
{
	summary_sort_by_column_click(summaryview, SORT_BY_SIZE);
}

static void summary_date_clicked(GtkWidget *button, SummaryView *summaryview)
{
	summary_sort_by_column_click(summaryview, SORT_BY_DATE);
}

static void summary_from_clicked(GtkWidget *button, SummaryView *summaryview)
{
	summary_sort_by_column_click(summaryview, SORT_BY_FROM);
}

static void summary_subject_clicked(GtkWidget *button,
				    SummaryView *summaryview)
{
	summary_sort_by_column_click(summaryview, SORT_BY_SUBJECT);
}

static void summary_start_drag(GtkWidget *widget, gint button, GdkEvent *event,
			       SummaryView *summaryview)
{
	GtkTargetList *list;
	GdkDragContext *context;

	g_return_if_fail(summaryview != NULL);
	g_return_if_fail(summaryview->folder_item != NULL);
	g_return_if_fail(summaryview->folder_item->folder != NULL);
	if (summaryview->selected == NULL) return;

	list = gtk_target_list_new(summary_drag_types, 1);

	if (FOLDER_ITEM_CAN_ADD(summaryview->folder_item)) {
		context = gtk_drag_begin
			(widget, list,
			 GDK_ACTION_MOVE | GDK_ACTION_COPY, button, event);
	} else {
		context = gtk_drag_begin(widget, list, GDK_ACTION_COPY,
					 button, event);
	}
	gtk_drag_set_icon_default(context);
}

static void summary_drag_data_get(GtkWidget        *widget,
				  GdkDragContext   *drag_context,
				  GtkSelectionData *selection_data,
				  guint             info,
				  guint             time,
				  SummaryView      *summaryview)
{
	if (info == TARGET_MAIL_URI_LIST) {
		GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
		GList *cur;
		MsgInfo *msginfo;
		gchar *mail_list = NULL, *tmp1, *tmp2;

		for (cur = GTK_CLIST(ctree)->selection;
		     cur != NULL; cur = cur->next) {
			msginfo = gtk_ctree_node_get_row_data
				(ctree, GTK_CTREE_NODE(cur->data));
			tmp2 = procmsg_get_message_file(msginfo);
			if (!tmp2) continue;
			tmp1 = g_strconcat("file://", tmp2, NULL);
			g_free(tmp2);

			if (!mail_list) {
				mail_list = tmp1;
			} else {
				tmp2 = g_strconcat(mail_list, tmp1, NULL);
				g_free(mail_list);
				g_free(tmp1);
				mail_list = tmp2;
			}
		}

		if (mail_list != NULL) {
			gtk_selection_data_set(selection_data,
					       selection_data->target, 8,
					       mail_list, strlen(mail_list));
			g_free(mail_list);
		} 
	} else if (info == TARGET_DUMMY) {
		if (GTK_CLIST(summaryview->ctree)->selection)
			gtk_selection_data_set(selection_data,
					       selection_data->target, 8,
					       "Dummy", 6);
	}
}


/* custom compare functions for sorting */

#define CMP_FUNC_DEF(func_name, val)					 \
static gint func_name(GtkCList *clist,					 \
		      gconstpointer ptr1, gconstpointer ptr2)		 \
{									 \
	MsgInfo *msginfo1 = ((GtkCListRow *)ptr1)->data;		 \
	MsgInfo *msginfo2 = ((GtkCListRow *)ptr2)->data;		 \
									 \
	if (!msginfo1 || !msginfo2)					 \
		return -1;						 \
									 \
	return (val);							 \
}

CMP_FUNC_DEF(summary_cmp_by_mark,
	     MSG_IS_MARKED(msginfo1->flags) - MSG_IS_MARKED(msginfo2->flags))
CMP_FUNC_DEF(summary_cmp_by_unread,
	     MSG_IS_UNREAD(msginfo1->flags) - MSG_IS_UNREAD(msginfo2->flags))
CMP_FUNC_DEF(summary_cmp_by_mime,
	     MSG_IS_MIME(msginfo1->flags) - MSG_IS_MIME(msginfo2->flags))
CMP_FUNC_DEF(summary_cmp_by_label,
	     MSG_GET_COLORLABEL(msginfo1->flags) -
	     MSG_GET_COLORLABEL(msginfo2->flags))

CMP_FUNC_DEF(summary_cmp_by_num, msginfo1->msgnum - msginfo2->msgnum)
CMP_FUNC_DEF(summary_cmp_by_size, msginfo1->size - msginfo2->size)
CMP_FUNC_DEF(summary_cmp_by_date, msginfo1->date_t - msginfo2->date_t)

#undef CMP_FUNC_DEF
#define CMP_FUNC_DEF(func_name, var_name)				 \
static gint func_name(GtkCList *clist,					 \
		      gconstpointer ptr1, gconstpointer ptr2)		 \
{									 \
	MsgInfo *msginfo1 = ((GtkCListRow *)ptr1)->data;		 \
	MsgInfo *msginfo2 = ((GtkCListRow *)ptr2)->data;		 \
									 \
	if (!msginfo1->var_name)					 \
		return (msginfo2->var_name != NULL);			 \
	if (!msginfo2->var_name)					 \
		return -1;						 \
									 \
	return strcasecmp(msginfo1->var_name, msginfo2->var_name);	 \
}

CMP_FUNC_DEF(summary_cmp_by_from, fromname)
CMP_FUNC_DEF(summary_cmp_by_to, to);

#undef CMP_FUNC_DEF

static gint summary_cmp_by_subject(GtkCList *clist,			 \
				   gconstpointer ptr1,			 \
				   gconstpointer ptr2)			 \
{									 \
	MsgInfo *msginfo1 = ((GtkCListRow *)ptr1)->data;		 \
	MsgInfo *msginfo2 = ((GtkCListRow *)ptr2)->data;		 \
									 \
	if (!msginfo1->subject)						 \
		return (msginfo2->subject != NULL);			 \
	if (!msginfo2->subject)						 \
		return -1;						 \
									 \
	return subject_compare_for_sort					 \
		(msginfo1->subject, msginfo2->subject);			 \
}
