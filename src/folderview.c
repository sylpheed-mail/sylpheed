/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2004 Hiroyuki Yamamoto
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
#include <gtk/gtkwidget.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkctree.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtkclist.h>
#include <gtk/gtkstyle.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkstatusbar.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkitemfactory.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "main.h"
#include "mainwindow.h"
#include "folderview.h"
#include "summaryview.h"
#include "summary_search.h"
#include "inputdialog.h"
#include "grouplistdialog.h"
#include "manage_window.h"
#include "alertpanel.h"
#include "menu.h"
#include "stock_pixmap.h"
#include "statusbar.h"
#include "procmsg.h"
#include "utils.h"
#include "gtkutils.h"
#include "prefs_common.h"
#include "prefs_account.h"
#include "prefs_filter.h"
#include "prefs_folder_item.h"
#include "account.h"
#include "folder.h"
#include "inc.h"

typedef enum
{
	COL_FOLDER	= 0,
	COL_NEW		= 1,
	COL_UNREAD	= 2,
	COL_TOTAL	= 3
} FolderColumnPos;

#define N_FOLDER_COLS		4
#define COL_FOLDER_WIDTH	150
#define COL_NUM_WIDTH		32

#define STATUSBAR_PUSH(mainwin, str) \
{ \
	gtk_statusbar_push(GTK_STATUSBAR(mainwin->statusbar), \
			   mainwin->folderview_cid, str); \
	gtkut_widget_wait_for_draw(mainwin->statusbar); \
}

#define STATUSBAR_POP(mainwin) \
{ \
	gtk_statusbar_pop(GTK_STATUSBAR(mainwin->statusbar), \
			  mainwin->folderview_cid); \
}

static GList *folderview_list = NULL;

static GtkStyle *bold_style;
static GtkStyle *bold_color_style;

static GdkPixmap *inboxxpm;
static GdkBitmap *inboxxpmmask;
static GdkPixmap *outboxxpm;
static GdkBitmap *outboxxpmmask;
static GdkPixmap *folderxpm;
static GdkBitmap *folderxpmmask;
static GdkPixmap *folderopenxpm;
static GdkBitmap *folderopenxpmmask;
static GdkPixmap *foldernoselectxpm;
static GdkBitmap *foldernoselectxpmmask;
static GdkPixmap *trashxpm;
static GdkBitmap *trashxpmmask;

static void folderview_select_node	 (FolderView	*folderview,
					  GtkCTreeNode	*node);
static void folderview_set_folders	 (FolderView	*folderview);
static void folderview_sort_folders	 (FolderView	*folderview,
					  GtkCTreeNode	*root,
					  Folder	*folder);
static void folderview_append_folder	 (FolderView	*folderview,
					  Folder	*folder);
static void folderview_update_node	 (FolderView	*folderview,
					  GtkCTreeNode	*node);

static gint folderview_clist_compare	(GtkCList	*clist,
					 gconstpointer	 ptr1,
					 gconstpointer	 ptr2);

/* callback functions */
static gboolean folderview_button_pressed	(GtkWidget	*ctree,
						 GdkEventButton	*event,
						 FolderView	*folderview);
static gboolean folderview_button_released	(GtkWidget	*ctree,
						 GdkEventButton	*event,
						 FolderView	*folderview);

static gboolean folderview_key_pressed	(GtkWidget	*widget,
					 GdkEventKey	*event,
					 FolderView	*folderview);
static void folderview_selected		(GtkCTree	*ctree,
					 GtkCTreeNode	*row,
					 gint		 column,
					 FolderView	*folderview);
static void folderview_tree_expanded	(GtkCTree	*ctree,
					 GtkCTreeNode	*node,
					 FolderView	*folderview);
static void folderview_tree_collapsed	(GtkCTree	*ctree,
					 GtkCTreeNode	*node,
					 FolderView	*folderview);
static void folderview_popup_close	(GtkMenuShell	*menu_shell,
					 FolderView	*folderview);
static void folderview_col_resized	(GtkCList	*clist,
					 gint		 column,
					 gint		 width,
					 FolderView	*folderview);

static void folderview_download_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);

static void folderview_update_tree_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);

static void folderview_new_folder_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);
static void folderview_rename_folder_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);
static void folderview_delete_folder_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);
static void folderview_empty_trash_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);
static void folderview_remove_mailbox_cb(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);

static void folderview_rm_imap_server_cb (FolderView	*folderview,
					  guint		 action,
					  GtkWidget	*widget);

static void folderview_new_news_group_cb(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);
static void folderview_rm_news_group_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);
static void folderview_rm_news_server_cb(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);

static void folderview_search_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);

static void folderview_property_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);

static gboolean folderview_drag_motion_cb(GtkWidget      *widget,
					  GdkDragContext *context,
					  gint            x,
					  gint            y,
					  guint           time,
					  FolderView     *folderview);
static void folderview_drag_leave_cb     (GtkWidget        *widget,
					  GdkDragContext   *context,
					  guint             time,
					  FolderView       *folderview);
static void folderview_drag_received_cb  (GtkWidget        *widget,
					  GdkDragContext   *context,
					  gint              x,
					  gint              y,
					  GtkSelectionData *data,
					  guint             info,
					  guint             time,
					  FolderView       *folderview);

static GtkItemFactoryEntry folderview_mail_popup_entries[] =
{
	{N_("/Create _new folder..."),	NULL, folderview_new_folder_cb,    0, NULL},
	{N_("/_Rename folder..."),	NULL, folderview_rename_folder_cb, 0, NULL},
	{N_("/_Delete folder"),		NULL, folderview_delete_folder_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/Empty _trash"),		NULL, folderview_empty_trash_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Check for new messages"),
					NULL, folderview_update_tree_cb, 0, NULL},
	{N_("/R_ebuild folder tree"),	NULL, folderview_update_tree_cb, 1, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Search messages..."),	NULL, folderview_search_cb, 0, NULL},
	{N_("/_Properties..."),		NULL, folderview_property_cb, 0, NULL}
};

static GtkItemFactoryEntry folderview_imap_popup_entries[] =
{
	{N_("/Create _new folder..."),	NULL, folderview_new_folder_cb,    0, NULL},
	{N_("/_Rename folder..."),	NULL, folderview_rename_folder_cb, 0, NULL},
	{N_("/_Delete folder"),		NULL, folderview_delete_folder_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/Empty _trash"),		NULL, folderview_empty_trash_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/Down_load"),		NULL, folderview_download_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Check for new messages"),
					NULL, folderview_update_tree_cb, 0, NULL},
	{N_("/R_ebuild folder tree"),	NULL, folderview_update_tree_cb, 1, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Search messages..."),	NULL, folderview_search_cb, 0, NULL},
	{N_("/_Properties..."),		NULL, folderview_property_cb, 0, NULL}
};

static GtkItemFactoryEntry folderview_news_popup_entries[] =
{
	{N_("/Su_bscribe to newsgroup..."),
					NULL, folderview_new_news_group_cb, 0, NULL},
	{N_("/_Remove newsgroup"),	NULL, folderview_rm_news_group_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/Down_load"),		NULL, folderview_download_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Check for new messages"),
					NULL, folderview_update_tree_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Search messages..."),	NULL, folderview_search_cb, 0, NULL},
	{N_("/_Properties..."),		NULL, folderview_property_cb, 0, NULL}
};


FolderView *folderview_create(void)
{
	FolderView *folderview;
	GtkWidget *scrolledwin;
	GtkWidget *ctree;
	gchar *titles[N_FOLDER_COLS];
	GtkWidget *mail_popup;
	GtkWidget *news_popup;
	GtkWidget *imap_popup;
	GtkItemFactory *mail_factory;
	GtkItemFactory *news_factory;
	GtkItemFactory *imap_factory;
	gint n_entries;
	gint i;

	debug_print(_("Creating folder view...\n"));
	folderview = g_new0(FolderView, 1);

	titles[COL_FOLDER] = _("Folder");
	titles[COL_NEW]    = _("New");
	titles[COL_UNREAD] = _("Unread");
	titles[COL_TOTAL]  = _("#");

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy
		(GTK_SCROLLED_WINDOW(scrolledwin),
		 GTK_POLICY_AUTOMATIC,
		 prefs_common.folderview_vscrollbar_policy);
	gtk_widget_set_size_request(scrolledwin,
				    prefs_common.folderview_width,
				    prefs_common.folderview_height);

	ctree = gtk_ctree_new_with_titles(N_FOLDER_COLS, COL_FOLDER, titles);
	gtk_container_add(GTK_CONTAINER(scrolledwin), ctree);
	gtk_clist_set_selection_mode(GTK_CLIST(ctree), GTK_SELECTION_BROWSE);
	gtk_clist_set_column_justification(GTK_CLIST(ctree), COL_NEW,
					   GTK_JUSTIFY_RIGHT);
	gtk_clist_set_column_justification(GTK_CLIST(ctree), COL_UNREAD,
					   GTK_JUSTIFY_RIGHT);
	gtk_clist_set_column_justification(GTK_CLIST(ctree), COL_TOTAL,
					   GTK_JUSTIFY_RIGHT);
	gtk_clist_set_column_width(GTK_CLIST(ctree), COL_FOLDER,
				   prefs_common.folder_col_folder);
	gtk_clist_set_column_width(GTK_CLIST(ctree), COL_NEW,
				   prefs_common.folder_col_new);
	gtk_clist_set_column_width(GTK_CLIST(ctree), COL_UNREAD,	
				   prefs_common.folder_col_unread);
	gtk_clist_set_column_width(GTK_CLIST(ctree), COL_TOTAL,
				   prefs_common.folder_col_total);
	gtk_ctree_set_line_style(GTK_CTREE(ctree), GTK_CTREE_LINES_DOTTED);
	gtk_ctree_set_expander_style(GTK_CTREE(ctree),
				     GTK_CTREE_EXPANDER_SQUARE);
	gtk_ctree_set_indent(GTK_CTREE(ctree), CTREE_INDENT);
	gtk_clist_set_compare_func(GTK_CLIST(ctree), folderview_clist_compare);

	/* don't let title buttons take key focus */
	for (i = 0; i < N_FOLDER_COLS; i++)
		GTK_WIDGET_UNSET_FLAGS(GTK_CLIST(ctree)->column[i].button,
				       GTK_CAN_FOCUS);

	/* popup menu */
	n_entries = sizeof(folderview_mail_popup_entries) /
		sizeof(folderview_mail_popup_entries[0]);
	mail_popup = menu_create_items(folderview_mail_popup_entries,
				       n_entries,
				       "<MailFolder>", &mail_factory,
				       folderview);
	n_entries = sizeof(folderview_imap_popup_entries) /
		sizeof(folderview_imap_popup_entries[0]);
	imap_popup = menu_create_items(folderview_imap_popup_entries,
				       n_entries,
				       "<IMAPFolder>", &imap_factory,
				       folderview);
	n_entries = sizeof(folderview_news_popup_entries) /
		sizeof(folderview_news_popup_entries[0]);
	news_popup = menu_create_items(folderview_news_popup_entries,
				       n_entries,
				       "<NewsFolder>", &news_factory,
				       folderview);

	g_signal_connect(G_OBJECT(ctree), "key_press_event",
			 G_CALLBACK(folderview_key_pressed),
			 folderview);
	g_signal_connect(G_OBJECT(ctree), "button_press_event",
			 G_CALLBACK(folderview_button_pressed),
			 folderview);
	g_signal_connect(G_OBJECT(ctree), "button_release_event",
			 G_CALLBACK(folderview_button_released),
			 folderview);
	g_signal_connect(G_OBJECT(ctree), "tree_select_row",
			 G_CALLBACK(folderview_selected), folderview);

	g_signal_connect_after(G_OBJECT(ctree), "tree_expand",
			       G_CALLBACK(folderview_tree_expanded),
			       folderview);
	g_signal_connect_after(G_OBJECT(ctree), "tree_collapse",
			       G_CALLBACK(folderview_tree_collapsed),
			       folderview);

	g_signal_connect(G_OBJECT(ctree), "resize_column",
			 G_CALLBACK(folderview_col_resized), folderview);

	g_signal_connect(G_OBJECT(mail_popup), "selection_done",
			 G_CALLBACK(folderview_popup_close), folderview);
	g_signal_connect(G_OBJECT(imap_popup), "selection_done",
			 G_CALLBACK(folderview_popup_close), folderview);
	g_signal_connect(G_OBJECT(news_popup), "selection_done",
			 G_CALLBACK(folderview_popup_close), folderview);

        /* drop callback */
	gtk_drag_dest_set(ctree, GTK_DEST_DEFAULT_ALL &
			  ~GTK_DEST_DEFAULT_HIGHLIGHT,
			  summary_drag_types, 1,
			  GDK_ACTION_MOVE | GDK_ACTION_COPY);
	g_signal_connect(G_OBJECT(ctree), "drag_motion",
			 G_CALLBACK(folderview_drag_motion_cb), folderview);
	g_signal_connect(G_OBJECT(ctree), "drag_leave",
			 G_CALLBACK(folderview_drag_leave_cb), folderview);
	g_signal_connect(G_OBJECT(ctree), "drag_data_received",
			 G_CALLBACK(folderview_drag_received_cb), folderview);

	folderview->scrolledwin  = scrolledwin;
	folderview->ctree        = ctree;
	folderview->mail_popup   = mail_popup;
	folderview->mail_factory = mail_factory;
	folderview->imap_popup   = imap_popup;
	folderview->imap_factory = imap_factory;
	folderview->news_popup   = news_popup;
	folderview->news_factory = news_factory;

	gtk_widget_show_all(scrolledwin);

	folderview_list = g_list_append(folderview_list, folderview);

	return folderview;
}

void folderview_init(FolderView *folderview)
{
	GtkWidget *ctree = folderview->ctree;

	gtk_widget_realize(ctree);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_INBOX, &inboxxpm, &inboxxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_OUTBOX,
			 &outboxxpm, &outboxxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_DIR_CLOSE,
			 &folderxpm, &folderxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_DIR_OPEN,
			 &folderopenxpm, &folderopenxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_DIR_NOSELECT,
			 &foldernoselectxpm, &foldernoselectxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_TRASH, &trashxpm, &trashxpmmask);

	if (!bold_style) {
		bold_style = gtk_style_copy(gtk_widget_get_style(ctree));
		pango_font_description_set_weight
			(bold_style->font_desc, PANGO_WEIGHT_BOLD);
		bold_color_style = gtk_style_copy(bold_style);
		bold_color_style->fg[GTK_STATE_NORMAL] = folderview->color_new;
	}
}

void folderview_set(FolderView *folderview)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	MainWindow *mainwin = folderview->mainwin;

	debug_print(_("Setting folder info...\n"));
	STATUSBAR_PUSH(mainwin, _("Setting folder info..."));

	main_window_cursor_wait(mainwin);

	folderview->selected = NULL;
	folderview->opened = NULL;

	gtk_clist_freeze(GTK_CLIST(ctree));
	gtk_clist_clear(GTK_CLIST(ctree));
	gtk_clist_thaw(GTK_CLIST(ctree));
	gtk_clist_freeze(GTK_CLIST(ctree));

	folderview_set_folders(folderview);

	gtk_clist_thaw(GTK_CLIST(ctree));
	main_window_cursor_normal(mainwin);
	STATUSBAR_POP(mainwin);
}

void folderview_set_all(void)
{
	GList *list;

	for (list = folderview_list; list != NULL; list = list->next)
		folderview_set((FolderView *)list->data);
}

void folderview_select(FolderView *folderview, FolderItem *item)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	GtkCTreeNode *node;

	if (!item) return;

	node = gtk_ctree_find_by_row_data(ctree, NULL, item);
	if (node) folderview_select_node(folderview, node);
}

static void folderview_select_node(FolderView *folderview, GtkCTreeNode *node)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);

	g_return_if_fail(node != NULL);

	folderview->open_folder = TRUE;
	gtkut_ctree_set_focus_row(ctree, node);
	gtk_ctree_select(ctree, node);
	if (folderview->summaryview->folder_item &&
	    folderview->summaryview->folder_item->total > 0)
		gtk_widget_grab_focus(folderview->summaryview->ctree);
	else
		gtk_widget_grab_focus(folderview->ctree);

	gtkut_ctree_expand_parent_all(ctree, node);
}

void folderview_unselect(FolderView *folderview)
{
	if (folderview->opened && !GTK_CTREE_ROW(folderview->opened)->children)
		gtk_ctree_collapse
			(GTK_CTREE(folderview->ctree), folderview->opened);

	folderview->selected = folderview->opened = NULL;
}

static GtkCTreeNode *folderview_find_next_unread(GtkCTree *ctree,
						 GtkCTreeNode *node)
{
	FolderItem *item;

	if (node)
		node = gtkut_ctree_node_next(ctree, node);
	else
		node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);

	for (; node != NULL; node = gtkut_ctree_node_next(ctree, node)) {
		item = gtk_ctree_node_get_row_data(ctree, node);
		if (item && item->unread > 0 && item->stype != F_TRASH)
			return node;
	}

	return NULL;
}

void folderview_select_next_unread(FolderView *folderview)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	GtkCTreeNode *node = NULL;

	if ((node = folderview_find_next_unread(ctree, folderview->opened))
	    != NULL) {
		folderview_select_node(folderview, node);
		return;
	}

	if (!folderview->opened ||
	    folderview->opened == GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list))
		return;
	/* search again from the first node */
	if ((node = folderview_find_next_unread(ctree, NULL)) != NULL)
		folderview_select_node(folderview, node);
}

FolderItem *folderview_get_selected_item(FolderView *folderview)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);

	if (!folderview->selected) return NULL;
	return gtk_ctree_node_get_row_data(ctree, folderview->selected);
}

void folderview_update_msg_num(FolderView *folderview, GtkCTreeNode *row)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	static GtkCTreeNode *prev_row = NULL;
	FolderItem *item;
	gint new, unread, total;
	gchar *new_str, *unread_str, *total_str;

	if (!row) return;

	item = gtk_ctree_node_get_row_data(ctree, row);
	if (!item) return;

	gtk_ctree_node_get_text(ctree, row, COL_NEW, &new_str);
	gtk_ctree_node_get_text(ctree, row, COL_UNREAD, &unread_str);
	gtk_ctree_node_get_text(ctree, row, COL_TOTAL, &total_str);
	new = atoi(new_str);
	unread = atoi(unread_str);
	total = atoi(total_str);

	if (prev_row     == row    &&
	    item->new    == new    &&
	    item->unread == unread &&
	    item->total  == total)
		return;

	prev_row = row;

	folderview_update_node(folderview, row);
}

void folderview_append_item(FolderItem *item)
{
	FolderItem *parent;
	GList *list;

	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(item->parent != NULL);

	parent = item->parent;

	for (list = folderview_list; list != NULL; list = list->next) {
		FolderView *folderview = (FolderView *)list->data;
		GtkCTree *ctree = GTK_CTREE(folderview->ctree);
		GtkCTreeNode *node, *child;

		node = gtk_ctree_find_by_row_data(ctree, NULL, parent);
		if (node) {
			child = gtk_ctree_find_by_row_data(ctree, node, item);
			if (!child) {
				gchar *text[N_FOLDER_COLS] =
					{NULL, "0", "0", "0"};

				gtk_clist_freeze(GTK_CLIST(ctree));

				text[COL_FOLDER] = item->name;
				child = gtk_ctree_insert_node
					(ctree, node, NULL, text,
					 FOLDER_SPACING,
					 folderxpm, folderxpmmask,
					 folderopenxpm, folderopenxpmmask,
					 FALSE, FALSE);
				gtk_ctree_node_set_row_data(ctree, child, item);
				gtk_ctree_expand(ctree, node);
				folderview_update_node(folderview, child);
				folderview_sort_folders(folderview, node,
							item->folder);

				gtk_clist_thaw(GTK_CLIST(ctree));
			}
		}
	}
}

static void folderview_set_folders(FolderView *folderview)
{
	GList *list;

	list = folder_get_list();

	for (; list != NULL; list = list->next)
		folderview_append_folder(folderview, FOLDER(list->data));
}

static void folderview_scan_tree_func(Folder *folder, FolderItem *item,
				      gpointer data)
{
	GList *list;
	gchar *rootpath;

	if (FOLDER_IS_LOCAL(folder))
		rootpath = LOCAL_FOLDER(folder)->rootpath;
	else if (FOLDER_TYPE(folder) == F_IMAP && folder->account &&
		 folder->account->recv_server)
		rootpath = folder->account->recv_server;
	else if (FOLDER_TYPE(folder) == F_NEWS && folder->account &&
		 folder->account->nntp_server)
		rootpath = folder->account->nntp_server;
	else
		return;

	for (list = folderview_list; list != NULL; list = list->next) {
		FolderView *folderview = (FolderView *)list->data;
		MainWindow *mainwin = folderview->mainwin;
		gchar *str;

		if (item->path)
			str = g_strdup_printf(_("Scanning folder %s%c%s ..."),
					      rootpath, G_DIR_SEPARATOR,
					      item->path);
		else
			str = g_strdup_printf(_("Scanning folder %s ..."),
					      rootpath);

		STATUSBAR_PUSH(mainwin, str);
		STATUSBAR_POP(mainwin);
		g_free(str);
	}
}

static GtkWidget *label_window_create(const gchar *str)
{
	GtkWidget *window;
	GtkWidget *label;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_size_request(window, 380, 60);
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_title(GTK_WINDOW(window), str);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, FALSE, FALSE);
	manage_window_set_transient(GTK_WINDOW(window));

	label = gtk_label_new(str);
	gtk_container_add(GTK_CONTAINER(window), label);
	gtk_widget_show(label);

	gtk_widget_show_now(window);

	return window;
}

static void folderview_rescan_tree(FolderView *folderview, Folder *folder)
{
	GtkWidget *window;
	AlertValue avalue;

	g_return_if_fail(folder != NULL);

	if (!folder->klass->scan_tree) return;

	avalue = alertpanel
		(_("Rebuild folder tree"),
		 _("The folder tree will be rebuilt. Continue?"),
		 GTK_STOCK_YES, GTK_STOCK_NO, NULL);
	if (avalue != G_ALERTDEFAULT) return;

	if (!FOLDER_IS_LOCAL(folder) &&
	    !main_window_toggle_online_if_offline(folderview->mainwin))
		return;

	inc_lock();
	window = label_window_create(_("Rebuilding folder tree..."));

	summary_show(folderview->summaryview, NULL, FALSE);

	folder_set_ui_func(folder, folderview_scan_tree_func, NULL);
	if (folder->klass->scan_tree(folder) < 0)
		alertpanel_error(_("Rebuilding of the folder tree failed."));
	folder_set_ui_func(folder, NULL, NULL);

	folder_write_list();
	folderview_set_all();
	statusbar_pop_all();

	gtk_widget_destroy(window);
	inc_unlock();
}

#if 0
void folderview_rescan_all(void)
{
	GList *list;
	GtkWidget *window;

	inc_lock();
	window = label_window_create(_("Rebuilding all folder trees..."));

	list = folder_get_list();
	for (; list != NULL; list = list->next) {
		Folder *folder = list->data;

		if (!folder->klass->scan_tree) continue;
		folder_set_ui_func(folder, folderview_scan_tree_func, NULL);
		folder->klass->scan_tree(folder);
		folder_set_ui_func(folder, NULL, NULL);
	}

	folder_write_list();
	folderview_set_all();
	gtk_widget_destroy(window);
	inc_unlock();
}
#endif

void folderview_check_new(Folder *folder)
{
	GList *list;
	FolderItem *item;
	FolderView *folderview;
	GtkCTree *ctree;
	GtkCTreeNode *node;

	for (list = folderview_list; list != NULL; list = list->next) {
		folderview = (FolderView *)list->data;
		ctree = GTK_CTREE(folderview->ctree);

		if (folder && !FOLDER_IS_LOCAL(folder)) {
			if (!main_window_toggle_online_if_offline
				(folderview->mainwin))
				return;
		}

		inc_lock();
		main_window_lock(folderview->mainwin);
		gtk_widget_set_sensitive(folderview->ctree, FALSE);

		for (node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);
		     node != NULL; node = gtkut_ctree_node_next(ctree, node)) {
			item = gtk_ctree_node_get_row_data(ctree, node);
			if (!item || !item->path || !item->folder) continue;
			if (item->no_select) continue;
			if (folder && folder != item->folder) continue;
			if (!folder && !FOLDER_IS_LOCAL(item->folder)) continue;

			folderview_scan_tree_func(item->folder, item, NULL);
			if (folder_item_scan(item) < 0) {
				if (folder && !FOLDER_IS_LOCAL(folder))
					break;
			}
			folderview_update_node(folderview, node);
		}

		gtk_widget_set_sensitive(folderview->ctree, TRUE);
		main_window_unlock(folderview->mainwin);
		inc_unlock();
		statusbar_pop_all();
	}

	folder_write_list();
}

void folderview_check_new_all(void)
{
	GList *list;
	GtkWidget *window;
	FolderView *folderview;

	folderview = (FolderView *)folderview_list->data;

	inc_lock();
	main_window_lock(folderview->mainwin);
	window = label_window_create
		(_("Checking for new messages in all folders..."));

	list = folder_get_list();
	for (; list != NULL; list = list->next) {
		Folder *folder = list->data;

		folderview_check_new(folder);
	}

	gtk_widget_destroy(window);
	main_window_unlock(folderview->mainwin);
	inc_unlock();
}

static gboolean folderview_search_new_recursive(GtkCTree *ctree,
						GtkCTreeNode *node)
{
	FolderItem *item;

	if (node) {
		item = gtk_ctree_node_get_row_data(ctree, node);
		if (item) {
			if (item->new > 0 ||
			    (item->stype == F_QUEUE && item->total > 0))
				return TRUE;
		}
		node = GTK_CTREE_ROW(node)->children;
	} else
		node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);

	while (node) {
		if (folderview_search_new_recursive(ctree, node) == TRUE)
			return TRUE;
		node = GTK_CTREE_ROW(node)->sibling;
	}

	return FALSE;
}

static gboolean folderview_have_new_children(FolderView *folderview,
					     GtkCTreeNode *node)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);

	if (!node)
		node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);
	if (!node)
		return FALSE;

	node = GTK_CTREE_ROW(node)->children;

	while (node) {
		if (folderview_search_new_recursive(ctree, node) == TRUE)
			return TRUE;
		node = GTK_CTREE_ROW(node)->sibling;
	}

	return FALSE;
}

static gboolean folderview_search_unread_recursive(GtkCTree *ctree,
						   GtkCTreeNode *node)
{
	FolderItem *item;

	if (node) {
		item = gtk_ctree_node_get_row_data(ctree, node);
		if (item) {
			if (item->unread > 0 ||
			    (item->stype == F_QUEUE && item->total > 0))
				return TRUE;
		}
		node = GTK_CTREE_ROW(node)->children;
	} else
		node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);

	while (node) {
		if (folderview_search_unread_recursive(ctree, node) == TRUE)
			return TRUE;
		node = GTK_CTREE_ROW(node)->sibling;
	}

	return FALSE;
}

static gboolean folderview_have_unread_children(FolderView *folderview,
						GtkCTreeNode *node)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);

	if (!node)
		node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);
	if (!node)
		return FALSE;

	node = GTK_CTREE_ROW(node)->children;

	while (node) {
		if (folderview_search_unread_recursive(ctree, node) == TRUE)
			return TRUE;
		node = GTK_CTREE_ROW(node)->sibling;
	}

	return FALSE;
}

static void folderview_update_node(FolderView *folderview, GtkCTreeNode *node)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	GtkStyle *style = NULL;
	FolderItem *item;
	GdkPixmap *xpm, *openxpm;
	GdkBitmap *mask, *openmask;
	gchar *name;
	gchar *str;
	gboolean add_unread_mark;
	gboolean use_bold, use_color;

	item = gtk_ctree_node_get_row_data(ctree, node);
	g_return_if_fail(item != NULL);

	switch (item->stype) {
	case F_INBOX:
		xpm = openxpm = inboxxpm;
		mask = openmask = inboxxpmmask;
		name = g_strdup(FOLDER_IS_LOCAL(item->folder) &&
				!strcmp2(item->name, INBOX_DIR) ? _("Inbox") :
				item->name);
		break;
	case F_OUTBOX:
		xpm = openxpm = outboxxpm;
		mask = openmask = outboxxpmmask;
		name = g_strdup(FOLDER_IS_LOCAL(item->folder) &&
				!strcmp2(item->name, OUTBOX_DIR) ? _("Sent") :
				item->name);
		break;
	case F_QUEUE:
		xpm = openxpm = outboxxpm;
		mask = openmask = outboxxpmmask;
		name = g_strdup(FOLDER_IS_LOCAL(item->folder) &&
				!strcmp2(item->name, QUEUE_DIR) ? _("Queue") :
				item->name);
		break;
	case F_TRASH:
		xpm = openxpm = trashxpm;
		mask = openmask = trashxpmmask;
		name = g_strdup(FOLDER_IS_LOCAL(item->folder) &&
				!strcmp2(item->name, TRASH_DIR) ? _("Trash") :
				item->name);
		break;
	case F_DRAFT:
		xpm = folderxpm;
		mask = folderxpmmask;
		openxpm = folderopenxpm;
		openmask = folderopenxpmmask;
		name = g_strdup(FOLDER_IS_LOCAL(item->folder) &&
				!strcmp2(item->name, DRAFT_DIR) ? _("Drafts") :
				item->name);
		break;
	default:
		if (item->no_select) {
			xpm = openxpm = foldernoselectxpm;
			mask = openmask = foldernoselectxpmmask;
		} else {
			xpm = folderxpm;
			mask = folderxpmmask;
			openxpm = folderopenxpm;
			openmask = folderopenxpmmask;
		}

		if (!item->parent) {
			switch (FOLDER_TYPE(item->folder)) {
			case F_MH:
				name = " (MH)"; break;
			case F_IMAP:
				name = " (IMAP4)"; break;
			case F_NEWS:
				name = " (News)"; break;
			default:
				name = "";
			}
			name = g_strconcat(item->name, name, NULL);
		} else {
			if (FOLDER_TYPE(item->folder) == F_NEWS &&
			    item->path &&
			    !strcmp2(item->name, item->path))
				name = get_abbrev_newsgroup_name
					(item->path,
					 prefs_common.ng_abbrev_len);
			else
				name = g_strdup(item->name);
		}
	}

	if (!GTK_CTREE_ROW(node)->expanded &&
	    folderview_have_unread_children(folderview, node))
		add_unread_mark = TRUE;
	else
		add_unread_mark = FALSE;

	if (item->stype == F_QUEUE && item->total > 0 &&
	    prefs_common.display_folder_unread) {
		str = g_strdup_printf("%s (%d%s)", name, item->total,
				      add_unread_mark ? "+" : "");
		gtk_ctree_set_node_info(ctree, node, str, FOLDER_SPACING,
					xpm, mask, openxpm, openmask,
					FALSE, GTK_CTREE_ROW(node)->expanded);
		g_free(str);
	} else if ((item->unread > 0 || add_unread_mark) &&
		 prefs_common.display_folder_unread) {

		if (item->unread > 0)
			str = g_strdup_printf("%s (%d%s)", name, item->unread,
					      add_unread_mark ? "+" : "");
		else
			str = g_strdup_printf("%s (+)", name);
		gtk_ctree_set_node_info(ctree, node, str, FOLDER_SPACING,
					xpm, mask, openxpm, openmask,
					FALSE, GTK_CTREE_ROW(node)->expanded);
		g_free(str);
	} else
		gtk_ctree_set_node_info(ctree, node, name, FOLDER_SPACING,
					xpm, mask, openxpm, openmask,
					FALSE, GTK_CTREE_ROW(node)->expanded);
	g_free(name);

	if (!item->parent) {
		gtk_ctree_node_set_text(ctree, node, COL_NEW,    "-");
		gtk_ctree_node_set_text(ctree, node, COL_UNREAD, "-");
		gtk_ctree_node_set_text(ctree, node, COL_TOTAL,  "-");
	} else {
		gtk_ctree_node_set_text(ctree, node, COL_NEW,    itos(item->new));
		gtk_ctree_node_set_text(ctree, node, COL_UNREAD, itos(item->unread));
		gtk_ctree_node_set_text(ctree, node, COL_TOTAL,  itos(item->total));
	}

	if (item->stype == F_OUTBOX || item->stype == F_DRAFT ||
	    item->stype == F_TRASH) {
		use_bold = use_color = FALSE;
	} else if (item->stype == F_QUEUE) {
		/* highlight queue folder if there are any messages */
		use_bold = use_color = (item->total > 0);
	} else {
		/* if unread messages exist, print with bold font */
		use_bold = (item->unread > 0) || add_unread_mark;
		/* if new messages exist, print with colored letter */
		use_color =
			(item->new > 0) ||
			(add_unread_mark &&
			 folderview_have_new_children(folderview, node));
	}

	gtk_ctree_node_set_foreground(ctree, node, NULL);

	if (item->no_select)
		gtk_ctree_node_set_foreground(ctree, node,
					      &folderview->color_noselect);
	else if (use_bold && use_color)
		style = bold_color_style;
	else if (use_bold)
		style = bold_style;
	else if (use_color)
		gtk_ctree_node_set_foreground(ctree, node,
					      &folderview->color_new);

	gtk_ctree_node_set_row_style(ctree, node, style);

	item->updated = FALSE;

	if ((node = gtkut_ctree_find_collapsed_parent(ctree, node)) != NULL)
		folderview_update_node(folderview, node);
}

void folderview_update_item(FolderItem *item, gboolean update_summary)
{
	GList *list;
	FolderView *folderview;
	GtkCTree *ctree;
	GtkCTreeNode *node;

	g_return_if_fail(item != NULL);

	for (list = folderview_list; list != NULL; list = list->next) {
		folderview = (FolderView *)list->data;
		ctree = GTK_CTREE(folderview->ctree);

		node = gtk_ctree_find_by_row_data(ctree, NULL, item);
		if (node) {
			folderview_update_node(folderview, node);
			if (update_summary && folderview->opened == node)
				summary_show(folderview->summaryview,
					     item, FALSE);
		}
	}
}

static void folderview_update_item_foreach_func(gpointer key, gpointer val,
						gpointer data)
{
	folderview_update_item((FolderItem *)key, GPOINTER_TO_INT(data));
}

void folderview_update_item_foreach(GHashTable *table, gboolean update_summary)
{
	g_hash_table_foreach(table, folderview_update_item_foreach_func,
			     GINT_TO_POINTER(update_summary));
}

static gboolean folderview_update_all_updated_func(GNode *node, gpointer data)
{
	FolderItem *item;

	item = FOLDER_ITEM(node->data);
	if (item->updated) {
		debug_print("folderview_update_all_updated(): '%s' is updated\n", item->path);
		folderview_update_item(item, GPOINTER_TO_INT(data));
	}

	return FALSE;
}

void folderview_update_all_updated(gboolean update_summary)
{
	GList *list;
	Folder *folder;

	for (list = folder_get_list(); list != NULL; list = list->next) {
		folder = (Folder *)list->data;
		g_node_traverse(folder->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
				folderview_update_all_updated_func,
				GINT_TO_POINTER(update_summary));
	}
}

static gboolean folderview_gnode_func(GtkCTree *ctree, guint depth,
				      GNode *gnode, GtkCTreeNode *cnode,
				      gpointer data)
{
	FolderView *folderview = (FolderView *)data;
	FolderItem *item = FOLDER_ITEM(gnode->data);

	g_return_val_if_fail(item != NULL, FALSE);

	gtk_ctree_node_set_row_data(ctree, cnode, item);
	folderview_update_node(folderview, cnode);

	return TRUE;
}

static void folderview_expand_func(GtkCTree *ctree, GtkCTreeNode *node,
				   gpointer data)
{
	FolderView *folderview = (FolderView *)data;
	FolderItem *item;

	if (GTK_CTREE_ROW(node)->children) {
		item = gtk_ctree_node_get_row_data(ctree, node);
		g_return_if_fail(item != NULL);

		if (!item->collapsed)
			gtk_ctree_expand(ctree, node);
		else
			folderview_update_node(folderview, node);
	}
}

#define SET_SPECIAL_FOLDER(ctree, item) \
{ \
	if (item) { \
		GtkCTreeNode *node, *parent, *sibling; \
 \
		node = gtk_ctree_find_by_row_data(ctree, root, item); \
		if (!node) \
			g_warning("%s not found.\n", item->path); \
		else { \
			parent = GTK_CTREE_ROW(node)->parent; \
			if (prev && parent == GTK_CTREE_ROW(prev)->parent) \
				sibling = GTK_CTREE_ROW(prev)->sibling; \
			else \
				sibling = GTK_CTREE_ROW(parent)->children; \
			while (sibling) { \
				FolderItem *tmp; \
 \
				tmp = gtk_ctree_node_get_row_data \
					(ctree, sibling); \
				if (tmp->stype != F_NORMAL) \
					sibling = GTK_CTREE_ROW(sibling)->sibling; \
				else \
					break; \
			} \
			if (node != sibling) \
				gtk_ctree_move(ctree, node, parent, sibling); \
		} \
 \
		prev = node; \
	} \
}

static void folderview_sort_folders(FolderView *folderview, GtkCTreeNode *root,
				    Folder *folder)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	GtkCTreeNode *prev = NULL;

	gtk_ctree_sort_recursive(ctree, root);

	if (GTK_CTREE_ROW(root)->parent) return;

	SET_SPECIAL_FOLDER(ctree, folder->inbox);
	SET_SPECIAL_FOLDER(ctree, folder->outbox);
	SET_SPECIAL_FOLDER(ctree, folder->draft);
	SET_SPECIAL_FOLDER(ctree, folder->queue);
	SET_SPECIAL_FOLDER(ctree, folder->trash);
}

static void folderview_append_folder(FolderView *folderview, Folder *folder)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	GtkCTreeNode *root;

	g_return_if_fail(folder != NULL);

	root = gtk_ctree_insert_gnode(ctree, NULL, NULL, folder->node,
				      folderview_gnode_func, folderview);
	gtk_ctree_pre_recursive(ctree, root, folderview_expand_func,
				folderview);
	folderview_sort_folders(folderview, root, folder);
}

void folderview_new_folder(FolderView *folderview)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	switch (FOLDER_TYPE(item->folder)) {
	case F_MH:
	case F_MBOX:
	case F_MAILDIR:
	case F_IMAP:
		folderview_new_folder_cb(folderview, 0, NULL);
		break;
	case F_NEWS:
		folderview_new_news_group_cb(folderview, 0, NULL);
		break;
	default:
		break;
	}
}

void folderview_rename_folder(FolderView *folderview)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	if (!item->path) return;
	if (item->stype != F_NORMAL) return;

	switch (FOLDER_TYPE(item->folder)) {
	case F_MH:
	case F_MBOX:
	case F_MAILDIR:
	case F_IMAP:
		folderview_rename_folder_cb(folderview, 0, NULL);
		break;
	case F_NEWS:
	default:
		break;
	}
}

void folderview_delete_folder(FolderView *folderview)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	if (!item->path) return;
	if (item->stype != F_NORMAL) return;

	switch (FOLDER_TYPE(item->folder)) {
	case F_MH:
	case F_MBOX:
	case F_MAILDIR:
	case F_IMAP:
		folderview_delete_folder_cb(folderview, 0, NULL);
		break;
	case F_NEWS:
		folderview_rm_news_group_cb(folderview, 0, NULL);
		break;
	default:
		break;
	}
}

void folderview_check_new_selected(FolderView *folderview)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	if (item->parent != NULL) return;

	folderview_check_new(item->folder);
}

void folderview_remove_mailbox(FolderView *folderview)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	if (item->parent != NULL) return;

	switch (FOLDER_TYPE(item->folder)) {
	case F_MH:
	case F_MBOX:
	case F_MAILDIR:
		folderview_remove_mailbox_cb(folderview, 0, NULL);
		break;
	case F_IMAP:
		folderview_rm_imap_server_cb(folderview, 0, NULL);
		break;
	case F_NEWS:
		folderview_rm_news_server_cb(folderview, 0, NULL);
		break;
	default:
		break;
	}
}

void folderview_rebuild_tree(FolderView *folderview)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	if (item->parent != NULL) return;

	folderview_rescan_tree(folderview, item->folder);
}


/* callback functions */

static gboolean folderview_button_pressed(GtkWidget *ctree,
					  GdkEventButton *event,
					  FolderView *folderview)
{
	GtkCList *clist = GTK_CLIST(ctree);
	gint prev_row = -1, row = -1, column = -1;
	FolderItem *item;
	Folder *folder;
	GtkWidget *popup;
	gboolean new_folder      = FALSE;
	gboolean rename_folder   = FALSE;
	gboolean delete_folder   = FALSE;
	gboolean empty_trash     = FALSE;
	gboolean download_msg    = FALSE;
	gboolean update_tree     = FALSE;
	gboolean rescan_tree     = FALSE;
	gboolean remove_tree     = FALSE;
	gboolean search_folder   = FALSE;
	gboolean folder_property = FALSE;

	if (!event) return FALSE;

	if (event->button == 1) {
		folderview->open_folder = TRUE;
		return FALSE;
	}

	if (event->button == 2 || event->button == 3) {
		/* right clicked */
		if (clist->selection) {
			GtkCTreeNode *node;

			node = GTK_CTREE_NODE(clist->selection->data);
			if (node)
				prev_row = gtkut_ctree_get_nth_from_node
					(GTK_CTREE(ctree), node);
		}

		if (!gtk_clist_get_selection_info(clist, event->x, event->y,
						  &row, &column))
			return FALSE;
		if (prev_row != row) {
			gtk_clist_unselect_all(clist);
			if (event->button == 2)
				folderview_select_node
					(folderview,
					 gtk_ctree_node_nth(GTK_CTREE(ctree),
					 		    row));
			else
				gtk_clist_select_row(clist, row, column);
		}
	}

	if (event->button != 3) return FALSE;

	item = gtk_clist_get_row_data(clist, row);
	g_return_val_if_fail(item != NULL, FALSE);
	g_return_val_if_fail(item->folder != NULL, FALSE);
	folder = item->folder;

	if (folderview->mainwin->lock_count == 0) {
		new_folder = TRUE;
		if (item->parent == NULL) {
			update_tree = remove_tree = TRUE;
			if (folder->account)
				folder_property = TRUE;
		} else {
			folder_property = TRUE;
			if (folderview->selected == folderview->opened)
				search_folder = TRUE;
		}
		if (FOLDER_IS_LOCAL(folder) || FOLDER_TYPE(folder) == F_IMAP) {
			if (item->parent == NULL)
				update_tree = rescan_tree = TRUE;
			else if (item->stype == F_NORMAL)
				rename_folder = delete_folder = TRUE;
			else if (item->stype == F_TRASH)
				empty_trash = TRUE;
		} else if (FOLDER_TYPE(folder) == F_NEWS) {
			if (item->parent != NULL)
				delete_folder = TRUE;
		}
		if (FOLDER_TYPE(folder) == F_IMAP ||
		    FOLDER_TYPE(folder) == F_NEWS) {
			if (item->parent != NULL && item->no_select == FALSE)
				download_msg = TRUE;
		}
	}

#define SET_SENS(factory, name, sens) \
	menu_set_sensitive(folderview->factory, name, sens)

	if (FOLDER_IS_LOCAL(folder)) {
		popup = folderview->mail_popup;
		menu_set_insensitive_all(GTK_MENU_SHELL(popup));
		SET_SENS(mail_factory, "/Create new folder...", new_folder);
		SET_SENS(mail_factory, "/Rename folder...", rename_folder);
		SET_SENS(mail_factory, "/Delete folder", delete_folder);
		SET_SENS(mail_factory, "/Empty trash", empty_trash);
		SET_SENS(mail_factory, "/Check for new messages", update_tree);
		SET_SENS(mail_factory, "/Rebuild folder tree", rescan_tree);
		SET_SENS(mail_factory, "/Search messages...", search_folder);
		SET_SENS(mail_factory, "/Properties...", folder_property);
	} else if (FOLDER_TYPE(folder) == F_IMAP) {
		popup = folderview->imap_popup;
		menu_set_insensitive_all(GTK_MENU_SHELL(popup));
		SET_SENS(imap_factory, "/Create new folder...", new_folder);
		SET_SENS(imap_factory, "/Rename folder...", rename_folder);
		SET_SENS(imap_factory, "/Delete folder", delete_folder);
		SET_SENS(imap_factory, "/Empty trash", empty_trash);
		SET_SENS(imap_factory, "/Download", download_msg);
		SET_SENS(imap_factory, "/Check for new messages", update_tree);
		SET_SENS(imap_factory, "/Rebuild folder tree", rescan_tree);
		SET_SENS(imap_factory, "/Search messages...", search_folder);
		SET_SENS(imap_factory, "/Properties...", folder_property);
	} else if (FOLDER_TYPE(folder) == F_NEWS) {
		popup = folderview->news_popup;
		menu_set_insensitive_all(GTK_MENU_SHELL(popup));
		SET_SENS(news_factory, "/Subscribe to newsgroup...", new_folder);
		SET_SENS(news_factory, "/Remove newsgroup", delete_folder);
		SET_SENS(news_factory, "/Download", download_msg);
		SET_SENS(news_factory, "/Check for new messages", update_tree);
		SET_SENS(news_factory, "/Search messages...", search_folder);
		SET_SENS(news_factory, "/Properties...", folder_property);
	} else
		return FALSE;

#undef SET_SENS

	gtk_menu_popup(GTK_MENU(popup), NULL, NULL, NULL, NULL,
		       event->button, event->time);

	return FALSE;
}

static gboolean folderview_button_released(GtkWidget *ctree,
					   GdkEventButton *event,
					   FolderView *folderview)
{
	if (!event) return FALSE;

	if (event->button == 1 && folderview->open_folder == FALSE &&
	    folderview->opened != NULL) {
		gtkut_ctree_set_focus_row(GTK_CTREE(ctree),
					  folderview->opened);
		gtk_ctree_select(GTK_CTREE(ctree), folderview->opened);
	}

	return FALSE;
}

static gboolean folderview_key_pressed(GtkWidget *widget, GdkEventKey *event,
				       FolderView *folderview)
{
	if (!event) return FALSE;

	switch (event->keyval) {
	case GDK_Return:
		if (folderview->selected) {
			folderview_select_node(folderview,
					       folderview->selected);
		}
		break;
	case GDK_space:
		if (folderview->selected) {
			if (folderview->opened == folderview->selected &&
			    (!folderview->summaryview->folder_item ||
			     folderview->summaryview->folder_item->total == 0))
				folderview_select_next_unread(folderview);
			else
				folderview_select_node(folderview,
						       folderview->selected);
		}
		break;
	default:
		break;
	}

	return FALSE;
}

static void folderview_selected(GtkCTree *ctree, GtkCTreeNode *row,
				gint column, FolderView *folderview)
{
	static gboolean can_select = TRUE;	/* exclusive lock */
	gboolean opened;
	FolderItem *item;

	folderview->selected = row;

	main_window_set_menu_sensitive(folderview->mainwin);

	if (folderview->opened == row) {
		folderview->open_folder = FALSE;
		return;
	}

	if (!can_select || summary_is_locked(folderview->summaryview)) {
		gtkut_ctree_set_focus_row(ctree, folderview->opened);
		gtk_ctree_select(ctree, folderview->opened);
		return;
	}

	if (!folderview->open_folder) return;

	item = gtk_ctree_node_get_row_data(ctree, row);
	if (!item) return;

	can_select = FALSE;

	if (item->path)
		debug_print(_("Folder %s is selected\n"), item->path);

	if (!GTK_CTREE_ROW(row)->children)
		gtk_ctree_expand(ctree, row);
	if (folderview->opened &&
	    !GTK_CTREE_ROW(folderview->opened)->children)
		gtk_ctree_collapse(ctree, folderview->opened);

	/* ungrab the mouse event */
	if (GTK_WIDGET_HAS_GRAB(ctree)) {
		gtk_grab_remove(GTK_WIDGET(ctree));
		if (gdk_pointer_is_grabbed())
			gdk_pointer_ungrab(GDK_CURRENT_TIME);
	}

	opened = summary_show(folderview->summaryview, item, FALSE);

	if (!opened) {
		gtkut_ctree_set_focus_row(ctree, folderview->opened);
		gtk_ctree_select(ctree, folderview->opened);
	} else {
		folderview->opened = row;
		if (gtk_ctree_node_is_visible(ctree, row)
		    != GTK_VISIBILITY_FULL)
			gtk_ctree_node_moveto(ctree, row, -1, 0.5, 0);
	}

	folderview->open_folder = FALSE;
	can_select = TRUE;
}

static void folderview_tree_expanded(GtkCTree *ctree, GtkCTreeNode *node,
				     FolderView *folderview)
{
	FolderItem *item;

	item = gtk_ctree_node_get_row_data(ctree, node);
	g_return_if_fail(item != NULL);
	item->collapsed = FALSE;
	folderview_update_node(folderview, node);
}

static void folderview_tree_collapsed(GtkCTree *ctree, GtkCTreeNode *node,
				      FolderView *folderview)
{
	FolderItem *item;

	item = gtk_ctree_node_get_row_data(ctree, node);
	g_return_if_fail(item != NULL);
	item->collapsed= TRUE;
	folderview_update_node(folderview, node);
}

static void folderview_popup_close(GtkMenuShell *menu_shell,
				   FolderView *folderview)
{
	if (!folderview->opened) return;

	gtkut_ctree_set_focus_row(GTK_CTREE(folderview->ctree),
				  folderview->opened);
	gtk_ctree_select(GTK_CTREE(folderview->ctree), folderview->opened);
}

static void folderview_col_resized(GtkCList *clist, gint column, gint width,
				   FolderView *folderview)
{
	switch (column) {
	case COL_FOLDER:
		prefs_common.folder_col_folder = width;
		break;
	case COL_NEW:
		prefs_common.folder_col_new = width;
		break;
	case COL_UNREAD:
		prefs_common.folder_col_unread = width;
		break;
	case COL_TOTAL:
		prefs_common.folder_col_total = width;
		break;
	default:
		break;
	}
}

static void folderview_download_func(Folder *folder, FolderItem *item,
				     gpointer data)
{
	GList *list;

	for (list = folderview_list; list != NULL; list = list->next) {
		FolderView *folderview = (FolderView *)list->data;
		MainWindow *mainwin = folderview->mainwin;
		gchar *str;

		str = g_strdup_printf
			(_("Downloading messages in %s ..."), item->path);
		main_window_progress_set(mainwin,
					 GPOINTER_TO_INT(data), item->total);
		STATUSBAR_PUSH(mainwin, str);
		STATUSBAR_POP(mainwin);
		g_free(str);
	}
}

static void folderview_download_cb(FolderView *folderview, guint action,
				   GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	MainWindow *mainwin = folderview->mainwin;
	FolderItem *item;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	if (!main_window_toggle_online_if_offline(folderview->mainwin))
		return;

	main_window_cursor_wait(mainwin);
	inc_lock();
	main_window_lock(mainwin);
	gtk_widget_set_sensitive(folderview->ctree, FALSE);
	main_window_progress_on(mainwin);
	GTK_EVENTS_FLUSH();
	folder_set_ui_func(item->folder, folderview_download_func, NULL);
	if (folder_item_fetch_all_msg(item) < 0) {
		gchar *name;

		name = trim_string(item->name, 32);
		alertpanel_error(_("Error occurred while downloading messages in `%s'."), name);
		g_free(name);
	}
	folder_set_ui_func(item->folder, NULL, NULL);
	main_window_progress_off(mainwin);
	gtk_widget_set_sensitive(folderview->ctree, TRUE);
	main_window_unlock(mainwin);
	inc_unlock();
	main_window_cursor_normal(mainwin);
	statusbar_pop_all();
}

static void folderview_update_tree_cb(FolderView *folderview, guint action,
				      GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	if (action == 0)
		folderview_check_new(item->folder);
	else
		folderview_rescan_tree(folderview, item->folder);
}

static void folderview_new_folder_cb(FolderView *folderview, guint action,
				     GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;
	FolderItem *new_item;
	gchar *new_folder;
	gchar *name;
	gchar *p;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	if (FOLDER_TYPE(item->folder) == F_IMAP)
		g_return_if_fail(item->folder->account != NULL);

	if (FOLDER_TYPE(item->folder) == F_IMAP) {
		new_folder = input_dialog
			(_("New folder"),
			 _("Input the name of new folder:\n"
			   "(if you want to create a folder to store subfolders,\n"
			   " append `/' at the end of the name)"),
			 _("NewFolder"));
	} else {
		new_folder = input_dialog(_("New folder"),
					  _("Input the name of new folder:"),
					  _("NewFolder"));
	}
	if (!new_folder) return;
	AUTORELEASE_STR(new_folder, {g_free(new_folder); return;});

	p = strchr(new_folder, G_DIR_SEPARATOR);
	if ((p && FOLDER_TYPE(item->folder) != F_IMAP) ||
	    (p && FOLDER_TYPE(item->folder) == F_IMAP && *(p + 1) != '\0')) {
		alertpanel_error(_("`%c' can't be included in folder name."),
				 G_DIR_SEPARATOR);
		return;
	}

	name = trim_string(new_folder, 32);
	AUTORELEASE_STR(name, {g_free(name); return;});

	/* find whether the directory already exists */
	if (folder_find_child_item_by_name(item, new_folder)) {
		alertpanel_error(_("The folder `%s' already exists."), name);
		return;
	}

	new_item = item->folder->klass->create_folder(item->folder, item,
						      new_folder);
	if (!new_item) {
		alertpanel_error(_("Can't create the folder `%s'."), name);
		return;
	}

	folderview_append_item(new_item);
	folder_write_list();
}

static void folderview_rename_folder_cb(FolderView *folderview, guint action,
					GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;
	gchar *new_folder;
	gchar *name;
	gchar *message;
	gchar *old_path;
	gchar *old_id;
	gchar *new_id;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->path != NULL);
	g_return_if_fail(item->folder != NULL);

	name = trim_string(item->name, 32);
	message = g_strdup_printf(_("Input new name for `%s':"), name);
	new_folder = input_dialog(_("Rename folder"), message,
				  g_basename(item->path));
	g_free(message);
	g_free(name);
	if (!new_folder) return;
	AUTORELEASE_STR(new_folder, {g_free(new_folder); return;});

	if (strchr(new_folder, G_DIR_SEPARATOR) != NULL) {
		alertpanel_error(_("`%c' can't be included in folder name."),
				 G_DIR_SEPARATOR);
		return;
	}

	if (folder_find_child_item_by_name(item->parent, new_folder)) {
		name = trim_string(new_folder, 32);
		alertpanel_error(_("The folder `%s' already exists."), name);
		g_free(name);
		return;
	}

	Xstrdup_a(old_path, item->path, {g_free(new_folder); return;});
	old_id = folder_item_get_identifier(item);

	if (item->folder->klass->rename_folder(item->folder, item,
					       new_folder) < 0) {
		g_free(old_id);
		return;
	}

	if (folder_get_default_folder() == item->folder)
		prefs_filter_rename_path(old_path, item->path);
	new_id = folder_item_get_identifier(item);
	prefs_filter_rename_path(old_id, new_id);
	g_free(old_id);
	g_free(new_id);

	gtk_clist_freeze(GTK_CLIST(ctree));

	folderview_update_node(folderview, folderview->selected);
	folderview_sort_folders(folderview,
				GTK_CTREE_ROW(folderview->selected)->parent,
				item->folder);
	if (folderview->opened == folderview->selected ||
	    gtk_ctree_is_ancestor(ctree,
				  folderview->selected,
				  folderview->opened)) {
		GtkCTreeNode *node = folderview->opened;
		folderview_unselect(folderview);
		folderview_select_node(folderview, node);
	}

	gtk_clist_thaw(GTK_CLIST(ctree));

	folder_write_list();
}

static void folderview_delete_folder_cb(FolderView *folderview, guint action,
					GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	Folder *folder;
	FolderItem *item;
	gchar *message, *name;
	AlertValue avalue;
	gchar *old_path;
	gchar *old_id;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->path != NULL);
	g_return_if_fail(item->folder != NULL);

	folder = item->folder;

	name = trim_string(item->name, 32);
	AUTORELEASE_STR(name, {g_free(name); return;});
	message = g_strdup_printf
		(_("All folder(s) and message(s) under `%s' will be deleted.\n"
		   "Do you really want to delete?"), name);
	avalue = alertpanel(_("Delete folder"), message,
			    _("Yes"), _("+No"), NULL);
	g_free(message);
	if (avalue != G_ALERTDEFAULT) return;

	Xstrdup_a(old_path, item->path, return);
	old_id = folder_item_get_identifier(item);

	if (folderview->opened == folderview->selected ||
	    gtk_ctree_is_ancestor(ctree,
				  folderview->selected, folderview->opened)) {
		summary_clear_all(folderview->summaryview);
		folderview->opened = NULL;
	}

	if (folder->klass->remove_folder(folder, item) < 0) {
		alertpanel_error(_("Can't remove the folder `%s'."), name);
		g_free(old_id);
		return;
	}

	if (folder_get_default_folder() == folder)
		prefs_filter_delete_path(old_path);
	prefs_filter_delete_path(old_id);
	g_free(old_id);

	gtk_ctree_remove_node(ctree, folderview->selected);
	folder_write_list();
}

static void folderview_empty_trash_cb(FolderView *folderview, guint action,
				      GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;
	Folder *folder;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->path != NULL);
	g_return_if_fail(item->folder != NULL);

	folder = item->folder;

	if (folder->trash != item) return;
	if (item->stype != F_TRASH) return;

	if (alertpanel(_("Empty trash"), _("Empty all messages in trash?"),
		       GTK_STOCK_YES, GTK_STOCK_NO, NULL) != G_ALERTDEFAULT)
		return;

	procmsg_empty_trash(folder->trash);
	statusbar_pop_all();
	folderview_update_item(folder->trash, TRUE);

	if (folderview->opened == folderview->selected)
		gtk_widget_grab_focus(folderview->ctree);
}

static void folderview_remove_mailbox_cb(FolderView *folderview, guint action,
					 GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	GtkCTreeNode *node;
	FolderItem *item;
	gchar *name;
	gchar *message;
	AlertValue avalue;

	if (!folderview->selected) return;
	node = folderview->selected;
	item = gtk_ctree_node_get_row_data(ctree, node);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	if (item->parent) return;

	name = trim_string(item->folder->name, 32);
	message = g_strdup_printf
		(_("Really remove the mailbox `%s' ?\n"
		   "(The messages are NOT deleted from the disk)"), name);
	avalue = alertpanel(_("Remove mailbox"), message,
			    _("Yes"), _("+No"), NULL);
	g_free(message);
	g_free(name);
	if (avalue != G_ALERTDEFAULT) return;

	if (folderview->summaryview->folder_item &&
	    folderview->summaryview->folder_item->folder == item->folder) {
		summary_clear_all(folderview->summaryview);
		folderview->opened = NULL;
	}
	folder_destroy(item->folder);
	gtk_ctree_remove_node(ctree, node);
	folder_write_list();
}

static void folderview_rm_imap_server_cb(FolderView *folderview, guint action,
					 GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;
	PrefsAccount *account;
	gchar *name;
	gchar *message;
	AlertValue avalue;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(FOLDER_TYPE(item->folder) == F_IMAP);
	g_return_if_fail(item->folder->account != NULL);

	name = trim_string(item->folder->name, 32);
	message = g_strdup_printf(_("Really delete IMAP4 account `%s'?"), name);
	avalue = alertpanel(_("Delete IMAP4 account"), message,
			    _("Yes"), _("+No"), NULL);
	g_free(message);
	g_free(name);

	if (avalue != G_ALERTDEFAULT) return;

	if (folderview->summaryview->folder_item &&
	    folderview->summaryview->folder_item->folder == item->folder) {
		summary_clear_all(folderview->summaryview);
		folderview->opened = NULL;
	}

	account = item->folder->account;
	folder_destroy(item->folder);
	account_destroy(account);
	gtk_ctree_remove_node(ctree, folderview->selected);
	account_set_menu();
	main_window_reflect_prefs_all();
	folder_write_list();
}

static void folderview_new_news_group_cb(FolderView *folderview, guint action,
					 GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	GtkCTreeNode *servernode, *node;
	Folder *folder;
	FolderItem *item;
	FolderItem *rootitem;
	FolderItem *newitem;
	GSList *new_subscr;
	GSList *cur;
	GNode *gnode;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	folder = item->folder;
	g_return_if_fail(folder != NULL);
	g_return_if_fail(FOLDER_TYPE(folder) == F_NEWS);
	g_return_if_fail(folder->account != NULL);

	if (GTK_CTREE_ROW(folderview->selected)->parent != NULL)
		servernode = GTK_CTREE_ROW(folderview->selected)->parent;
	else
		servernode = folderview->selected;

	rootitem = gtk_ctree_node_get_row_data(ctree, servernode);

	new_subscr = grouplist_dialog(folder);

	/* remove unsubscribed newsgroups */
	for (gnode = folder->node->children; gnode != NULL; ) {
		GNode *next = gnode->next;

		item = FOLDER_ITEM(gnode->data);
		if (g_slist_find_custom(new_subscr, item->path,
					(GCompareFunc)g_strcasecmp) != NULL) {
			gnode = next;
			continue;
		}

		node = gtk_ctree_find_by_row_data(ctree, servernode, item);
		if (!node) {
			gnode = next;
			continue;
		}

		if (folderview->opened == node) {
			summary_clear_all(folderview->summaryview);
			folderview->opened = NULL;
		}

		folder_item_remove(item);
		gtk_ctree_remove_node(ctree, node);

		gnode = next;
	}

	gtk_clist_freeze(GTK_CLIST(ctree));

	/* add subscribed newsgroups */
	for (cur = new_subscr; cur != NULL; cur = cur->next) {
		gchar *name = (gchar *)cur->data;

		if (folder_find_child_item_by_name(rootitem, name) != NULL)
			continue;

		newitem = folder_item_new(name, name);
		folder_item_append(rootitem, newitem);
		folderview_append_item(newitem);
	}

	gtk_clist_thaw(GTK_CLIST(ctree));

	slist_free_strings(new_subscr);
	g_slist_free(new_subscr);

	folder_write_list();
}

static void folderview_rm_news_group_cb(FolderView *folderview, guint action,
					GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;
	gchar *name;
	gchar *message;
	AlertValue avalue;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(FOLDER_TYPE(item->folder) == F_NEWS);
	g_return_if_fail(item->folder->account != NULL);

	name = trim_string_before(item->path, 32);
	message = g_strdup_printf(_("Really delete newsgroup `%s'?"), name);
	avalue = alertpanel(_("Delete newsgroup"), message,
			    _("Yes"), _("+No"), NULL);
	g_free(message);
	g_free(name);
	if (avalue != G_ALERTDEFAULT) return;

	if (folderview->opened == folderview->selected) {
		summary_clear_all(folderview->summaryview);
		folderview->opened = NULL;
	}

	folder_item_remove(item);
	gtk_ctree_remove_node(ctree, folderview->selected);
	folder_write_list();
}

static void folderview_rm_news_server_cb(FolderView *folderview, guint action,
					 GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;
	PrefsAccount *account;
	gchar *name;
	gchar *message;
	AlertValue avalue;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(FOLDER_TYPE(item->folder) == F_NEWS);
	g_return_if_fail(item->folder->account != NULL);

	name = trim_string(item->folder->name, 32);
	message = g_strdup_printf(_("Really delete news account `%s'?"), name);
	avalue = alertpanel(_("Delete news account"), message,
			    _("Yes"), _("+No"), NULL);
	g_free(message);
	g_free(name);

	if (avalue != G_ALERTDEFAULT) return;

	if (folderview->summaryview->folder_item &&
	    folderview->summaryview->folder_item->folder == item->folder) {
		summary_clear_all(folderview->summaryview);
		folderview->opened = NULL;
	}

	account = item->folder->account;
	folder_destroy(item->folder);
	account_destroy(account);
	gtk_ctree_remove_node(ctree, folderview->selected);
	account_set_menu();
	main_window_reflect_prefs_all();
	folder_write_list();
}

static void folderview_search_cb(FolderView *folderview, guint action,
				 GtkWidget *widget)
{
	summary_search(folderview->summaryview);
}

static void folderview_property_cb(FolderView *folderview, guint action,
				   GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	if (item->parent == NULL && item->folder->account)
		account_open(item->folder->account);
	else
		prefs_folder_item_open(item);
}

static void folderview_defer_expand_stop(FolderView *folderview)
{
	if (folderview->spring_timer > 0) {
		gtk_timeout_remove(folderview->spring_timer);
		folderview->spring_timer = 0;
	}
	folderview->spring_node = NULL;
}

static gint folderview_defer_expand(gpointer data)
{
	FolderView *folderview = (FolderView *)data;

	if (folderview->spring_node) {
		gtk_ctree_expand(GTK_CTREE(folderview->ctree),
				 folderview->spring_node);
	}
	folderview_defer_expand_stop(folderview);

	return FALSE;
}

static gboolean folderview_drag_motion_cb(GtkWidget      *widget,
					  GdkDragContext *context,
					  gint            x,
					  gint            y,
					  guint           time,
					  FolderView     *folderview)
{
	gint row, column;
	FolderItem *item, *src_item;
	GtkCTreeNode *node = NULL;
	gboolean acceptable = FALSE;

	if (gtk_clist_get_selection_info
		(GTK_CLIST(widget), x - 24, y - 24, &row, &column)) {
		node = gtk_ctree_node_nth(GTK_CTREE(widget), row);
		item = gtk_ctree_node_get_row_data(GTK_CTREE(widget), node);
		src_item = folderview->summaryview->folder_item;
		if (src_item && src_item != item)
			acceptable = FOLDER_ITEM_CAN_ADD(item);
	}

	if (node != folderview->spring_node) {
		folderview_defer_expand_stop(folderview);
		if (node && !GTK_CTREE_ROW(node)->expanded &&
		    GTK_CTREE_ROW(node)->children) {
			folderview->spring_timer =
				gtk_timeout_add(1000, folderview_defer_expand,
						folderview);
			folderview->spring_node = node;
		}
	}

	if (acceptable) {
		g_signal_handlers_block_by_func
			(G_OBJECT(widget),
			 G_CALLBACK(folderview_selected), folderview);
		gtk_ctree_select(GTK_CTREE(widget), node);
		g_signal_handlers_unblock_by_func
			(G_OBJECT(widget),
			 G_CALLBACK(folderview_selected), folderview);
		if ((context->actions & GDK_ACTION_MOVE) != 0)
			gdk_drag_status(context, GDK_ACTION_MOVE, time);
		else if ((context->actions & GDK_ACTION_COPY) != 0)
			gdk_drag_status(context, GDK_ACTION_COPY, time);
		else if ((context->actions & GDK_ACTION_LINK) != 0)
			gdk_drag_status(context, GDK_ACTION_LINK, time);
		else
			gdk_drag_status(context, 0, time);
	} else {
		gtk_ctree_select(GTK_CTREE(widget), folderview->opened);
		gdk_drag_status(context, 0, time);
	}

	return acceptable;
}

static void folderview_drag_leave_cb(GtkWidget      *widget,
				     GdkDragContext *context,
				     guint           time,
				     FolderView     *folderview)
{
	folderview_defer_expand_stop(folderview);
	gtk_ctree_select(GTK_CTREE(widget), folderview->opened);
}

static void folderview_drag_received_cb(GtkWidget        *widget,
					GdkDragContext   *context,
					gint              x,
					gint              y,
					GtkSelectionData *data,
					guint             info,
					guint             time,
					FolderView       *folderview)
{
	gint row, column;
	FolderItem *item, *src_item;
	GtkCTreeNode *node;

	folderview_defer_expand_stop(folderview);

	if (gtk_clist_get_selection_info
		(GTK_CLIST(widget), x - 24, y - 24, &row, &column) == 0)
		return;

	node = gtk_ctree_node_nth(GTK_CTREE(widget), row);
	item = gtk_ctree_node_get_row_data(GTK_CTREE(widget), node);
	src_item = folderview->summaryview->folder_item;
	if (FOLDER_ITEM_CAN_ADD(item) && src_item && src_item != item) {
		if ((context->actions & GDK_ACTION_MOVE) != 0) {
			summary_move_selected_to(folderview->summaryview, item);
			gtk_drag_finish(context, TRUE, TRUE, time);
		} else if ((context->actions & GDK_ACTION_COPY) != 0) {
			summary_copy_selected_to(folderview->summaryview, item);
			gtk_drag_finish(context, TRUE, TRUE, time);
		} else
			gtk_drag_finish(context, FALSE, FALSE, time);
	} else
		gtk_drag_finish(context, FALSE, FALSE, time);
}

static gint folderview_clist_compare(GtkCList *clist,
				     gconstpointer ptr1, gconstpointer ptr2)
{
	FolderItem *item1 = ((GtkCListRow *)ptr1)->data;
	FolderItem *item2 = ((GtkCListRow *)ptr2)->data;

	if (!item1->name)
		return (item2->name != NULL);
	if (!item2->name)
		return -1;

	return g_strcasecmp(item1->name, item2->name);
}
