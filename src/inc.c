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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkstock.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "main.h"
#include "inc.h"
#include "mainwindow.h"
#include "folderview.h"
#include "summaryview.h"
#include "prefs_common.h"
#include "prefs_account.h"
#include "account.h"
#include "procmsg.h"
#include "socket.h"
#include "socks.h"
#include "pop.h"
#include "recv.h"
#include "mbox.h"
#include "imap.h"
#include "utils.h"
#include "gtkutils.h"
#include "statusbar.h"
#include "manage_window.h"
#include "stock_pixmap.h"
#include "progressdialog.h"
#include "alertpanel.h"
#include "trayicon.h"
#include "notificationwindow.h"
#include "filter.h"
#include "folder.h"
#include "procheader.h"
#include "plugin.h"


typedef struct _IncAccountNewMsgCount
{
	PrefsAccount *account;
	gint new_messages;
} IncAccountNewMsgCount;

typedef struct _IncMsgSummary
{
	gchar *subject;
	gchar *from;
} IncMsgSummary;

struct _IncResult
{
	GSList *count_list;
	GSList *msg_summaries;
};


static GList *inc_dialog_list = NULL;

static gboolean inc_is_running = FALSE;

static guint inc_lock_count = 0;
static gboolean block_notify = FALSE;

static GdkPixbuf *current_pixbuf;
static GdkPixbuf *error_pixbuf;
static GdkPixbuf *ok_pixbuf;


static void inc_finished		(MainWindow		*mainwin,
					 IncResult		*result);
static GSList *inc_add_message_count	(GSList			*list,
					 PrefsAccount		*account,
					 gint			 new_messages);
static void inc_result_free		(IncResult		*result,
					 gboolean		 free_self);

static gint inc_remote_account_mail	(MainWindow		*mainwin,
					 PrefsAccount		*account);
static gint inc_account_mail_real	(MainWindow		*mainwin,
					 PrefsAccount		*account,
					 IncResult		*result);

static IncProgressDialog *inc_progress_dialog_create
					(gboolean		 autocheck);
static void inc_progress_dialog_set_list(IncProgressDialog	*inc_dialog);
static void inc_progress_dialog_destroy	(IncProgressDialog	*inc_dialog);

static IncSession *inc_session_new	(PrefsAccount		*account);
static void inc_session_destroy		(IncSession		*session);
static gint inc_start			(IncProgressDialog	*inc_dialog);
static IncState inc_pop3_session_do	(IncSession		*session);

static void inc_progress_dialog_update	(IncProgressDialog	*inc_dialog,
					 IncSession		*inc_session);

static void inc_progress_dialog_set_label
					(IncProgressDialog	*inc_dialog,
					 IncSession		*inc_session);
static void inc_progress_dialog_set_progress
					(IncProgressDialog	*inc_dialog,
					 IncSession		*inc_session);

static void inc_update_folderview	(IncProgressDialog	*inc_dialog,
					 IncSession		*inc_session);

static void inc_progress_dialog_update_periodic
					(IncProgressDialog	*inc_dialog,
					 IncSession		*inc_session);
static void inc_update_folderview_periodic
					(IncProgressDialog	*inc_dialog,
					 IncSession		*inc_session);

static gint inc_recv_data_progressive	(Session	*session,
					 guint		 cur_len,
					 guint		 total_len,
					 gpointer	 data);
static gint inc_recv_data_finished	(Session	*session,
					 guint		 len,
					 gpointer	 data);
static gint inc_recv_message		(Session	*session,
					 const gchar	*msg,
					 gpointer	 data);
static gint inc_drop_message		(Pop3Session	*session,
					 const gchar	*file);

static void inc_put_error		(IncSession	*session,
					 IncState	 istate,
					 const gchar	*pop3_msg);

static void inc_cancel_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void inc_cancel_all_cb		(GtkWidget	*widget,
					 gpointer	 data);
static gint inc_dialog_delete_cb	(GtkWidget	*widget,
					 GdkEventAny	*event,
					 gpointer	 data);

static gint inc_spool			(void);
static gint get_spool			(FolderItem	*dest,
					 const gchar	*mbox);

static void inc_autocheck_timer_set_interval	(guint		 interval);
static gint inc_autocheck_func			(gpointer	 data);


/**
 * inc_finished:
 * @mainwin: Main window.
 * @result: Information of incorporation result.
 * @new_messages: Number of received messages.
 * 
 * Update the folder view and the summary view after receiving
 * messages.  If @new_messages is 0, this function avoids unneeded
 * updating.
 **/
static void inc_finished(MainWindow *mainwin, IncResult *result)
{
	FolderItem *item;
	gint new_messages = 0;
	gint other_new = 0;
	IncAccountNewMsgCount *count;
	GSList *cur;

	if (result) {
		for (cur = result->count_list; cur != NULL; cur = cur->next) {
			count = cur->data;
			if (count->new_messages > 0)
				new_messages += count->new_messages;
		}
	}

	debug_print("inc_finished: %d new message(s)\n", new_messages);

	if (prefs_common.scan_all_after_inc) {
		other_new = folderview_check_new(NULL);
		new_messages += other_new;
	}

	if (new_messages > 0 && !block_notify) {
		GString *str;
		gint c = 0;

		str = g_string_new("");
		g_string_printf(str, _("Sylpheed: %d new messages"),
				new_messages);
		if (result) {
			for (cur = result->count_list; cur != NULL; cur = cur->next) {
				count = cur->data;
				if (count->new_messages > 0) {
					if (c == 0)
						g_string_append(str, "\n");
					c++;
					g_string_append(str, "\n");
					if (!count->account)
						g_string_append_printf(str, _("[Local]: %d"), count->new_messages);
					else
						g_string_append_printf(str, "%s: %d", count->account->account_name ? count->account->account_name : "[?]", count->new_messages);
				}
			}
		}
		debug_print("inc_finished: %s\n", str->str);
		trayicon_set_tooltip(str->str);
		trayicon_set_notify(TRUE);

		if (prefs_common.enable_newmsg_notify_window) {
			gchar buf[1024];

			g_snprintf(buf, sizeof(buf), _("Sylpheed: %d new messages"), new_messages);
			g_string_truncate(str, 0);
			if (result) {
				for (cur = result->msg_summaries; cur != NULL; cur = cur->next) {
					IncMsgSummary *summary = cur->data;
					gchar *markup;

					if (str->len > 0)
						g_string_append_c(str, '\n');
					markup = g_markup_printf_escaped("<b>%s</b>  %s", summary->subject, summary->from);
					g_string_append(str, markup);
					g_free(markup);
				}
			}

			notification_window_open(buf, str->str, prefs_common.notify_window_period);
		}

		g_string_free(str, TRUE);
	}

	syl_plugin_signal_emit("inc-mail-finished", new_messages);

	inc_block_notify(FALSE);

	if (new_messages <= 0 && !prefs_common.scan_all_after_inc) return;

	if (prefs_common.open_inbox_on_inc) {
		item = cur_account && cur_account->inbox
			? folder_find_item_from_identifier(cur_account->inbox)
			: folder_get_default_inbox();
		folderview_select(mainwin->folderview, item);
	} else if (prefs_common.scan_all_after_inc) {
		item = mainwin->summaryview->folder_item;
		if (item)
			folderview_update_item(item, TRUE);
	}

	/* Notification */

	if (new_messages > 0 &&
	    prefs_common.enable_newmsg_notify_sound &&
	    prefs_common.newmsg_notify_sound) {
		play_sound(prefs_common.newmsg_notify_sound, TRUE);
	}

	if (new_messages > 0 &&
	    prefs_common.enable_newmsg_notify &&
	    prefs_common.newmsg_notify_cmd) {
		gchar buf[1024];

		if (str_find_format_times
			(prefs_common.newmsg_notify_cmd, 'd') == 1)
			g_snprintf(buf, sizeof(buf),
				   prefs_common.newmsg_notify_cmd,
				   new_messages);
		else
			strncpy2(buf, prefs_common.newmsg_notify_cmd,
				 sizeof(buf));
		execute_command_line(buf, TRUE);
	}
}

static GSList *inc_add_message_count(GSList *list, PrefsAccount *account,
				     gint new_messages)
{
	IncAccountNewMsgCount *count;

	count = g_new(IncAccountNewMsgCount, 1);
	count->account = account;
	count->new_messages = new_messages;

	return g_slist_append(list, count);
}

static void inc_result_free(IncResult *result, gboolean free_self)
{
	GSList *cur;

	slist_free_strings(result->count_list);
	g_slist_free(result->count_list);
	for (cur = result->msg_summaries; cur != NULL; cur = cur->next) {
		IncMsgSummary *sum = cur->data;
		g_free(sum->subject);
		g_free(sum->from);
		g_free(sum);
	}
	g_slist_free(result->msg_summaries);

	if (free_self)
		g_free(result);
}

void inc_mail(MainWindow *mainwin)
{
	IncResult result = {NULL, NULL};
	gint new_msgs = 0;

	if (inc_lock_count) return;
	if (inc_is_active()) return;

	if (!main_window_toggle_online_if_offline(mainwin))
		return;

	inc_is_running = TRUE;

	inc_autocheck_timer_remove();
	summary_write_cache(mainwin->summaryview);
	main_window_lock(mainwin);

	syl_plugin_signal_emit("inc-mail-start", cur_account);

	if (prefs_common.use_extinc && prefs_common.extinc_cmd) {
		/* external incorporating program */
		if (execute_command_line(prefs_common.extinc_cmd, FALSE) != 0) {
			inc_is_running = FALSE;
			main_window_unlock(mainwin);
			inc_autocheck_timer_set();
			return;
		}

		if (prefs_common.inc_local) {
			new_msgs = inc_spool();
			result.count_list = inc_add_message_count(result.count_list, NULL, new_msgs);
		}
	} else {
		if (prefs_common.inc_local) {
			new_msgs = inc_spool();
			if (new_msgs < 0)
				new_msgs = 0;
			result.count_list = inc_add_message_count(result.count_list, NULL, new_msgs);
		}

		new_msgs = inc_account_mail_real(mainwin, cur_account, &result);
	}

	inc_finished(mainwin, &result);
	inc_result_free(&result, FALSE);

	inc_is_running = FALSE;

	main_window_unlock(mainwin);
	inc_autocheck_timer_set();
}

static gint inc_remote_account_mail(MainWindow *mainwin, PrefsAccount *account)
{
	FolderItem *item = mainwin->summaryview->folder_item;
	gint new_msgs = 0;
	gboolean update_summary = FALSE;

	g_return_val_if_fail(account != NULL, 0);
	g_return_val_if_fail(account->folder != NULL, 0);

	if (account->protocol == A_IMAP4 &&
	    account->imap_filter_inbox_on_recv) {
		FolderItem *inbox = FOLDER(account->folder)->inbox;
		GSList *mlist, *cur;
		FilterInfo *fltinfo;
		GSList junk_fltlist = {NULL, NULL};
		FilterRule *junk_rule;
		gint n_filtered = 0;

		debug_print("inc_remote_account_mail(): filtering IMAP4 INBOX\n");
		mlist = folder_item_get_uncached_msg_list(inbox);
		debug_print("inc_remote_account_mail(): uncached messages: %d\n", g_slist_length(mlist));

		junk_rule = filter_junk_rule_create(account, NULL, TRUE);
		if (junk_rule)
			junk_fltlist.data = junk_rule;

		for (cur = mlist; cur != NULL; cur = cur->next) {
			MsgInfo *msginfo = (MsgInfo *)cur->data;
			gboolean is_junk = FALSE;

			fltinfo = filter_info_new();
			fltinfo->account = account;
			fltinfo->flags = msginfo->flags;

			if (prefs_common.enable_junk &&
			    prefs_common.filter_junk_on_recv &&
			    prefs_common.filter_junk_before && junk_rule) {
				filter_apply_msginfo
					(&junk_fltlist, msginfo, fltinfo);
				if (fltinfo->drop_done)
					is_junk = TRUE;
			}

			if (!fltinfo->drop_done) {
				filter_apply_msginfo(prefs_common.fltlist,
						     msginfo, fltinfo);
			}

			if (!fltinfo->drop_done &&
			    prefs_common.enable_junk &&
			    prefs_common.filter_junk_on_recv &&
			    !prefs_common.filter_junk_before && junk_rule) {
				filter_apply_msginfo
					(&junk_fltlist, msginfo, fltinfo);
				if (fltinfo->drop_done)
					is_junk = TRUE;
			}

			if (msginfo->flags.perm_flags !=
			    fltinfo->flags.perm_flags) {
				msginfo->flags = fltinfo->flags;
				inbox->mark_dirty = TRUE;
				if (fltinfo->actions[FLT_ACTION_MARK])
					imap_msg_set_perm_flags
						(msginfo, MSG_MARKED);
				if (fltinfo->actions[FLT_ACTION_MARK_READ])
					imap_msg_unset_perm_flags
						(msginfo, MSG_NEW|MSG_UNREAD);
			}

			if (fltinfo->actions[FLT_ACTION_MOVE] &&
			    fltinfo->move_dest) {
				folder_item_move_msg
					(fltinfo->move_dest, msginfo);
				if (account->imap_check_inbox_only ||
				    fltinfo->move_dest->folder !=
				    inbox->folder) {
					if (!is_junk &&
					    fltinfo->move_dest->stype != F_TRASH &&
					    fltinfo->move_dest->stype != F_JUNK &&
					    (MSG_IS_NEW(fltinfo->flags) ||
					     MSG_IS_UNREAD(fltinfo->flags)))
						++new_msgs;
				}
			} else if (fltinfo->actions[FLT_ACTION_DELETE])
				folder_item_remove_msg(inbox, msginfo);
			else if (!is_junk && (MSG_IS_NEW(msginfo->flags) ||
					      MSG_IS_UNREAD(msginfo->flags)))
				++new_msgs;

			if (fltinfo->drop_done)
				++n_filtered;

			filter_info_free(fltinfo);
		}

		if (junk_rule)
			filter_rule_free(junk_rule);

		procmsg_msg_list_free(mlist);

		debug_print("inc_remote_account_mail(): INBOX: %d new, %d filtered\n",
			    new_msgs, n_filtered);

		if (!prefs_common.scan_all_after_inc && item != NULL &&
		    inbox == item)
			update_summary = TRUE;
	}

	if (account->protocol == A_IMAP4 && account->imap_check_inbox_only) {
		FolderItem *inbox = FOLDER(account->folder)->inbox;

		new_msgs += folderview_check_new_item(inbox);
		if (!prefs_common.scan_all_after_inc && item != NULL &&
		    inbox == item)
			update_summary = TRUE;
	} else {
		new_msgs += folderview_check_new(FOLDER(account->folder));
		if (!prefs_common.scan_all_after_inc && item != NULL &&
		    FOLDER(account->folder) == item->folder)
			update_summary = TRUE;
	}

	if (update_summary)
		folderview_update_item(item, TRUE);
	folderview_update_all_updated(FALSE);

	return new_msgs;
}

static gint inc_account_mail_real(MainWindow *mainwin, PrefsAccount *account,
				  IncResult *result)
{
	IncProgressDialog *inc_dialog;
	IncSession *session;

	g_return_val_if_fail(account != NULL, 0);

	if (account->protocol == A_IMAP4 || account->protocol == A_NNTP)
		return inc_remote_account_mail(mainwin, account);

	session = inc_session_new(account);
	if (!session) return 0;

	inc_dialog = inc_progress_dialog_create(FALSE);
	inc_dialog->queue_list = g_list_append(inc_dialog->queue_list, session);
	inc_dialog->mainwin = mainwin;
	inc_dialog->result = result;
	inc_progress_dialog_set_list(inc_dialog);

	main_window_set_toolbar_sensitive(mainwin);
	main_window_set_menu_sensitive(mainwin);

	return inc_start(inc_dialog);
}

gint inc_account_mail(MainWindow *mainwin, PrefsAccount *account)
{
	IncResult result = {NULL, NULL};
	gint new_msgs;

	if (inc_lock_count) return 0;
	if (inc_is_active()) return 0;

	if (!main_window_toggle_online_if_offline(mainwin))
		return 0;

	inc_is_running = TRUE;

	inc_autocheck_timer_remove();
	summary_write_cache(mainwin->summaryview);
	main_window_lock(mainwin);

	syl_plugin_signal_emit("inc-mail-start", account);

	new_msgs = inc_account_mail_real(mainwin, account, &result);

	inc_finished(mainwin, &result);
	inc_result_free(&result, FALSE);

	inc_is_running = FALSE;

	main_window_unlock(mainwin);
	inc_autocheck_timer_set();

	return new_msgs;
}

void inc_all_account_mail(MainWindow *mainwin, gboolean autocheck)
{
	GList *list, *queue_list = NULL;
	IncProgressDialog *inc_dialog;
	IncResult result = {NULL, NULL};
	gint new_msgs = 0;

	if (inc_lock_count) return;
	if (inc_is_active()) return;

	if (!main_window_toggle_online_if_offline(mainwin))
		return;

	inc_is_running = TRUE;

	inc_autocheck_timer_remove();
	summary_write_cache(mainwin->summaryview);
	main_window_lock(mainwin);

	syl_plugin_signal_emit("inc-mail-start", NULL);

	if (prefs_common.inc_local) {
		new_msgs = inc_spool();
		if (new_msgs < 0)
			new_msgs = 0;
		result.count_list = inc_add_message_count(result.count_list, NULL, new_msgs);
	}

	/* check IMAP4 / News folders */
	for (list = account_get_list(); list != NULL; list = list->next) {
		PrefsAccount *account = list->data;
		if ((account->protocol == A_IMAP4 ||
		     account->protocol == A_NNTP) && account->recv_at_getall) {
			new_msgs = inc_remote_account_mail(mainwin, account);
			result.count_list = inc_add_message_count(result.count_list, account, new_msgs);
		}
	}

	/* check POP3 accounts */
	for (list = account_get_list(); list != NULL; list = list->next) {
		IncSession *session;
		PrefsAccount *account = list->data;

		if (account->recv_at_getall) {
			session = inc_session_new(account);
			if (session)
				queue_list = g_list_append(queue_list, session);
		}
	}

	if (queue_list) {
		inc_dialog = inc_progress_dialog_create(autocheck);
		inc_dialog->queue_list = queue_list;
		inc_dialog->mainwin = mainwin;
		inc_dialog->result = &result;
		inc_progress_dialog_set_list(inc_dialog);

		main_window_set_toolbar_sensitive(mainwin);
		main_window_set_menu_sensitive(mainwin);

		inc_start(inc_dialog);
	}

	inc_finished(mainwin, &result);
	inc_result_free(&result, FALSE);

	inc_is_running = FALSE;

	main_window_unlock(mainwin);
	inc_autocheck_timer_set();
}

gint inc_pop_before_smtp(PrefsAccount *account)
{
	MainWindow *mainwin;
	IncProgressDialog *inc_dialog;
	IncSession *session;

	if (inc_lock_count) return -1;

	mainwin = main_window_get();

	if (!main_window_toggle_online_if_offline(mainwin))
		return -1;

	inc_is_running = TRUE;

	inc_autocheck_timer_remove();
	main_window_lock(mainwin);

	session = inc_session_new(account);
	if (!session) return -1;
	POP3_SESSION(session->session)->auth_only = TRUE;

	inc_dialog = inc_progress_dialog_create(FALSE);
	gtk_window_set_title(GTK_WINDOW(inc_dialog->dialog->window),
			     _("Authenticating with POP3"));
	inc_dialog->queue_list = g_list_append(inc_dialog->queue_list, session);
	inc_dialog->mainwin = mainwin;
	inc_dialog->result = NULL;
	inc_progress_dialog_set_list(inc_dialog);
	inc_dialog->show_dialog = TRUE;

	main_window_set_toolbar_sensitive(mainwin);
	main_window_set_menu_sensitive(mainwin);

	inc_start(inc_dialog);

	inc_is_running = FALSE;

	main_window_unlock(mainwin);
	inc_autocheck_timer_set();

	return 0;
}

static IncProgressDialog *inc_progress_dialog_create(gboolean autocheck)
{
	IncProgressDialog *dialog;
	ProgressDialog *progress;
	GtkWidget *cancel_all_btn;

	dialog = g_new0(IncProgressDialog, 1);

	progress = progress_dialog_create();
	gtk_window_set_title(GTK_WINDOW(progress->window),
			     _("Retrieving new messages"));
	cancel_all_btn = gtk_dialog_add_button(GTK_DIALOG(progress->window),
					       _("Cancel _all"),
					       GTK_RESPONSE_NONE);
	g_signal_connect(G_OBJECT(progress->cancel_btn), "clicked",
			 G_CALLBACK(inc_cancel_cb), dialog);
	g_signal_connect(G_OBJECT(cancel_all_btn), "clicked",
			 G_CALLBACK(inc_cancel_all_cb), dialog);
	g_signal_connect(G_OBJECT(progress->window), "delete_event",
			 G_CALLBACK(inc_dialog_delete_cb), dialog);
	/* manage_window_set_transient(GTK_WINDOW(progress->window)); */

	progress_dialog_set_value(progress, 0.0);

	stock_pixbuf_gdk(progress->treeview, STOCK_PIXMAP_COMPLETE, &ok_pixbuf);
	stock_pixbuf_gdk(progress->treeview, STOCK_PIXMAP_CONTINUE,
			 &current_pixbuf);
	stock_pixbuf_gdk(progress->treeview, STOCK_PIXMAP_ERROR, &error_pixbuf);

	if (prefs_common.recv_dialog_mode == RECV_DIALOG_ALWAYS ||
	    (prefs_common.recv_dialog_mode == RECV_DIALOG_MANUAL &&
	     !autocheck)) {
		dialog->show_dialog = TRUE;
		gtk_widget_show_now(progress->window);
	}

	dialog->dialog = progress;
	g_get_current_time(&dialog->progress_tv);
	g_get_current_time(&dialog->folder_tv);
	dialog->queue_list = NULL;
	dialog->cur_row = 0;

	inc_dialog_list = g_list_append(inc_dialog_list, dialog);

	return dialog;
}

static void inc_progress_dialog_set_list(IncProgressDialog *inc_dialog)
{
	GList *list;

	for (list = inc_dialog->queue_list; list != NULL; list = list->next) {
		IncSession *session = list->data;
		Pop3Session *pop3_session = POP3_SESSION(session->session);

		session->data = inc_dialog;
		progress_dialog_append(inc_dialog->dialog, NULL,
				       pop3_session->ac_prefs->account_name,
				       _("Standby"), "", NULL);
	}
}

static void inc_progress_dialog_clear(IncProgressDialog *inc_dialog)
{
	progress_dialog_set_value(inc_dialog->dialog, 0.0);
	progress_dialog_set_label(inc_dialog->dialog, "");
	main_window_progress_off(inc_dialog->mainwin);
}

static void inc_progress_dialog_destroy(IncProgressDialog *inc_dialog)
{
	g_return_if_fail(inc_dialog != NULL);

	inc_dialog_list = g_list_remove(inc_dialog_list, inc_dialog);

	manage_window_destroy(inc_dialog->dialog->window, NULL);

	main_window_progress_off(inc_dialog->mainwin);
	progress_dialog_destroy(inc_dialog->dialog);

	g_free(inc_dialog);
}

static IncSession *inc_session_new(PrefsAccount *account)
{
	IncSession *session;
	FilterRule *rule;

	g_return_val_if_fail(account != NULL, NULL);

	if (account->protocol != A_POP3)
		return NULL;
	if (!account->recv_server || !account->userid)
		return NULL;

	session = g_new0(IncSession, 1);

	session->session = pop3_session_new(account);
	session->session->data = session;
	POP3_SESSION(session->session)->drop_message = inc_drop_message;
	session_set_recv_message_notify(session->session,
					inc_recv_message, session);
	session_set_recv_data_progressive_notify(session->session,
						 inc_recv_data_progressive,
						 session);
	session_set_recv_data_notify(session->session,
				     inc_recv_data_finished, session);

	session->inc_state = INC_SUCCESS;

	session->folder_table = g_hash_table_new(NULL, NULL);
	session->tmp_folder_table = g_hash_table_new(NULL, NULL);

	rule = filter_junk_rule_create(account, NULL, FALSE);
	if (rule)
		session->junk_fltlist = g_slist_append(NULL, rule);
	else
		session->junk_fltlist = NULL;

	session->cur_total_bytes = 0;
	session->new_msgs = 0;

	session->start_num = 0;
	session->start_recv_bytes = 0;

	session->retr_count = 0;

	return session;
}

static void inc_session_destroy(IncSession *session)
{
	g_return_if_fail(session != NULL);

	session_destroy(session->session);
	g_hash_table_destroy(session->folder_table);
	g_hash_table_destroy(session->tmp_folder_table);
	if (session->junk_fltlist)
		filter_rule_list_free(session->junk_fltlist);
	g_free(session);
}

static void inc_update_folder_foreach(GHashTable *table)
{
	procmsg_flush_folder_foreach(table);
	folderview_update_item_foreach(table, TRUE);
}

static gint inc_start(IncProgressDialog *inc_dialog)
{
	IncSession *session;
	GList *qlist;
	Pop3Session *pop3_session;
	IncState inc_state;
	gint error_num = 0;
	gint new_msgs = 0;
	gchar *msg;
	gchar *fin_msg;

	qlist = inc_dialog->queue_list;
	while (qlist != NULL) {
		GList *next = qlist->next;

		session = qlist->data;
		if (session->inc_state == INC_CANCEL) {
			qlist = next;
			continue;
		}

		pop3_session = POP3_SESSION(session->session); 
		if (!pop3_session->pass) {
			gchar *pass;

			if (inc_dialog->show_dialog)
				manage_window_focus_in
					(inc_dialog->dialog->window,
					 NULL, NULL);

			pass = input_query_password
				(pop3_session->ac_prefs->recv_server,
				 pop3_session->user);

			if (inc_dialog->show_dialog)
				manage_window_focus_out
					(inc_dialog->dialog->window,
					 NULL, NULL);

			if (pass) {
				pop3_session->ac_prefs->tmp_pass =
					g_strdup(pass);
				pop3_session->pass = pass;
			}
		}

		qlist = next;
	}

#define SET_PIXMAP_AND_TEXT(pixbuf, status, progress)			\
{									\
	progress_dialog_set_row_pixbuf(inc_dialog->dialog,		\
				       inc_dialog->cur_row, pixbuf);	\
	progress_dialog_set_row_status(inc_dialog->dialog,		\
				       inc_dialog->cur_row, status);	\
	if (progress)							\
		progress_dialog_set_row_progress(inc_dialog->dialog,	\
						 inc_dialog->cur_row,	\
						 progress);		\
}

	for (; inc_dialog->queue_list != NULL; inc_dialog->cur_row++) {
		session = inc_dialog->queue_list->data;
		pop3_session = POP3_SESSION(session->session);

		if (session->inc_state == INC_CANCEL ||
		    pop3_session->pass == NULL) {
			SET_PIXMAP_AND_TEXT(ok_pixbuf, _("Cancelled"), NULL);
			inc_session_destroy(session);
			inc_dialog->queue_list =
				g_list_remove(inc_dialog->queue_list, session);
			continue;
		}

		inc_progress_dialog_clear(inc_dialog);
		progress_dialog_scroll_to_row(inc_dialog->dialog,
					      inc_dialog->cur_row);

		SET_PIXMAP_AND_TEXT(current_pixbuf, _("Retrieving"), NULL);

		/* begin POP3 session */
		inc_state = inc_pop3_session_do(session);

		switch (inc_state) {
		case INC_SUCCESS:
			if (pop3_session->cur_total_num > 0)
				msg = g_strdup_printf
					(_("%d message(s) (%s) received"),
					 pop3_session->cur_total_num,
					 to_human_readable(pop3_session->cur_total_recv_bytes));
			else
				msg = g_strdup_printf(_("no new messages"));
			SET_PIXMAP_AND_TEXT(ok_pixbuf, _("Done"), msg);
			g_free(msg);
			break;
		case INC_LOOKUP_ERROR:
			SET_PIXMAP_AND_TEXT(error_pixbuf,
					    _("Server not found"), NULL);
			break;
		case INC_CONNECT_ERROR:
			SET_PIXMAP_AND_TEXT(error_pixbuf,
					    _("Connection failed"), NULL);
			break;
		case INC_AUTH_FAILED:
			SET_PIXMAP_AND_TEXT(error_pixbuf, _("Auth failed"),
					    NULL);
			break;
		case INC_LOCKED:
			SET_PIXMAP_AND_TEXT(error_pixbuf, _("Locked"), NULL);
			break;
		case INC_ERROR:
		case INC_NO_SPACE:
		case INC_IO_ERROR:
		case INC_SOCKET_ERROR:
		case INC_EOF:
			SET_PIXMAP_AND_TEXT(error_pixbuf, _("Error"), NULL);
			break;
		case INC_TIMEOUT:
			SET_PIXMAP_AND_TEXT(error_pixbuf, _("Timeout"), NULL);
			break;
		case INC_CANCEL:
			SET_PIXMAP_AND_TEXT(ok_pixbuf, _("Cancelled"), NULL);
			break;
		default:
			break;
		}

		if (inc_dialog->result)
			inc_dialog->result->count_list = inc_add_message_count(inc_dialog->result->count_list, pop3_session->ac_prefs, session->new_msgs);
		new_msgs += session->new_msgs;

		if (!prefs_common.scan_all_after_inc) {
			inc_update_folder_foreach(session->folder_table);
		}

		if (pop3_session->error_val == PS_AUTHFAIL &&
		    pop3_session->ac_prefs->tmp_pass) {
			g_free(pop3_session->ac_prefs->tmp_pass);
			pop3_session->ac_prefs->tmp_pass = NULL;
		}

		pop3_write_uidl_list(pop3_session);

		if (inc_state != INC_SUCCESS && inc_state != INC_CANCEL) {
			error_num++;
			if (inc_dialog->show_dialog)
				manage_window_focus_in
					(inc_dialog->dialog->window,
					 NULL, NULL);
			inc_put_error(session, inc_state,
				      pop3_session->error_msg);
			if (inc_dialog->show_dialog)
				manage_window_focus_out
					(inc_dialog->dialog->window,
					 NULL, NULL);
			if (inc_state == INC_NO_SPACE ||
			    inc_state == INC_IO_ERROR)
				break;
		}

		inc_session_destroy(session);
		inc_dialog->queue_list =
			g_list_remove(inc_dialog->queue_list, session);
	}

#undef SET_PIXMAP_AND_TEXT

	if (new_msgs > 0)
		fin_msg = g_strdup_printf(_("Finished (%d new message(s))"),
					  new_msgs);
	else
		fin_msg = g_strdup_printf(_("Finished (no new messages)"));

	progress_dialog_set_label(inc_dialog->dialog, fin_msg);

#if 0
	if (error_num && !prefs_common.no_recv_err_panel) {
		if (inc_dialog->show_dialog)
			manage_window_focus_in(inc_dialog->dialog->window,
					       NULL, NULL);
		alertpanel_error(_("Some errors occurred while getting mail."));
		if (inc_dialog->show_dialog)
			manage_window_focus_out(inc_dialog->dialog->window,
						NULL, NULL);
	}
#endif

	while (inc_dialog->queue_list != NULL) {
		session = inc_dialog->queue_list->data;
		inc_session_destroy(session);
		inc_dialog->queue_list =
			g_list_remove(inc_dialog->queue_list, session);
	}

	if (prefs_common.close_recv_dialog || !inc_dialog->show_dialog)
		inc_progress_dialog_destroy(inc_dialog);
	else {
		gtk_window_set_title(GTK_WINDOW(inc_dialog->dialog->window),
				     fin_msg);
		gtk_button_set_label(GTK_BUTTON(inc_dialog->dialog->cancel_btn),
				     GTK_STOCK_CLOSE);
	}

	g_free(fin_msg);

	return new_msgs;
}

static IncState inc_pop3_session_do(IncSession *session)
{
	Pop3Session *pop3_session = POP3_SESSION(session->session);
	IncProgressDialog *inc_dialog = (IncProgressDialog *)session->data;
	PrefsAccount *ac = pop3_session->ac_prefs;
	SocksInfo *socks_info = NULL;
	gchar *buf;

	debug_print(_("getting new messages of account %s...\n"),
		    ac->account_name);

	if (pop3_session->auth_only)
		buf = g_strdup_printf(_("%s: Authenticating with POP3"),
				      ac->recv_server);
	else
		buf = g_strdup_printf(_("%s: Retrieving new messages"),
				      ac->recv_server);
	gtk_window_set_title(GTK_WINDOW(inc_dialog->dialog->window), buf);
	g_free(buf);

	buf = g_strdup_printf(_("Connecting to POP3 server: %s..."),
			      ac->recv_server);
	log_message("%s\n", buf);
	progress_dialog_set_label(inc_dialog->dialog, buf);
	g_free(buf);

	session_set_timeout(SESSION(pop3_session),
			    prefs_common.io_timeout_secs * 1000);

	if (ac->use_socks && ac->use_socks_for_recv) {
		socks_info = socks_info_new(ac->socks_type, ac->proxy_host, ac->proxy_port, ac->use_proxy_auth ? ac->proxy_name : NULL, ac->use_proxy_auth ? ac->proxy_pass : NULL);
	}

	GTK_EVENTS_FLUSH();

	if (session_connect_full(SESSION(pop3_session),
				 SESSION(pop3_session)->server,
				 SESSION(pop3_session)->port, socks_info) < 0) {
		log_warning(_("Can't connect to POP3 server: %s:%d\n"),
			    SESSION(pop3_session)->server,
			    SESSION(pop3_session)->port);
		session->inc_state = INC_CONNECT_ERROR;
		if (session_get_error(SESSION(pop3_session)) == SESSION_ERROR_LOOKUP)
			session->inc_state = INC_LOOKUP_ERROR;
		statusbar_pop_all();
		return session->inc_state;
	}

	while (session_is_connected(SESSION(pop3_session)) &&
	       session->inc_state != INC_CANCEL) {
		gtk_main_iteration();
	}
	log_window_flush();

	debug_print("inc_state: %d\n", session->inc_state);
	debug_print("pop3_session.error_val: %d\n", pop3_session->error_val);
	debug_print("pop3_session.error_msg: %s\n", pop3_session->error_msg ? pop3_session->error_msg : "(empty)");

	if (session->inc_state == INC_SUCCESS) {
		switch (pop3_session->error_val) {
		case PS_SUCCESS:
			switch (SESSION(pop3_session)->state) {
			case SESSION_ERROR:
				if (pop3_session->state == POP3_READY)
					session->inc_state = INC_CONNECT_ERROR;
				else
					session->inc_state = INC_ERROR;
				if (session_get_error(SESSION(pop3_session)) == SESSION_ERROR_LOOKUP)
					session->inc_state = INC_LOOKUP_ERROR;
				break;
			case SESSION_EOF:
				session->inc_state = INC_EOF;
				break;
			case SESSION_TIMEOUT:
				session->inc_state = INC_TIMEOUT;
				break;
			default:
				session->inc_state = INC_SUCCESS;
				break;
			}
			break;
		case PS_AUTHFAIL:
			session->inc_state = INC_AUTH_FAILED;
			break;
		case PS_IOERR:
			session->inc_state = INC_IO_ERROR;
			break;
		case PS_SOCKET:
			session->inc_state = INC_SOCKET_ERROR;
			break;
		case PS_LOCKBUSY:
			session->inc_state = INC_LOCKED;
			break;
		default:
			session->inc_state = INC_ERROR;
			break;
		}
	}

	session_disconnect(SESSION(pop3_session));
	statusbar_pop_all();

	return session->inc_state;
}

static void inc_progress_dialog_update(IncProgressDialog *inc_dialog,
				       IncSession *inc_session)
{
	inc_progress_dialog_set_label(inc_dialog, inc_session);
	inc_progress_dialog_set_progress(inc_dialog, inc_session);
}

static void inc_progress_dialog_set_label(IncProgressDialog *inc_dialog,
					  IncSession *inc_session)
{
	ProgressDialog *dialog = inc_dialog->dialog;
	Pop3Session *session;

	g_return_if_fail(inc_session != NULL);

	session = POP3_SESSION(inc_session->session);

	switch (session->state) {
	case POP3_GREETING:
		break;
	case POP3_GETAUTH_USER:
	case POP3_GETAUTH_PASS:
	case POP3_GETAUTH_APOP:
		progress_dialog_set_label(dialog, _("Authenticating..."));
		statusbar_print_all(_("Retrieving messages from %s..."),
				    SESSION(session)->server);
		break;
	case POP3_GETRANGE_STAT:
		progress_dialog_set_label
			(dialog, _("Getting the number of new messages (STAT)..."));
		break;
	case POP3_GETRANGE_LAST:
		progress_dialog_set_label
			(dialog, _("Getting the number of new messages (LAST)..."));
		break;
	case POP3_GETRANGE_UIDL:
		progress_dialog_set_label
			(dialog, _("Getting the number of new messages (UIDL)..."));
		break;
	case POP3_GETSIZE_LIST:
		progress_dialog_set_label
			(dialog, _("Getting the size of messages (LIST)..."));
		break;
	case POP3_RETR:
	case POP3_RETR_RECV:
		break;
	case POP3_DELETE:
#if 0
		if (session->msg[session->cur_msg].recv_time <
			session->current_time) {
			gchar buf[BUFFSIZE];
			g_snprintf(buf, sizeof(buf), _("Deleting message %d"),
				   session->cur_msg);
			progress_dialog_set_label(dialog, buf);
		}
#endif
		break;
	case POP3_LOGOUT:
		progress_dialog_set_label(dialog, _("Quitting"));
		break;
	default:
		break;
	}
}

static void inc_progress_dialog_set_progress(IncProgressDialog *inc_dialog,
					     IncSession *inc_session)
{
	gchar buf[BUFFSIZE];
	Pop3Session *pop3_session = POP3_SESSION(inc_session->session);
	gint64 cur_total;
	gint64 total;
	gint cur_num;
	gint total_num_to_recv;

	if (!pop3_session->new_msg_exist) return;

	if (inc_session->retr_count == 0) {
		cur_num = total_num_to_recv = 0;
		cur_total = total = 0;
	} else {
		cur_num = pop3_session->cur_msg - inc_session->start_num + 1;
		total_num_to_recv = pop3_session->count - inc_session->start_num + 1;
		cur_total = inc_session->cur_total_bytes - inc_session->start_recv_bytes;
		total = pop3_session->total_bytes - inc_session->start_recv_bytes;
	}

	if ((pop3_session->state == POP3_RETR ||
	     pop3_session->state == POP3_RETR_RECV ||
	     pop3_session->state == POP3_DELETE) && total_num_to_recv > 0) {
		gchar total_size_str[16];

		to_human_readable_buf(total_size_str, sizeof(total_size_str),
				      total);
		g_snprintf(buf, sizeof(buf),
			   _("Retrieving message (%d / %d) (%s / %s)"),
			   cur_num, total_num_to_recv,
			   to_human_readable(cur_total), total_size_str);
		progress_dialog_set_label(inc_dialog->dialog, buf);
	}

	if (total > 0)
		progress_dialog_set_percentage
			(inc_dialog->dialog, (gfloat)cur_total / (gfloat)total);

	gtk_progress_set_show_text
		(GTK_PROGRESS(inc_dialog->mainwin->progressbar), TRUE);
	if (total_num_to_recv > 0)
		g_snprintf(buf, sizeof(buf), "%d / %d", cur_num, total_num_to_recv);
	else
		buf[0] = '\0';
	gtk_progress_set_format_string
		(GTK_PROGRESS(inc_dialog->mainwin->progressbar), buf);
	if (total > 0)
		gtk_progress_bar_update
			(GTK_PROGRESS_BAR(inc_dialog->mainwin->progressbar),
			 (gfloat)cur_total / (gfloat)total);

	if (pop3_session->cur_total_num > 0) {
		g_snprintf(buf, sizeof(buf),
			   _("%d message(s) (%s) received"),
			   pop3_session->cur_total_num,
			   to_human_readable(pop3_session->cur_total_recv_bytes));
		progress_dialog_set_row_progress(inc_dialog->dialog,
						 inc_dialog->cur_row, buf);
	}
}

static gboolean hash_remove_func(gpointer key, gpointer value, gpointer data)
{
	return TRUE;
}

static void inc_update_folderview(IncProgressDialog *inc_dialog,
				  IncSession *inc_session)
{
	MainWindow *mainwin;

	if (g_hash_table_size(inc_session->tmp_folder_table) > 0) {
		folderview_update_item_foreach(inc_session->tmp_folder_table,
					       FALSE);
		g_hash_table_foreach_remove(inc_session->tmp_folder_table,
					    hash_remove_func, NULL);
	}

	mainwin = main_window_get();
	summary_show_queued_msgs(mainwin->summaryview);
}

static void inc_progress_dialog_update_periodic(IncProgressDialog *inc_dialog,
						IncSession *inc_session)
{
	GTimeVal tv_cur;
	GTimeVal tv_result;
	gint msec;

	g_get_current_time(&tv_cur);

	tv_result.tv_sec = tv_cur.tv_sec - inc_dialog->progress_tv.tv_sec;
	tv_result.tv_usec = tv_cur.tv_usec - inc_dialog->progress_tv.tv_usec;
	if (tv_result.tv_usec < 0) {
		tv_result.tv_sec--;
		tv_result.tv_usec += G_USEC_PER_SEC;
	}

	msec = tv_result.tv_sec * 1000 + tv_result.tv_usec / 1000;
	if (msec > PROGRESS_UPDATE_INTERVAL) {
		inc_progress_dialog_update(inc_dialog, inc_session);
		inc_dialog->progress_tv.tv_sec = tv_cur.tv_sec;
		inc_dialog->progress_tv.tv_usec = tv_cur.tv_usec;
	}
}

static void inc_update_folderview_periodic(IncProgressDialog *inc_dialog,
					   IncSession *inc_session)
{
	GTimeVal tv_cur;
	GTimeVal tv_result;
	gint msec;

	g_get_current_time(&tv_cur);

	tv_result.tv_sec = tv_cur.tv_sec - inc_dialog->folder_tv.tv_sec;
	tv_result.tv_usec = tv_cur.tv_usec - inc_dialog->folder_tv.tv_usec;
	if (tv_result.tv_usec < 0) {
		tv_result.tv_sec--;
		tv_result.tv_usec += G_USEC_PER_SEC;
	}

	msec = tv_result.tv_sec * 1000 + tv_result.tv_usec / 1000;
	if (msec > FOLDER_UPDATE_INTERVAL) {
		inc_update_folderview(inc_dialog, inc_session);
		inc_dialog->folder_tv.tv_sec = tv_cur.tv_sec;
		inc_dialog->folder_tv.tv_usec = tv_cur.tv_usec;
	}
}

static gint inc_recv_data_progressive(Session *session, guint cur_len,
				      guint total_len, gpointer data)
{
	IncSession *inc_session = (IncSession *)data;
	Pop3Session *pop3_session = POP3_SESSION(session);
	IncProgressDialog *inc_dialog;
	gint64 cur_total;

	g_return_val_if_fail(inc_session != NULL, -1);

	if (pop3_session->state != POP3_RETR &&
	    pop3_session->state != POP3_RETR_RECV &&
	    pop3_session->state != POP3_DELETE &&
	    pop3_session->state != POP3_LOGOUT) return 0;

	if (!pop3_session->new_msg_exist) return 0;

	gdk_threads_enter();

	cur_total = pop3_session->cur_total_bytes + cur_len;
	if (cur_total > pop3_session->total_bytes)
		cur_total = pop3_session->total_bytes;
	inc_session->cur_total_bytes = cur_total;

	inc_dialog = (IncProgressDialog *)inc_session->data;
	inc_progress_dialog_update_periodic(inc_dialog, inc_session);
	inc_update_folderview_periodic(inc_dialog, inc_session);

	gdk_threads_leave();
	return 0;
}

static gint inc_recv_data_finished(Session *session, guint len, gpointer data)
{
	IncSession *inc_session = (IncSession *)data;
	IncProgressDialog *inc_dialog;

	g_return_val_if_fail(inc_session != NULL, -1);

	inc_dialog = (IncProgressDialog *)inc_session->data;

	inc_recv_data_progressive(session, 0, 0, inc_session);

	gdk_threads_enter();

	if (POP3_SESSION(session)->state == POP3_LOGOUT) {
		inc_progress_dialog_update(inc_dialog, inc_session);
		inc_update_folderview(inc_dialog, inc_session);
	}

	gdk_threads_leave();
	return 0;
}

static gint inc_recv_message(Session *session, const gchar *msg, gpointer data)
{
	IncSession *inc_session = (IncSession *)data;
	IncProgressDialog *inc_dialog;
	Pop3Session *pop3_session = POP3_SESSION(session);

	g_return_val_if_fail(inc_session != NULL, -1);

	inc_dialog = (IncProgressDialog *)inc_session->data;

	switch (POP3_SESSION(session)->state) {
	case POP3_GETAUTH_USER:
	case POP3_GETAUTH_PASS:
	case POP3_GETAUTH_APOP:
	case POP3_GETRANGE_STAT:
	case POP3_GETRANGE_LAST:
	case POP3_GETRANGE_UIDL:
	case POP3_GETSIZE_LIST:
		gdk_threads_enter();
		inc_progress_dialog_update(inc_dialog, inc_session);
		gdk_threads_leave();
		break;
	case POP3_RETR_RECV:
		if (inc_session->retr_count == 0) {
			inc_session->start_num = pop3_session->cur_msg;
			inc_session->start_recv_bytes = pop3_session->cur_total_bytes;
			inc_session->cur_total_bytes = pop3_session->cur_total_bytes;
#if 0
			g_print("total_bytes_to_recv = %lld total_num_to_recv = %d\n", pop3_session->total_bytes - inc_session->start_recv_bytes, pop3_session->count - inc_session->start_num + 1);
			g_print("pop: total_bytes = %lld cur_total_bytes = %lld\n", pop3_session->total_bytes, pop3_session->cur_total_bytes);
			g_print("pop: count = %d cur_msg = %d\n", pop3_session->count, pop3_session->cur_msg);
#endif
			inc_session->retr_count++;
			gdk_threads_enter();
			inc_progress_dialog_update(inc_dialog, inc_session);
			gdk_threads_leave();
		} else {
			inc_session->retr_count++;
			inc_recv_data_progressive(session, 0, 0, inc_session);
		}
		break;
	case POP3_LOGOUT:
		gdk_threads_enter();
		inc_progress_dialog_update(inc_dialog, inc_session);
		inc_update_folderview(inc_dialog, inc_session);
		gdk_threads_leave();
		break;
	default:
		break;
	}

	return 0;
}

/**
 * inc_drop_message:
 * @session: Current Pop3Session.
 * @file: Received message file.
 * 
 * Callback function to drop received message into local mailbox.
 *
 * Return value: DROP_OK if succeeded. DROP_ERROR if error occurred.
 *   DROP_DONT_RECEIVE if the message should be skipped.
 *   DROP_DELETE if the message should be deleted.
 **/
static gint inc_drop_message(Pop3Session *session, const gchar *file)
{
	FolderItem *inbox;
	GSList *cur;
	MsgInfo *msginfo;
	FilterInfo *fltinfo;
	IncSession *inc_session = (IncSession *)(SESSION(session)->data);
	gint val;
	gboolean is_junk = FALSE;
	gboolean is_counted = FALSE;
	IncProgressDialog *inc_dialog;

	g_return_val_if_fail(inc_session != NULL, DROP_ERROR);

	gdk_threads_enter();

	inc_dialog = (IncProgressDialog *)inc_session->data;

	if (session->ac_prefs->inbox) {
		inbox = folder_find_item_from_identifier
			(session->ac_prefs->inbox);
		if (!inbox)
			inbox = folder_get_default_inbox();
	} else
		inbox = folder_get_default_inbox();
	if (!inbox) {
		gdk_threads_leave();
		return DROP_ERROR;
	}

	fltinfo = filter_info_new();
	fltinfo->account = session->ac_prefs;
	fltinfo->flags.perm_flags = MSG_NEW|MSG_UNREAD;
	fltinfo->flags.tmp_flags = MSG_RECEIVED;

	msginfo = procheader_parse_file(file, fltinfo->flags, FALSE);
	if (!msginfo) {
		g_warning("inc_drop_message: procheader_parse_file failed");
		filter_info_free(fltinfo);
		gdk_threads_leave();
		return DROP_ERROR;
	}
	fltinfo->flags = msginfo->flags;
	msginfo->file_path = g_strdup(file);

	if (prefs_common.enable_junk &&
	    prefs_common.filter_junk_on_recv &&
	    prefs_common.filter_junk_before &&
	    inc_session->junk_fltlist) {
		filter_apply_msginfo(inc_session->junk_fltlist, msginfo,
				     fltinfo);
		if (fltinfo->drop_done)
			is_junk = TRUE;
		else if (fltinfo->error == FLT_ERROR_EXEC_FAILED ||
			 fltinfo->last_exec_exit_status >= 3) {
			g_warning("inc_drop_message: junk filter command returned %d",
				  fltinfo->last_exec_exit_status);
			alertpanel_error
				(_("Execution of the junk filter command failed.\n"
				   "Please check the junk mail control setting."));
			procmsg_msginfo_free(msginfo);
			filter_info_free(fltinfo);
			inc_session->inc_state = INC_ERROR;
			gdk_threads_leave();
			return DROP_ERROR;
		}
	}

	if (!fltinfo->drop_done && session->ac_prefs->filter_on_recv)
		filter_apply_msginfo(prefs_common.fltlist, msginfo, fltinfo);

	if (!fltinfo->drop_done) {
		if (prefs_common.enable_junk &&
		    prefs_common.filter_junk_on_recv &&
		    !prefs_common.filter_junk_before &&
		    inc_session->junk_fltlist) {
			filter_apply_msginfo(inc_session->junk_fltlist,
					     msginfo, fltinfo);
			if (fltinfo->drop_done)
				is_junk = TRUE;
			else if (fltinfo->error == FLT_ERROR_EXEC_FAILED ||
				 fltinfo->last_exec_exit_status >= 3) {
				g_warning("inc_drop_message: junk filter command returned %d",
					  fltinfo->last_exec_exit_status);
				alertpanel_error
					(_("Execution of the junk filter command failed.\n"
					   "Please check the junk mail control setting."));
				procmsg_msginfo_free(msginfo);
				filter_info_free(fltinfo);
				inc_session->inc_state = INC_ERROR;
				gdk_threads_leave();
				return DROP_ERROR;
			}
		}
	}

	if (!fltinfo->drop_done) {
		msginfo->flags = fltinfo->flags;
		if (folder_item_add_msg_msginfo(inbox, msginfo, FALSE) < 0) {
			procmsg_msginfo_free(msginfo);
			filter_info_free(fltinfo);
			gdk_threads_leave();
			return DROP_ERROR;
		}
		fltinfo->dest_list = g_slist_append(fltinfo->dest_list, inbox);
	}

	for (cur = fltinfo->dest_list; cur != NULL; cur = cur->next) {
		FolderItem *drop_folder = (FolderItem *)cur->data;

		val = GPOINTER_TO_INT(g_hash_table_lookup
				      (inc_session->folder_table, drop_folder));
		if (val == 0)
			g_hash_table_insert(inc_session->folder_table,
					    drop_folder, GINT_TO_POINTER(1));
		g_hash_table_insert(inc_session->tmp_folder_table, drop_folder,
				    GINT_TO_POINTER(1));

		if (drop_folder->stype != F_TRASH &&
		    drop_folder->stype != F_JUNK)
			is_counted = TRUE;
	}

	if (fltinfo->actions[FLT_ACTION_NOT_RECEIVE] == TRUE)
		val = DROP_DONT_RECEIVE;
	else if (fltinfo->actions[FLT_ACTION_DELETE] == TRUE)
		val = DROP_DELETE;
	else {
		val = DROP_OK;
		if (!is_junk && is_counted &&
		    fltinfo->actions[FLT_ACTION_MARK_READ] == FALSE) {
			inc_session->new_msgs++;

			if (inc_dialog->result &&
			    msginfo->subject && msginfo->fromname &&
			    g_slist_length(inc_dialog->result->msg_summaries) < 5) {
				IncMsgSummary *summary;
				summary = g_new(IncMsgSummary, 1);
				summary->subject = g_strdup(msginfo->subject);
				summary->from = g_strdup(msginfo->fromname);
				inc_dialog->result->msg_summaries = g_slist_append(inc_dialog->result->msg_summaries, summary);
			}
		}
	}

	procmsg_msginfo_free(msginfo);
	filter_info_free(fltinfo);

	gdk_threads_leave();
	return val;
}

static void inc_put_error(IncSession *session, IncState istate, const gchar *pop3_msg)
{
	gchar *log_msg = NULL;
	gchar *err_msg = NULL;
	gboolean fatal_error = FALSE;

	switch (istate) {
	case INC_LOOKUP_ERROR:
		log_msg = _("Server not found.");
		if (prefs_common.no_recv_err_panel)
			break;
		err_msg = g_strdup_printf
			(_("Server %s not found."), session->session->server);
		break;
	case INC_CONNECT_ERROR:
		log_msg = _("Connection failed.");
		if (prefs_common.no_recv_err_panel)
			break;
		err_msg = g_strdup_printf
			(_("Connection to %s:%d failed."),
			 session->session->server, session->session->port);
		break;
	case INC_ERROR:
		log_msg = _("Error occurred while processing mail.");
		if (prefs_common.no_recv_err_panel)
			break;
		if (pop3_msg)
			err_msg = g_strdup_printf
				(_("Error occurred while processing mail:\n%s"),
				 pop3_msg);
		else
			err_msg = g_strdup(log_msg);
		break;
	case INC_NO_SPACE:
		log_msg = _("No disk space left.");
		err_msg = g_strdup(log_msg);
		fatal_error = TRUE;
		break;
	case INC_IO_ERROR:
		log_msg = _("Can't write file.");
		err_msg = g_strdup(log_msg);
		fatal_error = TRUE;
		break;
	case INC_SOCKET_ERROR:
		log_msg = _("Socket error.");
		if (prefs_common.no_recv_err_panel)
			break;
		err_msg = g_strdup(log_msg);
		break;
	case INC_EOF:
		log_msg = _("Connection closed by the remote host.");
		if (prefs_common.no_recv_err_panel)
			break;
		err_msg = g_strdup(log_msg);
		break;
	case INC_LOCKED:
		log_msg = _("Mailbox is locked.");
		if (prefs_common.no_recv_err_panel)
			break;
		if (pop3_msg)
			err_msg = g_strdup_printf(_("Mailbox is locked:\n%s"),
						  pop3_msg);
		else
			err_msg = g_strdup(log_msg);
		break;
	case INC_AUTH_FAILED:
		log_msg = _("Authentication failed.");
		if (prefs_common.no_recv_err_panel)
			break;
		if (pop3_msg)
			err_msg = g_strdup_printf
				(_("Authentication failed:\n%s"), pop3_msg);
		else
			err_msg = g_strdup(log_msg);
		break;
	case INC_TIMEOUT:
		log_msg = _("Session timed out.");
		if (prefs_common.no_recv_err_panel)
			break;
		err_msg = g_strdup(log_msg);
		break;
	default:
		break;
	}

	if (log_msg) {
		if (fatal_error)
			log_error("%s\n", log_msg);
		else
			log_warning("%s\n", log_msg);
	}
	if (err_msg) {
		alertpanel_error("%s", err_msg);
		g_free(err_msg);
	}
}

static void inc_cancel(IncProgressDialog *dialog, gboolean cancel_all)
{
	IncSession *session;
	GList *list;

	g_return_if_fail(dialog != NULL);

	if (dialog->queue_list == NULL) {
		inc_progress_dialog_destroy(dialog);
		return;
	}

	for (list = dialog->queue_list; list != NULL; list = list->next) {
		session = list->data;
		session->inc_state = INC_CANCEL;
		session_disconnect(session->session);
		if (!cancel_all)
			break;
	}

	log_message(_("Incorporation cancelled\n"));
}

gboolean inc_is_active(void)
{
	GList *cur;

	if (inc_is_running)
		return TRUE;

	if (inc_dialog_list == NULL)
		return FALSE;

	for (cur = inc_dialog_list; cur != NULL; cur = cur->next) {
		IncProgressDialog *dialog = cur->data;
		if (dialog->queue_list)
			return TRUE;
	}

	return FALSE;
}

void inc_block_notify(gboolean block)
{
	if (!block)
		block_notify = FALSE;
	else if (inc_is_active())
		block_notify = TRUE;
}

void inc_cancel_all(void)
{
	GList *cur;

	for (cur = inc_dialog_list; cur != NULL; cur = cur->next)
		inc_cancel((IncProgressDialog *)cur->data, TRUE);
}

static void inc_cancel_cb(GtkWidget *widget, gpointer data)
{
	inc_cancel((IncProgressDialog *)data, FALSE);
}

static void inc_cancel_all_cb(GtkWidget *widget, gpointer data)
{
	inc_cancel((IncProgressDialog *)data, TRUE);
}

static gint inc_dialog_delete_cb(GtkWidget *widget, GdkEventAny *event,
				 gpointer data)
{
	IncProgressDialog *dialog = (IncProgressDialog *)data;

	if (dialog->queue_list == NULL)
		inc_progress_dialog_destroy(dialog);
	else
		inc_cancel(dialog, TRUE);

	return TRUE;
}

static gint inc_spool(void)
{
	gchar *spool_path;
	gchar *mbox;
	gint msgs;

	spool_path = prefs_common.spool_path
		? prefs_common.spool_path : DEFAULT_SPOOL_PATH;
	if (is_file_exist(spool_path))
		mbox = g_strdup(spool_path);
	else if (is_dir_exist(spool_path))
		mbox = g_strconcat(spool_path, G_DIR_SEPARATOR_S,
				   g_get_user_name(), NULL);
	else {
		debug_print("%s: local mailbox not found.\n", spool_path);
		return -1;
	}

	msgs = get_spool(folder_get_default_inbox(), mbox);
	g_free(mbox);

	return msgs;
}

static gint get_spool(FolderItem *dest, const gchar *mbox)
{
	gint msgs, size;
	gint lockfd;
	LockType locktype = LOCK_FLOCK;
	gchar tmp_mbox[MAXPATHLEN + 1];
	GHashTable *folder_table = NULL;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(mbox != NULL, -1);

	if (!is_file_exist(mbox) || (size = get_file_size(mbox)) == 0) {
		debug_print("%s: no messages in local mailbox.\n", mbox);
		return 0;
	} else if (size < 0)
		return -1;

	if ((lockfd = lock_mbox(mbox, locktype)) < 0) {
		locktype = LOCK_FILE;
		if ((lockfd = lock_mbox(mbox, locktype)) < 0)
			return -1;
	}

	g_snprintf(tmp_mbox, sizeof(tmp_mbox), "%s%ctmpmbox.%p",
		   get_tmp_dir(), G_DIR_SEPARATOR, mbox);

	if (copy_mbox(mbox, tmp_mbox) < 0) {
		unlock_mbox(mbox, lockfd, locktype);
		return -1;
	}

	debug_print(_("Getting new messages from %s into %s...\n"),
		    mbox, dest->path);

	folder_table = g_hash_table_new(NULL, NULL);

	msgs = proc_mbox_full(dest, tmp_mbox, folder_table,
			      prefs_common.filter_on_inc,
			      prefs_common.enable_junk &&
			      prefs_common.filter_junk_on_recv);

	g_unlink(tmp_mbox);
	if (msgs >= 0) empty_mbox(mbox);
	unlock_mbox(mbox, lockfd, locktype);

	if (!prefs_common.scan_all_after_inc) {
		inc_update_folder_foreach(folder_table);
	}

	g_hash_table_destroy(folder_table);

	return msgs;
}

void inc_lock(void)
{
	inc_lock_count++;
}

void inc_unlock(void)
{
	if (inc_lock_count > 0)
		inc_lock_count--;
}

static guint autocheck_timer = 0;
static gpointer autocheck_data = NULL;

void inc_autocheck_timer_init(MainWindow *mainwin)
{
	autocheck_data = mainwin;
	inc_autocheck_timer_set();
}

static void inc_autocheck_timer_set_interval(guint interval)
{
	inc_autocheck_timer_remove();

	if (prefs_common.autochk_newmail && autocheck_data) {
		autocheck_timer = g_timeout_add_full
			(G_PRIORITY_LOW, interval, inc_autocheck_func,
			 autocheck_data, NULL);
		debug_print("added timer = %d\n", autocheck_timer);
	}
}

void inc_autocheck_timer_set(void)
{
	inc_autocheck_timer_set_interval(prefs_common.autochk_itv * 60000);
}

void inc_autocheck_timer_remove(void)
{
	if (autocheck_timer) {
		debug_print("removed timer = %d\n", autocheck_timer);
		g_source_remove(autocheck_timer);
		autocheck_timer = 0;
	}
}

static gint inc_autocheck_func(gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	gdk_threads_enter();

	if (inc_lock_count) {
		debug_print("autocheck is locked.\n");
		inc_autocheck_timer_set_interval(1000);
		gdk_threads_leave();
		return FALSE;
	}

	inc_all_account_mail(mainwin, TRUE);

	gdk_threads_leave();

	return FALSE;
}
