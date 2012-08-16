/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2012 Hiroyuki Yamamoto
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

#ifndef __MAINWINDOW_H__
#define __MAINWINDOW_H__

#include <glib.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkitemfactory.h>
#include <gtk/gtktooltips.h>

typedef struct _MainWindow	MainWindow;

#include "enums.h"
#include "folderview.h"
#include "summaryview.h"
#include "headerview.h"
#include "messageview.h"
#include "logwindow.h"
#include "trayicon.h"
#include "gtkutils.h"

typedef enum
{
	SEPARATE_NONE	 = 0,
	SEPARATE_FOLDER	 = 1 << 0,
	SEPARATE_MESSAGE = 1 << 1,
	SEPARATE_BOTH	 = (SEPARATE_FOLDER | SEPARATE_MESSAGE)
} SeparateType;

struct _MainWindow
{
	SeparateType type;

	union CompositeWin {
		struct 
		{
			GtkWidget *hpaned;
			GtkWidget *vpaned;
		} sep_none;
		struct {
			GtkWidget *folderwin;
			GtkWidget *vpaned;
		} sep_folder;
		struct {
			GtkWidget *messagewin;
			GtkWidget *hpaned;
		} sep_message;
		struct {
			GtkWidget *folderwin;
			GtkWidget *messagewin;
		} sep_both;
	} win;

	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *menubar;

	GtkItemFactory *menu_factory;

	/* toolbar */
	GtkWidget *toolbar;
	GtkWidget *get_btn;
	GtkWidget *getall_btn;
	GtkWidget *rpop3_btn;
	GtkWidget *send_btn;
	GtkWidget *compose_btn;
	GtkWidget *reply_btn;
	ComboButton *reply_combo;
	GtkWidget *replyall_btn;
	GtkWidget *fwd_btn;
	ComboButton *fwd_combo;
	GtkWidget *delete_btn;
	GtkWidget *junk_btn;
	GtkWidget *notjunk_btn;
	GtkWidget *exec_btn;
	GtkWidget *next_btn;
	GtkWidget *prev_btn;
	GtkWidget *address_btn;
	GtkWidget *search_btn;
	GtkWidget *print_btn;
	GtkWidget *stop_btn;
	GtkWidget *prefs_common_btn;
	GtkWidget *prefs_account_btn;

	/* body */
	GtkWidget *vbox_body;
	GtkWidget *statusbar;
	GtkWidget *progressbar;
	GtkWidget *statuslabel;
	GtkWidget *online_switch;
	GtkWidget *online_pixmap;
	GtkWidget *offline_pixmap;
	GtkTooltips *online_tip;
	GtkWidget *ac_button;
	GtkWidget *ac_label;
	GtkWidget *ac_menu;

	TrayIcon *tray_icon;

	/* context IDs for status bar */
	gint mainwin_cid;
	gint folderview_cid;
	gint summaryview_cid;
	gint messageview_cid;

	ToolbarStyle toolbar_style;

	guint lock_count;
	guint menu_lock_count;
	guint cursor_count;

	gboolean window_hidden;
	gboolean window_obscured;

	FolderView	*folderview;
	SummaryView	*summaryview;
	MessageView	*messageview;
	LogWindow	*logwin;

	GtkTooltips *toolbar_tip;
};

MainWindow *main_window_create		(SeparateType	 type);

void main_window_cursor_wait		(MainWindow	*mainwin);
void main_window_cursor_normal		(MainWindow	*mainwin);

void main_window_lock			(MainWindow	*mainwin);
void main_window_unlock			(MainWindow	*mainwin);

void main_window_reflect_prefs_all	(void);
void main_window_set_summary_column	(void);
void main_window_set_account_menu	(GList		*account_list);

void main_window_change_cur_account	(void);

MainWindow *main_window_get		(void);

GtkWidget *main_window_get_folder_window	(MainWindow	*mainwin);
GtkWidget *main_window_get_message_window	(MainWindow	*mainwin);

void main_window_hide			(MainWindow	*mainwin);
void main_window_change_layout		(MainWindow	*mainwin,
					 LayoutType	 layout,
					 SeparateType	 type);

void main_window_toggle_message_view	(MainWindow	*mainwin);

void main_window_get_size		(MainWindow	*mainwin);
void main_window_get_position		(MainWindow	*mainwin);

void main_window_progress_on		(MainWindow	*mainwin);
void main_window_progress_off		(MainWindow	*mainwin);
void main_window_progress_set		(MainWindow	*mainwin,
					 gint		 cur,
					 gint		 total);

void main_window_progress_show		(gint		 cur,
					 gint		 total);

void main_window_toggle_online			(MainWindow	*mainwin,
						 gboolean	 online);
gboolean main_window_toggle_online_if_offline	(MainWindow	*mainwin);

void main_window_empty_trash		(MainWindow	*mainwin,
					 gboolean	 confirm);
void main_window_add_mailbox		(MainWindow	*mainwin);
void main_window_send_queue		(MainWindow	*mainwin);

void main_window_set_toolbar_sensitive	(MainWindow	*mainwin);
void main_window_set_menu_sensitive	(MainWindow	*mainwin);

void main_window_popup			(MainWindow	*mainwin);

#endif /* __MAINWINDOW_H__ */
