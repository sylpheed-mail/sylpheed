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
#include <gtk/gtkwindow.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkstock.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkuimanager.h>
#include <string.h>

#include "rpop3.h"
#include "mainwindow.h"
#include "folderview.h"
#include "prefs_account.h"
#include "socks.h"
#include "pop.h"
#include "procheader.h"
#include "procmsg.h"
#include "folder.h"
#include "inc.h"
#include "utils.h"
#include "gtkutils.h"
#include "manage_window.h"
#include "alertpanel.h"
#include "prefs_common.h"
#include "about.h"

/* POP3 NOOP ping interval (sec) */
#define POP3_PING_ITV	30

#define POP3_TOP	(N_POP3_STATE + 1000)
#define POP3_TOP_RECV	(N_POP3_STATE + 1001)
#define POP3_NOOP	(N_POP3_STATE + 1002)
#define POP3_IDLE	(N_POP3_STATE + 1003)

enum
{
	COL_NUMBER,
	COL_SUBJECT,
	COL_FROM,
	COL_DATE,
	COL_SIZE,
	COL_MSGINFO,
	COL_DELETED,
	N_COLS
};

static struct RPop3Window {
	GtkWidget *window;

	GtkWidget *treeview;
	GtkListStore *store;

	GtkWidget *status_label;

	GtkWidget *recv_btn;
	GtkWidget *open_btn;
	GtkWidget *delete_btn;
	GtkWidget *stop_btn;
	GtkWidget *close_btn;

	GtkAction *recv_action;
	GtkAction *open_action;
	GtkAction *delete_action;
	GtkAction *update_action;
	GtkAction *stop_action;

	Pop3Session *session;
	guint ping_tag;
	gboolean stop_load;
	gboolean cancelled;
	gboolean finished;
	GArray *delete_array;
	gint delete_cur;
	GArray *recv_array;
	gint recv_cur;
} rpop3_window;

static const gchar *ui_def =
	"<ui>"
	"  <menubar name='RPop3Menu'>"
	"    <menu name='File' action='FileAction'>"
	"      <menuitem name='Receive' action='ReceiveAction'/>"
	"      <menuitem name='Open' action='OpenAction'/>"
	"      <menuitem name='Delete' action='DeleteAction'/>"
	"      <separator />"
	"      <menuitem name='Resume' action='ResumeAction'/>"
	"      <menuitem name='Stop' action='StopAction'/>"
	"      <separator />"
	"      <menuitem name='Close' action='CloseAction'/>"
	"    </menu>"
	"    <menu name='Help' action='HelpAction'>"
	"      <menuitem name='About' action='AboutAction'/>"
	"    </menu>"
	"  </menubar>"
	"</ui>";

static void rpop3_window_create	(PrefsAccount	*account);

gint pop3_greeting_recv          (Pop3Session *session,
                                  const gchar *msg);
gint pop3_getauth_user_send      (Pop3Session *session);
gint pop3_getauth_pass_send      (Pop3Session *session);
gint pop3_getauth_apop_send      (Pop3Session *session);
#if USE_SSL
gint pop3_stls_send              (Pop3Session *session);
gint pop3_stls_recv              (Pop3Session *session);
#endif
gint pop3_getrange_stat_send     (Pop3Session *session);
gint pop3_getrange_stat_recv     (Pop3Session *session,
                                  const gchar *msg);
gint pop3_getrange_last_send     (Pop3Session *session);
gint pop3_getrange_last_recv     (Pop3Session *session,
                                  const gchar *msg);
gint pop3_getrange_uidl_send     (Pop3Session *session);
gint pop3_getrange_uidl_recv     (Pop3Session *session,
                                  const gchar *data,
                                  guint        len);
gint pop3_getsize_list_send      (Pop3Session *session);
gint pop3_getsize_list_recv      (Pop3Session *session,
                                  const gchar *data,
                                  guint        len);
gint pop3_retr_send              (Pop3Session *session);
gint pop3_retr_recv              (Pop3Session *session,
                                  FILE        *fp,
                                  guint        len);
gint pop3_logout_send            (Pop3Session *session);

void pop3_gen_send               (Pop3Session    *session,
                                  const gchar    *format, ...);

gint pop3_write_msg_to_file	(const gchar	*file,
				 FILE		*src_fp,
				 guint		 len);

Pop3ErrorValue pop3_ok		(Pop3Session	*session,
				 const gchar	*msg);

static gint rpop3_start			(Session	*session);
static void rpop3_status_label_set	(const gchar	*fmt,
					 ...) G_GNUC_PRINTF(1, 2);
static void rpop3_clear_list		(void);

static void rpop3_idle		(gboolean	 is_idle);

static gint rpop3_noop_send	(Pop3Session	*session);

static gint rpop3_top_send	(Pop3Session	*session);
static gint rpop3_top_recv	(Pop3Session	*session,
				 FILE		*fp,
				 guint		 len);
static gint rpop3_retr_send	(Pop3Session	*session);
static gint rpop3_retr_recv	(Pop3Session	*session,
				 FILE		*fp,
				 guint		 len);
static gint rpop3_delete_send	(Pop3Session	*session);
static gint rpop3_delete_recv	(Pop3Session	*session);

static gint rpop3_session_recv_msg		(Session	*session,
						 const gchar	*msg);
static gint rpop3_session_recv_data_finished	(Session	*session,
						 guchar		*data,
						 guint		 len);
static gint rpop3_session_recv_data_as_file_finished
						(Session	*session,
						 FILE		*fp,
						 guint		 len);

static gint window_deleted	(GtkWidget	*widget,
				 GdkEventAny	*event,
				 gpointer	 data);
static gboolean key_pressed	(GtkWidget	*widget,
				 GdkEventKey	*event,
				 gpointer	 data);

static void rpop3_row_activated	(GtkTreeView		*treeview,
				 GtkTreePath		*path,
				 GtkTreeViewColumn	*column,
				 gpointer		 data);

static void rpop3_recv		(GtkButton	*button,
				 gpointer	 data);
static void rpop3_open		(GtkButton	*button,
				 gpointer	 data);
static void rpop3_delete	(GtkButton	*button,
				 gpointer	 data);
static void rpop3_stop		(GtkButton	*button,
				 gpointer	 data);
static void rpop3_close		(GtkButton	*button,
				 gpointer	 data);

static void rpop3_recv_cb	(void);
static void rpop3_open_cb	(void);
static void rpop3_delete_cb	(void);
static void rpop3_resume_cb	(void);
static void rpop3_stop_cb	(void);
static void rpop3_close_cb	(void);
static void rpop3_about_cb	(void);

static gint cmp_by_subject	(GtkTreeModel	*model,
				 GtkTreeIter	*a,
				 GtkTreeIter	*b,
				 gpointer	 data);
static gint cmp_by_date		(GtkTreeModel	*model,
				 GtkTreeIter	*a,
				 GtkTreeIter	*b,
				 gpointer	 data);
static gint cmp_by_size		(GtkTreeModel	*model,
				 GtkTreeIter	*a,
				 GtkTreeIter	*b,
				 gpointer	 data);

static GtkActionEntry action_entries[] = {
	{"FileAction", NULL, N_("_File"), NULL, NULL, NULL},
	{"ReceiveAction", NULL, N_("_Get"), "<Control>G", NULL, rpop3_recv_cb},
	{"OpenAction", GTK_STOCK_OPEN, NULL, NULL, NULL, rpop3_open_cb},
	{"DeleteAction", GTK_STOCK_DELETE, NULL, "<Shift>Delete", NULL, rpop3_delete_cb},
	{"ResumeAction", GTK_STOCK_REFRESH, N_("_Resume"), NULL, NULL, rpop3_resume_cb},
	{"StopAction", GTK_STOCK_STOP, NULL, NULL, NULL, rpop3_stop_cb},
	{"CloseAction", GTK_STOCK_CLOSE, NULL, NULL, NULL, rpop3_close_cb},
	{"HelpAction", NULL, N_("_Help"), NULL, NULL, NULL},
	{"AboutAction", GTK_STOCK_ABOUT, N_("_About"), NULL, NULL,
	 rpop3_about_cb}
};


gint rpop3_account(PrefsAccount *account)
{
	Session *session;
	gint ret;

	if (!account || account->protocol != A_POP3)
		return -1;
	if (inc_is_active())
		return -1;
	if (rpop3_window.window)
		return -1;

	inc_lock();

	rpop3_window_create(account);

	session = pop3_session_new(account);
	rpop3_window.session = POP3_SESSION(session);
	rpop3_window.stop_load = FALSE;
	rpop3_window.cancelled = FALSE;
	rpop3_window.finished = FALSE;
	if (POP3_SESSION(session)->uidl_table) {
		hash_free_strings(POP3_SESSION(session)->uidl_table);
		g_hash_table_destroy(POP3_SESSION(session)->uidl_table);
		POP3_SESSION(session)->uidl_table =
			g_hash_table_new(g_str_hash, g_str_equal);
	}

	/* override Pop3Session handlers */
	session->recv_msg = rpop3_session_recv_msg;
	session->send_data_finished = NULL;
	session->recv_data_finished = rpop3_session_recv_data_finished;
	session->recv_data_as_file_finished =
		rpop3_session_recv_data_as_file_finished;

	if (!POP3_SESSION(session)->pass) {
		gchar *pass;

		pass = input_query_password(account->recv_server,
					    account->userid);
		if (pass) {
			account->tmp_pass = g_strdup(pass);
			POP3_SESSION(session)->pass = pass;
		}
	}

	session_set_timeout(session, prefs_common.io_timeout_secs * 1000);

	ret = rpop3_start(session);

	while (!rpop3_window.finished)
		gtk_main_iteration();

	if (POP3_SESSION(session)->error_val == PS_AUTHFAIL &&
	    account->tmp_pass) {
		debug_print("rpop3_account: remove temporary password because of authentication failure\n");
		g_free(account->tmp_pass);
		account->tmp_pass = NULL;
	}

	rpop3_idle(FALSE);
	session_destroy(session);
	rpop3_clear_list();
	gtk_widget_destroy(rpop3_window.window);
	memset(&rpop3_window, 0, sizeof(rpop3_window));

	inc_unlock();

	return ret;
}

static void rpop3_window_create(PrefsAccount *account)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkActionGroup *group;
	GtkUIManager *ui;
	GtkAction *action;
	GtkWidget *menubar;
	GtkWidget *vbox2;
	GtkWidget *scrwin;
	GtkWidget *treeview;
	GtkListStore *store;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeSelection *selection;
	GtkWidget *hbox;
	GtkWidget *status_label;
	GtkWidget *hbbox;
	GtkWidget *recv_btn;
	GtkWidget *open_btn;
	GtkWidget *delete_btn;
	GtkWidget *stop_btn;
	GtkWidget *close_btn;
	gchar buf[BUFFSIZE];

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_snprintf(buf, sizeof(buf), _("%s - POP3 Remote mailbox"),
		   account->account_name ? account->account_name : "");
	gtk_window_set_title(GTK_WINDOW(window), buf);
	gtk_widget_set_size_request(window, 640 * gtkut_get_dpi_multiplier(), -1);
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, TRUE);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(window_deleted), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	group = gtk_action_group_new("rpop3");
	gtk_action_group_set_translation_domain(group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions(group, action_entries,
				     sizeof(action_entries) /
				     sizeof(action_entries[0]), NULL);

	ui = gtk_ui_manager_new();
	gtk_ui_manager_insert_action_group(ui, group, 0);
	gtk_ui_manager_add_ui_from_string(ui, ui_def, -1, NULL);
	menubar = gtk_ui_manager_get_widget(ui, "/RPop3Menu");
	gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0);
	gtk_window_add_accel_group(GTK_WINDOW(window),
				   gtk_ui_manager_get_accel_group(ui));

	vbox2 = gtk_vbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(vbox), vbox2, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox2), 8);

	scrwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_box_pack_start(GTK_BOX(vbox2), scrwin, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrwin),
					    GTK_SHADOW_IN);
	gtk_widget_set_size_request(scrwin, -1, 320 * gtkut_get_dpi_multiplier());

	store = gtk_list_store_new(N_COLS, G_TYPE_INT, G_TYPE_STRING,
				   G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
				   G_TYPE_POINTER, G_TYPE_BOOLEAN);
	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(store);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);

	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store), COL_SUBJECT,
					cmp_by_subject, NULL, NULL);
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store), COL_DATE,
					cmp_by_date, NULL, NULL);
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store), COL_SIZE,
					cmp_by_size, NULL, NULL);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);

	gtk_container_add(GTK_CONTAINER(scrwin), treeview);

#define APPEND_COLUMN(label, col, width)				\
{									\
        renderer = gtk_cell_renderer_text_new();			\
        column = gtk_tree_view_column_new_with_attributes		\
		(label, renderer, "text", col,				\
		 "strikethrough", COL_DELETED, NULL);			\
	gtk_tree_view_column_set_resizable(column, TRUE);		\
        if (width) {							\
                gtk_tree_view_column_set_sizing				\
                        (column, GTK_TREE_VIEW_COLUMN_FIXED);		\
                gtk_tree_view_column_set_fixed_width(column, width);	\
	}								\
	gtk_tree_view_column_set_sort_column_id(column, col);		\
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);	\
}

        APPEND_COLUMN(_("No."), COL_NUMBER, 0);
        APPEND_COLUMN(_("Subject"), COL_SUBJECT, 200);
#if GTK_CHECK_VERSION(2, 14, 0)
	gtk_tree_view_column_set_expand(column, TRUE);
#endif
        APPEND_COLUMN(_("From"), COL_FROM, 160);
        APPEND_COLUMN(_("Date"), COL_DATE, 0);
        APPEND_COLUMN(_("Size"), COL_SIZE, 0);

	gtk_widget_show_all(scrwin);

	g_signal_connect(G_OBJECT(treeview), "row-activated",
			 G_CALLBACK(rpop3_row_activated), NULL);

	hbox = gtk_hbox_new(FALSE, 8);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, FALSE, 0);

	status_label = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(hbox), status_label, FALSE, FALSE, 0);

	hbbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(hbbox), 6);
	gtk_box_pack_end(GTK_BOX(vbox2), hbbox, FALSE, FALSE, 0);

	recv_btn = gtk_button_new_with_mnemonic(_("_Get"));
	gtk_box_pack_start(GTK_BOX(hbbox), recv_btn, FALSE, FALSE, 0);
	gtk_widget_set_sensitive(recv_btn, FALSE);
	action = gtk_ui_manager_get_action(ui, "/RPop3Menu/File/Receive");
	g_object_set(action, "sensitive", FALSE, NULL);
	rpop3_window.recv_action = action;

	open_btn = gtk_button_new_from_stock(GTK_STOCK_OPEN);
	gtk_box_pack_start(GTK_BOX(hbbox), open_btn, FALSE, FALSE, 0);
	gtk_widget_set_sensitive(open_btn, FALSE);
	action = gtk_ui_manager_get_action(ui, "/RPop3Menu/File/Open");
	g_object_set(action, "sensitive", FALSE, NULL);
	rpop3_window.open_action = action;

	delete_btn = gtk_button_new_from_stock(GTK_STOCK_DELETE);
	gtk_box_pack_start(GTK_BOX(hbbox), delete_btn, FALSE, FALSE, 0);
	gtk_widget_set_sensitive(delete_btn, FALSE);
	action = gtk_ui_manager_get_action(ui, "/RPop3Menu/File/Delete");
	g_object_set(action, "sensitive", FALSE, NULL);
	rpop3_window.delete_action = action;

	action = gtk_ui_manager_get_action(ui, "/RPop3Menu/File/Resume");
	g_object_set(action, "sensitive", FALSE, NULL);
	rpop3_window.update_action = action;

	stop_btn = gtk_button_new_from_stock(GTK_STOCK_STOP);
	gtk_box_pack_start(GTK_BOX(hbbox), stop_btn, FALSE, FALSE, 0);
	gtk_widget_set_sensitive(stop_btn, FALSE);
	action = gtk_ui_manager_get_action(ui, "/RPop3Menu/File/Stop");
	g_object_set(action, "sensitive", FALSE, NULL);
	rpop3_window.stop_action = action;

	close_btn = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_box_pack_start(GTK_BOX(hbbox), close_btn, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(recv_btn), "clicked",
			 G_CALLBACK(rpop3_recv), NULL);
	g_signal_connect(G_OBJECT(open_btn), "clicked",
			 G_CALLBACK(rpop3_open), NULL);
	g_signal_connect(G_OBJECT(delete_btn), "clicked",
			 G_CALLBACK(rpop3_delete), NULL);
	g_signal_connect(G_OBJECT(stop_btn), "clicked",
			 G_CALLBACK(rpop3_stop), NULL);
	g_signal_connect(G_OBJECT(close_btn), "clicked",
			 G_CALLBACK(rpop3_close), NULL);

	gtk_widget_show_all(window);

	rpop3_window.window = window;
	rpop3_window.treeview = treeview;
	rpop3_window.store = store;
	rpop3_window.status_label = status_label;
	rpop3_window.recv_btn = recv_btn;
	rpop3_window.open_btn = open_btn;
	rpop3_window.delete_btn = delete_btn;
	rpop3_window.stop_btn = stop_btn;
	rpop3_window.close_btn = close_btn;
}

static gint rpop3_start(Session *session)
{
	SessionState state;
	PrefsAccount *ac;
	SocksInfo *socks_info = NULL;

	g_return_val_if_fail(session != NULL, -1);

	rpop3_status_label_set(_("Connecting to %s:%d ..."),
			       session->server, session->port);

	ac = POP3_SESSION(session)->ac_prefs;
	if (ac->use_socks && ac->use_socks_for_recv) {
		socks_info = socks_info_new(ac->socks_type, ac->proxy_host, ac->proxy_port, ac->use_proxy_auth ? ac->proxy_name : NULL, ac->use_proxy_auth ? ac->proxy_pass : NULL);
	}

	if (session_connect_full(session, session->server, session->port,
				 socks_info) < 0) {
		manage_window_focus_in(rpop3_window.window, NULL, NULL);
		alertpanel_error(_("Can't connect to POP3 server: %s:%d"),
				 session->server, session->port);
		return -1;
	}

	while (session_is_connected(session)) {
		if (session->state == SESSION_READY && rpop3_window.cancelled)
			break;
		gtk_main_iteration();
	}

	state = session->state;
	session_disconnect(session);

	switch (POP3_SESSION(session)->error_val) {
	case PS_AUTHFAIL:
		manage_window_focus_in(rpop3_window.window, NULL, NULL);
		rpop3_status_label_set(_("Authentication failed."));
		if (POP3_SESSION(session)->error_msg)
			alertpanel_error(_("Authentication failed:\n%s"),
					 POP3_SESSION(session)->error_msg);
		else
			alertpanel_error(_("Authentication failed."));
		return -1;
	case PS_SUCCESS:
		break;
	default:
		manage_window_focus_in(rpop3_window.window, NULL, NULL);
		rpop3_status_label_set(_("Error occurred during POP3 session."));
		if (POP3_SESSION(session)->error_msg)
			alertpanel_error
				(_("Error occurred during POP3 session:\n%s"),
				 POP3_SESSION(session)->error_msg);
		else
			alertpanel_error(_("Error occurred during POP3 session."));
		return -1;
	}

	switch (state) {
	case SESSION_EOF:
		manage_window_focus_in(rpop3_window.window, NULL, NULL);
		rpop3_status_label_set
			(_("Connection closed by the remote host."));
		alertpanel_error(_("Connection closed by the remote host."));
		return -1;
	case SESSION_TIMEOUT:
		manage_window_focus_in(rpop3_window.window, NULL, NULL);
		rpop3_status_label_set(_("Session timed out."));
		alertpanel_error(_("Session timed out."));
		return -1;
	case SESSION_ERROR:
		manage_window_focus_in(rpop3_window.window, NULL, NULL);
		if (POP3_SESSION(session)->state == POP3_READY) {
			rpop3_status_label_set
				(_("Can't connect to POP3 server: %s:%d"),
				 session->server, session->port);
			alertpanel_error
				(_("Can't connect to POP3 server: %s:%d"),
				 session->server, session->port);
		} else {
			rpop3_status_label_set
				(_("Error occurred during POP3 session."));
			alertpanel_error
				(_("Error occurred during POP3 session."));
		}
		return -1;
	case SESSION_READY:
		debug_print("Session cancelled.\n");
		return -1;
	default:
		break;
	}

	return 0;
}

static void rpop3_status_label_set(const gchar *fmt, ...)
{
	va_list args;
	gchar buf[1024];

	va_start(args, fmt);
	g_vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	gtk_label_set_text(GTK_LABEL(rpop3_window.status_label), buf);
}

static gboolean clear_func(GtkTreeModel *model, GtkTreePath *path,
			   GtkTreeIter *iter, gpointer data)
{
	MsgInfo *msginfo;

	gtk_tree_model_get(model, iter, COL_MSGINFO, &msginfo, -1);
	procmsg_msginfo_free(msginfo);

	return FALSE;
}

static void rpop3_clear_list(void)
{
	
	gtk_tree_model_foreach(GTK_TREE_MODEL(rpop3_window.store), clear_func,
			       NULL);
	gtk_list_store_clear(rpop3_window.store);
}

static gboolean rpop3_ping_cb(gpointer data)
{
	gdk_threads_enter();

	if (rpop3_window.ping_tag > 0) {
		g_source_remove(rpop3_window.ping_tag);
		rpop3_window.ping_tag = 0;
	}
	if (rpop3_window.session->state == POP3_IDLE)
		rpop3_noop_send(rpop3_window.session);

	gdk_threads_leave();

	return FALSE;
}

static void rpop3_idle(gboolean is_idle)
{
	if (rpop3_window.ping_tag > 0)
		g_source_remove(rpop3_window.ping_tag);
	rpop3_window.ping_tag = 0;

	if (is_idle) {
		debug_print("Entered idle state\n");
		rpop3_window.session->state = POP3_IDLE;
		if (POP3_PING_ITV < prefs_common.io_timeout_secs)
			rpop3_window.ping_tag =
				g_timeout_add_full(G_PRIORITY_LOW,
						   POP3_PING_ITV * 1000,
						   rpop3_ping_cb, NULL, NULL);
	}
}

static gint rpop3_noop_send(Pop3Session *session)
{
	session->state = POP3_NOOP;
	pop3_gen_send(session, "NOOP");
	return PS_SUCCESS;
}

static gint rpop3_top_send(Pop3Session *session)
{
	session->state = POP3_TOP;
	pop3_gen_send(session, "TOP %d 0", session->cur_msg);
	return PS_SUCCESS;
}

static gint rpop3_top_recv(Pop3Session *session, FILE *fp, guint len)
{
	MsgInfo *msginfo;
	MsgFlags flags = {0, 0};
	GtkTreeIter iter;
	const gchar *subject, *from, *date;
	gchar buf[1024];

	session->msg[session->cur_msg].received = TRUE;

	msginfo = procheader_parse_stream(fp, flags, FALSE);

	msginfo->size = session->msg[session->cur_msg].size;

	subject = msginfo->subject ? msginfo->subject : _("(No Subject)");
	from = msginfo->from ? msginfo->from : _("(No From)");
	if (msginfo->date_t) {
		procheader_date_get_localtime(buf, sizeof(buf),
					      msginfo->date_t);
		date = buf;
	} else if (msginfo->date)
		date = msginfo->date;
	else
		date = _("(No Date)");

	gtk_list_store_append(rpop3_window.store, &iter);
	gtk_list_store_set(rpop3_window.store, &iter,
			   COL_NUMBER, session->cur_msg,
			   COL_SUBJECT, subject,
			   COL_FROM, from,
			   COL_DATE, date,
			   COL_SIZE, to_human_readable
				(session->msg[session->cur_msg].size),
			   COL_MSGINFO, msginfo,
			   COL_DELETED, FALSE,
			   -1);

	rpop3_status_label_set(_("Retrieving message headers (%d / %d) ..."),
			       session->count - session->cur_msg + 1,
			       session->count);

	return PS_SUCCESS;
}

static gint rpop3_retr_send(Pop3Session *session)
{
	if (rpop3_window.recv_array) {
		g_return_val_if_fail(rpop3_window.recv_cur < rpop3_window.recv_array->len, -1);
		session->cur_msg = g_array_index(rpop3_window.recv_array, gint,
						 rpop3_window.recv_cur);
	}

	rpop3_idle(FALSE);
	session->state = POP3_RETR;
	pop3_gen_send(session, "RETR %d", session->cur_msg);
	return PS_SUCCESS;
}

static void msgview_destroy_cb(GtkWidget *widget, gpointer data)
{
	gchar *file = (gchar *)data;

	if (file) {
		debug_print("msgview_destroy_cb: removing temporary file: %s\n", file);
		g_unlink(file);
		g_free(file);
	}
}

static gint rpop3_retr_recv(Pop3Session *session, FILE *fp, guint len)
{
	gchar *file;
	MsgInfo *msginfo;
	MsgFlags flags = {MSG_NEW|MSG_UNREAD, MSG_RECEIVED};
	MessageView *msgview;

	file = get_tmp_file();
	if (pop3_write_msg_to_file(file, fp, len) < 0) {
		g_free(file);
		session->error_val = PS_IOERR;
		return -1;
	}

	if (rpop3_window.recv_array) {
		FolderItem *inbox;

		if (session->ac_prefs->inbox) {
			inbox = folder_find_item_from_identifier
				(session->ac_prefs->inbox);
			if (!inbox)
				inbox = folder_get_default_inbox();
		} else
			inbox = folder_get_default_inbox();
		if (!inbox) {
			session->error_val = PS_IOERR;
			return -1;
		}

		if (folder_item_add_msg(inbox, file, &flags, FALSE) < 0) {
			session->error_val = PS_IOERR;
			return -1;
		}
		if (rpop3_window.recv_cur + 1 == rpop3_window.recv_array->len)
			folderview_update_item(inbox, TRUE);
		else
			folderview_update_item(inbox, FALSE);

		return PS_SUCCESS;
	}

	msginfo = procheader_parse_file(file, flags, FALSE);
	if (!msginfo) {
		session->error_val = PS_IOERR;
		return -1;
	}

	msginfo->file_path = g_strdup(file);

	msgview = messageview_create_with_new_window();
	messageview_show(msgview, msginfo, FALSE);

	g_signal_connect(G_OBJECT(msgview->window), "destroy",
			 G_CALLBACK(msgview_destroy_cb), file);

	return PS_SUCCESS;
}

static gint rpop3_delete_send(Pop3Session *session)
{
	g_return_val_if_fail(rpop3_window.delete_array != NULL, -1);
	g_return_val_if_fail
		(rpop3_window.delete_cur < rpop3_window.delete_array->len, -1);

	rpop3_idle(FALSE);
	session->state = POP3_DELETE;
	session->cur_msg = g_array_index(rpop3_window.delete_array, gint,
					 rpop3_window.delete_cur);
	pop3_gen_send(session, "DELE %d", session->cur_msg);
	return PS_SUCCESS;
}

static gint rpop3_delete_recv(Pop3Session *session)
{
	session->msg[session->cur_msg].recv_time = RECV_TIME_DELETE;
	session->msg[session->cur_msg].deleted = TRUE;
	return PS_SUCCESS;
}

static gint rpop3_session_recv_msg(Session *session, const gchar *msg)
{
	Pop3Session *pop3_session = POP3_SESSION(session);
	gint val = PS_SUCCESS;
	const gchar *body;

	gdk_threads_enter();

	body = msg;
	if (pop3_session->state != POP3_GETRANGE_UIDL_RECV &&
	    pop3_session->state != POP3_GETSIZE_LIST_RECV) {
		val = pop3_ok(pop3_session, msg);
		if (val != PS_SUCCESS) {
			if (val == PS_SOCKET) {
				pop3_session->state = POP3_ERROR;
				gdk_threads_leave();
				return -1;
			}
			if (val != PS_NOTSUPPORTED) {
				if (pop3_session->state != POP3_LOGOUT) {
					if (pop3_logout_send(pop3_session) == PS_SUCCESS) {
						gdk_threads_leave();
						return 0;
					} else {
						gdk_threads_leave();
						return -1;
					}
				}
			}
		}

		if (*body == '+' || *body == '-')
			body++;
		while (g_ascii_isalpha(*body))
			body++;
		while (g_ascii_isspace(*body))
			body++;
	}

	switch (pop3_session->state) {
	case POP3_READY:
	case POP3_GREETING:
		val = pop3_greeting_recv(pop3_session, body);
		rpop3_status_label_set(_("Authenticating..."));
#if USE_SSL
		if (pop3_session->ac_prefs->ssl_pop == SSL_STARTTLS)
			val = pop3_stls_send(pop3_session);
		else
#endif
		if (pop3_session->ac_prefs->use_apop_auth)
			val = pop3_getauth_apop_send(pop3_session);
		else
			val = pop3_getauth_user_send(pop3_session);
		break;
#if USE_SSL
	case POP3_STLS:
		if ((val = pop3_stls_recv(pop3_session)) != PS_SUCCESS)
			break;
		if (pop3_session->ac_prefs->use_apop_auth)
			val = pop3_getauth_apop_send(pop3_session);
		else
			val = pop3_getauth_user_send(pop3_session);
		break;
#endif
	case POP3_GETAUTH_USER:
		val = pop3_getauth_pass_send(pop3_session);
		break;
	case POP3_GETAUTH_PASS:
	case POP3_GETAUTH_APOP:
		rpop3_status_label_set(_("Getting the number of messages..."));
		val = pop3_getrange_stat_send(pop3_session);
		break;
	case POP3_GETRANGE_STAT:
		if ((val = pop3_getrange_stat_recv(pop3_session, body)) != PS_SUCCESS)
			break;
		if (pop3_session->count > 0)
			val = pop3_getrange_uidl_send(pop3_session);
		else {
			rpop3_status_label_set(_("No message"));
			val = pop3_logout_send(pop3_session);
		}
		break;
	case POP3_GETRANGE_LAST:
		if (val == PS_NOTSUPPORTED)
			pop3_session->error_val = PS_SUCCESS;
		else if ((val = pop3_getrange_last_recv
			(pop3_session, body)) != PS_SUCCESS)
			break;
		if (pop3_session->cur_msg > 0)
			val = pop3_getsize_list_send(pop3_session);
		else {
			rpop3_status_label_set(_("No message"));
			val = pop3_logout_send(pop3_session);
		}
		break;
	case POP3_GETRANGE_UIDL:
		if (val == PS_NOTSUPPORTED) {
			pop3_session->error_val = PS_SUCCESS;
			val = pop3_getrange_last_send(pop3_session);
		} else {
			pop3_session->state = POP3_GETRANGE_UIDL_RECV;
			val = session_recv_data(session, 0, ".\r\n");
		}
		break;
	case POP3_GETSIZE_LIST:
		pop3_session->state = POP3_GETSIZE_LIST_RECV;
		val = session_recv_data(session, 0, ".\r\n");
		break;
	case POP3_TOP:
		pop3_session->state = POP3_TOP_RECV;
		val = session_recv_data_as_file(session, 0, ".\r\n");
		break;
	case POP3_RETR:
		pop3_session->state = POP3_RETR_RECV;
		val = session_recv_data_as_file(session, 0, ".\r\n");
		break;
	case POP3_DELETE:
		val = rpop3_delete_recv(pop3_session);
		if (val != PS_SUCCESS)
			break;
		if (rpop3_window.delete_cur + 1 < rpop3_window.delete_array->len) {
			rpop3_window.delete_cur++;
			val = rpop3_delete_send(pop3_session);
		} else {
			rpop3_status_label_set(_("Deleted %d messages"),
				 	       rpop3_window.delete_cur + 1);
			g_array_free(rpop3_window.delete_array, TRUE);
			rpop3_window.delete_array = NULL;
			rpop3_window.delete_cur = 0;
			rpop3_idle(TRUE);
		}
		break;
	case POP3_NOOP:
		rpop3_idle(TRUE);
		break;
	case POP3_LOGOUT:
		if (val == PS_SUCCESS)
			pop3_session->state = POP3_DONE;
		else
			pop3_session->state = POP3_ERROR;
		session_disconnect(session);
		break;
	case POP3_ERROR:
	default:
		val = -1;
		break;
	}

	gdk_threads_leave();

	if (val == PS_SUCCESS)
		return 0;
	else
		return -1;
}

static gint rpop3_session_recv_data_finished(Session *session, guchar *data,
					     guint len)
{
        Pop3Session *pop3_session = POP3_SESSION(session);
        Pop3ErrorValue val = PS_SUCCESS;
	gint ret = 0;

	gdk_threads_enter();

        switch (pop3_session->state) {
        case POP3_GETRANGE_UIDL_RECV:
                val = pop3_getrange_uidl_recv(pop3_session, (gchar *)data, len);
                if (val == PS_SUCCESS) {
			if (rpop3_window.cancelled) {
				rpop3_status_label_set(_("Quitting..."));
				pop3_logout_send(pop3_session);
			} else
				pop3_getsize_list_send(pop3_session);
		} else
			ret = -1;
		break;
        case POP3_GETSIZE_LIST_RECV:
                val = pop3_getsize_list_recv(pop3_session, (gchar *)data, len);
                if (val == PS_SUCCESS) {
			pop3_session->cur_msg = 1;
			if (rpop3_window.cancelled || pop3_session->count == 0) {
				rpop3_status_label_set(_("Quitting..."));
				pop3_logout_send(pop3_session);
			} else {
				pop3_session->cur_msg = pop3_session->count;
				gtk_widget_set_sensitive(rpop3_window.stop_btn,
							 TRUE);
				g_object_set(rpop3_window.update_action,
					     "sensitive", FALSE, NULL);
				g_object_set(rpop3_window.stop_action,
					     "sensitive", TRUE, NULL);
				rpop3_top_send(pop3_session);
			}
		} else
			ret = -1;
		break;
	case POP3_ERROR:
	default:
		ret = -1;
		break;
	}

	gdk_threads_leave();

	return ret;
}

static gint rpop3_session_recv_data_as_file_finished(Session *session,
						     FILE *fp, guint len)
{
        Pop3Session *pop3_session = POP3_SESSION(session);
	gint ret = 0;

	gdk_threads_enter();

        switch (pop3_session->state) {
	case POP3_RETR_RECV:
        	if (rpop3_retr_recv(pop3_session, fp, len) != PS_SUCCESS) {
                	ret = -1;
			break;
		}
		if (rpop3_window.recv_array) {
			if (rpop3_window.recv_cur + 1 < rpop3_window.recv_array->len) {
				rpop3_window.recv_cur++;
				if (rpop3_retr_send(pop3_session) != PS_SUCCESS)
					ret = -1;
			} else {
				rpop3_status_label_set(_("Retrieved %d messages"), rpop3_window.recv_cur + 1);
				g_array_free(rpop3_window.recv_array, TRUE);
				rpop3_window.recv_array = NULL;
				rpop3_window.recv_cur = 0;
				rpop3_idle(TRUE);
			}
		} else {
			rpop3_status_label_set(_("Opened message %d"),
					       pop3_session->cur_msg);
			rpop3_idle(TRUE);
		}
		break;
	case POP3_TOP_RECV:
		if (rpop3_top_recv(pop3_session, fp, len) == PS_SUCCESS) {
			if (rpop3_window.cancelled) {
				rpop3_status_label_set(_("Quitting..."));
				pop3_logout_send(pop3_session);
			} else if (!rpop3_window.stop_load &&
				 (pop3_session->cur_msg > 1)) {
				pop3_session->cur_msg--;
				rpop3_top_send(pop3_session);
			} else {
				if (pop3_session->cur_msg > 1)
					rpop3_status_label_set
						(_("Retrieved %d (of %d) message headers"),
						 pop3_session->count - pop3_session->cur_msg + 1, pop3_session->count);
				else
					rpop3_status_label_set
						(_("Retrieved %d message headers"),
						 pop3_session->count - pop3_session->cur_msg + 1);
				gtk_widget_set_sensitive
					(rpop3_window.recv_btn, TRUE);
				gtk_widget_set_sensitive
					(rpop3_window.open_btn, TRUE);
				gtk_widget_set_sensitive
					(rpop3_window.delete_btn, TRUE);
				if (pop3_session->cur_msg == 1)
					gtk_widget_set_sensitive(rpop3_window.stop_btn, FALSE);
				else
					gtk_button_set_label(GTK_BUTTON(rpop3_window.stop_btn), _("_Resume"));
				g_object_set(rpop3_window.recv_action,
					     "sensitive", TRUE, NULL);
				g_object_set(rpop3_window.open_action,
					     "sensitive", TRUE, NULL);
				g_object_set(rpop3_window.delete_action,
					     "sensitive", TRUE, NULL);
				g_object_set(rpop3_window.stop_action,
					     "sensitive", FALSE, NULL);
				if (pop3_session->cur_msg == 1)
					g_object_set(rpop3_window.update_action, "sensitive", FALSE, NULL);
				else
					g_object_set(rpop3_window.update_action, "sensitive", TRUE, NULL);
				rpop3_idle(TRUE);
			}
		} else
			ret = -1;
		break;
	default:
		ret = -1;
		break;
	}

	gdk_threads_leave();

	return ret;
}

static gint window_deleted(GtkWidget *widget, GdkEventAny *event,
			   gpointer data)
{
	gtk_button_clicked(GTK_BUTTON(rpop3_window.close_btn));
	return TRUE;
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event,
			    gpointer data)
{
	return FALSE;
}

static void rpop3_row_activated(GtkTreeView *treeview, GtkTreePath *path,
				GtkTreeViewColumn *column, gpointer data)
{
	gtk_button_clicked(GTK_BUTTON(rpop3_window.open_btn));
}

static void rpop3_recv(GtkButton *button, gpointer data)
{
	GtkTreeModel *model = GTK_TREE_MODEL(rpop3_window.store);
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GList *rows, *cur;
	gint num;
	gboolean deleted;
	GArray *array;

	if (rpop3_window.session->state != POP3_IDLE)
		return;

	selection = gtk_tree_view_get_selection
		(GTK_TREE_VIEW(rpop3_window.treeview));

	rows = gtk_tree_selection_get_selected_rows(selection, NULL);
	array = g_array_sized_new(FALSE, FALSE, sizeof(gint),
				  g_list_length(rows));

	for (cur = rows; cur != NULL; cur = cur->next) {
		gtk_tree_model_get_iter(model, &iter, (GtkTreePath *)cur->data);
		gtk_tree_model_get(model, &iter, COL_NUMBER, &num,
				   COL_DELETED, &deleted, -1);
		if (!deleted) {
			debug_print("rpop3_recv: receiving message %d\n", num);
			g_array_append_val(array, num);
		}
	}

	g_list_foreach(rows, (GFunc)gtk_tree_path_free, NULL);
	g_list_free(rows);

	if (array->len > 0) {
		rpop3_window.recv_array = array;
		rpop3_window.recv_cur = 0;
		rpop3_retr_send(rpop3_window.session);
	} else
		g_array_free(array, TRUE);
}

static void rpop3_open(GtkButton *button, gpointer data)
{
	GtkTreeModel *model = GTK_TREE_MODEL(rpop3_window.store);
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GList *rows, *cur;
	gint num;
	gboolean deleted;

	if (rpop3_window.session->state != POP3_IDLE)
		return;

	selection = gtk_tree_view_get_selection
		(GTK_TREE_VIEW(rpop3_window.treeview));

	rows = gtk_tree_selection_get_selected_rows(selection, NULL);

	for (cur = rows; cur != NULL; cur = cur->next) {
		gtk_tree_model_get_iter(model, &iter, (GtkTreePath *)cur->data);
		gtk_tree_model_get(model, &iter, COL_NUMBER, &num,
				   COL_DELETED, &deleted, -1);
		if (!deleted) {
			debug_print("rpop3_open: opening message %d\n", num);
			rpop3_status_label_set(_("Retrieving message %d ..."),
					       num);
			rpop3_window.session->cur_msg = num;
			rpop3_retr_send(rpop3_window.session);
			break;
		}
	}

	g_list_foreach(rows, (GFunc)gtk_tree_path_free, NULL);
	g_list_free(rows);
}

static void rpop3_delete(GtkButton *button, gpointer data)
{
	GtkTreeModel *model = GTK_TREE_MODEL(rpop3_window.store);
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GList *rows, *cur;
	gint num;
	gboolean deleted;
	GArray *array;
	AlertValue val;

	if (rpop3_window.session->state != POP3_IDLE)
		return;

	val = alertpanel(_("Delete messages"),
			 _("Really delete selected messages from server?\n"
			   "This operation cannot be reverted."),
			 GTK_STOCK_YES, GTK_STOCK_NO, NULL);
	if (val != G_ALERTDEFAULT)
		return;

	selection = gtk_tree_view_get_selection
		(GTK_TREE_VIEW(rpop3_window.treeview));

	rows = gtk_tree_selection_get_selected_rows(selection, NULL);

	array = g_array_sized_new(FALSE, FALSE, sizeof(gint),
				  g_list_length(rows));

	for (cur = rows; cur != NULL; cur = cur->next) {
		gtk_tree_model_get_iter(model, &iter, (GtkTreePath *)cur->data);
		gtk_tree_model_get(model, &iter, COL_NUMBER, &num,
				   COL_DELETED, &deleted, -1);
		if (!deleted) {
			debug_print("rpop3_delete: marked %d to delete\n", num);
			g_array_append_val(array, num);
			gtk_list_store_set(GTK_LIST_STORE(model), &iter,
					   COL_DELETED, TRUE, -1);
		}
	}

	g_list_foreach(rows, (GFunc)gtk_tree_path_free, NULL);
	g_list_free(rows);

	if (array->len > 0) {
		rpop3_window.delete_array = array;
		rpop3_window.delete_cur = 0;
		rpop3_delete_send(rpop3_window.session);
	} else
		g_array_free(array, TRUE);
}

static void rpop3_read_next(GtkButton *button, gpointer data)
{
	gint i;

	if (rpop3_window.session->state != POP3_IDLE)
		return;

	for (i = rpop3_window.session->count; i > 0; i--) {
		if (!rpop3_window.session->msg[i].received)
			break;
	}

	if (i == 0)
		return;

	debug_print("rpop3_read_next: next: %d\n", i);
	rpop3_window.session->cur_msg = i;

	rpop3_window.stop_load = FALSE;

	gtk_widget_set_sensitive(rpop3_window.recv_btn, FALSE);
	gtk_widget_set_sensitive(rpop3_window.open_btn, FALSE);
	gtk_widget_set_sensitive(rpop3_window.delete_btn, FALSE);
	gtk_button_set_label(GTK_BUTTON(rpop3_window.stop_btn), GTK_STOCK_STOP);
	gtk_widget_set_sensitive(rpop3_window.stop_btn, TRUE);
	g_object_set(rpop3_window.update_action, "sensitive", FALSE, NULL);
	g_object_set(rpop3_window.stop_action, "sensitive", TRUE, NULL);
	rpop3_idle(FALSE);
	rpop3_top_send(rpop3_window.session);
}

static void rpop3_stop(GtkButton *button, gpointer data)
{
	if (rpop3_window.session->state == POP3_IDLE)
		rpop3_read_next(NULL, NULL);
	else
		rpop3_window.stop_load = TRUE;
}

static void rpop3_close(GtkButton *button, gpointer data)
{
	rpop3_window.finished = TRUE;

	if (rpop3_window.session->state == POP3_IDLE) {
		rpop3_status_label_set(_("Quitting..."));
		rpop3_idle(FALSE);
		pop3_logout_send(rpop3_window.session);
	} else if (rpop3_window.session->state != POP3_DONE &&
		   rpop3_window.session->state != POP3_ERROR)
		rpop3_window.cancelled = TRUE;
}

static void rpop3_recv_cb(void)
{
	rpop3_recv(NULL, NULL);
}

static void rpop3_open_cb(void)
{
	rpop3_open(NULL, NULL);
}

static void rpop3_delete_cb(void)
{
	rpop3_delete(NULL, NULL);
}

static void rpop3_resume_cb(void)
{
	rpop3_stop(NULL, NULL);
}

static void rpop3_stop_cb(void)
{
	rpop3_stop(NULL, NULL);
}

static void rpop3_close_cb(void)
{
	rpop3_close(NULL, NULL);
}

static void rpop3_about_cb(void)
{
	about_show();
}

static gint cmp_by_subject(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b,
			   gpointer data)
{
	MsgInfo *msginfo_a = NULL, *msginfo_b = NULL;
	gint ret;

	gtk_tree_model_get(model, a, COL_MSGINFO, &msginfo_a, -1);
	gtk_tree_model_get(model, b, COL_MSGINFO, &msginfo_b, -1);

	if (!msginfo_a || !msginfo_b)
		return 0;

	if (!msginfo_a->subject)
		return -(msginfo_b->subject != NULL);
	if (!msginfo_b->subject)
		return (msginfo_a->subject != NULL);

	ret = subject_compare_for_sort(msginfo_a->subject, msginfo_b->subject);
	return (ret != 0) ? ret : (msginfo_a->msgnum - msginfo_b->msgnum);
}

static gint cmp_by_date(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b,
			gpointer data)
{
	MsgInfo *msginfo_a = NULL, *msginfo_b = NULL;
	gint ret;

	gtk_tree_model_get(model, a, COL_MSGINFO, &msginfo_a, -1);
	gtk_tree_model_get(model, b, COL_MSGINFO, &msginfo_b, -1);

	if (!msginfo_a || !msginfo_b)
		return 0;

	ret = msginfo_a->date_t - msginfo_b->date_t;
	return (ret != 0) ? ret : (msginfo_a->msgnum - msginfo_b->msgnum);
}

static gint cmp_by_size(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b,
			gpointer data)
{
	MsgInfo *msginfo_a = NULL, *msginfo_b = NULL;
	gint ret;

	gtk_tree_model_get(model, a, COL_MSGINFO, &msginfo_a, -1);
	gtk_tree_model_get(model, b, COL_MSGINFO, &msginfo_b, -1);

	if (!msginfo_a || !msginfo_b)
		return 0;

	ret = msginfo_a->size - msginfo_b->size;
	return (ret != 0) ? ret : (msginfo_a->msgnum - msginfo_b->msgnum);
}
