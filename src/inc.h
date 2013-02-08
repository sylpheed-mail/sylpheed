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

#ifndef __INC_H__
#define __INC_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>

#include "mainwindow.h"
#include "progressdialog.h"
#include "prefs_account.h"
#include "session.h"
#include "pop.h"

typedef struct _IncResult		IncResult;
typedef struct _IncProgressDialog	IncProgressDialog;
typedef struct _IncSession		IncSession;

typedef enum
{
	INC_SUCCESS,
	INC_CONNECT_ERROR,
	INC_AUTH_FAILED,
	INC_LOCKED,
	INC_ERROR,
	INC_NO_SPACE,
	INC_IO_ERROR,
	INC_SOCKET_ERROR,
	INC_EOF,
	INC_TIMEOUT,
	INC_CANCEL,
	INC_LOOKUP_ERROR
} IncState;

struct _IncProgressDialog
{
	ProgressDialog *dialog;

	MainWindow *mainwin;

	gboolean show_dialog;

	GTimeVal progress_tv;
	GTimeVal folder_tv;

	GList *queue_list;	/* list of IncSession */
	gint cur_row;

	IncResult *result;
};

struct _IncSession
{
	Session *session;
	IncState inc_state;

	GHashTable *folder_table;	/* table of destination folders */
	GHashTable *tmp_folder_table;	/* for progressive update */

	GSList *junk_fltlist;

	gint64 cur_total_bytes;
	gint new_msgs;

	gint start_num;
	gint64 start_recv_bytes;

	gint retr_count;

	gpointer data;
};

#define TIMEOUT_ITV	200

void inc_mail			(MainWindow	*mainwin);
gint inc_account_mail		(MainWindow	*mainwin,
				 PrefsAccount	*account);
void inc_all_account_mail	(MainWindow	*mainwin,
				 gboolean	 autocheck);

gint inc_pop_before_smtp	(PrefsAccount	*account);

gboolean inc_is_active		(void);
void inc_block_notify		(gboolean	 notify);

void inc_cancel_all		(void);

void inc_lock			(void);
void inc_unlock			(void);

void inc_autocheck_timer_init	(MainWindow	*mainwin);
void inc_autocheck_timer_set	(void);
void inc_autocheck_timer_remove	(void);

#endif /* __INC_H__ */
