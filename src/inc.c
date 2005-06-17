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
#include <gtk/gtkstock.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

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
#include "ssl.h"
#include "pop.h"
#include "recv.h"
#include "mbox.h"
#include "utils.h"
#include "gtkutils.h"
#include "statusbar.h"
#include "manage_window.h"
#include "stock_pixmap.h"
#include "progressdialog.h"
#include "inputdialog.h"
#include "alertpanel.h"
#include "filter.h"
#include "folder.h"

static GList *inc_dialog_list = NULL;

static guint inc_lock_count = 0;

static GdkPixbuf *current_pixbuf;
static GdkPixbuf *error_pixbuf;
static GdkPixbuf *ok_pixbuf;

#define MSGBUFSIZE	8192

static void inc_finished		(MainWindow		*mainwin,
					 gint			 new_messages);
static gint inc_account_mail_real	(MainWindow		*mainwin,
					 PrefsAccount		*account);

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

static void inc_put_error		(IncState	 istate,
					 const gchar	*msg);

static void inc_cancel_cb		(GtkWidget	*widget,
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
 * @new_messages: Number of received messages.
 * 
 * Update the folder view and the summary view after receiving
 * messages.  If @new_messages is 0, this function avoids unneeded
 * updating.
 **/
static void inc_finished(MainWindow *mainwin, gint new_messages)
{
	FolderItem *item;

	debug_print("inc_finished(): %d new message(s)\n", new_messages);

	if (prefs_common.scan_all_after_inc)
		folderview_check_new(NULL);

	if (new_messages <= 0 && !prefs_common.scan_all_after_inc) return;

	if (prefs_common.open_inbox_on_inc) {
		item = cur_account && cur_account->inbox
			? folder_find_item_from_identifier(cur_account->inbox)
			: folder_get_default_inbox();
		folderview_unselect(mainwin->folderview);
		folderview_select(mainwin->folderview, item);
	} else if (prefs_common.scan_all_after_inc) {
		item = mainwin->summaryview->folder_item;
		if (item)
			folderview_update_item(item, TRUE);
	}

	if (new_messages > 0 &&
	    prefs_common.enable_newmsg_notify &&
	    prefs_common.newmsg_notify_cmd) {
		gchar buf[1024];
		gchar *p;

		if ((p = strchr(prefs_common.newmsg_notify_cmd, '%')) &&
		    *(p + 1) == 'd' && !strchr(p + 2, '%'))
			g_snprintf(buf, sizeof(buf),
				   prefs_common.newmsg_notify_cmd,
				   new_messages);
		else
			strncpy2(buf, prefs_common.newmsg_notify_cmd,
				 sizeof(buf));
		execute_command_line(buf, TRUE);
	}
}

void inc_mail(MainWindow *mainwin)
{
	gint new_msgs = 0;

	if (inc_lock_count) return;

	if (!main_window_toggle_online_if_offline(mainwin))
		return;

	inc_autocheck_timer_remove();
	summary_write_cache(mainwin->summaryview);
	main_window_lock(mainwin);

	if (prefs_common.use_extinc && prefs_common.extinc_cmd) {
		/* external incorporating program */
		if (execute_command_line(prefs_common.extinc_cmd, FALSE) != 0) {
			main_window_unlock(mainwin);
			inc_autocheck_timer_set();
			return;
		}

		if (prefs_common.inc_local)
			new_msgs = inc_spool();
	} else {
		if (prefs_common.inc_local) {
			new_msgs = inc_spool();
			if (new_msgs < 0)
				new_msgs = 0;
		}

		new_msgs += inc_account_mail_real(mainwin, cur_account);
	}

	inc_finished(mainwin, new_msgs);
	main_window_unlock(mainwin);
	inc_autocheck_timer_set();
}

static gint inc_account_mail_real(MainWindow *mainwin, PrefsAccount *account)
{
	IncProgressDialog *inc_dialog;
	IncSession *session;

	if (account->protocol == A_IMAP4 || account->protocol == A_NNTP) {
		FolderItem *item = mainwin->summaryview->folder_item;

		folderview_check_new(FOLDER(account->folder));
		if (!prefs_common.scan_all_after_inc && item != NULL &&
		    FOLDER(account->folder) == item->folder)
			folderview_update_item(item, TRUE);
		return 1;
	}

	session = inc_session_new(account);
	if (!session) return 0;

	inc_dialog = inc_progress_dialog_create(FALSE);
	inc_dialog->queue_list = g_list_append(inc_dialog->queue_list, session);
	inc_dialog->mainwin = mainwin;
	inc_progress_dialog_set_list(inc_dialog);

	main_window_set_toolbar_sensitive(mainwin);
	main_window_set_menu_sensitive(mainwin);

	return inc_start(inc_dialog);
}

gint inc_account_mail(MainWindow *mainwin, PrefsAccount *account)
{
	gint new_msgs;

	if (inc_lock_count) return 0;

	if (!main_window_toggle_online_if_offline(mainwin))
		return 0;

	inc_autocheck_timer_remove();
	summary_write_cache(mainwin->summaryview);
	main_window_lock(mainwin);

	new_msgs = inc_account_mail_real(mainwin, account);

	inc_finished(mainwin, new_msgs);
	main_window_unlock(mainwin);
	inc_autocheck_timer_set();

	return new_msgs;
}

void inc_all_account_mail(MainWindow *mainwin, gboolean autocheck)
{
	GList *list, *queue_list = NULL;
	IncProgressDialog *inc_dialog;
	gint new_msgs = 0;

	if (inc_lock_count) return;

	if (!main_window_toggle_online_if_offline(mainwin))
		return;

	inc_autocheck_timer_remove();
	summary_write_cache(mainwin->summaryview);
	main_window_lock(mainwin);

	if (prefs_common.inc_local) {
		new_msgs = inc_spool();
		if (new_msgs < 0)
			new_msgs = 0;
	}

	/* check IMAP4 / News folders */
	for (list = account_get_list(); list != NULL; list = list->next) {
		PrefsAccount *account = list->data;
		if ((account->protocol == A_IMAP4 ||
		     account->protocol == A_NNTP) && account->recv_at_getall) {
			FolderItem *item = mainwin->summaryview->folder_item;

			folderview_check_new(FOLDER(account->folder));
			if (!prefs_common.scan_all_after_inc && item != NULL &&
			    FOLDER(account->folder) == item->folder)
				folderview_update_item(item, TRUE);
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
		inc_progress_dialog_set_list(inc_dialog);

		main_window_set_toolbar_sensitive(mainwin);
		main_window_set_menu_sensitive(mainwin);

		new_msgs += inc_start(inc_dialog);
	}

	inc_finished(mainwin, new_msgs);
	main_window_unlock(mainwin);
	inc_autocheck_timer_set();
}

static IncProgressDialog *inc_progress_dialog_create(gboolean autocheck)
{
	IncProgressDialog *dialog;
	ProgressDialog *progress;

	dialog = g_new0(IncProgressDialog, 1);

	progress = progress_dialog_create();
	gtk_window_set_title(GTK_WINDOW(progress->window),
			     _("Retrieving new messages"));
	g_signal_connect(G_OBJECT(progress->cancel_btn), "clicked",
			 G_CALLBACK(inc_cancel_cb), dialog);
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
	gettimeofday(&dialog->progress_tv, NULL);
	gettimeofday(&dialog->folder_tv, NULL);
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
				       _("Standby"), NULL);
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

	session->folder_table = g_hash_table_new(NULL, NULL);
	session->tmp_folder_table = g_hash_table_new(NULL, NULL);

	return session;
}

static void inc_session_destroy(IncSession *session)
{
	g_return_if_fail(session != NULL);

	session_destroy(session->session);
	g_hash_table_destroy(session->folder_table);
	g_hash_table_destroy(session->tmp_folder_table);
	g_free(session);
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
		pop3_session = POP3_SESSION(session->session); 
		pop3_session->user = g_strdup(pop3_session->ac_prefs->userid);
		if (pop3_session->ac_prefs->passwd)
			pop3_session->pass =
				g_strdup(pop3_session->ac_prefs->passwd);
		else if (pop3_session->ac_prefs->tmp_pass)
			pop3_session->pass =
				g_strdup(pop3_session->ac_prefs->tmp_pass);
		else {
			gchar *pass;

			if (inc_dialog->show_dialog)
				manage_window_focus_in
					(inc_dialog->dialog->window,
					 NULL, NULL);

			pass = input_dialog_query_password
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

#define SET_PIXMAP_AND_TEXT(pixbuf, str)				\
{									\
	progress_dialog_set_row_pixbuf(inc_dialog->dialog,		\
				       inc_dialog->cur_row, pixbuf);	\
	progress_dialog_set_row_status(inc_dialog->dialog,		\
				       inc_dialog->cur_row, str);	\
}

	for (; inc_dialog->queue_list != NULL; inc_dialog->cur_row++) {
		session = inc_dialog->queue_list->data;
		pop3_session = POP3_SESSION(session->session);

		if (pop3_session->pass == NULL) {
			SET_PIXMAP_AND_TEXT(ok_pixbuf, _("Cancelled"));
			inc_session_destroy(session);
			inc_dialog->queue_list =
				g_list_remove(inc_dialog->queue_list, session);
			continue;
		}

		inc_progress_dialog_clear(inc_dialog);
		progress_dialog_scroll_to_row(inc_dialog->dialog,
					      inc_dialog->cur_row);

		SET_PIXMAP_AND_TEXT(current_pixbuf, _("Retrieving"));

		/* begin POP3 session */
		inc_state = inc_pop3_session_do(session);

		switch (inc_state) {
		case INC_SUCCESS:
			if (pop3_session->cur_total_num > 0)
				msg = g_strdup_printf
					(_("Done (%d message(s) (%s) received)"),
					 pop3_session->cur_total_num,
					 to_human_readable(pop3_session->cur_total_recv_bytes));
			else
				msg = g_strdup_printf(_("Done (no new messages)"));
			SET_PIXMAP_AND_TEXT(ok_pixbuf, msg);
			g_free(msg);
			break;
		case INC_CONNECT_ERROR:
			SET_PIXMAP_AND_TEXT(error_pixbuf,
					    _("Connection failed"));
			break;
		case INC_AUTH_FAILED:
			SET_PIXMAP_AND_TEXT(error_pixbuf, _("Auth failed"));
			break;
		case INC_LOCKED:
			SET_PIXMAP_AND_TEXT(error_pixbuf, _("Locked"));
			break;
		case INC_ERROR:
		case INC_NO_SPACE:
		case INC_IO_ERROR:
		case INC_SOCKET_ERROR:
		case INC_EOF:
			SET_PIXMAP_AND_TEXT(error_pixbuf, _("Error"));
			break;
		case INC_TIMEOUT:
			SET_PIXMAP_AND_TEXT(error_pixbuf, _("Timeout"));
			break;
		case INC_CANCEL:
			SET_PIXMAP_AND_TEXT(ok_pixbuf, _("Cancelled"));
			break;
		default:
			break;
		}

		new_msgs += pop3_session->cur_total_num;

		if (!prefs_common.scan_all_after_inc) {
			folder_item_scan_foreach(session->folder_table);
			folderview_update_item_foreach
				(session->folder_table,
				 !prefs_common.open_inbox_on_inc);
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
			inc_put_error(inc_state, pop3_session->error_msg);
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
	gchar *server;
	gushort port;
	gchar *buf;

	debug_print(_("getting new messages of account %s...\n"),
		    pop3_session->ac_prefs->account_name);

	buf = g_strdup_printf(_("%s: Retrieving new messages"),
			      pop3_session->ac_prefs->recv_server);
	gtk_window_set_title(GTK_WINDOW(inc_dialog->dialog->window), buf);
	g_free(buf);

	server = pop3_session->ac_prefs->recv_server;
#if USE_SSL
	port = pop3_session->ac_prefs->set_popport ?
		pop3_session->ac_prefs->popport :
		pop3_session->ac_prefs->ssl_pop == SSL_TUNNEL ? 995 : 110;
	SESSION(pop3_session)->ssl_type = pop3_session->ac_prefs->ssl_pop;
	if (pop3_session->ac_prefs->ssl_pop != SSL_NONE)
		SESSION(pop3_session)->nonblocking =
			pop3_session->ac_prefs->use_nonblocking_ssl;
#else
	port = pop3_session->ac_prefs->set_popport ?
		pop3_session->ac_prefs->popport : 110;
#endif

	buf = g_strdup_printf(_("Connecting to POP3 server: %s..."), server);
	log_message("%s\n", buf);
	progress_dialog_set_label(inc_dialog->dialog, buf);
	g_free(buf);

	session_set_timeout(SESSION(pop3_session),
			    prefs_common.io_timeout_secs * 1000);

	if (session_connect(SESSION(pop3_session), server, port) < 0) {
		log_warning(_("Can't connect to POP3 server: %s:%d\n"),
			    server, port);
		session->inc_state = INC_CONNECT_ERROR;
		statusbar_pop_all();
		return INC_CONNECT_ERROR;
	}

	while (session_is_connected(SESSION(pop3_session)) &&
	       session->inc_state != INC_CANCEL)
		gtk_main_iteration();

	if (session->inc_state == INC_SUCCESS) {
		switch (pop3_session->error_val) {
		case PS_SUCCESS:
			switch (SESSION(pop3_session)->state) {
			case SESSION_ERROR:
				if (pop3_session->state == POP3_READY)
					session->inc_state = INC_CONNECT_ERROR;
				else
					session->inc_state = INC_ERROR;
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
			gchar buf[MSGBUFSIZE];
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
	gchar buf[MSGBUFSIZE];
	Pop3Session *pop3_session = POP3_SESSION(inc_session->session);
	gchar *total_size_str;
	gint cur_total;
	gint total;

	if (!pop3_session->new_msg_exist) return;

	cur_total = inc_session->cur_total_bytes;
	total = pop3_session->total_bytes;
	if (pop3_session->state == POP3_RETR ||
	    pop3_session->state == POP3_RETR_RECV ||
	    pop3_session->state == POP3_DELETE) {
		Xstrdup_a(total_size_str, to_human_readable(total), return);
		g_snprintf(buf, sizeof(buf),
			   _("Retrieving message (%d / %d) (%s / %s)"),
			   pop3_session->cur_msg, pop3_session->count,
			   to_human_readable(cur_total), total_size_str);
		progress_dialog_set_label(inc_dialog->dialog, buf);
	}

	progress_dialog_set_percentage
		(inc_dialog->dialog,(gfloat)cur_total / (gfloat)total);

	gtk_progress_set_show_text
		(GTK_PROGRESS(inc_dialog->mainwin->progressbar), TRUE);
	g_snprintf(buf, sizeof(buf), "%d / %d",
		   pop3_session->cur_msg, pop3_session->count);
	gtk_progress_set_format_string
		(GTK_PROGRESS(inc_dialog->mainwin->progressbar), buf);
	gtk_progress_bar_update
		(GTK_PROGRESS_BAR(inc_dialog->mainwin->progressbar),
		 (gfloat)cur_total / (gfloat)total);

	if (pop3_session->cur_total_num > 0) {
		g_snprintf(buf, sizeof(buf),
			   _("Retrieving (%d message(s) (%s) received)"),
			   pop3_session->cur_total_num,
			   to_human_readable
			   (pop3_session->cur_total_recv_bytes));
		progress_dialog_set_row_status(inc_dialog->dialog,
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
	if (g_hash_table_size(inc_session->tmp_folder_table) > 0) {
		folderview_update_item_foreach(inc_session->tmp_folder_table,
					       FALSE);
		g_hash_table_foreach_remove(inc_session->tmp_folder_table,
					    hash_remove_func, NULL);
	}
}

static void inc_progress_dialog_update_periodic(IncProgressDialog *inc_dialog,
						IncSession *inc_session)
{
	struct timeval tv_cur;
	struct timeval tv_result;
	gint msec;

	gettimeofday(&tv_cur, NULL);

	tv_result.tv_sec = tv_cur.tv_sec - inc_dialog->progress_tv.tv_sec;
	tv_result.tv_usec = tv_cur.tv_usec - inc_dialog->progress_tv.tv_usec;
	if (tv_result.tv_usec < 0) {
		tv_result.tv_sec--;
		tv_result.tv_usec += 1000000;
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
	struct timeval tv_cur;
	struct timeval tv_result;
	gint msec;

	gettimeofday(&tv_cur, NULL);

	tv_result.tv_sec = tv_cur.tv_sec - inc_dialog->folder_tv.tv_sec;
	tv_result.tv_usec = tv_cur.tv_usec - inc_dialog->folder_tv.tv_usec;
	if (tv_result.tv_usec < 0) {
		tv_result.tv_sec--;
		tv_result.tv_usec += 1000000;
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
	gint cur_total;

	g_return_val_if_fail(inc_session != NULL, -1);

	if (pop3_session->state != POP3_RETR &&
	    pop3_session->state != POP3_RETR_RECV &&
	    pop3_session->state != POP3_DELETE &&
	    pop3_session->state != POP3_LOGOUT) return 0;

	if (!pop3_session->new_msg_exist) return 0;

	cur_total = pop3_session->cur_total_bytes + cur_len;
	if (cur_total > pop3_session->total_bytes)
		cur_total = pop3_session->total_bytes;
	inc_session->cur_total_bytes = cur_total;

	inc_dialog = (IncProgressDialog *)inc_session->data;
	inc_progress_dialog_update_periodic(inc_dialog, inc_session);
	inc_update_folderview_periodic(inc_dialog, inc_session);

	return 0;
}

static gint inc_recv_data_finished(Session *session, guint len, gpointer data)
{
	IncSession *inc_session = (IncSession *)data;
	IncProgressDialog *inc_dialog;

	g_return_val_if_fail(inc_session != NULL, -1);

	inc_dialog = (IncProgressDialog *)inc_session->data;

	inc_recv_data_progressive(session, 0, 0, inc_session);

	if (POP3_SESSION(session)->state == POP3_LOGOUT) {
		inc_progress_dialog_update(inc_dialog, inc_session);
		inc_update_folderview(inc_dialog, inc_session);
	}

	return 0;
}

static gint inc_recv_message(Session *session, const gchar *msg, gpointer data)
{
	IncSession *inc_session = (IncSession *)data;
	IncProgressDialog *inc_dialog;

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
		inc_progress_dialog_update(inc_dialog, inc_session);
		break;
	case POP3_RETR:
		inc_recv_data_progressive(session, 0, 0, inc_session);
		break;
	case POP3_LOGOUT:
		inc_progress_dialog_update(inc_dialog, inc_session);
		inc_update_folderview(inc_dialog, inc_session);
		break;
	default:
		break;
	}

	return 0;
}

static gint inc_drop_message(Pop3Session *session, const gchar *file)
{
	FolderItem *inbox;
	GSList *cur;
	FilterInfo *fltinfo;
	IncSession *inc_session = (IncSession *)(SESSION(session)->data);
	gint val;

	g_return_val_if_fail(inc_session != NULL, DROP_ERROR);

	if (session->ac_prefs->inbox) {
		inbox = folder_find_item_from_identifier
			(session->ac_prefs->inbox);
		if (!inbox)
			inbox = folder_get_default_inbox();
	} else
		inbox = folder_get_default_inbox();
	if (!inbox)
		return DROP_ERROR;

	fltinfo = filter_info_new();
	fltinfo->account = session->ac_prefs;
	fltinfo->flags.perm_flags = MSG_NEW|MSG_UNREAD;
	fltinfo->flags.tmp_flags = MSG_RECEIVED;

	if (session->ac_prefs->filter_on_recv)
		filter_apply(prefs_common.fltlist, file, fltinfo);
	if (!fltinfo->drop_done) {
		if (prefs_common.enable_junk &&
		    prefs_common.filter_junk_on_recv)
			filter_apply(prefs_common.junk_fltlist, file, fltinfo);
	}

	if (!fltinfo->drop_done) {
		if (folder_item_add_msg
			(inbox, file, &fltinfo->flags, FALSE) < 0) {
			filter_info_free(fltinfo);
			return DROP_ERROR;
		}
		fltinfo->dest_list = g_slist_append(fltinfo->dest_list, inbox);
	}

	for (cur = fltinfo->dest_list; cur != NULL; cur = cur->next) {
		FolderItem *drop_folder = (FolderItem *)cur->data;

		val = GPOINTER_TO_INT(g_hash_table_lookup
				      (inc_session->folder_table, drop_folder));
		if (val == 0) {
			/* force updating */
			if (FOLDER_IS_LOCAL(drop_folder->folder))
				drop_folder->mtime = 0;
			g_hash_table_insert(inc_session->folder_table, drop_folder,
					    GINT_TO_POINTER(1));
		}
		g_hash_table_insert(inc_session->tmp_folder_table, drop_folder,
				    GINT_TO_POINTER(1));
	}

	if (fltinfo->actions[FLT_ACTION_NOT_RECEIVE] == TRUE)
		val = DROP_DONT_RECEIVE;
	else if (fltinfo->actions[FLT_ACTION_DELETE] == TRUE)
		val = DROP_DELETE;
	else
		val = DROP_OK;

	filter_info_free(fltinfo);

	return val;
}

static void inc_put_error(IncState istate, const gchar *msg)
{
	gchar *log_msg = NULL;
	gchar *err_msg = NULL;
	gboolean fatal_error = FALSE;

	switch (istate) {
	case INC_CONNECT_ERROR:
		log_msg = _("Connection failed.");
		if (prefs_common.no_recv_err_panel)
			break;
		err_msg = g_strdup(log_msg);
		break;
	case INC_ERROR:
		log_msg = _("Error occurred while processing mail.");
		if (prefs_common.no_recv_err_panel)
			break;
		if (msg)
			err_msg = g_strdup_printf
				(_("Error occurred while processing mail:\n%s"),
				 msg);
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
		if (msg)
			err_msg = g_strdup_printf(_("Mailbox is locked:\n%s"),
						  msg);
		else
			err_msg = g_strdup(log_msg);
		break;
	case INC_AUTH_FAILED:
		log_msg = _("Authentication failed.");
		if (prefs_common.no_recv_err_panel)
			break;
		if (msg)
			err_msg = g_strdup_printf
				(_("Authentication failed:\n%s"), msg);
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
		alertpanel_error(err_msg);
		g_free(err_msg);
	}
}

static void inc_cancel(IncProgressDialog *dialog)
{
	IncSession *session;

	g_return_if_fail(dialog != NULL);

	if (dialog->queue_list == NULL) {
		inc_progress_dialog_destroy(dialog);
		return;
	}

	session = dialog->queue_list->data;

	session->inc_state = INC_CANCEL;

	log_message(_("Incorporation cancelled\n"));
}

gboolean inc_is_active(void)
{
	return (inc_dialog_list != NULL);
}

void inc_cancel_all(void)
{
	GList *cur;

	for (cur = inc_dialog_list; cur != NULL; cur = cur->next)
		inc_cancel((IncProgressDialog *)cur->data);
}

static void inc_cancel_cb(GtkWidget *widget, gpointer data)
{
	inc_cancel((IncProgressDialog *)data);
}

static gint inc_dialog_delete_cb(GtkWidget *widget, GdkEventAny *event,
				 gpointer data)
{
	IncProgressDialog *dialog = (IncProgressDialog *)data;

	if (dialog->queue_list == NULL)
		inc_progress_dialog_destroy(dialog);

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
	gchar tmp_mbox[MAXPATHLEN + 1];
	GHashTable *folder_table = NULL;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(mbox != NULL, -1);

	if (!is_file_exist(mbox) || (size = get_file_size(mbox)) == 0) {
		debug_print("%s: no messages in local mailbox.\n", mbox);
		return 0;
	} else if (size < 0)
		return -1;

	if ((lockfd = lock_mbox(mbox, LOCK_FLOCK)) < 0)
		return -1;

	g_snprintf(tmp_mbox, sizeof(tmp_mbox), "%s%ctmpmbox.%p",
		   get_tmp_dir(), G_DIR_SEPARATOR, mbox);

	if (copy_mbox(mbox, tmp_mbox) < 0) {
		unlock_mbox(mbox, lockfd, LOCK_FLOCK);
		return -1;
	}

	debug_print(_("Getting new messages from %s into %s...\n"),
		    mbox, dest->path);

	if (prefs_common.filter_on_inc)
		folder_table = g_hash_table_new(NULL, NULL);
	msgs = proc_mbox(dest, tmp_mbox, folder_table);

	unlink(tmp_mbox);
	if (msgs >= 0) empty_mbox(mbox);
	unlock_mbox(mbox, lockfd, LOCK_FLOCK);

	if (folder_table) {
		if (!prefs_common.scan_all_after_inc) {
			folder_item_scan_foreach(folder_table);
			folderview_update_item_foreach
				(folder_table, !prefs_common.open_inbox_on_inc);
		}
		g_hash_table_destroy(folder_table);
	} else if (!prefs_common.scan_all_after_inc) {
		folder_item_scan(dest);
		folderview_update_item(dest, TRUE);
	}

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
		autocheck_timer = gtk_timeout_add
			(interval, inc_autocheck_func, autocheck_data);
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
		gtk_timeout_remove(autocheck_timer);
		autocheck_timer = 0;
	}
}

static gint inc_autocheck_func(gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	if (inc_lock_count) {
		debug_print("autocheck is locked.\n");
		inc_autocheck_timer_set_interval(1000);
		return FALSE;
	}

	inc_all_account_mail(mainwin, TRUE);

	return FALSE;
}
