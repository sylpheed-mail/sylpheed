/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2010 Hiroyuki Yamamoto
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

#ifndef __SUMMARY_H__
#define __SUMMARY_H__

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkitemfactory.h>
#include <gtk/gtktreestore.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkdnd.h>

typedef struct _SummaryView		SummaryView;
typedef struct _SummaryColumnState	SummaryColumnState;

#include "enums.h"
#include "mainwindow.h"
#include "folderview.h"
#include "headerview.h"
#include "messageview.h"
#include "quick_search.h"
#include "compose.h"
#include "filter.h"
#include "folder.h"
#include "procmsg.h"

typedef enum
{
	SUMMARY_NONE,
	SUMMARY_SELECTED_NONE,
	SUMMARY_SELECTED_SINGLE,
	SUMMARY_SELECTED_MULTIPLE
} SummarySelection;

struct _SummaryColumnState
{
	SummaryColumnType type;
	gboolean visible;
};

struct _SummaryView
{
	GtkWidget *vbox;

	GtkWidget *scrolledwin;
	GtkWidget *treeview;

	GtkTreeStore *store;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *columns[N_SUMMARY_VISIBLE_COLS];

	GtkWidget *hseparator;
	GtkWidget *hbox;
	GtkWidget *statlabel_folder;
	GtkWidget *statlabel_select;
	GtkWidget *statlabel_msgs;
	GtkWidget *toggle_eventbox;
	GtkWidget *toggle_arrow;
	GtkWidget *popupmenu;
	GtkWidget *colorlabel_menu;

	GtkItemFactory *popupfactory;

	GtkWidget *reedit_menuitem;
	GtkWidget *reedit_separator;
	GtkWidget *junk_menuitem;
	GtkWidget *nojunk_menuitem;
	GtkWidget *junk_separator;

	GtkWidget *window;

	GtkTreeRowReference *selected;
	GtkTreeRowReference *displayed;

	GList *selection_list;

	gboolean display_msg;

	SummaryColumnState col_state[N_SUMMARY_VISIBLE_COLS];
	gint col_pos[N_SUMMARY_VISIBLE_COLS];

	GdkColor color_marked;
	GdkColor color_dim;

	guint lock_count;

	MainWindow  *mainwin;
	FolderView  *folderview;
	HeaderView  *headerview;
	MessageView *messageview;
	QuickSearch *qsearch;

	FolderItem *folder_item;

	/* current message status */
	gint64 total_size;
	gint deleted;
	gint moved;
	gint copied;

/*
private:
*/
	/* table for looking up message-id */
	GHashTable *msgid_table;

	/* all message list */
	GSList *all_mlist;
	/* filtered message list */
	GSList *flt_mlist;

	gint64 total_flt_msg_size;
	gint flt_msg_total;
	gint flt_deleted;
	gint flt_moved;
	gint flt_copied;
	gint flt_new;
	gint flt_unread;

	gboolean on_filter;

	/* list for moving/deleting messages */
	GSList *tmp_mlist;
	FolderItem *to_folder;
	/* table for updating folder tree */
	GHashTable *folder_table;
	/* counter for filtering */
	gint filtered;
	gint flt_count;
	gint flt_total;

	gboolean on_button_press;

	/* for DnD */
	gboolean can_toggle_selection;
	gboolean on_drag;
	GtkTreePath *pressed_path;
	gchar *drag_list;

	/* unthreading */
	GSList *pos_list;

	guint write_lock_count;

	/* junk filter list */
	GSList *junk_fltlist;

	/* generic flag */
	gint tmp_flag;
};

SummaryView	*summary_create(void);

void summary_init		  (SummaryView		*summaryview);
gboolean summary_show		  (SummaryView		*summaryview,
				   FolderItem		*fitem,
				   gboolean		 update_cache);
void summary_clear_list		  (SummaryView		*summaryview);
void summary_clear_all		  (SummaryView		*summaryview);

void summary_show_queued_msgs	  (SummaryView		*summaryview);

/* full lock */
void summary_lock		  (SummaryView		*summaryview);
void summary_unlock		  (SummaryView		*summaryview);
gboolean summary_is_locked	  (SummaryView		*summaryview);
gboolean summary_is_read_locked	  (SummaryView		*summaryview);

/* write lock (read only) */
void summary_write_lock		  (SummaryView		*summaryview);
void summary_write_unlock	  (SummaryView		*summaryview);
gboolean summary_is_write_locked  (SummaryView		*summaryview);

FolderItem *summary_get_current_folder		(SummaryView	*summaryview);

SummarySelection summary_get_selection_type	(SummaryView	*summaryview);
GSList *summary_get_selected_msg_list		(SummaryView	*summaryview);
GSList *summary_get_changed_msg_list		(SummaryView	*summaryview);
GSList *summary_get_msg_list			(SummaryView	*summaryview);
GSList *summary_get_flagged_msg_list		(SummaryView	*summaryview,
						 MsgPermFlags	 flags);

MsgInfo *summary_get_msginfo_by_msgnum		(SummaryView	*summaryview,
						 guint		 msgnum);

void summary_select_prev_unread	  (SummaryView		*summaryview);
void summary_select_next_unread	  (SummaryView		*summaryview);
void summary_select_prev_new	  (SummaryView		*summaryview);
void summary_select_next_new	  (SummaryView		*summaryview);
void summary_select_prev_marked	  (SummaryView		*summaryview);
void summary_select_next_marked	  (SummaryView		*summaryview);
void summary_select_prev_labeled  (SummaryView		*summaryview);
void summary_select_next_labeled  (SummaryView		*summaryview);
void summary_select_by_msgnum	  (SummaryView		*summaryview,
				   guint		 msgnum);
gboolean summary_select_by_msginfo(SummaryView		*summaryview,
				   MsgInfo		*msginfo);

void summary_select_row		  (SummaryView		*summaryview,
				   GtkTreeIter		*iter,
				   gboolean		 display_msg,
				   gboolean		 do_refresh);

void summary_thread_build	  (SummaryView		*summaryview);
void summary_unthread		  (SummaryView		*summaryview);

void summary_expand_threads	  (SummaryView		*summaryview);
void summary_collapse_threads	  (SummaryView		*summaryview);

void summary_filter		  (SummaryView		*summaryview,
				   gboolean		 selected_only);
void summary_filter_open	  (SummaryView		*summaryview,
				   FilterCreateType	 type);

void summary_filter_junk	  (SummaryView		*summaryview,
				   gboolean		 selected_only);

void summary_junk		  (SummaryView		*summaryview);
void summary_not_junk		  (SummaryView		*summaryview);

void summary_sort		  (SummaryView		*summaryview,
				   FolderSortKey	 sort_key,
				   FolderSortType	 sort_type);

void summary_delete		  (SummaryView		*summaryview);
void summary_delete_duplicated	  (SummaryView		*summaryview);

gboolean summary_execute	  (SummaryView		*summaryview);

void summary_attract_by_subject	  (SummaryView		*summaryview);

gint summary_write_cache	  (SummaryView		*summaryview);

void summary_pass_key_press_event (SummaryView		*summaryview,
				   GdkEventKey		*event);

void summary_display_msg_selected (SummaryView		*summaryview,
				   gboolean		 new_window,
				   gboolean		 all_headers);
void summary_redisplay_msg	  (SummaryView		*summaryview);
void summary_open_msg		  (SummaryView		*summaryview);
void summary_view_source	  (SummaryView		*summaryview);
void summary_reedit		  (SummaryView		*summaryview);
gboolean summary_step		  (SummaryView		*summaryview,
				   GtkScrollType	 type);
void summary_toggle_view	  (SummaryView		*summaryview);

void summary_update_selected_rows (SummaryView		*summaryview);
void summary_update_by_msgnum	  (SummaryView		*summaryview,
				   guint		 msgnum);

void summary_move_selected_to	  (SummaryView		*summaryview,
				   FolderItem		*to_folder);
void summary_move_to		  (SummaryView		*summaryview);
void summary_copy_selected_to	  (SummaryView		*summaryview,
				   FolderItem		*to_folder);
void summary_copy_to		  (SummaryView		*summaryview);
void summary_save_as		  (SummaryView		*summaryview);
void summary_print		  (SummaryView		*summaryview);
void summary_mark		  (SummaryView		*summaryview);
void summary_unmark		  (SummaryView		*summaryview);
void summary_mark_as_unread	  (SummaryView		*summaryview);
void summary_mark_as_read	  (SummaryView		*summaryview);
void summary_mark_thread_as_read  (SummaryView		*summaryview);
void summary_mark_all_read	  (SummaryView		*summaryview);
void summary_add_address	  (SummaryView		*summaryview);
void summary_select_all		  (SummaryView		*summaryview);
void summary_unselect_all	  (SummaryView		*summaryview);
void summary_select_thread	  (SummaryView		*summaryview);

void summary_mark_displayed_read  (SummaryView		*summaryview,
				   GtkTreeIter		*iter);

void summary_reply		  (SummaryView		*summaryview,
				   ComposeMode		 mode);

void summary_set_colorlabel	  (SummaryView		*summaryview,
				   guint		 labelcolor,
				   GtkWidget		*widget);

void summary_set_column_order	  (SummaryView		*summaryview);
void summary_get_column_order	  (SummaryView		*summaryview);

void summary_qsearch_reset	  (SummaryView		*summaryview);
void summary_qsearch_clear_entry  (SummaryView		*summaryview);
void summary_qsearch		  (SummaryView		*summaryview);

#endif /* __SUMMARY_H__ */
