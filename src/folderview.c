/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2014 Hiroyuki Yamamoto
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
#include <gtk/gtkvbox.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktreestore.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkstatusbar.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkitemfactory.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkversion.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "main.h"
#include "mainwindow.h"
#include "folderview.h"
#include "summaryview.h"
#include "query_search.h"
#include "inputdialog.h"
#include "subscribedialog.h"
#include "foldersel.h"
#include "manage_window.h"
#include "alertpanel.h"
#include "menu.h"
#include "stock_pixmap.h"
#include "statusbar.h"
#include "procmsg.h"
#include "utils.h"
#include "gtkutils.h"
#include "trayicon.h"
#include "prefs_common.h"
#include "prefs_account.h"
#include "prefs_folder_item.h"
#include "prefs_search_folder.h"
#include "filter.h"
#include "account.h"
#include "account_dialog.h"
#include "folder.h"
#include "inc.h"
#include "send_message.h"
#include "virtual.h"
#include "plugin.h"

enum
{
	COL_FOLDER_NAME,
	COL_NEW,
	COL_UNREAD,
	COL_TOTAL,
	COL_FOLDER_ITEM,
	COL_PIXBUF,
	COL_PIXBUF_OPEN,
	COL_FOREGROUND,
	COL_BOLD,
	N_COLS
};

#define COL_FOLDER_WIDTH	150
#define COL_NUM_WIDTH		32

#define STATUSBAR_PUSH(mainwin, str) \
{ \
	gtk_statusbar_push(GTK_STATUSBAR(mainwin->statusbar), \
			   mainwin->folderview_cid, str); \
	gtkut_widget_draw_now(mainwin->statusbar); \
}

#define STATUSBAR_POP(mainwin) \
{ \
	gtk_statusbar_pop(GTK_STATUSBAR(mainwin->statusbar), \
			  mainwin->folderview_cid); \
}

static GList *folderview_list = NULL;

static GdkPixbuf *inbox_pixbuf;
static GdkPixbuf *outbox_pixbuf;
static GdkPixbuf *folder_pixbuf;
static GdkPixbuf *folderopen_pixbuf;
static GdkPixbuf *foldernoselect_pixbuf;
static GdkPixbuf *draft_pixbuf;
static GdkPixbuf *trash_pixbuf;
static GdkPixbuf *junk_pixbuf;
static GdkPixbuf *virtual_pixbuf;

static void folderview_set_columns	(FolderView	*folderview);

static void folderview_select_row	(FolderView	*folderview,
					 GtkTreeIter	*iter);
static void folderview_select_row_ref	(FolderView	*folderview,
					 GtkTreeRowReference *row);

static void folderview_set_folders	(FolderView	*folderview);
static void folderview_append_folder	(FolderView	*folderview,
					 Folder		*folder);

static void folderview_update_row	(FolderView	*folderview,
					 GtkTreeIter	*iter);
static void folderview_update_row_all	(FolderView	*folderview);

static gint folderview_folder_name_compare	(GtkTreeModel	*model,
						 GtkTreeIter	*a,
						 GtkTreeIter	*b,
						 gpointer	 data);

/* callback functions */
static gboolean folderview_button_pressed	(GtkWidget	*treeview,
						 GdkEventButton	*event,
						 FolderView	*folderview);
static gboolean folderview_button_released	(GtkWidget	*treeview,
						 GdkEventButton	*event,
						 FolderView	*folderview);

static gboolean folderview_key_pressed	(GtkWidget	*widget,
					 GdkEventKey	*event,
					 FolderView	*folderview);

static void folderview_selection_changed(GtkTreeSelection	*selection,
					 FolderView		*folderview);

static void folderview_row_expanded	(GtkTreeView		*treeview,
					 GtkTreeIter		*iter,
					 GtkTreePath		*path,
					 FolderView		*folderview);
static void folderview_row_collapsed	(GtkTreeView		*treeview,
					 GtkTreeIter		*iter,
					 GtkTreePath		*path,
					 FolderView		*folderview);

static void folderview_popup_close	(GtkMenuShell	*menu_shell,
					 FolderView	*folderview);

static void folderview_col_resized	(GtkWidget	*widget,
					 GtkAllocation	*allocation,
					 FolderView	*folderview);

static void folderview_download_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);

static void folderview_update_tree_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);

static void folderview_update_summary_cb(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);

static void folderview_mark_all_read_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);
static void folderview_send_queue_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);

static void folderview_new_folder_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);
static void folderview_rename_folder_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);
static void folderview_move_folder_cb	(FolderView	*folderview,
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

static GtkTargetEntry folderview_drag_types[] =
{
	{"text/plain", GTK_TARGET_SAME_APP, 0}
};

static GtkItemFactoryEntry folderview_mail_popup_entries[] =
{
	{N_("/Create _new folder..."),	NULL, folderview_new_folder_cb, 0, NULL},
	{N_("/_Rename folder..."),	NULL, folderview_rename_folder_cb, 0, NULL},
	{N_("/_Move folder..."),	NULL, folderview_move_folder_cb, 0, NULL},
	{N_("/_Delete folder"),		NULL, folderview_delete_folder_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/Empty _junk"),		NULL, folderview_empty_trash_cb, 0, NULL},
	{N_("/Empty _trash"),		NULL, folderview_empty_trash_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Check for new messages"),
					NULL, folderview_update_tree_cb, 0, NULL},
	{N_("/R_ebuild folder tree"),	NULL, folderview_update_tree_cb, 1, NULL},
	{N_("/_Update summary"),	NULL, folderview_update_summary_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/Mar_k all read"),		NULL, folderview_mark_all_read_cb, 0, NULL},
	{N_("/Send _queued messages"),	NULL, folderview_send_queue_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Search messages..."),	NULL, folderview_search_cb, 0, NULL},
	{N_("/Ed_it search condition..."),
					NULL, folderview_search_cb, 0, NULL},
	{N_("/_Properties..."),		NULL, folderview_property_cb, 0, NULL}
};

static GtkItemFactoryEntry folderview_imap_popup_entries[] =
{
	{N_("/Create _new folder..."),	NULL, folderview_new_folder_cb,    0, NULL},
	{N_("/_Rename folder..."),	NULL, folderview_rename_folder_cb, 0, NULL},
	{N_("/_Move folder..."),	NULL, folderview_move_folder_cb, 0, NULL},
	{N_("/_Delete folder"),		NULL, folderview_delete_folder_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/Empty _junk"),		NULL, folderview_empty_trash_cb, 0, NULL},
	{N_("/Empty _trash"),		NULL, folderview_empty_trash_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/Down_load"),		NULL, folderview_download_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Check for new messages"),
					NULL, folderview_update_tree_cb, 0, NULL},
	{N_("/R_ebuild folder tree"),	NULL, folderview_update_tree_cb, 1, NULL},
	{N_("/_Update summary"),	NULL, folderview_update_summary_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/Mar_k all read"),		NULL, folderview_mark_all_read_cb, 0, NULL},
	{N_("/Send _queued messages"),	NULL, folderview_send_queue_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Search messages..."),	NULL, folderview_search_cb, 0, NULL},
	{N_("/Ed_it search condition..."),
					NULL, folderview_search_cb, 0, NULL},
	{N_("/_Properties..."),		NULL, folderview_property_cb, 0, NULL}
};

static GtkItemFactoryEntry folderview_news_popup_entries[] =
{
	{N_("/Su_bscribe to newsgroup..."),
					NULL, folderview_new_news_group_cb, 0, NULL},
	{N_("/_Remove newsgroup"),	NULL, folderview_rm_news_group_cb, 0, NULL},
	{N_("/_Rename folder..."),	NULL, folderview_rename_folder_cb, 0, NULL},
	{N_("/_Delete folder"),		NULL, folderview_delete_folder_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/Down_load"),		NULL, folderview_download_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Check for new messages"),
					NULL, folderview_update_tree_cb, 0, NULL},
	{N_("/_Update summary"),	NULL, folderview_update_summary_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/Mar_k all read"),		NULL, folderview_mark_all_read_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Search messages..."),	NULL, folderview_search_cb, 0, NULL},
	{N_("/Ed_it search condition..."),
					NULL, folderview_search_cb, 0, NULL},
	{N_("/_Properties..."),		NULL, folderview_property_cb, 0, NULL}
};


FolderView *folderview_create(void)
{
	FolderView *folderview;
	GtkWidget *vbox;
	GtkWidget *scrolledwin;
	GtkWidget *treeview;
	GtkTreeStore *store;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkWidget *mail_popup;
	GtkWidget *news_popup;
	GtkWidget *imap_popup;
	GtkItemFactory *mail_factory;
	GtkItemFactory *news_factory;
	GtkItemFactory *imap_factory;
	gint n_entries;

	debug_print(_("Creating folder view...\n"));
	folderview = g_new0(FolderView, 1);

	vbox = gtk_vbox_new(FALSE, 1);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy
		(GTK_SCROLLED_WINDOW(scrolledwin),
		 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(vbox), scrolledwin, TRUE, TRUE, 0);
	gtk_widget_set_size_request(scrolledwin,
				    prefs_common.folderview_width,
				    prefs_common.folderview_height);

	store = gtk_tree_store_new(N_COLS, G_TYPE_STRING, G_TYPE_STRING,
				   G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER,
				   GDK_TYPE_PIXBUF, GDK_TYPE_PIXBUF,
				   GDK_TYPE_COLOR, G_TYPE_INT);
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store),
					COL_FOLDER_NAME,
					folderview_folder_name_compare,
					NULL, NULL);

	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(G_OBJECT(store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), TRUE);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), FALSE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(treeview),
					COL_FOLDER_NAME);
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(treeview), FALSE);
#if GTK_CHECK_VERSION(2, 12, 0)
	g_object_set(treeview, "fixed-height-mode", TRUE, NULL);
#endif

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);

	gtk_container_add(GTK_CONTAINER(scrolledwin), treeview);

	/* create folder icon + name column */
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_spacing(column, 1);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width
		(column, prefs_common.folder_col_folder);
	gtk_tree_view_column_set_resizable(column, TRUE);
#if GTK_CHECK_VERSION(2, 14, 0)
	gtk_tree_view_column_set_expand(column, TRUE);
#endif

	renderer = gtk_cell_renderer_pixbuf_new();
	g_object_set(renderer, "ypad", 0, NULL);
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_set_title(column, _("Folder"));
	gtk_tree_view_column_set_attributes
		(column, renderer,
		 "pixbuf", COL_PIXBUF,
		 "pixbuf-expander-open", COL_PIXBUF_OPEN,
		 "pixbuf-expander-closed", COL_PIXBUF,
		 NULL);

	renderer = gtk_cell_renderer_text_new();
#if GTK_CHECK_VERSION(2, 6, 0)
	g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, "ypad", 0,
		     NULL);
#else
	g_object_set(renderer, "ypad", 0, NULL);
#endif
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_set_attributes(column, renderer,
					    "text", COL_FOLDER_NAME,
					    "foreground-gdk", COL_FOREGROUND,
					    "weight", COL_BOLD,
					    NULL);

	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
	gtk_tree_view_set_expander_column(GTK_TREE_VIEW(treeview), column);
	g_signal_connect(G_OBJECT(column->button), "size-allocate",
			 G_CALLBACK(folderview_col_resized), folderview);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "xalign", 1.0, "ypad", 0, NULL);
	column = gtk_tree_view_column_new_with_attributes
		(_("New"), renderer, "text", COL_NEW,
		 "foreground-gdk", COL_FOREGROUND,
		 "weight", COL_BOLD, NULL);
	gtk_tree_view_column_set_alignment(column, 1.0);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width
		(column, prefs_common.folder_col_new);
	gtk_tree_view_column_set_min_width(column, 8);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
	g_signal_connect(G_OBJECT(column->button), "size-allocate",
			 G_CALLBACK(folderview_col_resized), folderview);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "xalign", 1.0, "ypad", 0, NULL);
	column = gtk_tree_view_column_new_with_attributes
		(_("Unread"), renderer, "text", COL_UNREAD,
		 "foreground-gdk", COL_FOREGROUND,
		 "weight", COL_BOLD, NULL);
	gtk_tree_view_column_set_alignment(column, 1.0);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width
		(column, prefs_common.folder_col_unread);
	gtk_tree_view_column_set_min_width(column, 8);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
	g_signal_connect(G_OBJECT(column->button), "size-allocate",
			 G_CALLBACK(folderview_col_resized), folderview);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "xalign", 1.0, "ypad", 0, NULL);
	column = gtk_tree_view_column_new_with_attributes
		(_("Total"), renderer, "text", COL_TOTAL,
		 "foreground-gdk", COL_FOREGROUND,
		 "weight", COL_BOLD, NULL);
	gtk_tree_view_column_set_alignment(column, 1.0);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width
		(column, prefs_common.folder_col_total);
	gtk_tree_view_column_set_min_width(column, 8);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
	g_signal_connect(G_OBJECT(column->button), "size-allocate",
			 G_CALLBACK(folderview_col_resized), folderview);

	/* add rightmost empty column */
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_min_width(column, 0);
	gtk_tree_view_column_set_max_width(column, 0);
	gtk_tree_view_column_set_clickable(column, FALSE);
	gtk_tree_view_column_set_reorderable(column, FALSE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store),
					     COL_FOLDER_NAME,
					     GTK_SORT_ASCENDING);

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

	g_signal_connect(G_OBJECT(treeview), "button_press_event",
			 G_CALLBACK(folderview_button_pressed), folderview);
	g_signal_connect(G_OBJECT(treeview), "button_release_event",
			 G_CALLBACK(folderview_button_released), folderview);
	g_signal_connect(G_OBJECT(treeview), "key_press_event",
			 G_CALLBACK(folderview_key_pressed), folderview);

	g_signal_connect(G_OBJECT(selection), "changed",
			 G_CALLBACK(folderview_selection_changed), folderview);

	g_signal_connect_after(G_OBJECT(treeview), "row-expanded",
			       G_CALLBACK(folderview_row_expanded),
			       folderview);
	g_signal_connect_after(G_OBJECT(treeview), "row-collapsed",
			       G_CALLBACK(folderview_row_collapsed),
			       folderview);

	g_signal_connect(G_OBJECT(mail_popup), "selection_done",
			 G_CALLBACK(folderview_popup_close), folderview);
	g_signal_connect(G_OBJECT(imap_popup), "selection_done",
			 G_CALLBACK(folderview_popup_close), folderview);
	g_signal_connect(G_OBJECT(news_popup), "selection_done",
			 G_CALLBACK(folderview_popup_close), folderview);

        /* drop callback */
	gtk_drag_dest_set(treeview, GTK_DEST_DEFAULT_ALL,
			  folderview_drag_types, 1,
			  GDK_ACTION_MOVE | GDK_ACTION_COPY);
	g_signal_connect(G_OBJECT(treeview), "drag-motion",
			 G_CALLBACK(folderview_drag_motion_cb),
			 folderview);
	g_signal_connect(G_OBJECT(treeview), "drag-leave",
			 G_CALLBACK(folderview_drag_leave_cb),
			 folderview);
	g_signal_connect(G_OBJECT(treeview), "drag-data-received",
			 G_CALLBACK(folderview_drag_received_cb),
			 folderview);

	folderview->vbox         = vbox;
	folderview->scrolledwin  = scrolledwin;
	folderview->treeview     = treeview;
	folderview->store        = store;
	folderview->selection    = selection;
	folderview->mail_popup   = mail_popup;
	folderview->mail_factory = mail_factory;
	folderview->imap_popup   = imap_popup;
	folderview->imap_factory = imap_factory;
	folderview->news_popup   = news_popup;
	folderview->news_factory = news_factory;

	folderview->display_folder_unread = prefs_common.display_folder_unread;

	folderview_set_columns(folderview);

	gtk_widget_show_all(vbox);

	folderview_list = g_list_append(folderview_list, folderview);

	return folderview;
}

void folderview_init(FolderView *folderview)
{
	GtkWidget *treeview = folderview->treeview;

	stock_pixbuf_gdk(treeview, STOCK_PIXMAP_INBOX, &inbox_pixbuf);
	stock_pixbuf_gdk(treeview, STOCK_PIXMAP_OUTBOX, &outbox_pixbuf);
	stock_pixbuf_gdk(treeview, STOCK_PIXMAP_FOLDER_CLOSE, &folder_pixbuf);
	stock_pixbuf_gdk(treeview, STOCK_PIXMAP_FOLDER_OPEN, &folderopen_pixbuf);
	stock_pixbuf_gdk(treeview, STOCK_PIXMAP_FOLDER_NOSELECT,
			 &foldernoselect_pixbuf);
	stock_pixbuf_gdk(treeview, STOCK_PIXMAP_DRAFT, &draft_pixbuf);
	stock_pixbuf_gdk(treeview, STOCK_PIXMAP_TRASH, &trash_pixbuf);
	stock_pixbuf_gdk(treeview, STOCK_PIXMAP_SPAM_SMALL, &junk_pixbuf);
	stock_pixbuf_gdk(treeview, STOCK_PIXMAP_FOLDER_SEARCH, &virtual_pixbuf);
}

void folderview_reflect_prefs(FolderView *folderview)
{
	folderview_set_columns(folderview);
	if (folderview->display_folder_unread !=
	    prefs_common.display_folder_unread) {
		folderview->display_folder_unread =
			prefs_common.display_folder_unread;
		folderview_update_row_all(folderview);
	}
}

void folderview_add_sub_widget(FolderView *folderview, GtkWidget *widget)
{
	g_return_if_fail(folderview != NULL);
	g_return_if_fail(widget != NULL);

	debug_print("folderview_add_sub_widget: adding sub widget\n");

	gtk_box_pack_start(GTK_BOX(folderview->vbox), widget, FALSE, FALSE, 0);
}

FolderView *folderview_get(void)
{
	return (FolderView *)folderview_list->data;
}

void folderview_set(FolderView *folderview)
{
	MainWindow *mainwin = folderview->mainwin;
	GtkTreeIter iter;

	debug_print(_("Setting folder info...\n"));
	STATUSBAR_PUSH(mainwin, _("Setting folder info..."));

	main_window_cursor_wait(mainwin);

	folderview_unselect(folderview);

	gtk_tree_store_clear(folderview->store);

	folderview_set_folders(folderview);

	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(folderview->store),
					  &iter))
		folderview_select_row(folderview, &iter);

	main_window_cursor_normal(mainwin);
	STATUSBAR_POP(mainwin);
}

void folderview_set_all(void)
{
	GList *list;

	for (list = folderview_list; list != NULL; list = list->next)
		folderview_set((FolderView *)list->data);
}

static void folderview_set_columns(FolderView *folderview)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(folderview->treeview);
	GtkTreeViewColumn *column;

	column = gtk_tree_view_get_column(treeview, COL_NEW);
	gtk_tree_view_column_set_visible
		(column, prefs_common.folder_col_visible[COL_NEW]);
	column = gtk_tree_view_get_column(treeview, COL_UNREAD);
	gtk_tree_view_column_set_visible
		(column, prefs_common.folder_col_visible[COL_UNREAD]);
	column = gtk_tree_view_get_column(treeview, COL_TOTAL);
	gtk_tree_view_column_set_visible
		(column, prefs_common.folder_col_visible[COL_TOTAL]);
	column = gtk_tree_view_get_column(treeview, COL_TOTAL + 1);
	gtk_tree_view_column_set_visible
		(column, prefs_common.folder_col_visible[COL_NEW] ||
			 prefs_common.folder_col_visible[COL_UNREAD] ||
			 prefs_common.folder_col_visible[COL_TOTAL]);
}

void folderview_select(FolderView *folderview, FolderItem *item)
{
	GtkTreeIter iter;

	if (!item) return;

	if (gtkut_tree_model_find_by_column_data
		(GTK_TREE_MODEL(folderview->store), &iter, NULL,
		 COL_FOLDER_ITEM, item))
		folderview_select_row(folderview, &iter);
}

static void folderview_select_row(FolderView *folderview, GtkTreeIter *iter)
{
	GtkTreeModel *model = GTK_TREE_MODEL(folderview->store);
	GtkTreePath *path;

	g_return_if_fail(iter != NULL);

	path = gtk_tree_model_get_path(model, iter);

	gtkut_tree_view_expand_parent_all(GTK_TREE_VIEW(folderview->treeview),
					  iter);

	folderview->open_folder = TRUE;
	gtk_tree_view_set_cursor(GTK_TREE_VIEW(folderview->treeview), path,
				 NULL, FALSE);
	if (folderview->summaryview->folder_item &&
	    folderview->summaryview->folder_item->total > 0)
		gtk_widget_grab_focus(folderview->summaryview->treeview);
	else
		gtk_widget_grab_focus(folderview->treeview);

	gtk_tree_path_free(path);
}

static void folderview_select_row_ref(FolderView *folderview,
				      GtkTreeRowReference *row)
{
	GtkTreePath *path;
	GtkTreeIter iter;

	if (!row) return;

	path = gtk_tree_row_reference_get_path(row);
	if (!path)
		return;
	gtk_tree_model_get_iter(GTK_TREE_MODEL(folderview->store), &iter, path);
	gtk_tree_path_free(path);

	folderview_select_row(folderview, &iter);
}

void folderview_unselect(FolderView *folderview)
{
	if (folderview->selected) {
		gtk_tree_row_reference_free(folderview->selected);
		folderview->selected = NULL;
	}
	if (folderview->opened) {
		gtk_tree_row_reference_free(folderview->opened);
		folderview->opened = NULL;
	}
}

static gboolean folderview_find_next_unread(GtkTreeModel *model,
					    GtkTreeIter *next,
					    GtkTreeIter *iter)
{
	FolderItem *item;
	GtkTreeIter iter_;
	gboolean valid;

	if (iter) {
		iter_ = *iter;
		valid = gtkut_tree_model_next(model, &iter_);
	} else
		valid = gtk_tree_model_get_iter_first(model, &iter_);

	while (valid) {
		item = NULL;
		gtk_tree_model_get(model, &iter_, COL_FOLDER_ITEM, &item, -1);
		if (item && item->unread > 0 && item->stype != F_TRASH) {
			if (next)
				*next = iter_;
			return TRUE;
		}

		valid = gtkut_tree_model_next(model, &iter_);
	}

	return FALSE;
}

void folderview_select_next_unread(FolderView *folderview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(folderview->store);
	GtkTreeIter iter, next;
	gboolean remember_last;

	if (folderview->opened) {
		GtkTreePath *path;

		path = gtk_tree_row_reference_get_path(folderview->opened);
		if (!path)
			return;
		gtk_tree_model_get_iter(model, &iter, path);
		gtk_tree_path_free(path);
	} else {
		if (!gtk_tree_model_get_iter_first(model, &iter))
			return;
	}
	if (folderview_find_next_unread(model, &next, &iter)) {
		remember_last = prefs_common.remember_last_selected;
		prefs_common.remember_last_selected = FALSE;
		folderview_select_row(folderview, &next);
		prefs_common.remember_last_selected = remember_last;
		return;
	}

	if (!folderview->opened)
		return;

	/* search again from the first row */
	if (folderview_find_next_unread(model, &next, NULL)) {
		remember_last = prefs_common.remember_last_selected;
		prefs_common.remember_last_selected = FALSE;
		folderview_select_row(folderview, &next);
		prefs_common.remember_last_selected = remember_last;
	}
}

FolderItem *folderview_get_selected_item(FolderView *folderview)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	FolderItem *item = NULL;

	if (!folderview->selected)
		return NULL;

	path = gtk_tree_row_reference_get_path(folderview->selected);
	if (!path)
		return NULL;
	gtk_tree_model_get_iter(GTK_TREE_MODEL(folderview->store), &iter, path);
	gtk_tree_path_free(path);
	gtk_tree_model_get(GTK_TREE_MODEL(folderview->store), &iter,
			   COL_FOLDER_ITEM, &item, -1);

	return item;
}

void folderview_set_opened_item(FolderView *folderview, FolderItem *item)
{
	GtkTreeModel *model = GTK_TREE_MODEL(folderview->store);
	GtkTreeIter iter;
	GtkTreePath *path;

	gtk_tree_row_reference_free(folderview->opened);
	folderview->opened = NULL;

	if (!item)
		return;

	if (gtkut_tree_model_find_by_column_data
		 (model, &iter, NULL, COL_FOLDER_ITEM, item)) {
		path = gtk_tree_model_get_path(model, &iter);
		folderview->opened = gtk_tree_row_reference_new(model, path);
		gtk_tree_path_free(path);
	}
}

void folderview_update_opened_msg_num(FolderView *folderview)
{
	GtkTreePath *path;
	GtkTreeIter iter;

	if (!folderview->opened)
		return;

	path = gtk_tree_row_reference_get_path(folderview->opened);
	if (!path)
		return;
	gtk_tree_model_get_iter(GTK_TREE_MODEL(folderview->store), &iter, path);
	gtk_tree_path_free(path);

	folderview_update_row(folderview, &iter);
}

gboolean folderview_append_item(FolderView *folderview, GtkTreeIter *iter,
				FolderItem *item, gboolean expand_parent)
{
	FolderItem *parent_item;
	GtkTreeModel *model = GTK_TREE_MODEL(folderview->store);
	GtkTreeIter iter_, child;
	GtkTreeIter *iter_p = &iter_;

	g_return_val_if_fail(item != NULL, FALSE);
	g_return_val_if_fail(item->folder != NULL, FALSE);

	parent_item = item->parent;

	if (!parent_item)
		iter_p = NULL;
	else if (!gtkut_tree_model_find_by_column_data
		(model, iter_p, NULL, COL_FOLDER_ITEM, parent_item))
		return FALSE;

	if (!gtkut_tree_model_find_by_column_data
		(model, &child, iter_p, COL_FOLDER_ITEM, item)) {
		gtk_tree_store_append(folderview->store, &child, iter_p);
		gtk_tree_store_set(folderview->store, &child,
				   COL_FOLDER_NAME, item->name,
				   COL_FOLDER_ITEM, item,
				   -1);
		folderview_update_row(folderview, &child);
		if (iter)
			*iter = child;
		if (expand_parent && iter_p) {
			GtkTreePath *path;

			path = gtk_tree_model_get_path(model, iter_p);
			gtk_tree_view_expand_row
				(GTK_TREE_VIEW(folderview->treeview),
				 path, FALSE);
			gtk_tree_path_free(path);
		}
		return TRUE;
	}

	return FALSE;
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
	g_signal_connect(G_OBJECT(window), "delete_event", G_CALLBACK(gtk_true),
			 NULL);

	label = gtk_label_new(str);
	gtk_container_add(GTK_CONTAINER(window), label);
	gtk_widget_show(label);

	gtk_widget_show(window);

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
	GTK_EVENTS_FLUSH();

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

gint folderview_check_new(Folder *folder)
{
	FolderItem *item;
	FolderView *folderview;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean valid;
	gint prev_new, prev_unread, n_updated = 0;

	folderview = (FolderView *)folderview_list->data;
	model = GTK_TREE_MODEL(folderview->store);

	if (folder && FOLDER_IS_REMOTE(folder)) {
		if (!main_window_toggle_online_if_offline(folderview->mainwin))
			return 0;
	}

	inc_lock();
	main_window_lock(folderview->mainwin);
	gtk_widget_set_sensitive(folderview->treeview, FALSE);
	GTK_EVENTS_FLUSH();

	for (valid = gtk_tree_model_get_iter_first(model, &iter);
	     valid; valid = gtkut_tree_model_next(model, &iter)) {
		item = NULL;
		gtk_tree_model_get(model, &iter,
				   COL_FOLDER_ITEM, &item, -1);
		if (!item || !item->path || !item->folder) continue;
		if (item->stype == F_VIRTUAL) continue;
		if (item->no_select) continue;
		if (folder && folder != item->folder) continue;
		if (!folder && FOLDER_IS_REMOTE(item->folder)) continue;

		prev_new = item->new;
		prev_unread = item->unread;
		folderview_scan_tree_func(item->folder, item, NULL);
		if (folder_item_scan(item) < 0) {
			if (folder && FOLDER_IS_REMOTE(folder) &&
			    REMOTE_FOLDER(folder)->session == NULL)
				break;
		}
		folderview_update_row(folderview, &iter);
		if (item->stype != F_TRASH && item->stype != F_JUNK) {
			if (prev_unread < item->unread)
				n_updated += item->unread - prev_unread;
			else if (prev_new < item->new)
				n_updated += item->new - prev_new;
		}
	}

	gtk_widget_set_sensitive(folderview->treeview, TRUE);
	main_window_unlock(folderview->mainwin);
	inc_unlock();
	statusbar_pop_all();

	folder_write_list();

	return n_updated;
}

gint folderview_check_new_item(FolderItem *item)
{
	Folder *folder;
	FolderView *folderview;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint prev_new, prev_unread, n_updated = 0;

	g_return_val_if_fail(item != NULL, 0);
	g_return_val_if_fail(item->folder != NULL, 0);

	if (!item->path || item->no_select)
		return 0;

	folderview = (FolderView *)folderview_list->data;
	model = GTK_TREE_MODEL(folderview->store);

	folder = item->folder;

	if (!FOLDER_IS_LOCAL(folder)) {
		if (!main_window_toggle_online_if_offline(folderview->mainwin))
			return 0;
	}

	if (!gtkut_tree_model_find_by_column_data
		(model, &iter, NULL, COL_FOLDER_ITEM, item))
		return 0;

	inc_lock();
	main_window_lock(folderview->mainwin);
	gtk_widget_set_sensitive(folderview->treeview, FALSE);
	GTK_EVENTS_FLUSH();

	prev_new = item->new;
	prev_unread = item->unread;
	folderview_scan_tree_func(folder, item, NULL);
	folder_item_scan(item);
	folderview_update_row(folderview, &iter);
	if (item->stype != F_TRASH && item->stype != F_JUNK) {
		if (prev_unread < item->unread)
			n_updated = item->unread - prev_unread;
		else if (prev_new < item->new)
			n_updated = item->new - prev_new;
	}

	gtk_widget_set_sensitive(folderview->treeview, TRUE);
	main_window_unlock(folderview->mainwin);
	inc_unlock();
	statusbar_pop_all();

	folder_write_list();

	return n_updated;
}

gint folderview_check_new_all(void)
{
	GList *list;
	GtkWidget *window;
	FolderView *folderview;
	gint n_updated = 0;

	folderview = (FolderView *)folderview_list->data;

	inc_lock();
	main_window_lock(folderview->mainwin);
	window = label_window_create
		(_("Checking for new messages in all folders..."));

	list = folder_get_list();
	for (; list != NULL; list = list->next) {
		Folder *folder = list->data;

		n_updated += folderview_check_new(folder);
	}

	gtk_widget_destroy(window);
	main_window_unlock(folderview->mainwin);
	inc_unlock();

	return n_updated;
}

static gboolean folderview_search_new_recursive(GtkTreeModel *model,
						GtkTreeIter *iter)
{
	FolderItem *item = NULL;
	GtkTreeIter iter_;
	gboolean valid;

	if (iter) {
		gtk_tree_model_get(model, iter, COL_FOLDER_ITEM, &item, -1);
		if (item) {
			if (item->new > 0 ||
			    (item->stype == F_QUEUE && item->total > 0))
				return TRUE;
		}
		valid = gtk_tree_model_iter_children(model, &iter_, iter);
	} else
		valid = gtk_tree_model_get_iter_first(model, &iter_);

	while (valid) {
		if (folderview_search_new_recursive(model, &iter_) == TRUE)
			return TRUE;
		valid = gtk_tree_model_iter_next(model, &iter_);
	}

	return FALSE;
}

static gboolean folderview_have_new_children(FolderView *folderview,
					     GtkTreeIter *iter)
{
	GtkTreeModel *model = GTK_TREE_MODEL(folderview->store);
	GtkTreeIter iter_;
	gboolean valid;

	if (iter)
		valid = gtk_tree_model_iter_children(model, &iter_, iter);
	else
		valid = gtk_tree_model_get_iter_first(model, &iter_);

	while (valid) {
		if (folderview_search_new_recursive(model, &iter_) == TRUE)
			return TRUE;
		valid = gtk_tree_model_iter_next(model, &iter_);
	}

	return FALSE;
}

static gboolean folderview_search_unread_recursive(GtkTreeModel *model,
						   GtkTreeIter *iter)
{
	FolderItem *item = NULL;
	GtkTreeIter iter_;
	gboolean valid;

	if (iter) {
		gtk_tree_model_get(model, iter, COL_FOLDER_ITEM, &item, -1);
		if (item) {
			if (item->stype == F_TRASH)
				return FALSE;
			if (item->unread > 0 ||
			    (item->stype == F_QUEUE && item->total > 0))
				return TRUE;
		}
		valid = gtk_tree_model_iter_children(model, &iter_, iter);
	} else
		valid = gtk_tree_model_get_iter_first(model, &iter_);

	while (valid) {
		if (folderview_search_unread_recursive(model, &iter_) == TRUE)
			return TRUE;
		valid = gtk_tree_model_iter_next(model, &iter_);
	}

	return FALSE;
}

static gboolean folderview_have_unread_children(FolderView *folderview,
						GtkTreeIter *iter)
{
	GtkTreeModel *model = GTK_TREE_MODEL(folderview->store);
	GtkTreeIter iter_;
	gboolean valid;

	if (iter)
		valid = gtk_tree_model_iter_children(model, &iter_, iter);
	else
		valid = gtk_tree_model_get_iter_first(model, &iter_);

	while (valid) {
		if (folderview_search_unread_recursive(model, &iter_) == TRUE)
			return TRUE;
		valid = gtk_tree_model_iter_next(model, &iter_);
	}

	return FALSE;
}

static void folderview_update_row(FolderView *folderview, GtkTreeIter *iter)
{
	GtkTreeStore *store = folderview->store;
	GtkTreeModel *model = GTK_TREE_MODEL(store);
	GtkTreePath *path;
	GtkTreeIter parent;
	FolderItem *item = NULL;
	GdkPixbuf *pixbuf, *open_pixbuf;
	gchar *name, *str;
	gchar new_s[11], unread_s[11], total_s[11];
	gboolean add_unread_mark;
	gboolean use_color;
	PangoWeight weight = PANGO_WEIGHT_NORMAL;
	GdkColor *foreground = NULL;

	gtk_tree_model_get(model, iter, COL_FOLDER_ITEM, &item, -1);
	g_return_if_fail(item != NULL);

	switch (item->stype) {
	case F_INBOX:
		pixbuf = open_pixbuf = inbox_pixbuf;
		name = g_strdup(FOLDER_IS_LOCAL(item->folder) &&
				!strcmp2(item->name, INBOX_DIR) ? _("Inbox") :
				item->name);
		break;
	case F_OUTBOX:
		pixbuf = open_pixbuf = outbox_pixbuf;
		name = g_strdup(FOLDER_IS_LOCAL(item->folder) &&
				!strcmp2(item->name, OUTBOX_DIR) ? _("Sent") :
				item->name);
		break;
	case F_QUEUE:
		pixbuf = open_pixbuf = outbox_pixbuf;
		name = g_strdup(FOLDER_IS_LOCAL(item->folder) &&
				!strcmp2(item->name, QUEUE_DIR) ? _("Queue") :
				item->name);
		break;
	case F_TRASH:
		pixbuf = open_pixbuf = trash_pixbuf;
		name = g_strdup(FOLDER_IS_LOCAL(item->folder) &&
				!strcmp2(item->name, TRASH_DIR) ? _("Trash") :
				item->name);
		break;
	case F_DRAFT:
		pixbuf = open_pixbuf = draft_pixbuf;
		name = g_strdup(FOLDER_IS_LOCAL(item->folder) &&
				!strcmp2(item->name, DRAFT_DIR) ? _("Drafts") :
				item->name);
		break;
	case F_JUNK:
		pixbuf = open_pixbuf = junk_pixbuf;
		name = g_strdup(FOLDER_IS_LOCAL(item->folder) &&
				!strcmp2(item->name, JUNK_DIR) ? _("Junk") :
				item->name);
		break;
	case F_VIRTUAL:
		pixbuf = open_pixbuf = virtual_pixbuf;
		name = g_strdup(item->name);
		break;
	default:
		if (item->no_select) {
			pixbuf = open_pixbuf = foldernoselect_pixbuf;
		} else {
			pixbuf = folder_pixbuf;
			open_pixbuf = folderopen_pixbuf;
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

	path = gtk_tree_model_get_path(model, iter);
	if (!gtk_tree_view_row_expanded
		(GTK_TREE_VIEW(folderview->treeview), path) &&
	    folderview_have_unread_children(folderview, iter))
		add_unread_mark = TRUE;
	else
		add_unread_mark = FALSE;
	gtk_tree_path_free(path);

	if (item->stype == F_QUEUE && item->total > 0 &&
	    folderview->display_folder_unread) {
		str = g_strdup_printf("%s (%d%s)", name, item->total,
				      add_unread_mark ? "+" : "");
		g_free(name);
		name = str;
	} else if ((item->unread > 0 || add_unread_mark) &&
		   folderview->display_folder_unread) {
		if (item->unread > 0)
			str = g_strdup_printf("%s (%d%s)", name, item->unread,
					      add_unread_mark ? "+" : "");
		else
			str = g_strdup_printf("%s (+)", name);
		g_free(name);
		name = str;
	}

	if (!item->parent) {
		strcpy(new_s, "-");
		strcpy(unread_s, "-");
		strcpy(total_s, "-");
	} else {
		itos_buf(new_s, item->new);
		itos_buf(unread_s, item->unread);
		itos_buf(total_s, item->total);
	}

	if (item->stype == F_OUTBOX || item->stype == F_DRAFT ||
	    item->stype == F_TRASH || item->stype == F_JUNK) {
		use_color = FALSE;
		if (item->stype == F_JUNK) {
			if ((item->unread > 0) || add_unread_mark)
				weight = PANGO_WEIGHT_BOLD;
		}
	} else if (item->stype == F_QUEUE) {
		/* highlight queue folder if there are any messages */
		use_color = (item->total > 0);
		if (item->total > 0)
			weight = PANGO_WEIGHT_BOLD;
	} else {
		/* if unread messages exist, print with bold font */
		if ((item->unread > 0) || add_unread_mark)
			weight = PANGO_WEIGHT_BOLD;
		/* if new messages exist, print with colored letter */
		use_color =
			(item->new > 0) ||
			(add_unread_mark &&
			 folderview_have_new_children(folderview, iter));
	}

	if (item->no_select)
		foreground = &folderview->color_noselect;
	else if (use_color)
		foreground = &folderview->color_new;

	gtk_tree_store_set(store, iter,
			   COL_FOLDER_NAME, name,
			   COL_NEW, new_s,
			   COL_UNREAD, unread_s,
			   COL_TOTAL, total_s,
			   COL_FOLDER_ITEM, item,
			   COL_PIXBUF, pixbuf,
			   COL_PIXBUF_OPEN, open_pixbuf,
			   COL_FOREGROUND, foreground,
			   COL_BOLD, weight,
			   -1);
	/* g_print("folderview_update_row: %s: %s\n", item->path, name); */
	g_free(name);

	item->updated = FALSE;

	if (gtkut_tree_view_find_collapsed_parent
		(GTK_TREE_VIEW(folderview->treeview), &parent, iter))
		folderview_update_row(folderview, &parent);
}

static void folderview_update_row_all(FolderView *folderview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(folderview->store);
	GtkTreeIter iter;
	gboolean valid;

	valid = gtk_tree_model_get_iter_first(model, &iter);

	while (valid) {
		folderview_update_row(folderview, &iter);
		valid = gtkut_tree_model_next(model, &iter);
	}
}

void folderview_update_item(FolderItem *item, gboolean update_summary)
{
	FolderView *folderview;
	GtkTreeIter iter;

	g_return_if_fail(item != NULL);

	folderview = folderview_get();

	if (gtkut_tree_model_find_by_column_data
		(GTK_TREE_MODEL(folderview->store), &iter, NULL,
		 COL_FOLDER_ITEM, item)) {
		folderview_update_row(folderview, &iter);
		if (update_summary &&
		    folderview->summaryview->folder_item == item)
			summary_show(folderview->summaryview, item, FALSE);
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

static gboolean folderview_insert_item_recursive(FolderView *folderview,
						 FolderItem *item,
						 GtkTreeIter *iter)
{
	GNode *node;
	GtkTreeIter iter_;
	gboolean valid;

	g_return_val_if_fail(item != NULL, FALSE);

	valid = folderview_append_item(folderview, &iter_, item, FALSE);
	g_return_val_if_fail(valid == TRUE, FALSE);

	for (node = item->node->children; node != NULL; node = node->next) {
		FolderItem *child_item = FOLDER_ITEM(node->data);
		folderview_insert_item_recursive(folderview, child_item, NULL);
	}

	if (item->node->children && !item->collapsed) {
		GtkTreePath *path;

		path = gtk_tree_model_get_path
			(GTK_TREE_MODEL(folderview->store), &iter_);
		gtk_tree_view_expand_row(GTK_TREE_VIEW(folderview->treeview),
					 path, FALSE);
		gtk_tree_path_free(path);
	}

	if (iter)
		*iter = iter_;
	return TRUE;
}

static void folderview_append_folder(FolderView *folderview, Folder *folder)
{
	g_return_if_fail(folder != NULL);

	folderview_insert_item_recursive
		(folderview, FOLDER_ITEM(folder->node->data), NULL);
}

void folderview_new_folder(FolderView *folderview)
{
	FolderItem *item;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	g_return_if_fail(item->folder != NULL);

	if (item->folder->klass->create_folder)
		folderview_new_folder_cb(folderview, 0, NULL);
	else if (FOLDER_TYPE(item->folder) == F_NEWS)
		folderview_new_news_group_cb(folderview, 0, NULL);
}

void folderview_rename_folder(FolderView *folderview)
{
	FolderItem *item;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	g_return_if_fail(item->folder != NULL);

	if (!item->path) return;
	if (item->stype != F_NORMAL) return;

	if (item->folder->klass->rename_folder)
		folderview_rename_folder_cb(folderview, 0, NULL);
}

void folderview_move_folder(FolderView *folderview)
{
	FolderItem *item;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	g_return_if_fail(item->folder != NULL);

	if (!item->path) return;
	if (item->stype != F_NORMAL && item->stype != F_VIRTUAL) return;

	if (item->folder->klass->move_folder)
		folderview_move_folder_cb(folderview, 0, NULL);
}

void folderview_delete_folder(FolderView *folderview)
{
	FolderItem *item;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	g_return_if_fail(item->folder != NULL);

	if (!item->path) return;
	if (item->stype != F_NORMAL) return;

	if (item->folder->klass->remove_folder)
		folderview_delete_folder_cb(folderview, 0, NULL);
	else if (FOLDER_TYPE(item->folder) == F_NEWS)
		folderview_rm_news_group_cb(folderview, 0, NULL);
}

void folderview_check_new_selected(FolderView *folderview)
{
	FolderItem *item;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	g_return_if_fail(item->folder != NULL);
	if (item->parent != NULL) return;

	folderview_check_new(item->folder);
}

void folderview_remove_mailbox(FolderView *folderview)
{
	FolderItem *item;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

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
	FolderItem *item;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	g_return_if_fail(item->folder != NULL);
	if (item->parent != NULL) return;

	folderview_rescan_tree(folderview, item->folder);
}

static gboolean folderview_menu_popup(FolderView *folderview,
				      GdkEventButton *event)
{
	FolderItem *item = NULL;
	Folder *folder;
	GtkWidget *popup;
	GtkItemFactory *ifactory;
	GtkTreeModel *model = GTK_TREE_MODEL(folderview->store);
	GtkTreeIter iter;
	gboolean new_folder      = FALSE;
	gboolean rename_folder   = FALSE;
	gboolean move_folder     = FALSE;
	gboolean delete_folder   = FALSE;
	gboolean empty_junk      = FALSE;
	gboolean empty_trash     = FALSE;
	gboolean download_msg    = FALSE;
	gboolean update_tree     = FALSE;
	gboolean update_summary  = FALSE;
	gboolean mark_all_read   = FALSE;
	gboolean send_queue      = FALSE;
	gboolean rescan_tree     = FALSE;
	gboolean remove_tree     = FALSE;
	gboolean search_folder   = FALSE;
	gboolean folder_property = FALSE;

	if (!gtk_tree_selection_get_selected
		(folderview->selection, NULL, &iter))
		return FALSE;

	gtk_tree_model_get(model, &iter, COL_FOLDER_ITEM, &item, -1);
	g_return_val_if_fail(item != NULL, FALSE);
	g_return_val_if_fail(item->folder != NULL, FALSE);
	folder = item->folder;

	if (folderview->mainwin->lock_count == 0) {
		new_folder = TRUE;
		folder_property = TRUE;
		search_folder = TRUE;
		if (item->parent == NULL) {
			update_tree = remove_tree = TRUE;
		} else {
			if (FOLDER_TYPE(folder) != F_IMAP)
				mark_all_read = TRUE;
			if (gtkut_tree_row_reference_equal
				(folderview->selected, folderview->opened)) {
				update_summary = TRUE;
				mark_all_read = TRUE;
			}
		}
		if (FOLDER_IS_LOCAL(folder) || FOLDER_TYPE(folder) == F_IMAP) {
			if (item->parent == NULL)
				update_tree = rescan_tree = TRUE;
			else if (item->stype == F_NORMAL) {
				rename_folder = delete_folder = TRUE;
				if (folder->klass->move_folder)
					move_folder = TRUE;
			} else if (item->stype == F_TRASH) {
				if (item->total > 0)
					empty_trash = TRUE;
			} else if (item->stype == F_JUNK) {
				if (item->total > 0)
					empty_junk = TRUE;
			} else if (item->stype == F_QUEUE) {
				if (item->total > 0)
					send_queue = TRUE;
			}
		} else if (FOLDER_TYPE(folder) == F_NEWS) {
			if (item->parent != NULL)
				delete_folder = TRUE;
		}
		if (item->stype == F_VIRTUAL) {
			new_folder = FALSE;
			move_folder = rename_folder = delete_folder = TRUE;
		}
		if (FOLDER_TYPE(folder) == F_IMAP ||
		    FOLDER_TYPE(folder) == F_NEWS) {
			if (item->no_select == FALSE &&
			    item->stype != F_VIRTUAL)
				download_msg = TRUE;
		}
	} else {
		search_folder = TRUE;
		if (item->parent) {
			if (FOLDER_TYPE(folder) != F_IMAP)
				mark_all_read = TRUE;
			if (gtkut_tree_row_reference_equal
				(folderview->selected, folderview->opened)) {
				update_summary = TRUE;
				mark_all_read = TRUE;
			}
			if (item->stype == F_TRASH) {
				if (item->total > 0)
					empty_trash = TRUE;
			} else if (item->stype == F_JUNK) {
				if (item->total > 0)
					empty_junk = TRUE;
			}
		}
	}

#define SET_SENS(factory, name, sens)				\
{								\
	GtkWidget *widget;					\
	widget = gtk_item_factory_get_item(factory, name);	\
	if (widget)						\
		gtk_widget_set_sensitive(widget, sens);		\
}

#define SET_VISIBILITY(factory, name, visible)			\
{								\
	GtkWidget *widget;					\
	widget = gtk_item_factory_get_item(factory, name);	\
	if (widget) {						\
		if (visible)					\
			gtk_widget_show(widget);		\
		else						\
			gtk_widget_hide(widget);		\
	}							\
}

#define SET_VISIBILITY2(factory, name, visible)			\
{								\
	GtkWidget *widget;					\
	widget = gtk_item_factory_get_item(factory, name);	\
	if (widget) {						\
		GList *child;					\
		GtkWidget *sep = NULL;				\
								\
		child = g_list_find				\
			(GTK_MENU_SHELL(popup)->children, widget); \
		if (child && child->next)			\
			sep = GTK_WIDGET(child->next->data);	\
		if (visible) {					\
			gtk_widget_show(widget);		\
			gtk_widget_show(sep);			\
		} else {					\
			gtk_widget_hide(widget);		\
			gtk_widget_hide(sep);			\
		}						\
	}							\
}

	if (FOLDER_IS_LOCAL(folder)) {
		popup = folderview->mail_popup;
		ifactory = folderview->mail_factory;
	} else if (FOLDER_TYPE(folder) == F_IMAP) {
		popup = folderview->imap_popup;
		ifactory = folderview->imap_factory;
	} else if (FOLDER_TYPE(folder) == F_NEWS) {
		popup = folderview->news_popup;
		ifactory = folderview->news_factory;
	} else
		return FALSE;

	menu_set_insensitive_all(GTK_MENU_SHELL(popup));

	SET_SENS(ifactory, "/Create new folder...", new_folder);
	SET_SENS(ifactory, "/Rename folder...", rename_folder);
	SET_SENS(ifactory, "/Move folder...", move_folder);
	SET_SENS(ifactory, "/Delete folder", delete_folder);
	SET_SENS(ifactory, "/Empty junk", empty_junk);
	SET_SENS(ifactory, "/Empty trash", empty_trash);
	SET_SENS(ifactory, "/Download", download_msg);
	SET_SENS(ifactory, "/Check for new messages", update_tree);
	SET_SENS(ifactory, "/Rebuild folder tree", rescan_tree);
	SET_SENS(ifactory, "/Update summary", update_summary);
	SET_SENS(ifactory, "/Mark all read", mark_all_read);
	SET_SENS(ifactory, "/Send queued messages", send_queue);
	SET_SENS(ifactory, "/Search messages...", search_folder);
	SET_SENS(ifactory, "/Edit search condition...", search_folder);
	SET_SENS(ifactory, "/Properties...", folder_property);

	if (FOLDER_TYPE(folder) == F_NEWS) {
		SET_SENS(ifactory, "/Subscribe to newsgroup...", new_folder);
		SET_SENS(ifactory, "/Remove newsgroup", delete_folder);
		SET_VISIBILITY(ifactory, "/Remove newsgroup",
			       item->stype != F_VIRTUAL);
		SET_VISIBILITY(ifactory, "/Rename folder...",
			       item->stype == F_VIRTUAL);
		SET_VISIBILITY(ifactory, "/Delete folder",
			       item->stype == F_VIRTUAL);
	}

	if (item->stype == F_JUNK) {
		SET_VISIBILITY(ifactory, "/Empty junk", TRUE);
		SET_VISIBILITY2(ifactory, "/Empty trash", TRUE);
		SET_VISIBILITY(ifactory, "/Empty trash", FALSE);
	} else {
		SET_VISIBILITY(ifactory, "/Empty junk", FALSE);
		SET_VISIBILITY2(ifactory, "/Empty trash",
				item->stype == F_TRASH);
	}

	SET_VISIBILITY(ifactory, "/Check for new messages",
		       item->parent == NULL);
	SET_VISIBILITY(ifactory, "/Rebuild folder tree", item->parent == NULL);
	SET_VISIBILITY(ifactory, "/Update summary", item->parent != NULL);

	if (FOLDER_TYPE(folder) == F_NEWS) {
		SET_VISIBILITY2(ifactory, "/Mark all read",
				item->parent != NULL && item->stype != F_QUEUE);
	} else {
		SET_VISIBILITY(ifactory, "/Mark all read",
			       item->parent != NULL && item->stype != F_QUEUE);
		if (item->parent != NULL) {
			SET_VISIBILITY2(ifactory, "/Send queued messages", TRUE);
			SET_VISIBILITY(ifactory, "/Send queued messages",
				       item->stype == F_QUEUE);
		} else {
			SET_VISIBILITY2(ifactory, "/Send queued messages",
					item->stype == F_QUEUE);
		}
	}

	SET_VISIBILITY(ifactory, "/Search messages...",
		       item->stype != F_VIRTUAL);
	SET_VISIBILITY(ifactory, "/Edit search condition...",
		       item->stype == F_VIRTUAL);

#undef SET_SENS
#undef SET_VISIBILITY
#undef SET_VISIBILITY2

	syl_plugin_signal_emit("folderview-menu-popup", ifactory);

	if (event)
		gtk_menu_popup(GTK_MENU(popup), NULL, NULL, NULL, NULL,
			       event->button, event->time);
	else
		gtk_menu_popup(GTK_MENU(popup), NULL, NULL,
			       menu_widget_position, folderview->treeview,
			       0, GDK_CURRENT_TIME);

	return FALSE;
}


/* callback functions */

static gboolean folderview_button_pressed(GtkWidget *widget,
					  GdkEventButton *event,
					  FolderView *folderview)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(widget);
	GtkTreePath *path;

	if (!event)
		return FALSE;

	if (!gtk_tree_view_get_path_at_pos(treeview, event->x, event->y,
					   &path, NULL, NULL, NULL))
		return TRUE;

	if (folderview->selection_locked ||
	    summary_is_locked(folderview->summaryview))
		return TRUE;

	if (event->button == 1 || event->button == 2) {
		if (event->type == GDK_2BUTTON_PRESS) {
			if (gtk_tree_view_row_expanded(treeview, path))
				gtk_tree_view_collapse_row(treeview, path);
			else
				gtk_tree_view_expand_row(treeview, path, FALSE);
		}
		folderview->open_folder = TRUE;
	} else if (event->button == 3) {
		if (folderview->selected) {
			folderview->prev_selected =
				gtk_tree_row_reference_copy
					(folderview->selected);
		}
		gtk_tree_selection_select_path(folderview->selection, path);
		folderview_menu_popup(folderview, event);
		gtk_tree_path_free(path);
		return TRUE;
	}

	gtk_tree_path_free(path);
	return FALSE;
}

static gboolean folderview_button_released(GtkWidget *treeview,
					   GdkEventButton *event,
					   FolderView *folderview)
{
	folderview->open_folder = FALSE;
	return FALSE;
}

static gboolean folderview_key_pressed(GtkWidget *widget, GdkEventKey *event,
				       FolderView *folderview)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(widget);
	GtkTreePath *opened = NULL, *selected = NULL;
	GtkAdjustment *adj;
	gboolean moved;

	if (!event) return FALSE;

	if (folderview->selection_locked ||
	    summary_is_locked(folderview->summaryview))
		return TRUE;

	switch (event->keyval) {
	case GDK_Return:
	case GDK_KP_Enter:
		if (folderview->selected) {
			folderview_select_row_ref(folderview,
						  folderview->selected);
		}
		return TRUE;
	case GDK_space:
	case GDK_KP_Space:
		if (folderview->selected) {
			if (folderview->opened)
				opened = gtk_tree_row_reference_get_path
					(folderview->opened);
			selected = gtk_tree_row_reference_get_path
				(folderview->selected);
			if (opened && selected &&
			    gtk_tree_path_compare(opened, selected) == 0 &&
			    (!folderview->summaryview->folder_item ||
			     folderview->summaryview->folder_item->total == 0))
				folderview_select_next_unread(folderview);
			else
				folderview_select_row_ref(folderview,
							  folderview->selected);
			gtk_tree_path_free(selected);
			gtk_tree_path_free(opened);
			return TRUE;
		}
		break;
	case GDK_Left:
	case GDK_KP_Left:
		if ((event->state &
		     (GDK_SHIFT_MASK|GDK_MOD1_MASK|GDK_CONTROL_MASK)) != 0)
			return FALSE;
		adj = gtk_scrolled_window_get_hadjustment
			(GTK_SCROLLED_WINDOW(folderview->scrolledwin));
		if (adj->lower < adj->value)
			return FALSE;
		if (folderview->selected) {
			selected = gtk_tree_row_reference_get_path
				(folderview->selected);
			if (selected) {
				if (gtk_tree_view_row_expanded(treeview, selected)) {
					gtk_tree_view_collapse_row(treeview, selected);
					gtk_tree_path_free(selected);
					return TRUE;
				}
				gtk_tree_path_free(selected);
			}
		}
		g_signal_emit_by_name(G_OBJECT(treeview),
				      "select-cursor-parent", &moved);
		return TRUE;
	case GDK_Right:
	case GDK_KP_Right:
		if ((event->state &
		     (GDK_SHIFT_MASK|GDK_MOD1_MASK|GDK_CONTROL_MASK)) != 0)
			return FALSE;
		adj = gtk_scrolled_window_get_hadjustment
			(GTK_SCROLLED_WINDOW(folderview->scrolledwin));
		if (adj->upper - adj->page_size > adj->value)
			return FALSE;
		if (folderview->selected) {
			selected = gtk_tree_row_reference_get_path
				(folderview->selected);
			if (selected) {
				if (!gtk_tree_view_row_expanded(treeview, selected)) {
					gtk_tree_view_expand_row(treeview, selected, FALSE);
					gtk_tree_path_free(selected);
					return TRUE;
				}
				gtk_tree_path_free(selected);
			}
		}
		break;
	case GDK_F10:
		if ((event->state & GDK_SHIFT_MASK) != 0) {
			folderview_menu_popup(folderview, NULL);
			return TRUE;
		}
		break;
	case GDK_Menu:
		folderview_menu_popup(folderview, NULL);
		return TRUE;
	default:
		break;
	}

	return FALSE;
}

static gboolean folderview_focus_idle_func(gpointer data)
{
	FolderView *folderview = (FolderView *)data;

	gdk_threads_enter();
	GTK_WIDGET_SET_FLAGS(folderview->treeview, GTK_CAN_FOCUS);
	gdk_threads_leave();

	return FALSE;
}

static void folderview_selection_changed(GtkTreeSelection *selection,
					 FolderView *folderview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(folderview->store);
	FolderItem *item = NULL;
	GtkTreeIter iter;
	GtkTreePath *path;
	gboolean opened;

	if (!gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		if (folderview->selected) {
			gtk_tree_row_reference_free(folderview->selected);
			folderview->selected = NULL;
		}
		return;
	}

	path = gtk_tree_model_get_path(model, &iter);

	gtk_tree_row_reference_free(folderview->selected);
	folderview->selected = gtk_tree_row_reference_new(model, path);

	main_window_set_menu_sensitive(folderview->mainwin);

	if (!folderview->open_folder) {
		gtk_tree_path_free(path);
		return;
	}
	folderview->open_folder = FALSE;

	gtk_tree_model_get(model, &iter, COL_FOLDER_ITEM, &item, -1);
	if (!item) {
		gtk_tree_path_free(path);
		return;
	}

	if (item->path)
		debug_print(_("Folder %s is selected\n"), item->path);

	if (summary_is_locked(folderview->summaryview)) {
		gtk_tree_path_free(path);
		return;
	}

	if (folderview->opened) {
		GtkTreePath *open_path = NULL;

		open_path = gtk_tree_row_reference_get_path(folderview->opened);
		if (open_path && gtk_tree_path_compare(open_path, path) == 0) {
			gtk_tree_path_free(open_path);
			gtk_tree_path_free(path);
			return;
		}
		gtk_tree_path_free(open_path);
	}

	folderview->selection_locked = TRUE;

	GTK_EVENTS_FLUSH();
	opened = summary_show(folderview->summaryview, item, FALSE);

	if (opened) {
		gtk_tree_row_reference_free(folderview->opened);
		folderview->opened = gtk_tree_row_reference_new(model, path);
		gtk_tree_view_scroll_to_cell
			(GTK_TREE_VIEW(folderview->treeview), path, NULL, FALSE,
			 0.0, 0.0);
		if (item->total > 0) {
			/* don't let GtkTreeView::gtk_tree_view_button_press()
			 * grab focus */
			GTK_WIDGET_UNSET_FLAGS(folderview->treeview,
					       GTK_CAN_FOCUS);
			g_idle_add(folderview_focus_idle_func, folderview);
		}
	} else
		folderview_select_row_ref(folderview, folderview->opened);

	gtk_tree_path_free(path);

	folderview->selection_locked = FALSE;

	if (prefs_common.change_account_on_folder_sel) {
		PrefsAccount *account;

		account = account_find_from_item_property(item);
		if (!account && item->folder)
			account = item->folder->account;
		if (!account)
			account = account_get_default();
		if (account && account != cur_account) {
			cur_account = account;
			main_window_change_cur_account();
		}
	}
}

static void folderview_row_expanded(GtkTreeView *treeview, GtkTreeIter *iter,
				    GtkTreePath *path, FolderView *folderview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(folderview->store);
	FolderItem *item = NULL;
	GtkTreeIter iter_;
	gboolean valid;

	folderview->open_folder = FALSE;

	gtk_tree_model_get(GTK_TREE_MODEL(folderview->store), iter,
			   COL_FOLDER_ITEM, &item, -1);
	g_return_if_fail(item != NULL);
	item->collapsed = FALSE;
	folderview_update_row(folderview, iter);

	valid = gtk_tree_model_iter_children(model, &iter_, iter);

	while (valid) {
		FolderItem *child_item = NULL;

		gtk_tree_model_get(model, &iter_, COL_FOLDER_ITEM, &child_item,
				   -1);
		if (child_item && child_item->node->children &&
		    !child_item->collapsed) {
			GtkTreePath *path;

			path = gtk_tree_model_get_path(model, &iter_);
			gtk_tree_view_expand_row
				(GTK_TREE_VIEW(folderview->treeview),
				 path, FALSE);
			gtk_tree_path_free(path);
		}
		valid = gtk_tree_model_iter_next(model, &iter_);
	}
}

static void folderview_row_collapsed(GtkTreeView *treeview, GtkTreeIter *iter,
				     GtkTreePath *path, FolderView *folderview)
{
	FolderItem *item = NULL;

	folderview->open_folder = FALSE;

	gtk_tree_model_get(GTK_TREE_MODEL(folderview->store), iter,
			   COL_FOLDER_ITEM, &item, -1);
	g_return_if_fail(item != NULL);
	item->collapsed = TRUE;
	folderview_update_row(folderview, iter);
}

static void folderview_popup_close(GtkMenuShell *menu_shell,
				   FolderView *folderview)
{
	GtkTreePath *path;

	if (!folderview->prev_selected) return;

	path = gtk_tree_row_reference_get_path(folderview->prev_selected);
	gtk_tree_row_reference_free(folderview->prev_selected);
	folderview->prev_selected = NULL;
	if (!path)
		return;
	gtk_tree_selection_select_path(folderview->selection, path);
	gtk_tree_path_free(path);
}

static void folderview_col_resized(GtkWidget *widget, GtkAllocation *allocation,
				   FolderView *folderview)
{
	GtkTreeViewColumn *column;
	gint type;
	gint width = allocation->width;

	for (type = 0; type <= COL_TOTAL; type++) {
		column = gtk_tree_view_get_column
			(GTK_TREE_VIEW(folderview->treeview), type);
		if (column && column->button == widget) {
			switch (type) {
			case COL_FOLDER_NAME:
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
			break;
		}
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
	MainWindow *mainwin = folderview->mainwin;
	FolderItem *item;
	gint ret = 0;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;
	if (item->stype == F_VIRTUAL)
		return;

	g_return_if_fail(item->folder != NULL);

	if (item->parent == NULL) {
		gchar *name, *msg;

		name = trim_string(item->name, 32);
		msg = g_strdup_printf(_("Download all messages under '%s' ?"),
				      name);
		g_free(name);
		if (alertpanel(_("Download all messages"), msg,
			       GTK_STOCK_YES, GTK_STOCK_NO, NULL)
		    != G_ALERTDEFAULT) {
			g_free(msg);
			return;
		}
		g_free(msg);
	}

	if (!main_window_toggle_online_if_offline(folderview->mainwin))
		return;

	main_window_cursor_wait(mainwin);
	inc_lock();
	main_window_lock(mainwin);
	gtk_widget_set_sensitive(folderview->treeview, FALSE);
	main_window_progress_on(mainwin);
	GTK_EVENTS_FLUSH();
	folder_set_ui_func(item->folder, folderview_download_func, NULL);

	if (item->parent == NULL) {
		GtkTreeModel *model = GTK_TREE_MODEL(folderview->store);
		GtkTreeIter iter;
		gboolean valid;
		FolderItem *cur_item;

		valid = gtkut_tree_model_find_by_column_data
			(model, &iter, NULL, COL_FOLDER_ITEM, item);
		while ((valid = gtkut_tree_model_next(model, &iter)) == TRUE) {
			cur_item = NULL;
			gtk_tree_model_get(model, &iter, COL_FOLDER_ITEM,
					   &cur_item, -1);
			if (!cur_item || cur_item->folder != item->folder)
				break;
			if (!cur_item->no_select &&
			    cur_item->stype != F_VIRTUAL &&
			    cur_item->stype != F_TRASH) {
				ret = folder_item_fetch_all_msg(cur_item);
				if (ret < 0)
					break;
			}
		}
	} else
		ret = folder_item_fetch_all_msg(item);

	if (ret < 0) {
		gchar *name;

		name = trim_string(item->name, 32);
		alertpanel_error(_("Error occurred while downloading messages in `%s'."), name);
		g_free(name);
	}

	folder_set_ui_func(item->folder, NULL, NULL);
	main_window_progress_off(mainwin);
	gtk_widget_set_sensitive(folderview->treeview, TRUE);
	main_window_unlock(mainwin);
	inc_unlock();
	main_window_cursor_normal(mainwin);
	statusbar_pop_all();
}

static void folderview_update_tree_cb(FolderView *folderview, guint action,
				      GtkWidget *widget)
{
	FolderItem *item;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	g_return_if_fail(item->folder != NULL);

	if (action == 0)
		folderview_check_new(item->folder);
	else
		folderview_rescan_tree(folderview, item->folder);
}

static void folderview_update_summary_cb(FolderView *folderview, guint action,
					 GtkWidget *widget)
{
	if (!folderview->summaryview->folder_item)
		return;

	GTK_EVENTS_FLUSH();
	summary_show(folderview->summaryview,
		     folderview->summaryview->folder_item, TRUE);
}

static void folderview_mark_all_read_cb(FolderView *folderview, guint action,
					GtkWidget *widget)
{
	FolderItem *item;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	if (item == folderview->summaryview->folder_item)
		summary_mark_all_read(folderview->summaryview);
	else {
		procmsg_mark_all_read(item);
		folderview_update_item(item, FALSE);
		trayicon_set_tooltip(NULL);
		trayicon_set_notify(FALSE);
	}
}

static void folderview_send_queue_cb(FolderView *folderview, guint action,
				     GtkWidget *widget)
{
	FolderItem *item;
	gint ret;

	if (!main_window_toggle_online_if_offline(folderview->mainwin))
		return;

	item = folderview_get_selected_item(folderview);
	if (!item || item->stype != F_QUEUE)
		return;

	ret = send_message_queue_all(item, prefs_common.savemsg,
				     prefs_common.filter_sent);
	statusbar_pop_all();
	if (ret > 0)
		folder_item_scan(item);

	folderview_update_item(item, TRUE);
	main_window_set_menu_sensitive(folderview->mainwin);
	main_window_set_toolbar_sensitive(folderview->mainwin);
}

static void folderview_new_folder_cb(FolderView *folderview, guint action,
				     GtkWidget *widget)
{
	FolderItem *item;
	FolderItem *new_item;
	gchar *new_folder;
	gchar *name;
	gchar *p;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

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

#ifdef G_OS_WIN32
	p = strpbrk(new_folder, "\\/:*?\"<>|");
	if ((p && FOLDER_TYPE(item->folder) != F_IMAP) || (p && *p != '/') ||
	    (p && *p == '/' &&
	     FOLDER_TYPE(item->folder) == F_IMAP && *(p + 1) != '\0')) {
		alertpanel_error(_("`%c' can't be included in folder name."),
				 *p);
		return;
	}
#else
	p = strchr(new_folder, G_DIR_SEPARATOR);
	if ((p && FOLDER_TYPE(item->folder) != F_IMAP) ||
	    (p && FOLDER_TYPE(item->folder) == F_IMAP && *(p + 1) != '\0')) {
		alertpanel_error(_("`%c' can't be included in folder name."),
				 G_DIR_SEPARATOR);
		return;
	}
#endif

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

	folderview_append_item(folderview, NULL, new_item, TRUE);
	folder_write_list();
}

static void folderview_rename_folder_cb(FolderView *folderview, guint action,
					GtkWidget *widget)
{
	FolderItem *item;
	gchar *new_folder;
	gchar *name;
	gchar *message;
	gchar *old_path;
	gchar *old_id;
	gchar *new_id;
	GtkTreePath *sel_path;
	GtkTreePath *open_path = NULL;
	GtkTreeIter iter;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	g_return_if_fail(item->path != NULL);
	g_return_if_fail(item->folder != NULL);

	sel_path = gtk_tree_row_reference_get_path(folderview->selected);

	name = trim_string(item->name, 32);
	message = g_strdup_printf(_("Input new name for `%s':"), name);
	new_folder = input_dialog(_("Rename folder"), message,
				  g_basename(item->path));
	g_free(message);
	g_free(name);
	if (!new_folder) {
		gtk_tree_path_free(sel_path);
		return;
	}
	AUTORELEASE_STR(new_folder, {g_free(new_folder); gtk_tree_path_free(sel_path); return;});

	if (strchr(new_folder, G_DIR_SEPARATOR) != NULL) {
		alertpanel_error(_("`%c' can't be included in folder name."),
				 G_DIR_SEPARATOR);
		gtk_tree_path_free(sel_path);
		return;
	}

	if (folder_find_child_item_by_name(item->parent, new_folder)) {
		name = trim_string(new_folder, 32);
		alertpanel_error(_("The folder `%s' already exists."), name);
		g_free(name);
		gtk_tree_path_free(sel_path);
		return;
	}

	old_path = g_strdup(item->path);
	old_id = folder_item_get_identifier(item);

	if (item->stype == F_VIRTUAL) {
		if (virtual_get_class()->rename_folder(item->folder, item,
						       new_folder) < 0) {
			alertpanel_error(_("Can't rename the folder '%s'."),
					 item->name);
			g_free(old_id);
			g_free(old_path);
			gtk_tree_path_free(sel_path);
			return;
		}
	} else if (item->folder->klass->rename_folder(item->folder, item,
						      new_folder) < 0) {
		alertpanel_error(_("Can't rename the folder '%s'."),
				 item->name);
		g_free(old_id);
		g_free(old_path);
		gtk_tree_path_free(sel_path);
		return;
	}

	if (folder_get_default_folder() == item->folder) {
		filter_list_rename_path(old_path, item->path);
		prefs_common_junk_folder_rename_path(old_path, item->path);
	}
	new_id = folder_item_get_identifier(item);
	filter_list_rename_path(old_id, new_id);
	prefs_common_junk_folder_rename_path(old_id, new_id);
	g_free(new_id);
	g_free(old_id);
	g_free(old_path);

	if (folderview->opened)
		open_path = gtk_tree_row_reference_get_path(folderview->opened);
	if (sel_path) {
		gtk_tree_model_get_iter(GTK_TREE_MODEL(folderview->store),
					&iter, sel_path);
		folderview_update_row(folderview, &iter);
	}
	if (sel_path && open_path &&
	    (gtk_tree_path_compare(open_path, sel_path) == 0 ||
	     gtk_tree_path_is_ancestor(sel_path, open_path))) {
		GtkTreeRowReference *row;

		row = gtk_tree_row_reference_copy(folderview->opened);
		folderview_unselect(folderview);
		folderview_select_row_ref(folderview, row);
		gtk_tree_row_reference_free(row);
	}
	gtk_tree_path_free(open_path);
	gtk_tree_path_free(sel_path);

	folder_write_list();
}

static void folderview_move_folder_cb(FolderView *folderview, guint action,
				      GtkWidget *widget)
{
	FolderItem *item;
	FolderItem *new_parent;
	GtkTreePath *sel_path;
	GtkTreePath *open_path = NULL;
	GtkTreeIter iter;
	gchar *old_path, *old_id, *new_id;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	g_return_if_fail(item->path != NULL);
	g_return_if_fail(item->folder != NULL);

	sel_path = gtk_tree_row_reference_get_path(folderview->selected);
	g_return_if_fail(sel_path != NULL);

	new_parent = foldersel_folder_sel(item->folder, FOLDER_SEL_MOVE_FOLDER,
					  NULL);
	if (!new_parent || new_parent->folder != item->folder ||
	    new_parent == item->parent || new_parent->stype == F_VIRTUAL) {
		gtk_tree_path_free(sel_path);
		return;
	}

	old_path = g_strdup(item->path);
	old_id = folder_item_get_identifier(item);

	if (item->folder->klass->move_folder
		(item->folder, item, new_parent) < 0) {
		alertpanel_error(_("Can't move the folder `%s'."), item->name);
		g_free(old_id);
		g_free(old_path);
		gtk_tree_path_free(sel_path);
		return;
	}

	if (folder_get_default_folder() == item->folder) {
		filter_list_rename_path(old_path, item->path);
		prefs_common_junk_folder_rename_path(old_path, item->path);
	}
	new_id = folder_item_get_identifier(item);
	filter_list_rename_path(old_id, new_id);
	prefs_common_junk_folder_rename_path(old_id, new_id);
	g_free(new_id);
	g_free(old_id);
	g_free(old_path);

	if (folderview->opened)
		open_path = gtk_tree_row_reference_get_path(folderview->opened);
	gtk_tree_model_get_iter(GTK_TREE_MODEL(folderview->store), &iter,
				sel_path);
	if (sel_path && open_path &&
	    (gtk_tree_path_compare(open_path, sel_path) == 0 ||
	     gtk_tree_path_is_ancestor(sel_path, open_path))) {
		summary_clear_all(folderview->summaryview);
		gtk_tree_row_reference_free(folderview->opened);
		folderview->opened = NULL;
	}
	gtk_tree_path_free(open_path);
	gtk_tree_path_free(sel_path);

	gtk_tree_store_remove(folderview->store, &iter);
	if (folderview_insert_item_recursive(folderview, item, &iter)) {
		gtkut_tree_view_expand_parent_all
			(GTK_TREE_VIEW(folderview->treeview), &iter);
	}

	folder_write_list();
}

static void folderview_delete_folder_cb(FolderView *folderview, guint action,
					GtkWidget *widget)
{
	Folder *folder;
	FolderItem *item;
	gchar *message, *name;
	AlertValue avalue;
	gchar *old_path;
	gchar *old_id;
	GtkTreePath *sel_path, *open_path = NULL;
	GtkTreeIter iter;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	g_return_if_fail(item->path != NULL);
	g_return_if_fail(item->folder != NULL);

	sel_path = gtk_tree_row_reference_get_path(folderview->selected);
	g_return_if_fail(sel_path != NULL);

	folder = item->folder;

	name = trim_string(item->name, 32);
	AUTORELEASE_STR(name, {g_free(name); gtk_tree_path_free(sel_path); return;});
	if (item->stype == F_VIRTUAL) {
		message = g_strdup_printf
			(_("Delete the search folder '%s' ?\n"
			   "The real messages are not deleted."), name);
		avalue = alertpanel_full(_("Delete search folder"), message,
					 ALERT_WARNING, G_ALERTALTERNATE, FALSE,
					 GTK_STOCK_YES, GTK_STOCK_NO, NULL);
	} else {
		message = g_strdup_printf
			(_("All folders and messages under '%s' will be permanently deleted.\n"
			   "Recovery will not be possible.\n\n"
			   "Do you really want to delete?"), name);
		avalue = alertpanel_full(_("Delete folder"), message,
					 ALERT_WARNING, G_ALERTALTERNATE, FALSE,
					 GTK_STOCK_YES, GTK_STOCK_NO, NULL);
	}
	g_free(message);
	if (avalue != G_ALERTDEFAULT) {
		gtk_tree_path_free(sel_path);
		return;
	}

	old_path = g_strdup(item->path);
	old_id = folder_item_get_identifier(item);

	if (folderview->opened)
		open_path = gtk_tree_row_reference_get_path(folderview->opened);
	gtk_tree_model_get_iter(GTK_TREE_MODEL(folderview->store), &iter,
				sel_path);
	if (sel_path && open_path &&
	    (gtk_tree_path_compare(open_path, sel_path) == 0 ||
	     gtk_tree_path_is_ancestor(sel_path, open_path))) {
		summary_clear_all(folderview->summaryview);
		gtk_tree_row_reference_free(folderview->opened);
		folderview->opened = NULL;
	}
	gtk_tree_path_free(open_path);
	gtk_tree_path_free(sel_path);

	if (item->stype == F_VIRTUAL) {
		if (virtual_get_class()->remove_folder(folder, item) < 0) {
			alertpanel_error(_("Can't remove the folder '%s'."),
					 name);
			g_free(old_id);
			g_free(old_path);
			return;
		}
	} else if (folder->klass->remove_folder(folder, item) < 0) {
		alertpanel_error(_("Can't remove the folder '%s'."), name);
		g_free(old_id);
		g_free(old_path);
		return;
	}

	if (folder_get_default_folder() == folder)
		filter_list_delete_path(old_path);
	filter_list_delete_path(old_id);
	g_free(old_id);
	g_free(old_path);

	gtk_tree_store_remove(folderview->store, &iter);

	folder_write_list();
}

static void folderview_empty_trash_cb(FolderView *folderview, guint action,
				      GtkWidget *widget)
{
	FolderItem *item;
	Folder *folder;
	GtkTreePath *sel_path, *open_path;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	g_return_if_fail(item->path != NULL);
	g_return_if_fail(item->folder != NULL);

	folder = item->folder;

	if (item->stype != F_TRASH && item->stype != F_JUNK) return;

	if (folderview->selection_locked ||
	    summary_is_locked(folderview->summaryview)) return;
	folderview->selection_locked = TRUE;

	sel_path = gtk_tree_row_reference_get_path(folderview->selected);

	if (item->stype == F_TRASH) {
		if (alertpanel(_("Empty trash"),
			       _("Delete all messages in the trash folder?"),
			       GTK_STOCK_YES, GTK_STOCK_NO, NULL) != G_ALERTDEFAULT) {
			gtk_tree_path_free(sel_path);
			folderview->selection_locked = FALSE;
			return;
		}
	} else {
		if (alertpanel(_("Empty junk"),
			       _("Delete all messages in the junk folder?"),
			       GTK_STOCK_YES, GTK_STOCK_NO, NULL) != G_ALERTDEFAULT) {
			gtk_tree_path_free(sel_path);
			folderview->selection_locked = FALSE;
			return;
		}
	}

	summary_lock(folderview->summaryview);
	procmsg_empty_trash(item);
	summary_unlock(folderview->summaryview);
	statusbar_pop_all();
	folderview_update_item(item, TRUE);
	trayicon_set_tooltip(NULL);
	trayicon_set_notify(FALSE);

	open_path = gtk_tree_row_reference_get_path(folderview->opened);
	if (open_path && sel_path &&
	    gtk_tree_path_compare(open_path, sel_path) == 0)
		gtk_widget_grab_focus(folderview->treeview);
	gtk_tree_path_free(open_path);
	gtk_tree_path_free(sel_path);

	folderview->selection_locked = FALSE;
}

static void folderview_remove_mailbox_cb(FolderView *folderview, guint action,
					 GtkWidget *widget)
{
	FolderItem *item;
	gchar *name;
	gchar *message;
	AlertValue avalue;
	GtkTreePath *sel_path;
	GtkTreeIter iter;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	g_return_if_fail(item->folder != NULL);
	if (item->parent) return;

	sel_path = gtk_tree_row_reference_get_path(folderview->selected);

	name = trim_string(item->folder->name, 32);
	message = g_strdup_printf
		(_("Really remove the mailbox `%s' ?\n"
		   "(The messages are NOT deleted from the disk)"), name);
	avalue = alertpanel_full(_("Remove mailbox"), message,
				 ALERT_WARNING, G_ALERTALTERNATE, FALSE,
				 GTK_STOCK_YES, GTK_STOCK_NO, NULL);
	g_free(message);
	g_free(name);
	if (avalue != G_ALERTDEFAULT) {
		gtk_tree_path_free(sel_path);
		return;
	}

	if (folderview->summaryview->folder_item &&
	    folderview->summaryview->folder_item->folder == item->folder) {
		summary_clear_all(folderview->summaryview);
		gtk_tree_row_reference_free(folderview->opened);
		folderview->opened = NULL;
	}
	folder_destroy(item->folder);

	if (sel_path) {
		gtk_tree_model_get_iter(GTK_TREE_MODEL(folderview->store),
					&iter, sel_path);
		gtk_tree_path_free(sel_path);
		gtk_tree_store_remove(folderview->store, &iter);
	}

	folder_write_list();
}

static void folderview_rm_imap_server_cb(FolderView *folderview, guint action,
					 GtkWidget *widget)
{
	FolderItem *item;
	PrefsAccount *account;
	gchar *name;
	gchar *message;
	AlertValue avalue;
	GtkTreePath *sel_path;
	GtkTreeIter iter;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(FOLDER_TYPE(item->folder) == F_IMAP);
	g_return_if_fail(item->folder->account != NULL);

	sel_path = gtk_tree_row_reference_get_path(folderview->selected);

	name = trim_string(item->folder->name, 32);
	message = g_strdup_printf(_("Really delete IMAP4 account `%s'?"), name);
	avalue = alertpanel_full(_("Delete IMAP4 account"), message,
				 ALERT_WARNING, G_ALERTALTERNATE, FALSE,
				 GTK_STOCK_YES, GTK_STOCK_NO, NULL);
	g_free(message);
	g_free(name);

	if (avalue != G_ALERTDEFAULT) {
		gtk_tree_path_free(sel_path);
		return;
	}

	if (folderview->summaryview->folder_item &&
	    folderview->summaryview->folder_item->folder == item->folder) {
		summary_clear_all(folderview->summaryview);
		gtk_tree_row_reference_free(folderview->opened);
		folderview->opened = NULL;
	}

	account = item->folder->account;
	folder_destroy(item->folder);
	account_destroy(account);
	account_write_config_all();

	if (sel_path) {
		gtk_tree_model_get_iter(GTK_TREE_MODEL(folderview->store),
					&iter, sel_path);
		gtk_tree_path_free(sel_path);
		gtk_tree_store_remove(folderview->store, &iter);
	}

	account_set_menu();
	main_window_reflect_prefs_all();
	folder_write_list();
}

static void folderview_new_news_group_cb(FolderView *folderview, guint action,
					 GtkWidget *widget)
{
	GtkTreeModel *model = GTK_TREE_MODEL(folderview->store);
	Folder *folder;
	FolderItem *item;
	FolderItem *rootitem = NULL;
	FolderItem *newitem;
	GSList *new_subscr;
	GSList *cur;
	GNode *gnode;
	GtkTreePath *server_path;
	GtkTreeIter iter, root;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	folder = item->folder;
	g_return_if_fail(folder != NULL);
	g_return_if_fail(FOLDER_TYPE(folder) == F_NEWS);
	g_return_if_fail(folder->account != NULL);

	server_path = gtk_tree_row_reference_get_path(folderview->selected);
	g_return_if_fail(server_path != NULL);
	gtk_tree_model_get_iter(model, &iter, server_path);
	gtk_tree_path_free(server_path);

	if (!gtk_tree_model_iter_parent(model, &root, &iter))
		root = iter;

	gtk_tree_model_get(model, &root, COL_FOLDER_ITEM, &rootitem, -1);

	new_subscr = subscribe_dialog(folder);

	/* remove unsubscribed newsgroups */
	for (gnode = folder->node->children; gnode != NULL; ) {
		GNode *next = gnode->next;
		GtkTreeIter found;

		item = FOLDER_ITEM(gnode->data);
		if (g_slist_find_custom(new_subscr, item->path,
					(GCompareFunc)g_ascii_strcasecmp)
		    != NULL) {
			gnode = next;
			continue;
		}

		if (!gtkut_tree_model_find_by_column_data
			(model, &found, &root, COL_FOLDER_ITEM, item)) {
			gnode = next;
			continue;
		}

		if (folderview->summaryview->folder_item == item) {
			summary_clear_all(folderview->summaryview);
			gtk_tree_row_reference_free(folderview->opened);
			folderview->opened = NULL;
		}

		folder_item_remove(item);
		gtk_tree_store_remove(folderview->store, &found);

		gnode = next;
	}

	/* add subscribed newsgroups */
	for (cur = new_subscr; cur != NULL; cur = cur->next) {
		gchar *name = (gchar *)cur->data;

		if (folder_find_child_item_by_name(rootitem, name) != NULL)
			continue;

		newitem = folder_item_new(name, name);
		folder_item_append(rootitem, newitem);
		folderview_append_item(folderview, NULL, newitem, TRUE);
	}

	if (new_subscr) {
		server_path = gtk_tree_model_get_path(model, &root);
		gtk_tree_view_expand_row(GTK_TREE_VIEW(folderview->treeview),
					 server_path, FALSE);
		gtk_tree_path_free(server_path);
	}

	slist_free_strings(new_subscr);
	g_slist_free(new_subscr);

	folder_write_list();
}

static void folderview_rm_news_group_cb(FolderView *folderview, guint action,
					GtkWidget *widget)
{
	FolderItem *item;
	gchar *name;
	gchar *message;
	AlertValue avalue;
	GtkTreePath *sel_path, *open_path = NULL;
	GtkTreeIter iter;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	if (item->stype == F_VIRTUAL) {
		folderview_delete_folder_cb(folderview, 0, widget);
		return;
	}

	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(FOLDER_TYPE(item->folder) == F_NEWS);
	g_return_if_fail(item->folder->account != NULL);

	sel_path = gtk_tree_row_reference_get_path(folderview->selected);
	g_return_if_fail(sel_path != NULL);

	name = trim_string_before(item->path, 32);
	message = g_strdup_printf(_("Really delete newsgroup `%s'?"), name);
	avalue = alertpanel_full(_("Delete newsgroup"), message,
				 ALERT_WARNING, G_ALERTALTERNATE, FALSE,
				 GTK_STOCK_YES, GTK_STOCK_NO, NULL);
	g_free(message);
	g_free(name);
	if (avalue != G_ALERTDEFAULT) {
		gtk_tree_path_free(sel_path);
		return;
	}

	if (folderview->opened)
		open_path = gtk_tree_row_reference_get_path(folderview->opened);
	gtk_tree_model_get_iter(GTK_TREE_MODEL(folderview->store), &iter,
				sel_path);
	if (open_path && sel_path &&
	    gtk_tree_path_compare(open_path, sel_path) == 0) {
		summary_clear_all(folderview->summaryview);
		gtk_tree_row_reference_free(folderview->opened);
		folderview->opened = NULL;
	}
	gtk_tree_path_free(open_path);
	gtk_tree_path_free(sel_path);

	folder_item_remove(item);
	gtk_tree_store_remove(folderview->store, &iter);
	folder_write_list();
}

static void folderview_rm_news_server_cb(FolderView *folderview, guint action,
					 GtkWidget *widget)
{
	FolderItem *item;
	PrefsAccount *account;
	gchar *name;
	gchar *message;
	AlertValue avalue;
	GtkTreePath *sel_path;
	GtkTreeIter iter;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(FOLDER_TYPE(item->folder) == F_NEWS);
	g_return_if_fail(item->folder->account != NULL);

	sel_path = gtk_tree_row_reference_get_path(folderview->selected);

	name = trim_string(item->folder->name, 32);
	message = g_strdup_printf(_("Really delete news account `%s'?"), name);
	avalue = alertpanel_full(_("Delete news account"), message,
				 ALERT_WARNING, G_ALERTALTERNATE, FALSE,
				 GTK_STOCK_YES, GTK_STOCK_NO, NULL);
	g_free(message);
	g_free(name);

	if (avalue != G_ALERTDEFAULT) {
		gtk_tree_path_free(sel_path);
		return;
	}

	if (folderview->summaryview->folder_item &&
	    folderview->summaryview->folder_item->folder == item->folder) {
		summary_clear_all(folderview->summaryview);
		gtk_tree_row_reference_free(folderview->opened);
		folderview->opened = NULL;
	}

	account = item->folder->account;
	folder_destroy(item->folder);
	account_destroy(account);
	account_write_config_all();

	if (sel_path) {
		gtk_tree_model_get_iter(GTK_TREE_MODEL(folderview->store),
					&iter, sel_path);
		gtk_tree_path_free(sel_path);
		gtk_tree_store_remove(folderview->store, &iter);
	}

	account_set_menu();
	main_window_reflect_prefs_all();
	folder_write_list();
}

static void folderview_search_cb(FolderView *folderview, guint action,
				 GtkWidget *widget)
{
	FolderItem *item;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	if (item->stype == F_VIRTUAL) {
		GtkTreePath *sel_path, *open_path;

		sel_path = gtk_tree_row_reference_get_path
			(folderview->selected);
		open_path = gtk_tree_row_reference_get_path(folderview->opened);

		if (prefs_search_folder_open(item)) {
			if (sel_path && open_path &&
			    gtk_tree_path_compare(open_path, sel_path) == 0) {
				GtkTreeRowReference *row;
				row = gtk_tree_row_reference_copy(folderview->opened);
				folderview_unselect(folderview);
				summary_clear_all(folderview->summaryview);
				folderview_select_row_ref(folderview, row);
				gtk_tree_row_reference_free(row);
			}
		}

		gtk_tree_path_free(open_path);
		gtk_tree_path_free(sel_path);
	} else
		query_search(item);
}

static void folderview_property_cb(FolderView *folderview, guint action,
				   GtkWidget *widget)
{
	FolderItem *item;

	item = folderview_get_selected_item(folderview);
	if (!item)
		return;

	g_return_if_fail(item->folder != NULL);

	if (item->parent == NULL && item->folder->account)
		account_open(item->folder->account);
	else
		prefs_folder_item_open(item);
}

static gint auto_expand_timeout(gpointer data)
{
	FolderView *folderview = data;
	GtkTreeView *treeview = GTK_TREE_VIEW(folderview->treeview);
	GtkTreePath *path = NULL;
	gint ret;

	gdk_threads_enter();

	gtk_tree_view_get_drag_dest_row(treeview, &path, NULL);

	if (path) {
		gtk_tree_view_expand_row(treeview, path, FALSE);
		gtk_tree_path_free(path);
		folderview->expand_timeout = 0;

		ret = FALSE;
	} else
		ret = TRUE;

	gdk_threads_leave();

	return ret;
}

static void remove_auto_expand_timeout(FolderView *folderview)
{
	if (folderview->expand_timeout != 0) {
		g_source_remove(folderview->expand_timeout);
		folderview->expand_timeout = 0;
	}
}

static gint auto_scroll_timeout(gpointer data)
{
	FolderView *folderview = data;

	gdk_threads_enter();

	gtkut_tree_view_vertical_autoscroll
		(GTK_TREE_VIEW(folderview->treeview));

	gdk_threads_leave();

	return TRUE;
}

static void remove_auto_scroll_timeout(FolderView *folderview)
{
	if (folderview->scroll_timeout != 0) {
		g_source_remove(folderview->scroll_timeout);
		folderview->scroll_timeout = 0;
	}
}

static gboolean folderview_drag_motion_cb(GtkWidget      *widget,
					  GdkDragContext *context,
					  gint            x,
					  gint            y,
					  guint           time,
					  FolderView     *folderview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(folderview->store);
	GtkTreePath *path = NULL, *prev_path = NULL;
	GtkTreeIter iter;
	FolderItem *item = NULL, *src_item = NULL;
	gboolean acceptable = FALSE;

	if (gtk_tree_view_get_dest_row_at_pos
		(GTK_TREE_VIEW(widget), x, y, &path, NULL)) {
		gtk_tree_model_get_iter(model, &iter, path);
		gtk_tree_model_get(model, &iter, COL_FOLDER_ITEM, &item, -1);
		src_item = folderview->summaryview->folder_item;
		if (src_item && src_item != item &&
		    src_item->stype != F_QUEUE && item->stype != F_QUEUE &&
		    item->stype != F_VIRTUAL)
			acceptable = FOLDER_ITEM_CAN_ADD(item);
	} else
		remove_auto_expand_timeout(folderview);

	if (summary_is_locked(folderview->summaryview))
		acceptable = FALSE;

	gtk_tree_view_get_drag_dest_row(GTK_TREE_VIEW(widget),
					&prev_path, NULL);
	if (!path || (prev_path && gtk_tree_path_compare(path, prev_path) != 0))
		remove_auto_expand_timeout(folderview);
	if (prev_path)
		gtk_tree_path_free(prev_path);

	gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW(widget), path,
					GTK_TREE_VIEW_DROP_INTO_OR_AFTER);

	if (path) {
		if (folderview->expand_timeout == 0) {
			folderview->expand_timeout =
				g_timeout_add(1000, auto_expand_timeout,
					      folderview);
		} else if (folderview->scroll_timeout == 0) {
			folderview->scroll_timeout =
				g_timeout_add(150, auto_scroll_timeout,
					      folderview);
		}
	}

#ifdef G_OS_WIN32
	/* Win32 hack: somehow context->actions is not properly set on Win32 */
	{
		GdkWindow *rootwin;
		GdkModifierType state;

		rootwin = gtk_widget_get_root_window(widget);
		gdk_window_get_pointer(rootwin, NULL, NULL, &state);
		if ((state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) == 0)
			context->actions = GDK_ACTION_MOVE | GDK_ACTION_COPY;
	}
#endif

	if (acceptable) {
		if ((context->actions & GDK_ACTION_MOVE) != 0 &&
		    FOLDER_ITEM_CAN_ADD(src_item))
			gdk_drag_status(context, GDK_ACTION_MOVE, time);
		else if ((context->actions & GDK_ACTION_COPY) != 0)
			gdk_drag_status(context, GDK_ACTION_COPY, time);
		else if ((context->actions & GDK_ACTION_LINK) != 0)
			gdk_drag_status(context, GDK_ACTION_LINK, time);
		else
			gdk_drag_status(context, 0, time);
	} else
		gdk_drag_status(context, 0, time);

	if (path)
		gtk_tree_path_free(path);

	return TRUE;
}

static void folderview_drag_leave_cb(GtkWidget      *widget,
				     GdkDragContext *context,
				     guint           time,
				     FolderView     *folderview)
{
	remove_auto_expand_timeout(folderview);
	remove_auto_scroll_timeout(folderview);

	gtk_tree_view_set_drag_dest_row
		(GTK_TREE_VIEW(widget), NULL, GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
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
	GtkTreeModel *model = GTK_TREE_MODEL(folderview->store);
	GtkTreePath *path = NULL;
	GtkTreeIter iter;
	FolderItem *item = NULL, *src_item;

	remove_auto_expand_timeout(folderview);
	remove_auto_scroll_timeout(folderview);

	if (!gtk_tree_view_get_dest_row_at_pos
		(GTK_TREE_VIEW(widget), x, y, &path, NULL))
		return;

	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_model_get(model, &iter, COL_FOLDER_ITEM, &item, -1);
	src_item = folderview->summaryview->folder_item;

	if (FOLDER_ITEM_CAN_ADD(item) && src_item && src_item != item &&
	    src_item->stype != F_QUEUE && item->stype != F_QUEUE &&
	    item->stype != F_VIRTUAL) {
		if ((context->actions & GDK_ACTION_MOVE) != 0 &&
		    FOLDER_ITEM_CAN_ADD(src_item)) {
			summary_move_selected_to(folderview->summaryview, item);
			context->action = 0;
			gtk_drag_finish(context, TRUE, FALSE, time);
		} else if ((context->actions & GDK_ACTION_COPY) != 0) {
			summary_copy_selected_to(folderview->summaryview, item);
			gtk_drag_finish(context, TRUE, FALSE, time);
		} else
			gtk_drag_finish(context, FALSE, FALSE, time);
	} else
		gtk_drag_finish(context, FALSE, FALSE, time);

	gtk_tree_path_free(path);
}

static gint folderview_folder_name_compare(GtkTreeModel *model,
					   GtkTreeIter *a, GtkTreeIter *b,
					   gpointer data)
{
	FolderItem *item_a = NULL, *item_b = NULL;

	gtk_tree_model_get(model, a, COL_FOLDER_ITEM, &item_a, -1);
	gtk_tree_model_get(model, b, COL_FOLDER_ITEM, &item_b, -1);

	return folder_item_compare(item_a, item_b);
}
