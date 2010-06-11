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

#ifndef __FOLDERVIEW_H__
#define __FOLDERVIEW_H__

#include <glib.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtktreestore.h>
#include <gtk/gtktreeview.h>

#include "folder.h"

typedef struct _FolderView	FolderView;

#include "mainwindow.h"
#include "summaryview.h"

struct _FolderView
{
	GtkWidget *vbox;

	GtkWidget *scrolledwin;

	GtkWidget *treeview;

	GtkTreeStore *store;
	GtkTreeSelection *selection;

	GtkWidget *mail_popup;
	GtkWidget *imap_popup;
	GtkWidget *news_popup;

	GtkItemFactory *mail_factory;
	GtkItemFactory *imap_factory;
	GtkItemFactory *news_factory;

	GtkTreeRowReference *selected;
	GtkTreeRowReference *opened;
	GtkTreeRowReference *prev_selected;

	gboolean display_folder_unread;

	gboolean open_folder;

	guint expand_timeout;
	guint scroll_timeout;

	gboolean selection_locked;

	GdkColor color_new;
	GdkColor color_noselect;

	MainWindow   *mainwin;
	SummaryView  *summaryview;
};

FolderView *folderview_create		(void);
void folderview_init			(FolderView	*folderview);
void folderview_reflect_prefs		(FolderView	*folderview);

void folderview_add_sub_widget		(FolderView	*folderview,
					 GtkWidget	*widget);

FolderView *folderview_get		(void);

void folderview_set			(FolderView	*folderview);
void folderview_set_all			(void);

void folderview_select			(FolderView	*folderview,
					 FolderItem	*item);
void folderview_unselect		(FolderView	*folderview);
void folderview_select_next_unread	(FolderView	*folderview);

FolderItem *folderview_get_selected_item(FolderView	*folderview);

void folderview_set_opened_item		(FolderView	*folderview,
					 FolderItem	*item);
void folderview_update_opened_msg_num	(FolderView	*folderview);

gboolean folderview_append_item		(FolderView	*folderview,
					 GtkTreeIter	*iter,
					 FolderItem	*item,
					 gboolean	 expand_parent);

gint folderview_check_new		(Folder		*folder);
gint folderview_check_new_item		(FolderItem	*item);
gint folderview_check_new_all		(void);

void folderview_update_item		(FolderItem	*item,
					 gboolean	 update_summary);
void folderview_update_item_foreach	(GHashTable	*table,
					 gboolean	 update_summary);
void folderview_update_all_updated	(gboolean	 update_summary);

void folderview_new_folder		(FolderView	*folderview);
void folderview_rename_folder		(FolderView	*folderview);
void folderview_move_folder		(FolderView	*folderview);
void folderview_delete_folder		(FolderView	*folderview);

void folderview_check_new_selected	(FolderView	*folderview);
void folderview_remove_mailbox		(FolderView	*folderview);
void folderview_rebuild_tree		(FolderView	*folderview);

#endif /* __FOLDERVIEW_H__ */
