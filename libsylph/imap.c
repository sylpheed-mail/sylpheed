/*
 * LibSylph -- E-Mail client library
 * Copyright (C) 1999-2013 Hiroyuki Yamamoto
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#if HAVE_ICONV
#  include <iconv.h>
#endif

#include "sylmain.h"
#include "imap.h"
#include "socket.h"
#include "socks.h"
#include "ssl.h"
#include "recv.h"
#include "procmsg.h"
#include "procheader.h"
#include "folder.h"
#include "prefs_account.h"
#include "codeconv.h"
#include "md5_hmac.h"
#include "base64.h"
#include "utils.h"
#include "prefs_common.h"
#include "virtual.h"

#define IMAP4_PORT	143
#if USE_SSL
#define IMAPS_PORT	993
#endif

#define IMAP_COPY_LIMIT	200
#define IMAP_CMD_LIMIT	1000

#define QUOTE_IF_REQUIRED(out, str)					\
{									\
	if (!str || *str == '\0') {					\
		Xstrdup_a(out, "\"\"", return IMAP_ERROR);		\
	} else if (strpbrk(str, " \t(){}[]%&*\"\\") != NULL) {		\
		gchar *__tmp;						\
		gint len;						\
		const gchar *p;						\
		gchar *tp;						\
									\
		len = strlen(str) * 2 + 3;				\
		Xalloca(__tmp, len, return IMAP_ERROR);			\
		tp = __tmp;						\
		*tp++ = '\"';						\
		for (p = str; *p != '\0'; p++) {			\
			if (*p == '\"' || *p == '\\')			\
				*tp++ = '\\';				\
			*tp++ = *p;					\
		}							\
		*tp++ = '\"';						\
		*tp = '\0';						\
		out = __tmp;						\
	} else {							\
		Xstrdup_a(out, str, return IMAP_ERROR);			\
	}								\
}

typedef gint (*IMAPThreadFunc)		(IMAPSession	*session,
					 gpointer	 data);
typedef gint (*IMAPProgressFunc)	(IMAPSession	*session,
					 gint		 count,
					 gint		 total,
					 gpointer	 data);

typedef struct _IMAPRealSession
{
	IMAPSession imap_session;
#if USE_THREADS
	GThreadPool *pool;
	IMAPThreadFunc thread_func;
	gpointer thread_data;
	gboolean is_running;
	gint prog_count;
	gint prog_total;
	gint flag;
	gint retval;
#endif
} IMAPRealSession;

static GList *session_list = NULL;

static void imap_folder_init		(Folder		*folder,
					 const gchar	*name,
					 const gchar	*path);

static Folder	*imap_folder_new	(const gchar	*name,
					 const gchar	*path);
static void	 imap_folder_destroy	(Folder		*folder);

static Session *imap_session_new	(PrefsAccount	*account);
static gint imap_session_connect	(IMAPSession	*session);
static gint imap_session_reconnect	(IMAPSession	*session);
static void imap_session_destroy	(Session	*session);
/* static void imap_session_destroy_all	(void); */

static gint imap_search_flags		(IMAPSession	*session,
					 GArray	       **uids,
					 GHashTable    **flags_table);
static gint imap_fetch_flags		(IMAPSession	*session,
					 GArray	       **uids,
					 GHashTable    **flags_table);

static GSList *imap_get_msg_list	(Folder		*folder,
					 FolderItem	*item,
					 gboolean	 use_cache);
static GSList *imap_get_uncached_msg_list
					(Folder		*folder,
					 FolderItem	*item);
static gchar *imap_fetch_msg		(Folder		*folder,
					 FolderItem	*item,
					 gint		 uid);
static MsgInfo *imap_get_msginfo	(Folder		*folder,
					 FolderItem	*item,
					 gint		 uid);
static gint imap_add_msg		(Folder		*folder,
					 FolderItem	*dest,
					 const gchar	*file,
					 MsgFlags	*flags,
					 gboolean	 remove_source);
static gint imap_add_msgs		(Folder		*folder,
					 FolderItem	*dest,
					 GSList		*file_list,
					 gboolean	 remove_source,
					 gint		*first);
static gint imap_add_msg_msginfo	(Folder		*folder,
					 FolderItem	*dest,
					 MsgInfo	*msginfo,
					 gboolean	 remove_source);
static gint imap_add_msgs_msginfo	(Folder		*folder,
					 FolderItem	*dest,
					 GSList		*msglist,
					 gboolean	 remove_source,
					 gint		*first);

static gint imap_move_msg		(Folder		*folder,
					 FolderItem	*dest,
					 MsgInfo	*msginfo);
static gint imap_move_msgs		(Folder		*folder,
					 FolderItem	*dest,
					 GSList		*msglist);
static gint imap_copy_msg		(Folder		*folder,
					 FolderItem	*dest,
					 MsgInfo	*msginfo);
static gint imap_copy_msgs		(Folder		*folder,
					 FolderItem	*dest,
					 GSList		*msglist);

static gint imap_remove_msg		(Folder		*folder,
					 FolderItem	*item,
					 MsgInfo	*msginfo);
static gint imap_remove_msgs		(Folder		*folder,
					 FolderItem	*item,
					 GSList		*msglist);
static gint imap_remove_all_msg		(Folder		*folder,
					 FolderItem	*item);

static gboolean imap_is_msg_changed	(Folder		*folder,
					 FolderItem	*item,
					 MsgInfo	*msginfo);

static gint imap_close			(Folder		*folder,
					 FolderItem	*item);

static gint imap_scan_folder		(Folder		*folder,
					 FolderItem	*item);
static gint imap_scan_tree		(Folder		*folder);

static gint imap_create_tree		(Folder		*folder);

static FolderItem *imap_create_folder	(Folder		*folder,
					 FolderItem	*parent,
					 const gchar	*name);
static gint imap_rename_folder		(Folder		*folder,
					 FolderItem	*item,
					 const gchar	*name);
static gint imap_move_folder		(Folder		*folder,
					 FolderItem	*item,
					 FolderItem	*new_parent);
static gint imap_remove_folder		(Folder		*folder,
					 FolderItem	*item);

static IMAPSession *imap_session_get	(Folder		*folder);

static gint imap_greeting		(IMAPSession	*session);
static gint imap_auth			(IMAPSession	*session,
					 const gchar	*user,
					 const gchar	*pass,
					 IMAPAuthType	 type);

static gint imap_scan_tree_recursive	(IMAPSession	*session,
					 FolderItem	*item,
					 GSList		*item_list);
static GSList *imap_get_folder_list	(IMAPSession	*session,
					 FolderItem	*item);
static GSList *imap_parse_list		(IMAPSession	*session,
					 const gchar	*real_path,
					 gchar		*separator);
static GSList *imap_add_inter_folders	(GSList		*item_list,
					 const gchar	*root_path);
static GSList *imap_get_part_folder_list(GSList		*item_list,
					 FolderItem	*item);

static void imap_create_missing_folders	(Folder			*folder);
static FolderItem *imap_create_special_folder
					(Folder			*folder,
					 SpecialFolderItemType	 stype,
					 const gchar		*name);

static gint imap_do_copy_msgs		(Folder		*folder,
					 FolderItem	*dest, 
					 GSList		*msglist,
					 gboolean	 remove_source);
static gint imap_remove_msgs_by_seq_set	(Folder		*folder,
					 FolderItem	*item,
					 GSList		*seq_list);

static GSList *imap_get_uncached_messages	(IMAPSession	*session,
						 FolderItem	*item,
						 guint32	 first_uid,
						 guint32	 last_uid,
						 gint		 exists,
						 gboolean	 update_count);
static void imap_delete_cached_message		(FolderItem	*item,
						 guint32	 uid);
static GSList *imap_delete_cached_messages	(GSList		*mlist,
						 FolderItem	*item,
						 guint32	 first_uid,
						 guint32	 last_uid);
static void imap_delete_all_cached_messages	(FolderItem	*item);

#if USE_SSL
static SockInfo *imap_open		(const gchar	*server,
					 gushort	 port,
					 SocksInfo	*socks_info,
					 SSLType	 ssl_type);
#else
static SockInfo *imap_open		(const gchar	*server,
					 gushort	 port,
					 SocksInfo	*socks_info);
#endif

static gint imap_msg_list_change_perm_flags	(GSList		*msglist,
						 MsgPermFlags	 flags,
						 gboolean	 is_set);
static gchar *imap_get_flag_str			(IMAPFlags	 flags);
static gint imap_set_message_flags		(IMAPSession	*session,
						 const gchar	*seq_set,
						 IMAPFlags	 flags,
						 gboolean	 is_set);
static gint imap_select				(IMAPSession	*session,
						 IMAPFolder	*folder,
						 const gchar	*path,
						 gint		*exists,
						 gint		*recent,
						 gint		*unseen,
						 guint32	*uid_validity);
static gint imap_status				(IMAPSession	*session,
						 IMAPFolder	*folder,
						 const gchar	*path,
						 gint		*messages,
						 gint		*recent,
						 guint32	*uid_next,
						 guint32	*uid_validity,
						 gint		*unseen);

static void imap_parse_namespace		(IMAPSession	*session,
						 IMAPFolder	*folder);
static void imap_get_namespace_by_list		(IMAPSession	*session,
						 IMAPFolder	*folder);
static IMAPNameSpace *imap_find_namespace	(IMAPFolder	*folder,
						 const gchar	*path);
static gchar imap_get_path_separator		(IMAPFolder	*folder,
						 const gchar	*path);
static gchar *imap_get_real_path		(IMAPFolder	*folder,
						 const gchar	*path);

static gchar *imap_parse_atom		(IMAPSession	*session,
					 gchar		*src,
					 gchar		*dest,
					 gint		 dest_len,
					 GString	*str);
static MsgFlags imap_parse_flags	(const gchar	*flag_str);
static IMAPFlags imap_parse_imap_flags	(const gchar	*flag_str);
static MsgInfo *imap_parse_envelope	(IMAPSession	*session,
					 FolderItem	*item,
					 GString	*line_str);

static gboolean imap_has_capability	(IMAPSession	*session,
					 const gchar	*capability);
static void imap_capability_free	(IMAPSession	*session);

/* low-level IMAP4rev1 commands */
static gint imap_cmd_capability	(IMAPSession	*session);
static gint imap_cmd_authenticate
				(IMAPSession	*session,
				 const gchar	*user,
				 const gchar	*pass,
				 IMAPAuthType	 type);
static gint imap_cmd_login	(IMAPSession	*session,
				 const gchar	*user,
				 const gchar	*pass);
static gint imap_cmd_logout	(IMAPSession	*session);
static gint imap_cmd_noop	(IMAPSession	*session);
#if USE_SSL
static gint imap_cmd_starttls	(IMAPSession	*session);
#endif
static gint imap_cmd_namespace	(IMAPSession	*session,
				 gchar	       **ns_str);
static gint imap_cmd_list	(IMAPSession	*session,
				 const gchar	*ref,
				 const gchar	*mailbox,
				 GPtrArray	*argbuf);
static gint imap_cmd_do_select	(IMAPSession	*session,
				 const gchar	*folder,
				 gboolean	 examine,
				 gint		*exists,
				 gint		*recent,
				 gint		*unseen,
				 guint32	*uid_validity);
static gint imap_cmd_select	(IMAPSession	*session,
				 const gchar	*folder,
				 gint		*exists,
				 gint		*recent,
				 gint		*unseen,
				 guint32	*uid_validity);
static gint imap_cmd_examine	(IMAPSession	*session,
				 const gchar	*folder,
				 gint		*exists,
				 gint		*recent,
				 gint		*unseen,
				 guint32	*uid_validity);
static gint imap_cmd_create	(IMAPSession	*session,
				 const gchar	*folder);
static gint imap_cmd_rename	(IMAPSession	*session,
				 const gchar	*oldfolder,
				 const gchar	*newfolder);
static gint imap_cmd_delete	(IMAPSession	*session,
				 const gchar	*folder);
static gint imap_cmd_subscribe	(IMAPSession	*session,
				 const gchar	*folder);
static gint imap_cmd_envelope	(IMAPSession	*session,
				 const gchar	*seq_set);
static gint imap_cmd_search	(IMAPSession	*session,
				 const gchar	*criteria,
				 GArray        **result);
static gint imap_cmd_fetch	(IMAPSession	*session,
				 guint32	 uid,
				 const gchar	*filename);
static gint imap_cmd_append	(IMAPSession	*session,
				 const gchar	*destfolder,
				 const gchar	*file,
				 IMAPFlags	 flags,
				 guint32	*new_uid);
static gint imap_cmd_copy	(IMAPSession	*session,
				 const gchar	*seq_set,
				 const gchar	*destfolder);
static gint imap_cmd_store	(IMAPSession	*session,
				 const gchar	*seq_set,
				 const gchar	*sub_cmd);
static gint imap_cmd_expunge	(IMAPSession	*session);
static gint imap_cmd_close	(IMAPSession	*session);

static gint imap_cmd_ok		(IMAPSession	*session,
				 GPtrArray	*argbuf);
static gint imap_cmd_ok_real	(IMAPSession	*session,
				 GPtrArray	*argbuf);
static gint imap_cmd_gen_send	(IMAPSession	*session,
				 const gchar	*format, ...);
static gint imap_cmd_gen_recv	(IMAPSession	*session,
				 gchar	       **ret);

static gint imap_cmd_gen_recv_silent	(IMAPSession	*session,
					 gchar	       **ret);

/* misc utility functions */
static gchar *strchr_cpy			(const gchar	*src,
						 gchar		 ch,
						 gchar		*dest,
						 gint		 len);
static gchar *get_quoted			(const gchar	*src,
						 gchar		 ch,
						 gchar		*dest,
						 gint		 len);
static gchar *search_array_contain_str		(GPtrArray	*array,
						 gchar		*str);
static gchar *search_array_str			(GPtrArray	*array,
						 gchar		*str);
static void imap_path_separator_subst		(gchar		*str,
						 gchar		 separator);

static gchar *imap_modified_utf7_to_utf8	(const gchar	*mutf7_str);
static gchar *imap_utf8_to_modified_utf7	(const gchar	*from);

static GSList *imap_get_seq_set_from_msglist	(GSList		*msglist,
						 gint		 limit);
static gint imap_seq_set_get_count		(const gchar	*seq_set);
static void imap_seq_set_free			(GSList		*seq_list);

static GHashTable *imap_get_uid_table		(GArray		*array);

static gboolean imap_rename_folder_func		(GNode		*node,
						 gpointer	 data);

#if USE_THREADS
static gint imap_thread_run		(IMAPSession		*session,
					 IMAPThreadFunc		 func,
					 gpointer		 data);
static gint imap_thread_run_progress	(IMAPSession		*session,
					 IMAPThreadFunc		 func,
					 IMAPProgressFunc	 progress_func,
					 gpointer		 data);
#endif

static FolderClass imap_class =
{
	F_IMAP,

	imap_folder_new,
	imap_folder_destroy,

	imap_scan_tree,
	imap_create_tree,

	imap_get_msg_list,
	imap_get_uncached_msg_list,
	imap_fetch_msg,
	imap_get_msginfo,
	imap_add_msg,
	imap_add_msgs,
	imap_add_msg_msginfo,
	imap_add_msgs_msginfo,
	imap_move_msg,
	imap_move_msgs,
	imap_copy_msg,
	imap_copy_msgs,
	imap_remove_msg,
	imap_remove_msgs,
	imap_remove_all_msg,
	imap_is_msg_changed,
	imap_close,
	imap_scan_folder,

	imap_create_folder,
	imap_rename_folder,
	imap_move_folder,
	imap_remove_folder
};


FolderClass *imap_get_class(void)
{
	return &imap_class;
}

static Folder *imap_folder_new(const gchar *name, const gchar *path)
{
	Folder *folder;

	folder = (Folder *)g_new0(IMAPFolder, 1);
	imap_folder_init(folder, name, path);

	return folder;
}

static void imap_folder_destroy(Folder *folder)
{
	g_return_if_fail(folder->account != NULL);

	if (REMOTE_FOLDER(folder)->remove_cache_on_destroy) {
		gchar *dir;
		gchar *server;

		dir = folder_get_path(folder);
		if (is_dir_exist(dir))
			remove_dir_recursive(dir);
		g_free(dir);

		server = uriencode_for_filename(folder->account->recv_server);
		dir = g_strconcat(get_imap_cache_dir(), G_DIR_SEPARATOR_S,
				  server, NULL);
		if (is_dir_exist(dir))
			g_rmdir(dir);
		g_free(dir);
		g_free(server);
	}

	folder_remote_folder_destroy(REMOTE_FOLDER(folder));
}

static void imap_folder_init(Folder *folder, const gchar *name,
			     const gchar *path)
{
	folder->klass = imap_get_class();
	folder_remote_folder_init(folder, name, path);
}

static IMAPSession *imap_session_get(Folder *folder)
{
	RemoteFolder *rfolder = REMOTE_FOLDER(folder);
	gint ret;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(FOLDER_TYPE(folder) == F_IMAP, NULL);
	g_return_val_if_fail(folder->account != NULL, NULL);

	if (!prefs_common.online_mode)
		return NULL;

	if (!rfolder->session) {
		rfolder->session = imap_session_new(folder->account);
		if (rfolder->session)
			imap_parse_namespace(IMAP_SESSION(rfolder->session),
					     IMAP_FOLDER(folder));
		return IMAP_SESSION(rfolder->session);
	}

	if (imap_is_session_active(IMAP_FOLDER(folder))) {
		g_warning("imap_session_get: session is busy.");
		return NULL;
	}

	if (time(NULL) - rfolder->session->last_access_time <
		SESSION_TIMEOUT_INTERVAL) {
		return IMAP_SESSION(rfolder->session);
	}

	if ((ret = imap_cmd_noop(IMAP_SESSION(rfolder->session))) != IMAP_SUCCESS) {
		if (ret == IMAP_EAGAIN) {
			g_warning("imap_session_get: session is busy.");
			return NULL;
		}

		log_warning(_("IMAP4 connection to %s has been"
			      " disconnected. Reconnecting...\n"),
			    folder->account->recv_server);
		if (imap_session_reconnect(IMAP_SESSION(rfolder->session))
		    == IMAP_SUCCESS)
			imap_parse_namespace(IMAP_SESSION(rfolder->session),
					     IMAP_FOLDER(folder));
		else {
			session_destroy(rfolder->session);
			rfolder->session = NULL;
		}
	}

	return IMAP_SESSION(rfolder->session);
}

static gint imap_greeting(IMAPSession *session)
{
	gchar *greeting;
	gint ok;

	if ((ok = imap_cmd_gen_recv(session, &greeting)) != IMAP_SUCCESS) {
		log_warning("Cannot get greeting message (%d)\n", ok);
		return ok;
	}

	if (greeting[0] != '*' || greeting[1] != ' ')
		ok = IMAP_ERROR;
	else if (!strncmp(greeting + 2, "OK", 2))
		ok = IMAP_SUCCESS;
	else if (!strncmp(greeting + 2, "PREAUTH", 7)) {
		session->authenticated = TRUE;
		ok = IMAP_SUCCESS;
	} else
		ok = IMAP_ERROR;

	g_free(greeting);
	return ok;
}

static gint imap_auth(IMAPSession *session, const gchar *user,
		      const gchar *pass, IMAPAuthType type)
{
	gboolean nologin;
	gint ok = IMAP_AUTHFAIL;

	nologin = imap_has_capability(session, "LOGINDISABLED");

	switch (type) {
	case 0:
		if (imap_has_capability(session, "AUTH=CRAM-MD5"))
			ok = imap_cmd_authenticate(session, user, pass,
						   IMAP_AUTH_CRAM_MD5);
		else if (imap_has_capability(session, "AUTH=PLAIN"))
			ok = imap_cmd_authenticate(session, user, pass,
						   IMAP_AUTH_PLAIN);
		else if (nologin)
			log_print(_("IMAP4 server disables LOGIN.\n"));
		else
			ok = imap_cmd_login(session, user, pass);
		break;
	case IMAP_AUTH_LOGIN:
		if (nologin)
			log_warning(_("IMAP4 server disables LOGIN.\n"));
		else
			ok = imap_cmd_login(session, user, pass);
		break;
	case IMAP_AUTH_CRAM_MD5:
	case IMAP_AUTH_PLAIN:
		ok = imap_cmd_authenticate(session, user, pass, type);
		break;
	default:
		break;
	}

	if (ok == IMAP_SUCCESS)
		session->authenticated = TRUE;

	return ok;
}

static Session *imap_session_new(PrefsAccount *account)
{
	IMAPSession *session;
	gushort port;

	g_return_val_if_fail(account != NULL, NULL);
	g_return_val_if_fail(account->recv_server != NULL, NULL);
	g_return_val_if_fail(account->userid != NULL, NULL);

#if USE_SSL
	port = account->set_imapport ? account->imapport
		: account->ssl_imap == SSL_TUNNEL ? IMAPS_PORT : IMAP4_PORT;
#else
	port = account->set_imapport ? account->imapport : IMAP4_PORT;
#endif

	session = IMAP_SESSION(g_new0(IMAPRealSession, 1));

	session_init(SESSION(session));

	SESSION(session)->type             = SESSION_IMAP;
	SESSION(session)->sock             = NULL;
	SESSION(session)->server           = g_strdup(account->recv_server);
	SESSION(session)->port             = port;
#if USE_SSL
	SESSION(session)->ssl_type         = account->ssl_imap;
#endif
	SESSION(session)->last_access_time = time(NULL);
	SESSION(session)->data             = account;

	SESSION(session)->destroy          = imap_session_destroy;

	session->authenticated = FALSE;
	session->capability    = NULL;
	session->uidplus       = FALSE;
	session->mbox          = NULL;
	session->cmd_count     = 0;

	session_list = g_list_append(session_list, session);

	if (imap_session_connect(session) != IMAP_SUCCESS) {
		log_warning(_("Could not establish IMAP connection.\n"));
		session_destroy(SESSION(session));
		return NULL;
	}

	return SESSION(session);
}

static gint imap_session_connect(IMAPSession *session)
{
	SockInfo *sock;
	SocksInfo *socks_info = NULL;
	PrefsAccount *account;
	const gchar *pass;

	g_return_val_if_fail(session != NULL, IMAP_ERROR);

	account = (PrefsAccount *)(SESSION(session)->data);

	log_message(_("creating IMAP4 connection to %s:%d ...\n"),
		    SESSION(session)->server, SESSION(session)->port);

	pass = account->passwd;
	if (!pass)
		pass = account->tmp_pass;
	if (!pass) {
		gchar *tmp_pass;

		tmp_pass = input_query_password(account->recv_server,
						account->userid);
		if (!tmp_pass)
			return IMAP_ERROR;

		account->tmp_pass = tmp_pass;
		pass = account->tmp_pass;
	}

	if (account->use_socks && account->use_socks_for_recv &&
	    account->proxy_host) {
		socks_info = socks_info_new(account->socks_type, account->proxy_host, account->proxy_port, account->use_proxy_auth ? account->proxy_name : NULL, account->use_proxy_auth ? account->proxy_pass : NULL);
	}

#if USE_SSL
	if ((sock = imap_open(SESSION(session)->server, SESSION(session)->port,
			      socks_info, SESSION(session)->ssl_type)) == NULL)
#else
	if ((sock = imap_open(SESSION(session)->server, SESSION(session)->port,
			      socks_info))
	    == NULL)
#endif
		return IMAP_ERROR;

	if (socks_info)
		socks_info_free(socks_info);

	SESSION(session)->sock = sock;

	if (imap_greeting(session) != IMAP_SUCCESS)
		return IMAP_ERROR;
	if (imap_cmd_capability(session) != IMAP_SUCCESS)
		return IMAP_ERROR;

	if (imap_has_capability(session, "UIDPLUS"))
		session->uidplus = TRUE;

#if USE_SSL
	if (account->ssl_imap == SSL_STARTTLS &&
	    imap_has_capability(session, "STARTTLS")) {
		gint ok;

		ok = imap_cmd_starttls(session);
		if (ok != IMAP_SUCCESS) {
			log_warning(_("Can't start TLS session.\n"));
			return IMAP_ERROR;
		}
		if (!ssl_init_socket_with_method(sock, SSL_METHOD_TLSv1))
			return IMAP_SOCKET;

		/* capability can be changed after STARTTLS */
		if (imap_cmd_capability(session) != IMAP_SUCCESS)
			return IMAP_ERROR;
	}
#endif

	if (!session->authenticated &&
	    imap_auth(session, account->userid, pass, account->imap_auth_type)
	    != IMAP_SUCCESS) {
		if (account->tmp_pass) {
			g_free(account->tmp_pass);
			account->tmp_pass = NULL;
		}
		imap_cmd_logout(session);
		return IMAP_AUTHFAIL;
	}

	return IMAP_SUCCESS;
}

static gint imap_session_reconnect(IMAPSession *session)
{
	g_return_val_if_fail(session != NULL, IMAP_ERROR);

	session_disconnect(SESSION(session));

	imap_capability_free(session);
	session->uidplus = FALSE;
	g_free(session->mbox);
	session->mbox = NULL;
	session->authenticated = FALSE;
	SESSION(session)->state = SESSION_READY;

	return imap_session_connect(session);
}

static void imap_session_destroy(Session *session)
{
#if USE_THREADS
	IMAPRealSession *real = (IMAPRealSession *)session;

	if (real->pool)
		g_thread_pool_free(real->pool, TRUE, TRUE);
#endif
	imap_capability_free(IMAP_SESSION(session));
	g_free(IMAP_SESSION(session)->mbox);
	session_list = g_list_remove(session_list, session);
}

#if 0
static void imap_session_destroy_all(void)
{
	while (session_list != NULL) {
		IMAPSession *session = (IMAPSession *)session_list->data;

		imap_cmd_logout(session);
		session_destroy(SESSION(session));
	}
}
#endif

#define THROW goto catch

static gint imap_search_flags(IMAPSession *session, GArray **uids,
			      GHashTable **flags_table)
{
	gint ok;
	gint i;
	GArray *flag_uids;
	GHashTable *unseen_table;
	GHashTable *flagged_table;
	GHashTable *answered_table;
	guint32 uid;
	IMAPFlags flags;

	ok = imap_cmd_search(session, "ALL", uids);
	if (ok != IMAP_SUCCESS) return ok;

	ok = imap_cmd_search(session, "UNSEEN", &flag_uids);
	if (ok != IMAP_SUCCESS) {
		g_array_free(*uids, TRUE);
		return ok;
	}
	unseen_table = imap_get_uid_table(flag_uids);
	g_array_free(flag_uids, TRUE);
	ok = imap_cmd_search(session, "FLAGGED", &flag_uids);
	if (ok != IMAP_SUCCESS) {
		g_hash_table_destroy(unseen_table);
		g_array_free(*uids, TRUE);
		return ok;
	}
	flagged_table = imap_get_uid_table(flag_uids);
	g_array_free(flag_uids, TRUE);
	ok = imap_cmd_search(session, "ANSWERED", &flag_uids);
	if (ok != IMAP_SUCCESS) {
		g_hash_table_destroy(flagged_table);
		g_hash_table_destroy(unseen_table);
		g_array_free(*uids, TRUE);
		return ok;
	}
	answered_table = imap_get_uid_table(flag_uids);
	g_array_free(flag_uids, TRUE);

	*flags_table = g_hash_table_new(NULL, g_direct_equal);

	for (i = 0; i < (*uids)->len; i++) {
		uid = g_array_index(*uids, guint32, i);
		flags = IMAP_FLAG_DRAFT;
		if (!g_hash_table_lookup(unseen_table, GUINT_TO_POINTER(uid)))
			flags |= IMAP_FLAG_SEEN;
		if (g_hash_table_lookup(flagged_table, GUINT_TO_POINTER(uid)))
			flags |= IMAP_FLAG_FLAGGED;
		if (g_hash_table_lookup(answered_table, GUINT_TO_POINTER(uid)))
			flags |= IMAP_FLAG_ANSWERED;
		g_hash_table_insert(*flags_table, GUINT_TO_POINTER(uid),
				    GINT_TO_POINTER(flags));
	}

	g_hash_table_destroy(answered_table);
	g_hash_table_destroy(flagged_table);
	g_hash_table_destroy(unseen_table);

	return IMAP_SUCCESS;
}

static gint imap_fetch_flags(IMAPSession *session, GArray **uids,
			     GHashTable **flags_table)
{
	gint ok;
	gchar *tmp;
	gchar *cur_pos;
	gchar buf[IMAPBUFSIZE];
	guint32 uid;
	IMAPFlags flags;

	if (imap_cmd_gen_send(session, "UID FETCH 1:* (UID FLAGS)") != IMAP_SUCCESS)
		return IMAP_ERROR;

	*uids = g_array_new(FALSE, FALSE, sizeof(guint32));
	*flags_table = g_hash_table_new(NULL, g_direct_equal);

	log_print("IMAP4< %s\n", _("(retrieving FLAGS...)"));

	while ((ok = imap_cmd_gen_recv_silent(session, &tmp)) == IMAP_SUCCESS) {
		if (tmp[0] != '*' || tmp[1] != ' ') {
			log_print("IMAP4< %s\n", tmp);
			g_free(tmp);
			break;
		}
		cur_pos = tmp + 2;

#define PARSE_ONE_ELEMENT(ch)					\
{								\
	cur_pos = strchr_cpy(cur_pos, ch, buf, sizeof(buf));	\
	if (cur_pos == NULL) {					\
		g_warning("cur_pos == NULL\n");			\
		g_free(tmp);					\
		g_hash_table_destroy(*flags_table);		\
		g_array_free(*uids, TRUE);			\
		return IMAP_ERROR;				\
	}							\
}

		PARSE_ONE_ELEMENT(' ');
		PARSE_ONE_ELEMENT(' ');
		if (strcmp(buf, "FETCH") != 0) {
			g_free(tmp);
			continue;
		}
		if (*cur_pos != '(') {
			g_free(tmp);
			continue;
		}
		cur_pos++;
		uid = 0;
		flags = 0;

		while (*cur_pos != '\0' && *cur_pos != ')') {
			while (*cur_pos == ' ') cur_pos++;

			if (!strncmp(cur_pos, "UID ", 4)) {
				cur_pos += 4;
				uid = strtoul(cur_pos, &cur_pos, 10);
			} else if (!strncmp(cur_pos, "FLAGS ", 6)) {
				cur_pos += 6;
				if (*cur_pos != '(') {
					g_warning("*cur_pos != '('\n");
					break;
				}
				cur_pos++;
				PARSE_ONE_ELEMENT(')');
				flags = imap_parse_imap_flags(buf);
				flags |= IMAP_FLAG_DRAFT;
			} else {
				g_warning("invalid FETCH response: %s\n", cur_pos);
				break;
			}
		}

#undef PARSE_ONE_ELEMENT

		if (uid > 0) {
			g_array_append_val(*uids, uid);
			g_hash_table_insert(*flags_table, GUINT_TO_POINTER(uid),
					    GINT_TO_POINTER(flags));
		}

		g_free(tmp);
	}

	if (ok != IMAP_SUCCESS) {
		g_hash_table_destroy(*flags_table);
		g_array_free(*uids, TRUE);
	}

	return ok;
}

static GSList *imap_get_msg_list_full(Folder *folder, FolderItem *item,
				      gboolean use_cache,
				      gboolean uncached_only)
{
	GSList *mlist = NULL;
	IMAPSession *session;
	gint ok, exists = 0, recent = 0, unseen = 0;
	guint32 uid_validity = 0;
	guint32 first_uid = 0, last_uid = 0;
	GSList *newlist = NULL;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(FOLDER_TYPE(folder) == F_IMAP, NULL);
	g_return_val_if_fail(folder->account != NULL, NULL);

	item->new = item->unread = item->total = 0;

	session = imap_session_get(folder);

	if (!session) {
		if (uncached_only)
			return NULL;
		mlist = procmsg_read_cache(item, FALSE);
		item->last_num = procmsg_get_last_num_in_msg_list(mlist);
		procmsg_set_flags(mlist, item);
		return mlist;
	}

	ok = imap_select(session, IMAP_FOLDER(folder), item->path,
			 &exists, &recent, &unseen, &uid_validity);
	if (ok != IMAP_SUCCESS) THROW;

	if (exists == 0) {
		imap_delete_all_cached_messages(item);
		return NULL;
	}

	/* invalidate current cache if UIDVALIDITY has been changed */
	if (item->mtime != uid_validity) {
		debug_print("imap_get_msg_list: "
			    "UIDVALIDITY has been changed.\n");
		use_cache = FALSE;
	}

	if (use_cache) {
		GArray *uids;
		GHashTable *msg_table;
		GHashTable *flags_table;
		guint32 cache_last;
		guint32 begin = 0;
		GSList *cur, *next = NULL;
		MsgInfo *msginfo;
		IMAPFlags imap_flags;
		guint color;

		/* get cache data */
		mlist = procmsg_read_cache(item, FALSE);
		procmsg_set_flags(mlist, item);
		cache_last = procmsg_get_last_num_in_msg_list(mlist);

		/* get all UID list and flags */
#if 0
		ok = imap_search_flags(session, &uids, &flags_table);
		if (ok != IMAP_SUCCESS) {
			if (ok == IMAP_SOCKET || ok == IMAP_IOERR) THROW;
			ok = imap_fetch_flags(session, &uids, &flags_table);
			if (ok != IMAP_SUCCESS) THROW;
		}
#else
		ok = imap_fetch_flags(session, &uids, &flags_table);
		if (ok != IMAP_SUCCESS) THROW;
#endif

		if (uids->len > 0) {
			first_uid = g_array_index(uids, guint32, 0);
			last_uid = g_array_index(uids, guint32, uids->len - 1);
		} else {
			g_array_free(uids, TRUE);
			g_hash_table_destroy(flags_table);
			THROW;
		}

		/* sync message flags with server */
		for (cur = mlist; cur != NULL; cur = next) {
			msginfo = (MsgInfo *)cur->data;
			next = cur->next;
			imap_flags = GPOINTER_TO_INT(g_hash_table_lookup
				(flags_table,
				 GUINT_TO_POINTER(msginfo->msgnum)));

			if (imap_flags == 0) {
				debug_print("imap_get_msg_list: "
					    "message %u has been deleted.\n",
					    msginfo->msgnum);
				imap_delete_cached_message
					(item, msginfo->msgnum);
				if (MSG_IS_NEW(msginfo->flags))
					item->new--;
				if (MSG_IS_UNREAD(msginfo->flags))
					item->unread--;
				item->total--;
				mlist = g_slist_remove(mlist, msginfo);
				procmsg_msginfo_free(msginfo);
				item->cache_dirty = TRUE;
				item->mark_dirty = TRUE;
				continue;
			}

			if (!IMAP_IS_SEEN(imap_flags)) {
				if (!MSG_IS_UNREAD(msginfo->flags)) {
					item->unread++;
					MSG_SET_PERM_FLAGS(msginfo->flags,
							   MSG_UNREAD);
					item->mark_dirty = TRUE;
				}
			} else {
				if (MSG_IS_NEW(msginfo->flags)) {
					item->new--;
					item->mark_dirty = TRUE;
				}
				if (MSG_IS_UNREAD(msginfo->flags)) {
					item->unread--;
					item->mark_dirty = TRUE;
				}
				MSG_UNSET_PERM_FLAGS(msginfo->flags,
						     MSG_NEW|MSG_UNREAD);
			}

			if (IMAP_IS_FLAGGED(imap_flags)) {
				if (!MSG_IS_MARKED(msginfo->flags)) {
					MSG_SET_PERM_FLAGS(msginfo->flags,
							   MSG_MARKED);
					item->mark_dirty = TRUE;
				}
			} else {
				if (MSG_IS_MARKED(msginfo->flags)) {
					MSG_UNSET_PERM_FLAGS(msginfo->flags,
							     MSG_MARKED);
					item->mark_dirty = TRUE;
				}
			}
			if (IMAP_IS_ANSWERED(imap_flags)) {
				if (!MSG_IS_REPLIED(msginfo->flags)) {
					MSG_SET_PERM_FLAGS(msginfo->flags,
							   MSG_REPLIED);
					item->mark_dirty = TRUE;
				}
			} else {
				if (MSG_IS_REPLIED(msginfo->flags)) {
					MSG_UNSET_PERM_FLAGS(msginfo->flags,
							     MSG_REPLIED);
					item->mark_dirty = TRUE;
				}
			}

			color = IMAP_GET_COLORLABEL_VALUE(imap_flags);
			if (MSG_GET_COLORLABEL_VALUE(msginfo->flags) != color) {
				MSG_UNSET_PERM_FLAGS(msginfo->flags,
						     MSG_CLABEL_FLAG_MASK);
				MSG_SET_COLORLABEL_VALUE(msginfo->flags, color);
				item->mark_dirty = TRUE;
			}
		}

		/* check for the first new message */
		msg_table = procmsg_msg_hash_table_create(mlist);
		if (msg_table == NULL)
			begin = first_uid;
		else {
			gint i;

			for (i = 0; i < uids->len; i++) {
				guint32 uid;

				uid = g_array_index(uids, guint32, i);
				if (g_hash_table_lookup
					(msg_table, GUINT_TO_POINTER(uid))
					== NULL) {
					debug_print("imap_get_msg_list: "
						    "first new UID: %u\n", uid);
					begin = uid;
					break;
				}
			}
			g_hash_table_destroy(msg_table);
		}

		g_array_free(uids, TRUE);
		g_hash_table_destroy(flags_table);

		/* remove ununsed caches */
		if (first_uid > 0 && last_uid > 0) {
			mlist = imap_delete_cached_messages
				(mlist, item, 0, first_uid - 1);
			mlist = imap_delete_cached_messages
				(mlist, item, begin > 0 ? begin : last_uid + 1,
				 UINT_MAX);
		}

		if (begin > 0 && begin <= last_uid) {
			newlist = imap_get_uncached_messages
				(session, item, begin, last_uid,
				 exists - item->total, TRUE);
			if (newlist) {
				item->cache_dirty = TRUE;
				item->mark_dirty = TRUE;
			}
			mlist = g_slist_concat(mlist, newlist);
		}
	} else {
		imap_delete_all_cached_messages(item);
		mlist = imap_get_uncached_messages(session, item, 0, 0, exists,
						   TRUE);
		last_uid = procmsg_get_last_num_in_msg_list(mlist);
		item->cache_dirty = TRUE;
		item->mark_dirty = TRUE;
		newlist = mlist;
	}

	if (!uncached_only)
		mlist = procmsg_sort_msg_list(mlist, item->sort_key,
					      item->sort_type);

	item->last_num = last_uid;

	if (item->mark_queue)
		item->mark_dirty = TRUE;

	debug_print("cache_dirty: %d, mark_dirty: %d\n",
		    item->cache_dirty, item->mark_dirty);

	if (!item->opened) {
		item->mtime = uid_validity;
		if (item->cache_dirty)
			procmsg_write_cache_list(item, mlist);
		if (item->mark_dirty)
			procmsg_write_flags_list(item, mlist);
	}

catch:
	if (uncached_only) {
		GSList *cur;

		if (newlist == NULL) {
			procmsg_msg_list_free(mlist);
			return NULL;
		}
		if (mlist == newlist)
			return newlist;
		for (cur = mlist; cur != NULL; cur = cur->next) {
			if (cur->next == newlist) {
				cur->next = NULL;
				procmsg_msg_list_free(mlist);
				return newlist;
			}
		}
		procmsg_msg_list_free(mlist);
		return NULL;
	}

	return mlist;
}

#undef THROW

static GSList *imap_get_msg_list(Folder *folder, FolderItem *item,
				 gboolean use_cache)
{
	return imap_get_msg_list_full(folder, item, use_cache, FALSE);
}

static GSList *imap_get_uncached_msg_list(Folder *folder, FolderItem *item)
{
	return imap_get_msg_list_full(folder, item, TRUE, TRUE);
}

static gchar *imap_fetch_msg(Folder *folder, FolderItem *item, gint uid)
{
	gchar *path, *filename;
	IMAPSession *session;
	gchar nstr[16];
	guint32 uid32 = (guint32)uid;
	gint ok;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);

	path = folder_item_get_path(item);
	if (!is_dir_exist(path))
		make_dir_hier(path);
	g_snprintf(nstr, sizeof(nstr), "%u", uid32);
	filename = g_strconcat(path, G_DIR_SEPARATOR_S, nstr, NULL);
	g_free(path);

	if (is_file_exist(filename) && get_file_size(filename) > 0) {
		debug_print("message %u has been already cached.\n", uid32);
		return filename;
	}

	session = imap_session_get(folder);
	if (!session) {
		g_free(filename);
		return NULL;
	}

	ok = imap_select(session, IMAP_FOLDER(folder), item->path,
			 NULL, NULL, NULL, NULL);
	if (ok != IMAP_SUCCESS) {
		g_warning("can't select mailbox %s\n", item->path);
		g_free(filename);
		return NULL;
	}

	status_print(_("Getting message %u"), uid32);
	debug_print("getting message %u...\n", uid32);
	ok = imap_cmd_fetch(session, uid32, filename);

	if (ok != IMAP_SUCCESS) {
		g_warning("can't fetch message %u\n", uid32);
		g_free(filename);
		return NULL;
	}

	return filename;
}

static MsgInfo *imap_get_msginfo(Folder *folder, FolderItem *item, gint uid)
{
	IMAPSession *session;
	GSList *list;
	MsgInfo *msginfo = NULL;
	gint ok;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);

	session = imap_session_get(folder);
	g_return_val_if_fail(session != NULL, NULL);

	ok = imap_select(session, IMAP_FOLDER(folder), item->path,
			 NULL, NULL, NULL, NULL);
	if (ok != IMAP_SUCCESS)
		return NULL;

	list = imap_get_uncached_messages(session, item, uid, uid, 0, FALSE);
	if (list) {
		msginfo = (MsgInfo *)list->data;
		list->data = NULL;
	}
	procmsg_msg_list_free(list);

	return msginfo;
}

static gint imap_add_msg(Folder *folder, FolderItem *dest, const gchar *file,
			 MsgFlags *flags, gboolean remove_source)
{
	GSList file_list;
	MsgFileInfo fileinfo;

	g_return_val_if_fail(file != NULL, -1);

	fileinfo.file = (gchar *)file;
	fileinfo.flags = flags;
	file_list.data = &fileinfo;
	file_list.next = NULL;

	return imap_add_msgs(folder, dest, &file_list, remove_source, NULL);
}

static gint imap_add_msgs(Folder *folder, FolderItem *dest, GSList *file_list,
			  gboolean remove_source, gint *first)
{
	gchar *destdir;
	IMAPSession *session;
	gint messages, recent, unseen;
	guint32 uid_next, uid_validity;
	guint32 last_uid = 0;
	GSList *cur;
	MsgFileInfo *fileinfo;
	gint count = 1;
	gint total;
	gint ok;
	GTimeVal tv_prev, tv_cur;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(file_list != NULL, -1);

	session = imap_session_get(folder);
	if (!session) return -1;

	g_get_current_time(&tv_prev);
	ui_update();

	ok = imap_status(session, IMAP_FOLDER(folder), dest->path,
			 &messages, &recent, &uid_next, &uid_validity, &unseen);
	if (ok != IMAP_SUCCESS) {
		g_warning("can't append messages\n");
		return -1;
	}

	destdir = imap_get_real_path(IMAP_FOLDER(folder), dest->path);

	if (!session->uidplus)
		last_uid = uid_next - 1;
	if (first)
		*first = uid_next;

	total = g_slist_length(file_list);

	for (cur = file_list; cur != NULL; cur = cur->next) {
		IMAPFlags iflags = 0;
		guint32 new_uid = 0;

		fileinfo = (MsgFileInfo *)cur->data;

		if (fileinfo->flags) {
			if (MSG_IS_MARKED(*fileinfo->flags))
				iflags |= IMAP_FLAG_FLAGGED;
			if (MSG_IS_REPLIED(*fileinfo->flags))
				iflags |= IMAP_FLAG_ANSWERED;
			if (!MSG_IS_UNREAD(*fileinfo->flags))
				iflags |= IMAP_FLAG_SEEN;
		}

		if (dest->stype == F_OUTBOX ||
		    dest->stype == F_QUEUE  ||
		    dest->stype == F_DRAFT)
			iflags |= IMAP_FLAG_SEEN;

		g_get_current_time(&tv_cur);
		if (tv_cur.tv_sec > tv_prev.tv_sec ||
		    tv_cur.tv_usec - tv_prev.tv_usec >
		    PROGRESS_UPDATE_INTERVAL * 1000) {
			status_print(_("Appending messages to %s (%d / %d)"),
				     dest->path, count, total);
			progress_show(count, total);
			ui_update();
			tv_prev = tv_cur;
		}
		++count;

		ok = imap_cmd_append(session, destdir, fileinfo->file, iflags,
				     &new_uid);

		if (ok != IMAP_SUCCESS) {
			g_warning("can't append message %s\n", fileinfo->file);
			g_free(destdir);
			progress_show(0, 0);
			return -1;
		}

		if (syl_app_get())
			g_signal_emit_by_name(syl_app_get(), "add-msg", dest, fileinfo->file, new_uid);

		if (!session->uidplus)
			last_uid++;
		else if (last_uid < new_uid)
			last_uid = new_uid;

		dest->last_num = last_uid;
		dest->total++;
		dest->updated = TRUE;

		if (fileinfo->flags) {
			if (MSG_IS_UNREAD(*fileinfo->flags))
				dest->unread++;
		} else
			dest->unread++;
	}

	progress_show(0, 0);
	g_free(destdir);

	if (remove_source) {
		for (cur = file_list; cur != NULL; cur = cur->next) {
			fileinfo = (MsgFileInfo *)cur->data;
			if (g_unlink(fileinfo->file) < 0)
				 FILE_OP_ERROR(fileinfo->file, "unlink");
		}
	}

	return last_uid;
}

static gint imap_add_msg_msginfo(Folder *folder, FolderItem *dest,
				 MsgInfo *msginfo, gboolean remove_source)
{
	GSList msglist;

	g_return_val_if_fail(msginfo != NULL, -1);

	msglist.data = msginfo;
	msglist.next = NULL;

	return imap_add_msgs_msginfo(folder, dest, &msglist, remove_source,
				     NULL);
}

static gint imap_add_msgs_msginfo(Folder *folder, FolderItem *dest,
				  GSList *msglist, gboolean remove_source,
				  gint *first)
{
	GSList *file_list;
	gint ret;

	file_list = procmsg_get_message_file_list(msglist);
	g_return_val_if_fail(file_list != NULL, -1);

	ret = imap_add_msgs(folder, dest, file_list, remove_source, first);

	procmsg_message_file_list_free(file_list);

	return ret;
}

static gint imap_do_copy_msgs(Folder *folder, FolderItem *dest, GSList *msglist,
			      gboolean remove_source)
{
	FolderItem *src;
	gchar *destdir;
	GSList *seq_list, *cur;
	MsgInfo *msginfo;
	IMAPSession *session;
	gint count = 0, total;
	gint ok = IMAP_SUCCESS;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msglist != NULL, -1);

	session = imap_session_get(folder);
	if (!session) return -1;

	ui_update();

	msginfo = (MsgInfo *)msglist->data;

	src = msginfo->folder;
	if (src == dest) {
		g_warning("the src folder is identical to the dest.\n");
		return -1;
	}

	ok = imap_select(session, IMAP_FOLDER(folder), src->path,
			 NULL, NULL, NULL, NULL);
	if (ok != IMAP_SUCCESS)
		return ok;

	destdir = imap_get_real_path(IMAP_FOLDER(folder), dest->path);

	total = g_slist_length(msglist);
	seq_list = imap_get_seq_set_from_msglist(msglist, IMAP_COPY_LIMIT);

	for (cur = seq_list; cur != NULL; cur = cur->next) {
		gchar *seq_set = (gchar *)cur->data;

		count += imap_seq_set_get_count(seq_set);

		if (remove_source) {
			status_print(_("Moving messages %s to %s ..."),
				     seq_set, dest->path);
			debug_print("Moving message %s/[%s] to %s ...\n",
				    src->path, seq_set, dest->path);
		} else {
			status_print(_("Copying messages %s to %s ..."),
				     seq_set, dest->path);
			debug_print("Copying message %s/[%s] to %s ...\n",
				    src->path, seq_set, dest->path);
		}
		progress_show(count, total);
		ui_update();

		ok = imap_cmd_copy(session, seq_set, destdir);
		if (ok != IMAP_SUCCESS) {
			imap_seq_set_free(seq_list);
			progress_show(0, 0);
			return -1;
		}
	}

	progress_show(0, 0);

	dest->updated = TRUE;

	imap_seq_set_free(seq_list);
	g_free(destdir);

	for (cur = msglist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;

		if (syl_app_get())
			g_signal_emit_by_name(syl_app_get(), "add-msg", dest, NULL, 0);

		dest->total++;
		if (MSG_IS_NEW(msginfo->flags))
			dest->new++;
		if (MSG_IS_UNREAD(msginfo->flags))
			dest->unread++;
	}

	if (remove_source) {
		ok = imap_remove_msgs(folder, src, msglist);
		if (ok != IMAP_SUCCESS)
			return ok;
	}

	if (ok == IMAP_SUCCESS)
		return 0;
	else
		return -1;
}

static gint imap_move_msg(Folder *folder, FolderItem *dest, MsgInfo *msginfo)
{
	GSList msglist;

	g_return_val_if_fail(msginfo != NULL, -1);

	msglist.data = msginfo;
	msglist.next = NULL;

	return imap_move_msgs(folder, dest, &msglist);
}

static gint imap_move_msgs(Folder *folder, FolderItem *dest, GSList *msglist)
{
	MsgInfo *msginfo;
	GSList *file_list;
	gint ret = 0;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msglist != NULL, -1);

	msginfo = (MsgInfo *)msglist->data;
	g_return_val_if_fail(msginfo->folder != NULL, -1);

	if (folder == msginfo->folder->folder)
		return imap_do_copy_msgs(folder, dest, msglist, TRUE);

	file_list = procmsg_get_message_file_list(msglist);
	g_return_val_if_fail(file_list != NULL, -1);

	ret = imap_add_msgs(folder, dest, file_list, FALSE, NULL);

	procmsg_message_file_list_free(file_list);

	if (ret != -1)
		ret = folder_item_remove_msgs(msginfo->folder, msglist);

	return ret;
}

static gint imap_copy_msg(Folder *folder, FolderItem *dest, MsgInfo *msginfo)
{
	GSList msglist;

	g_return_val_if_fail(msginfo != NULL, -1);

	msglist.data = msginfo;
	msglist.next = NULL;

	return imap_copy_msgs(folder, dest, &msglist);
}

static gint imap_copy_msgs(Folder *folder, FolderItem *dest, GSList *msglist)
{
	MsgInfo *msginfo;
	GSList *file_list;
	gint ret;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msglist != NULL, -1);

	msginfo = (MsgInfo *)msglist->data;
	g_return_val_if_fail(msginfo->folder != NULL, -1);

	if (folder == msginfo->folder->folder)
		return imap_do_copy_msgs(folder, dest, msglist, FALSE);

	file_list = procmsg_get_message_file_list(msglist);
	g_return_val_if_fail(file_list != NULL, -1);

	ret = imap_add_msgs(folder, dest, file_list, FALSE, NULL);

	procmsg_message_file_list_free(file_list);

	return ret;
}

static gint imap_remove_msgs_by_seq_set(Folder *folder, FolderItem *item,
					GSList *seq_list)
{
	gint ok;
	IMAPSession *session;
	GSList *cur;

	g_return_val_if_fail(seq_list != NULL, -1);

	session = imap_session_get(folder);
	if (!session) return -1;

	for (cur = seq_list; cur != NULL; cur = cur->next) {
		gchar *seq_set = (gchar *)cur->data;

		status_print(_("Removing messages %s"), seq_set);
		ui_update();

		ok = imap_set_message_flags(session, seq_set, IMAP_FLAG_DELETED,
					    TRUE);
		if (ok != IMAP_SUCCESS) {
			log_warning(_("can't set deleted flags: %s\n"),
				    seq_set);
			return ok;
		}
	}

	ok = imap_cmd_expunge(session);
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't expunge\n"));
	} else {
		/* for some broken IMAP servers */
		ok = imap_cmd_noop(session);
	}

	item->updated = TRUE;

	return ok;
}

static gint imap_remove_msg(Folder *folder, FolderItem *item, MsgInfo *msginfo)
{
	GSList msglist;

	g_return_val_if_fail(msginfo != NULL, -1);

	msglist.data = msginfo;
	msglist.next = NULL;

	return imap_remove_msgs(folder, item, &msglist);
}

static gint imap_remove_msgs(Folder *folder, FolderItem *item, GSList *msglist)
{
	gint ok;
	IMAPSession *session;
	GSList *seq_list, *cur;
	gchar *dir;
	gboolean dir_exist;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(FOLDER_TYPE(folder) == F_IMAP, -1);
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(msglist != NULL, -1);

	session = imap_session_get(folder);
	if (!session) return -1;

	ok = imap_select(session, IMAP_FOLDER(folder), item->path,
			 NULL, NULL, NULL, NULL);
	if (ok != IMAP_SUCCESS)
		return ok;

	seq_list = imap_get_seq_set_from_msglist(msglist, 0);
	ok = imap_remove_msgs_by_seq_set(folder, item, seq_list);
	imap_seq_set_free(seq_list);
	if (ok != IMAP_SUCCESS)
		return ok;

	dir = folder_item_get_path(item);
	dir_exist = is_dir_exist(dir);
	for (cur = msglist; cur != NULL; cur = cur->next) {
		MsgInfo *msginfo = (MsgInfo *)cur->data;
		guint32 uid = msginfo->msgnum;

		if (syl_app_get())
			g_signal_emit_by_name(syl_app_get(), "remove-msg", item, NULL, uid);

		if (dir_exist)
			remove_numbered_files(dir, uid, uid);
		item->total--;
		if (MSG_IS_NEW(msginfo->flags))
			item->new--;
		if (MSG_IS_UNREAD(msginfo->flags))
			item->unread--;
		MSG_SET_TMP_FLAGS(msginfo->flags, MSG_INVALID);
	}
	g_free(dir);

	return IMAP_SUCCESS;
}

static gint imap_remove_all_msg(Folder *folder, FolderItem *item)
{
	gint ok;
	IMAPSession *session;
	gchar *dir;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);

	session = imap_session_get(folder);
	if (!session) return -1;

	ok = imap_select(session, IMAP_FOLDER(folder), item->path,
			 NULL, NULL, NULL, NULL);
	if (ok != IMAP_SUCCESS)
		return ok;

	status_print(_("Removing all messages in %s"), item->path);
	ui_update();

	ok = imap_cmd_gen_send(session, "STORE 1:* +FLAGS.SILENT (\\Deleted)");
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't set deleted flags: 1:*\n"));
		return ok;
	}
	ok = imap_cmd_ok(session, NULL);
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't set deleted flags: 1:*\n"));
		return ok;
	}

	ok = imap_cmd_expunge(session);
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't expunge\n"));
		return ok;
	}

	if (syl_app_get())
		g_signal_emit_by_name(syl_app_get(), "remove-all-msg", item);

	item->new = item->unread = item->total = 0;
	item->updated = TRUE;

	dir = folder_item_get_path(item);
	if (is_dir_exist(dir))
		remove_all_numbered_files(dir);
	g_free(dir);

	return IMAP_SUCCESS;
}

static gboolean imap_is_msg_changed(Folder *folder, FolderItem *item,
				    MsgInfo *msginfo)
{
	/* TODO: properly implement this method */
	return FALSE;
}

static gint imap_close(Folder *folder, FolderItem *item)
{
	gint ok;
	IMAPSession *session;

	g_return_val_if_fail(folder != NULL, -1);

	if (!item->path) return 0;

	if (!REMOTE_FOLDER(folder)->session)
		return 0;

	session = imap_session_get(folder);
	if (!session) return -1;

	if (session->mbox) {
		if (strcmp2(session->mbox, item->path) != 0) return -1;

		ok = imap_cmd_close(session);
		if (ok != IMAP_SUCCESS)
			log_warning(_("can't close folder\n"));

		g_free(session->mbox);
		session->mbox = NULL;

		return ok;
	} else
		return 0;
}

static gint imap_scan_folder(Folder *folder, FolderItem *item)
{
	IMAPSession *session;
	gint messages, recent, unseen;
	guint32 uid_next, uid_validity;
	gint ok;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);

	session = imap_session_get(folder);
	if (!session) return -1;

	ok = imap_status(session, IMAP_FOLDER(folder), item->path,
			 &messages, &recent, &uid_next, &uid_validity, &unseen);
	if (ok != IMAP_SUCCESS) return -1;

	item->new = unseen > 0 ? recent : 0;
	item->unread = unseen;
	item->total = messages;
	item->last_num = (messages > 0 && uid_next > 0) ? uid_next - 1 : 0;
	/* item->mtime = uid_validity; */
	item->updated = TRUE;

	return 0;
}

static gint imap_scan_tree(Folder *folder)
{
	FolderItem *item = NULL;
	IMAPSession *session;
	gchar *root_folder = NULL;
	GSList *item_list, *cur;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(folder->account != NULL, -1);

	session = imap_session_get(folder);
	if (!session) {
		if (!folder->node) {
			folder_tree_destroy(folder);
			item = folder_item_new(folder->name, NULL);
			item->folder = folder;
			folder->node = item->node = g_node_new(item);
		}
		return -1;
	}

	if (folder->account->imap_dir && *folder->account->imap_dir) {
		gchar *real_path;
		GPtrArray *argbuf;
		gint ok;

		Xstrdup_a(root_folder, folder->account->imap_dir, return -1);
		extract_quote(root_folder, '"');
		subst_char(root_folder,
			   imap_get_path_separator(IMAP_FOLDER(folder),
						   root_folder),
			   '/');
		strtailchomp(root_folder, '/');
		real_path = imap_get_real_path
			(IMAP_FOLDER(folder), root_folder);
		debug_print("IMAP root directory: %s\n", real_path);

		/* check if root directory exist */
		argbuf = g_ptr_array_new();
		ok = imap_cmd_list(session, NULL, real_path, argbuf);
		if (ok != IMAP_SUCCESS ||
		    search_array_str(argbuf, "LIST ") == NULL) {
			log_warning(_("root folder %s not exist\n"), real_path);
			g_ptr_array_free(argbuf, TRUE);
			g_free(real_path);
			return -1;
		}
		g_ptr_array_free(argbuf, TRUE);
		g_free(real_path);
	}

	if (folder->node)
		item = FOLDER_ITEM(folder->node->data);
	if (!item || ((item->path || root_folder) &&
		      strcmp2(item->path, root_folder) != 0)) {
		folder_tree_destroy(folder);
		item = folder_item_new(folder->name, root_folder);
		item->folder = folder;
		folder->node = item->node = g_node_new(item);
	}

	item_list = imap_get_folder_list(session, item);
	imap_scan_tree_recursive(session, item, item_list);
	imap_create_missing_folders(folder);

	for (cur = item_list; cur != NULL; cur = cur->next)
		folder_item_destroy(FOLDER_ITEM(cur->data));
	g_slist_free(item_list);

	return 0;
}

static gint imap_scan_tree_recursive(IMAPSession *session, FolderItem *item,
				     GSList *item_list)
{
	Folder *folder;
	FolderItem *new_item;
	GSList *part_list, *cur;
	GNode *node;

	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->folder != NULL, -1);
	g_return_val_if_fail(item->no_sub == FALSE, -1);

	folder = item->folder;

	part_list = imap_get_part_folder_list(item_list, item);

	node = item->node->children;
	while (node != NULL) {
		FolderItem *old_item = FOLDER_ITEM(node->data);
		GNode *next = node->next;

		new_item = NULL;

		for (cur = part_list; cur != NULL; cur = cur->next) {
			FolderItem *cur_item = FOLDER_ITEM(cur->data);
			if (!strcmp2(old_item->path, cur_item->path)) {
				new_item = cur_item;
				break;
			}
		}
		if (!new_item) {
			if (old_item->stype != F_VIRTUAL) {
				debug_print("folder '%s' not found. removing...\n", old_item->path);
				folder_item_remove(old_item);
			}
		} else if (old_item->stype == F_VIRTUAL) {
				debug_print("IMAP4 folder found at the location of virtual folder '%s'. removing virtual folder...\n", old_item->path);
				virtual_get_class()->remove_folder
					(folder, old_item);
		} else {
			old_item->no_sub = new_item->no_sub;
			old_item->no_select = new_item->no_select;
			if (old_item->no_select == TRUE)
				old_item->new = old_item->unread =
					old_item->total = 0;
			if (old_item->no_sub == TRUE && node->children) {
				debug_print("folder '%s' doesn't have "
					    "subfolders. removing...\n",
					    old_item->path);
				folder_item_remove_children(old_item);
			}
		}

		node = next;
	}

	for (cur = part_list; cur != NULL; cur = cur->next) {
		FolderItem *cur_item = FOLDER_ITEM(cur->data);
		new_item = NULL;
		for (node = item->node->children; node != NULL;
		     node = node->next) {
			if (!strcmp2(FOLDER_ITEM(node->data)->path,
				     cur_item->path)) {
				new_item = FOLDER_ITEM(node->data);
				cur_item = NULL;
				break;
			}
		}
		if (!new_item) {
			new_item = folder_item_copy(cur_item);
			debug_print("new folder '%s' found.\n", new_item->path);
			folder_item_append(item, new_item);
		}

		if (!g_ascii_strcasecmp(new_item->path, "INBOX")) {
			new_item->stype = F_INBOX;
			folder->inbox = new_item;
		} else if (!item->parent || item->stype == F_INBOX) {
			const gchar *base;

			base = g_basename(new_item->path);

			if (!folder->outbox &&
			    !g_ascii_strcasecmp(base, "Sent")) {
				new_item->stype = F_OUTBOX;
				folder->outbox = new_item;
			} else if (!folder->draft &&
				   !g_ascii_strcasecmp(base, "Drafts")) {
				new_item->stype = F_DRAFT;
				folder->draft = new_item;
			} else if (!folder->queue &&
				   !g_ascii_strcasecmp(base, "Queue")) {
				new_item->stype = F_QUEUE;
				folder->queue = new_item;
			} else if (!folder->trash &&
				   !g_ascii_strcasecmp(base, "Trash")) {
				new_item->stype = F_TRASH;
				folder->trash = new_item;
			}
		}

#if 0
		if (new_item->no_select == FALSE)
			imap_scan_folder(folder, new_item);
#endif
		if (new_item->no_sub == FALSE)
			imap_scan_tree_recursive(session, new_item, item_list);
	}

	g_slist_free(part_list);

	return IMAP_SUCCESS;
}

static GSList *imap_get_folder_list(IMAPSession *session, FolderItem *item)
{
	Folder *folder;
	IMAPFolder *imapfolder;
	gchar *real_path;
	gchar *wildcard_path;
	gchar separator;
	GSList *item_list = NULL;

	folder = item->folder;
	imapfolder = IMAP_FOLDER(folder);

	separator = imap_get_path_separator(imapfolder, item->path);

	if (folder->ui_func)
		folder->ui_func(folder, item, folder->ui_func_data);

	if (item->path) {
		real_path = imap_get_real_path(imapfolder, item->path);
		strtailchomp(real_path, separator);
		wildcard_path = g_strdup_printf("%s%c*", real_path, separator);
	} else {
		real_path = g_strdup("");
		wildcard_path = g_strdup("*");
	}

	if (imap_cmd_gen_send(session, "LIST \"\" \"%s\"", wildcard_path) == IMAP_SUCCESS) {
		item_list = imap_parse_list(session, real_path, NULL);
		item_list = imap_add_inter_folders(item_list, item->path);
	}
	g_free(real_path);
	g_free(wildcard_path);

	return item_list;
}

static GSList *imap_parse_list(IMAPSession *session, const gchar *real_path,
			       gchar *separator)
{
	gchar buf[IMAPBUFSIZE];
	gchar flags[256];
	gchar separator_str[16];
	gchar *p;
	const gchar *name;
	gchar *loc_name, *loc_path;
	GSList *item_list = NULL;
	GString *str;
	FolderItem *new_item;

	debug_print("getting list of %s ...\n",
		    *real_path ? real_path : "\"\"");

	str = g_string_new(NULL);

	for (;;) {
		if (sock_gets(SESSION(session)->sock, buf, sizeof(buf)) <= 0) {
			log_warning(_("error occurred while getting LIST.\n"));
			break;
		}
		strretchomp(buf);
		if (buf[0] != '*' || buf[1] != ' ') {
			log_print("IMAP4< %s\n", buf);
			if (sscanf(buf, "%*d %16s", buf) < 1 ||
			    strcmp(buf, "OK") != 0)
				log_warning(_("error occurred while getting LIST.\n"));
				
			break;
		}
		debug_print("IMAP4< %s\n", buf);

		g_string_assign(str, buf);
		p = str->str + 2;
		if (strncmp(p, "LIST ", 5) != 0) continue;
		p += 5;

		if (*p != '(') continue;
		p++;
		p = strchr_cpy(p, ')', flags, sizeof(flags));
		if (!p) continue;
		while (*p == ' ') p++;

		p = strchr_cpy(p, ' ', separator_str, sizeof(separator_str));
		if (!p) continue;
		extract_quote(separator_str, '"');
		if (!strcmp(separator_str, "NIL"))
			separator_str[0] = '\0';
		if (separator)
			*separator = separator_str[0];

		buf[0] = '\0';
		while (*p == ' ') p++;
		if ((*p == '~' && *(p + 1) == '{') || *p == '{' || *p == '"')
			p = imap_parse_atom(session, p, buf, sizeof(buf), str);
		else
			strncpy2(buf, p, sizeof(buf));
		strtailchomp(buf, separator_str[0]);
		if (buf[0] == '\0') continue;
		if (!strcmp(buf, real_path)) continue;

		if (separator_str[0] != '\0')
			subst_char(buf, separator_str[0], '/');
		name = g_basename(buf);
		if (name[0] == '.') continue;

		loc_name = imap_modified_utf7_to_utf8(name);
		loc_path = imap_modified_utf7_to_utf8(buf);
		new_item = folder_item_new(loc_name, loc_path);
		if (strcasestr(flags, "\\Noinferiors") != NULL)
			new_item->no_sub = TRUE;
		if (g_ascii_strcasecmp(buf, "INBOX") != 0 &&
		    strcasestr(flags, "\\Noselect") != NULL)
			new_item->no_select = TRUE;

		item_list = g_slist_prepend(item_list, new_item);

		debug_print("folder '%s' found.\n", loc_path);
		g_free(loc_path);
		g_free(loc_name);
	}

	g_string_free(str, TRUE);

	item_list = g_slist_reverse(item_list);
	return item_list;
}

static GSList *imap_add_inter_folders(GSList *item_list, const gchar *root_path)
{
	FolderItem *item;
	GSList *cur;
	GSList *add_list = NULL;
	GHashTable *exist;
	const gchar *p;
	gint root_path_len = 0;

	if (root_path)
		root_path_len = strlen(root_path);

	exist = g_hash_table_new(g_str_hash, g_str_equal);

	for (cur = item_list; cur != NULL; cur = cur->next) {
		item = FOLDER_ITEM(cur->data);

		if (root_path_len > 0 &&
		    strncmp(root_path, item->path, root_path_len) != 0)
			continue;
		p = item->path + root_path_len;
		if (root_path_len > 0 && *p != '/') continue;
		while (*p == '/') p++;
		if (*p == '\0') continue;
		g_hash_table_insert(exist, (gpointer)p, GINT_TO_POINTER(1));
	}

	for (cur = item_list; cur != NULL; cur = cur->next) {
		const gchar *q, *r;
		gchar *parent, *full_parent;
		FolderItem *new_item;

		item = FOLDER_ITEM(cur->data);

		if (root_path_len > 0 &&
		    strncmp(root_path, item->path, root_path_len) != 0)
			continue;
		p = item->path + root_path_len;
		if (root_path_len > 0 && *p != '/') continue;
		while (*p == '/') p++;
		if (*p == '\0') continue;

		q = p;
		while ((q = strchr(q, '/')) != NULL) {
			parent = g_strndup(p, q - p);
			if (!g_hash_table_lookup(exist, parent)) {
				if (root_path_len > 0)
					full_parent = g_strconcat
						(root_path, "/", parent, NULL);
				else
					full_parent = g_strdup(parent);
				new_item = folder_item_new(g_basename(parent),
							   full_parent);
				new_item->no_select = TRUE;
				add_list = g_slist_prepend(add_list, new_item);
				r = new_item->path + root_path_len;
				while (*r == '/') r++;
				g_hash_table_insert(exist, (gpointer)r,
						    GINT_TO_POINTER(1));
				debug_print("intermediate folder '%s' added\n",
					    full_parent);
				g_free(full_parent);
			}
			g_free(parent);
			while (*q == '/') q++;
		}
	}

	g_hash_table_destroy(exist);

	add_list = g_slist_reverse(add_list);
	item_list = g_slist_concat(item_list, add_list);

	return item_list;
}

static GSList *imap_get_part_folder_list(GSList *item_list, FolderItem *item)
{
	FolderItem *cur_item;
	GSList *part_list = NULL, *cur;
	gint len;

	if (!item->path) {
		debug_print("imap_get_part_folder_list(): get root folders\n");
		for (cur = item_list; cur != NULL; cur = cur->next) {
			cur_item = FOLDER_ITEM(cur->data);

			if (!strchr(cur_item->path, '/')) {
				part_list = g_slist_prepend(part_list,
							    cur_item);
				debug_print("append '%s'\n", cur_item->path);
			}
		}
		part_list = g_slist_reverse(part_list);
		return part_list;
	}

	len = strlen(item->path);
	debug_print("imap_get_part_folder_list(): get folders under '%s'\n",
		    item->path);

	for (cur = item_list; cur != NULL; cur = cur->next) {
		cur_item = FOLDER_ITEM(cur->data);

		if (!strncmp(cur_item->path, item->path, len) &&
		    cur_item->path[len] == '/' &&
		    !strchr(cur_item->path + len + 1, '/')) {
			part_list = g_slist_prepend(part_list, cur_item);
			debug_print("append '%s'\n", cur_item->path);
		}
	}

	part_list = g_slist_reverse(part_list);
	return part_list;
}

static gint imap_create_tree(Folder *folder)
{
	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(folder->node != NULL, -1);
	g_return_val_if_fail(folder->node->data != NULL, -1);
	g_return_val_if_fail(folder->account != NULL, -1);

	if (imap_scan_tree(folder) < 0)
		return -1;
	imap_create_missing_folders(folder);

	return 0;
}

static void imap_create_missing_folders(Folder *folder)
{
	g_return_if_fail(folder != NULL);

	if (!folder->inbox)
		folder->inbox = imap_create_special_folder
			(folder, F_INBOX, "INBOX");
#if 0
	if (!folder->outbox)
		folder->outbox = imap_create_special_folder
			(folder, F_OUTBOX, "Sent");
	if (!folder->draft)
		folder->draft = imap_create_special_folder
			(folder, F_DRAFT, "Drafts");
	if (!folder->queue)
		folder->queue = imap_create_special_folder
			(folder, F_QUEUE, "Queue");
#endif
	if (!folder->trash)
		folder->trash = imap_create_special_folder
			(folder, F_TRASH, "Trash");
}

static FolderItem *imap_create_special_folder(Folder *folder,
					      SpecialFolderItemType stype,
					      const gchar *name)
{
	FolderItem *item;
	FolderItem *new_item;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(folder->node != NULL, NULL);
	g_return_val_if_fail(folder->node->data != NULL, NULL);
	g_return_val_if_fail(folder->account != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	item = FOLDER_ITEM(folder->node->data);
	new_item = imap_create_folder(folder, item, name);

	if (!new_item) {
		g_warning(_("Can't create '%s'\n"), name);
		if (!folder->inbox) return NULL;

		new_item = imap_create_folder(folder, folder->inbox, name);
		if (!new_item)
			g_warning(_("Can't create '%s' under INBOX\n"), name);
		else
			new_item->stype = stype;
	} else
		new_item->stype = stype;

	return new_item;
}

static FolderItem *imap_create_folder(Folder *folder, FolderItem *parent,
				      const gchar *name)
{
	gchar *dirpath, *imap_path;
	IMAPSession *session;
	FolderItem *new_item;
	gchar separator;
	gchar *new_name;
	const gchar *p;
	gboolean no_sub = FALSE, no_select = FALSE;
	gint ok;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(folder->account != NULL, NULL);
	g_return_val_if_fail(parent != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	session = imap_session_get(folder);
	if (!session) return NULL;

	if (!parent->parent && g_ascii_strcasecmp(name, "INBOX") == 0)
		dirpath = g_strdup(name);
	else if (parent->path)
		dirpath = g_strconcat(parent->path, "/", name, NULL);
	else if ((p = strchr(name, '/')) != NULL && *(p + 1) != '\0')
		dirpath = g_strdup(name);
	else if (folder->account->imap_dir && *folder->account->imap_dir) {
		gchar *imap_dir;

		Xstrdup_a(imap_dir, folder->account->imap_dir, return NULL);
		strtailchomp(imap_dir, '/');
		dirpath = g_strconcat(imap_dir, "/", name, NULL);
	} else
		dirpath = g_strdup(name);

	/* keep trailing directory separator to create a folder that contains
	   sub folder */
	imap_path = imap_utf8_to_modified_utf7(dirpath);
	strtailchomp(dirpath, '/');
	Xstrdup_a(new_name, name, {g_free(dirpath); return NULL;});
	strtailchomp(new_name, '/');
	separator = imap_get_path_separator(IMAP_FOLDER(folder), imap_path);
	imap_path_separator_subst(imap_path, separator);
	subst_char(new_name, '/', separator);

	if (g_ascii_strcasecmp(name, "INBOX") != 0) {
		GPtrArray *argbuf;
		gint i;
		gboolean exist = FALSE;

		argbuf = g_ptr_array_new();
		ok = imap_cmd_list(session, NULL, imap_path, argbuf);
		if (ok != IMAP_SUCCESS) {
			log_warning(_("can't create mailbox: LIST failed\n"));
			g_free(imap_path);
			g_free(dirpath);
			g_ptr_array_free(argbuf, TRUE);
			return NULL;
		}

		for (i = 0; i < argbuf->len; i++) {
			gchar *str;
			str = g_ptr_array_index(argbuf, i);
			if (!strncmp(str, "LIST ", 5)) {
				exist = TRUE;
				if (strcasestr(str + 5, "\\Noinferiors"))
					no_sub = TRUE;
				if (strcasestr(str + 5, "\\Noselect"))
					no_select = TRUE;
				break;
			}
		}

		g_ptr_array_free(argbuf, TRUE);
		argbuf = NULL;

		if (!exist) {
			ok = imap_cmd_create(session, imap_path);
			if (ok != IMAP_SUCCESS) {
				log_warning(_("can't create mailbox\n"));
				g_free(imap_path);
				g_free(dirpath);
				return NULL;
			}

			ok = imap_cmd_subscribe(session, imap_path);
			if (ok != IMAP_SUCCESS) {
				log_warning(_("can't subscribe mailbox\n"));
			}

			argbuf = g_ptr_array_new();
			ok = imap_cmd_list(session, NULL, imap_path, argbuf);
			if (ok != IMAP_SUCCESS) {
				log_warning("LIST failed\n");
				g_free(imap_path);
				g_free(dirpath);
				g_ptr_array_free(argbuf, TRUE);
				return NULL;
			}

			for (i = 0; i < argbuf->len; i++) {
				gchar *str;
				str = g_ptr_array_index(argbuf, i);
				if (!strncmp(str, "LIST ", 5)) {
					if (strcasestr(str + 5, "\\Noinferiors"))
						no_sub = TRUE;
					if (strcasestr(str + 5, "\\Noselect"))
						no_select = TRUE;
					break;
				}
			}
			g_ptr_array_free(argbuf, TRUE);
		}

	}

	new_item = folder_item_new(new_name, dirpath);
	new_item->no_sub = no_sub;
	new_item->no_select = no_select;
	folder_item_append(parent, new_item);
	g_free(imap_path);
	g_free(dirpath);

	dirpath = folder_item_get_path(new_item);
	if (!is_dir_exist(dirpath))
		make_dir_hier(dirpath);
	g_free(dirpath);

	return new_item;
}

static gint imap_rename_folder_real(Folder *folder, FolderItem *item,
				    FolderItem *new_parent, const gchar *name)
{
	gchar *newpath;
	gchar *real_oldpath;
	gchar *real_newpath;
	gchar *paths[2];
	gchar *old_cache_dir;
	gchar *new_cache_dir;
	IMAPSession *session;
	gchar separator;
	gint ok;
	gint exists, recent, unseen;
	guint32 uid_validity;
	gchar *old_id, *new_id;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(folder == item->folder, -1);
	g_return_val_if_fail(item->path != NULL, -1);
	g_return_val_if_fail(new_parent != NULL || name != NULL, -1);
	if (new_parent) {
		g_return_val_if_fail(item != new_parent, -1);
		g_return_val_if_fail(item->parent != new_parent, -1);
		g_return_val_if_fail(item->folder == new_parent->folder, -1);
		if (g_node_is_ancestor(item->node, new_parent->node)) {
			g_warning("folder to be moved is ancestor of new parent\n");
			return -1;
		}
	}

	session = imap_session_get(folder);
	if (!session) return -1;

	real_oldpath = imap_get_real_path(IMAP_FOLDER(folder), item->path);

	g_free(session->mbox);
	session->mbox = NULL;
	ok = imap_cmd_examine(session, "INBOX",
			      &exists, &recent, &unseen, &uid_validity);
	if (ok != IMAP_SUCCESS) {
		g_free(real_oldpath);
		return -1;
	}

	separator = imap_get_path_separator(IMAP_FOLDER(folder), item->path);
	if (new_parent) {
		if (name) {
			if (new_parent->path)
				newpath = g_strconcat(new_parent->path,
						      "/", name, NULL);
			else
				newpath = g_strdup(name);
		} else {
			gchar *name_;

			name_ = g_path_get_basename(item->path);
			if (new_parent->path)
				newpath = g_strconcat(new_parent->path,
						      "/", name_, NULL);
			else
				newpath = g_strdup(name_);
			AUTORELEASE_STR(name_, );
			name = name_;
		}
	} else {
		if (strchr(item->path, '/')) {
			gchar *dirpath;

			dirpath = g_dirname(item->path);
			newpath = g_strconcat(dirpath, "/", name, NULL);
			g_free(dirpath);
		} else
			newpath = g_strdup(name);
	}

	real_newpath = imap_utf8_to_modified_utf7(newpath);
	imap_path_separator_subst(real_newpath, separator);

	ok = imap_cmd_rename(session, real_oldpath, real_newpath);
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't rename mailbox: %s to %s\n"),
			    real_oldpath, real_newpath);
		g_free(real_oldpath);
		g_free(newpath);
		g_free(real_newpath);
		return -1;
	}

	old_id = folder_item_get_identifier(item);

	if (new_parent) {
		g_node_unlink(item->node);
		g_node_append(new_parent->node, item->node);
		item->parent = new_parent;
	}

	g_free(item->name);
	item->name = g_strdup(name);

	old_cache_dir = folder_item_get_path(item);

	paths[0] = g_strdup(item->path);
	paths[1] = newpath;
	g_node_traverse(item->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			imap_rename_folder_func, paths);

	if (is_dir_exist(old_cache_dir)) {
		new_cache_dir = folder_item_get_path(item);
		if (g_rename(old_cache_dir, new_cache_dir) < 0) {
			FILE_OP_ERROR(old_cache_dir, "rename");
		}
		g_free(new_cache_dir);
	}

	g_free(old_cache_dir);
	g_free(paths[0]);
	g_free(newpath);
	g_free(real_oldpath);
	g_free(real_newpath);

	new_id = folder_item_get_identifier(item);
	if (syl_app_get())
		g_signal_emit_by_name(syl_app_get(), "move-folder", item,
				      old_id, new_id);
	g_free(new_id);
	g_free(old_id);

	return 0;
}

static gint imap_rename_folder(Folder *folder, FolderItem *item,
			       const gchar *name)
{
	return imap_rename_folder_real(folder, item, NULL, name);
}

static gint imap_move_folder(Folder *folder, FolderItem *item,
			     FolderItem *new_parent)
{
	return imap_rename_folder_real(folder, item, new_parent, NULL);
}

static gint imap_remove_folder(Folder *folder, FolderItem *item)
{
	gint ok;
	IMAPSession *session;
	gchar *path;
	gchar *cache_dir;
	gint exists, recent, unseen;
	guint32 uid_validity;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->path != NULL, -1);

	session = imap_session_get(folder);
	if (!session) return -1;

	path = imap_get_real_path(IMAP_FOLDER(folder), item->path);

	ok = imap_cmd_examine(session, "INBOX",
			      &exists, &recent, &unseen, &uid_validity);
	if (ok != IMAP_SUCCESS) {
		g_free(path);
		return -1;
	}

	ok = imap_cmd_delete(session, path);
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't delete mailbox\n"));
		g_free(path);
		return -1;
	}

	g_free(path);
	cache_dir = folder_item_get_path(item);
	if (is_dir_exist(cache_dir) && remove_dir_recursive(cache_dir) < 0)
		g_warning("can't remove directory '%s'\n", cache_dir);
	g_free(cache_dir);

	if (syl_app_get())
		g_signal_emit_by_name(syl_app_get(), "remove-folder", item);
	folder_item_remove(item);

	return 0;
}

typedef struct _IMAPGetData
{
	FolderItem *item;
	gint exists;
	gboolean update_count;
	GSList *newlist;
} IMAPGetData;

static gint imap_get_uncached_messages_progress_func(IMAPSession *session,
						     gint count, gint total,
						     gpointer data)
{
	status_print(_("Getting message headers (%d / %d)"), count, total);
	progress_show(count, total);
#ifndef USE_THREADS
	ui_update();
#endif
	return 0;
}

static gint imap_get_uncached_messages_func(IMAPSession *session, gpointer data)
{
	gchar *tmp;
	GSList *newlist = NULL;
	GSList *llast = NULL;
	GString *str;
	MsgInfo *msginfo;
	gint count = 1;
	GTimeVal tv_prev, tv_cur;
	IMAPGetData *get_data = (IMAPGetData *)data;
	FolderItem *item = get_data->item;
	gint exists = get_data->exists;
	gboolean update_count = get_data->update_count;

	g_get_current_time(&tv_prev);
#ifndef USE_THREADS
	ui_update();
#endif

#if USE_THREADS
	((IMAPRealSession *)session)->prog_total = exists;
#endif

	str = g_string_new(NULL);

	for (;;) {
		if (exists > 0 && count <= exists) {
			g_get_current_time(&tv_cur);
			if (tv_cur.tv_sec > tv_prev.tv_sec ||
			    tv_cur.tv_usec - tv_prev.tv_usec >
			    PROGRESS_UPDATE_INTERVAL * 1000) {
#if USE_THREADS
				((IMAPRealSession *)session)->prog_count = count;
				g_main_context_wakeup(NULL);
#else
				imap_get_uncached_messages_progress_func
					(session, count, exists, data);
#endif
				tv_prev = tv_cur;
			}
		}
		++count;

		if (sock_getline(SESSION(session)->sock, &tmp) < 0) {
			log_warning(_("error occurred while getting envelope.\n"));
			g_string_free(str, TRUE);
			return IMAP_SOCKET;
		}
		strretchomp(tmp);
		if (tmp[0] != '*' || tmp[1] != ' ') {
			log_print("IMAP4< %s\n", tmp);
			g_free(tmp);
			break;
		}
		if (strstr(tmp, "FETCH") == NULL) {
			log_print("IMAP4< %s\n", tmp);
			g_free(tmp);
			continue;
		}
		log_print("IMAP4< %s\n", tmp);
		g_string_assign(str, tmp);
		g_free(tmp);

		msginfo = imap_parse_envelope(session, item, str);
		if (!msginfo) {
			log_warning(_("can't parse envelope: %s\n"), str->str);
			continue;
		}
		if (update_count) {
			if (MSG_IS_NEW(msginfo->flags))
				item->new++;
			if (MSG_IS_UNREAD(msginfo->flags))
				item->unread++;
		}
		if (item->stype == F_QUEUE) {
			MSG_SET_TMP_FLAGS(msginfo->flags, MSG_QUEUED);
		} else if (item->stype == F_DRAFT) {
			MSG_SET_TMP_FLAGS(msginfo->flags, MSG_DRAFT);
		}

		msginfo->folder = item;

		if (!newlist)
			llast = newlist = g_slist_append(newlist, msginfo);
		else {
			llast = g_slist_append(llast, msginfo);
			llast = llast->next;
		}

		if (update_count)
			item->total++;
	}

	g_string_free(str, TRUE);

	session_set_access_time(SESSION(session));

	get_data->newlist = newlist;
	return IMAP_SUCCESS;
}

static GSList *imap_get_uncached_messages(IMAPSession *session,
					  FolderItem *item,
					  guint32 first_uid, guint32 last_uid,
					  gint exists, gboolean update_count)
{
	IMAPGetData get_data = {item, exists, update_count, NULL};
	gchar seq_set[22];
	gint ok;

	g_return_val_if_fail(session != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(item->folder != NULL, NULL);
	g_return_val_if_fail(FOLDER_TYPE(item->folder) == F_IMAP, NULL);
	g_return_val_if_fail(first_uid <= last_uid, NULL);

	if (first_uid == 0 && last_uid == 0)
		strcpy(seq_set, "1:*");
	else
		g_snprintf(seq_set, sizeof(seq_set), "%u:%u",
			   first_uid, last_uid);
	if (imap_cmd_envelope(session, seq_set) != IMAP_SUCCESS) {
		log_warning(_("can't get envelope\n"));
		return NULL;
	}

#if USE_THREADS
	ok = imap_thread_run_progress(session, imap_get_uncached_messages_func,
				      imap_get_uncached_messages_progress_func,
				      &get_data);
#else
	ok = imap_get_uncached_messages_func(session, &get_data);
#endif

	progress_show(0, 0);
	return get_data.newlist;
}

static void imap_delete_cached_message(FolderItem *item, guint32 uid)
{
	gchar *dir;
	gchar *file;

	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(FOLDER_TYPE(item->folder) == F_IMAP);

	dir = folder_item_get_path(item);
	file = g_strdup_printf("%s%c%u", dir, G_DIR_SEPARATOR, uid);

	debug_print("Deleting cached message: %s\n", file);

	g_unlink(file);

	g_free(file);
	g_free(dir);
}

static GSList *imap_delete_cached_messages(GSList *mlist, FolderItem *item,
					   guint32 first_uid, guint32 last_uid)
{
	GSList *cur, *next;
	MsgInfo *msginfo;
	gchar *dir;

	g_return_val_if_fail(item != NULL, mlist);
	g_return_val_if_fail(item->folder != NULL, mlist);
	g_return_val_if_fail(FOLDER_TYPE(item->folder) == F_IMAP, mlist);

	if (first_uid == 0 && last_uid == 0)
		return mlist;

	debug_print("Deleting cached messages %u - %u ... ",
		    first_uid, last_uid);

	dir = folder_item_get_path(item);
	if (is_dir_exist(dir))
		remove_numbered_files(dir, first_uid, last_uid);
	g_free(dir);

	for (cur = mlist; cur != NULL; ) {
		next = cur->next;

		msginfo = (MsgInfo *)cur->data;
		if (msginfo != NULL && first_uid <= msginfo->msgnum &&
		    msginfo->msgnum <= last_uid) {
			procmsg_msginfo_free(msginfo);
			mlist = g_slist_remove(mlist, msginfo);
		}

		cur = next;
	}

	debug_print("done.\n");

	return mlist;
}

static void imap_delete_all_cached_messages(FolderItem *item)
{
	gchar *dir;

	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(FOLDER_TYPE(item->folder) == F_IMAP);

	debug_print("Deleting all cached messages... ");

	dir = folder_item_get_path(item);
	if (is_dir_exist(dir))
		remove_all_numbered_files(dir);
	g_free(dir);

	debug_print("done.\n");
}

#if USE_SSL
static SockInfo *imap_open(const gchar *server, gushort port,
			   SocksInfo *socks_info, SSLType ssl_type)
#else
static SockInfo *imap_open(const gchar *server, gushort port,
			   SocksInfo *socks_info)
#endif
{
	SockInfo *sock = NULL;
	const gchar *server_;
	gushort port_;
#if USE_THREADS
	gint conn_id;
#endif

	if (socks_info) {
		server_ = socks_info->proxy_host;
		port_ = socks_info->proxy_port;
	} else {
		server_ = server;
		port_ = port;
	}

#if USE_THREADS
	if ((conn_id = sock_connect_async_thread(server_, port_)) < 0 ||
	    sock_connect_async_thread_wait(conn_id, &sock) < 0) {
		log_warning(_("Can't connect to IMAP4 server: %s:%d\n"),
			    server, port);
		return NULL;
	}
#else
	if ((sock = sock_connect(server_, port_)) == NULL) {
		log_warning(_("Can't connect to IMAP4 server: %s:%d\n"),
			    server, port);
		return NULL;
	}
#endif

	if (socks_info) {
		if (socks_connect(sock, server, port, socks_info) < 0) {
			log_warning("Can't establish SOCKS connection: %s:%d\n",
				    server, port);
			sock_close(sock);
			return NULL;
		}
	}

#if USE_SSL
	if (ssl_type == SSL_TUNNEL && !ssl_init_socket(sock)) {
		log_warning(_("Can't establish IMAP4 session with: %s:%d\n"),
			    server, port);
		sock_close(sock);
		return NULL;
	}
#endif

	return sock;
}

static GList *imap_parse_namespace_str(gchar *str)
{
	gchar *p = str;
	gchar *name;
	gchar *separator;
	IMAPNameSpace *namespace;
	GList *ns_list = NULL;

	while (*p != '\0') {
		/* parse ("#foo" "/") */

		while (*p && *p != '(') p++;
		if (*p == '\0') break;
		p++;

		while (*p && *p != '"') p++;
		if (*p == '\0') break;
		p++;
		name = p;

		while (*p && *p != '"') p++;
		if (*p == '\0') break;
		*p = '\0';
		p++;

		while (*p && g_ascii_isspace(*p)) p++;
		if (*p == '\0') break;
		if (strncmp(p, "NIL", 3) == 0)
			separator = NULL;
		else if (*p == '"') {
			p++;
			separator = p;
			while (*p && *p != '"') p++;
			if (*p == '\0') break;
			*p = '\0';
			p++;
		} else break;

		while (*p && *p != ')') p++;
		if (*p == '\0') break;
		p++;

		namespace = g_new(IMAPNameSpace, 1);
		namespace->name = g_strdup(name);
		namespace->separator = separator ? separator[0] : '\0';
		ns_list = g_list_append(ns_list, namespace);
	}

	return ns_list;
}

static void imap_parse_namespace(IMAPSession *session, IMAPFolder *folder)
{
	gchar *ns_str = NULL;
	gchar **str_array;

	g_return_if_fail(session != NULL);
	g_return_if_fail(folder != NULL);

	if (folder->ns_personal != NULL ||
	    folder->ns_others   != NULL ||
	    folder->ns_shared   != NULL)
		return;

	if (imap_cmd_namespace(session, &ns_str) != IMAP_SUCCESS) {
		log_warning(_("can't get namespace\n"));
		imap_get_namespace_by_list(session, folder);
		return;
	}

	str_array = strsplit_parenthesis(ns_str, '(', ')', 3);
	if (str_array[0])
		folder->ns_personal = imap_parse_namespace_str(str_array[0]);
	if (str_array[0] && str_array[1])
		folder->ns_others = imap_parse_namespace_str(str_array[1]);
	if (str_array[0] && str_array[1] && str_array[2])
		folder->ns_shared = imap_parse_namespace_str(str_array[2]);
	g_strfreev(str_array);
	g_free(ns_str);
}

static void imap_get_namespace_by_list(IMAPSession *session, IMAPFolder *folder)
{
	GSList *item_list, *cur;
	gchar separator = '\0';
	IMAPNameSpace *namespace;

	g_return_if_fail(session != NULL);
	g_return_if_fail(folder != NULL);

	if (folder->ns_personal != NULL ||
	    folder->ns_others   != NULL ||
	    folder->ns_shared   != NULL)
		return;

	if (imap_cmd_gen_send(session, "LIST \"\" \"\"") != IMAP_SUCCESS)
		return;
	item_list = imap_parse_list(session, "", &separator);
	for (cur = item_list; cur != NULL; cur = cur->next)
		folder_item_destroy(FOLDER_ITEM(cur->data));
	g_slist_free(item_list);

	namespace = g_new(IMAPNameSpace, 1);
	namespace->name = g_strdup("");
	namespace->separator = separator;
	folder->ns_personal = g_list_append(NULL, namespace);
}

static IMAPNameSpace *imap_find_namespace_from_list(GList *ns_list,
						    const gchar *path)
{
	IMAPNameSpace *namespace = NULL;
	gchar *tmp_path, *name;

	if (!path) path = "";

	for (; ns_list != NULL; ns_list = ns_list->next) {
		IMAPNameSpace *tmp_ns = ns_list->data;

		Xstrcat_a(tmp_path, path, "/", return namespace);
		Xstrdup_a(name, tmp_ns->name, return namespace);
		if (tmp_ns->separator && tmp_ns->separator != '/') {
			subst_char(tmp_path, tmp_ns->separator, '/');
			subst_char(name, tmp_ns->separator, '/');
		}
		if (strncmp(tmp_path, name, strlen(name)) == 0)
			namespace = tmp_ns;
	}

	return namespace;
}

static IMAPNameSpace *imap_find_namespace(IMAPFolder *folder,
					  const gchar *path)
{
	IMAPNameSpace *namespace;

	g_return_val_if_fail(folder != NULL, NULL);

	namespace = imap_find_namespace_from_list(folder->ns_personal, path);
	if (namespace) return namespace;
	namespace = imap_find_namespace_from_list(folder->ns_others, path);
	if (namespace) return namespace;
	namespace = imap_find_namespace_from_list(folder->ns_shared, path);
	if (namespace) return namespace;

	return NULL;
}

static gchar imap_get_path_separator(IMAPFolder *folder, const gchar *path)
{
	IMAPNameSpace *namespace;
	gchar separator = '/';

	namespace = imap_find_namespace(folder, path);
	if (namespace && namespace->separator)
		separator = namespace->separator;

	return separator;
}

static gchar *imap_get_real_path(IMAPFolder *folder, const gchar *path)
{
	gchar *real_path;
	gchar separator;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(path != NULL, NULL);

	real_path = imap_utf8_to_modified_utf7(path);
	separator = imap_get_path_separator(folder, path);
	imap_path_separator_subst(real_path, separator);

	return real_path;
}

static gchar *imap_parse_atom(IMAPSession *session, gchar *src,
			      gchar *dest, gint dest_len, GString *str)
{
	gchar *cur_pos = src;
	gchar *nextline;

	g_return_val_if_fail(str != NULL, cur_pos);

	/* read the next line if the current response buffer is empty */
	while (g_ascii_isspace(*cur_pos)) cur_pos++;
	while (*cur_pos == '\0') {
		if (sock_getline(SESSION(session)->sock, &nextline) < 0)
			return cur_pos;
		g_string_assign(str, nextline);
		cur_pos = str->str;
		strretchomp(nextline);
		/* log_print("IMAP4< %s\n", nextline); */
		debug_print("IMAP4< %s\n", nextline);
		g_free(nextline);

		while (g_ascii_isspace(*cur_pos)) cur_pos++;
	}

	if (*cur_pos == '~' && *(cur_pos + 1) == '{')
		cur_pos++;

	if (!strncmp(cur_pos, "NIL", 3)) {
		*dest = '\0';
		cur_pos += 3;
	} else if (*cur_pos == '\"') {
		gchar *p;

		p = get_quoted(cur_pos, '\"', dest, dest_len);
		cur_pos = p ? p : cur_pos + 2;
	} else if (*cur_pos == '{') {
		gchar buf[32];
		gint len;
		gint block_len = 0;

		cur_pos = strchr_cpy(cur_pos + 1, '}', buf, sizeof(buf));
		len = atoi(buf);
		g_return_val_if_fail(len >= 0, cur_pos);

		g_string_truncate(str, 0);
		cur_pos = str->str;

		do {
			gint cur_len;

			cur_len = sock_getline(SESSION(session)->sock,
					       &nextline);
			if (cur_len < 0)
				return cur_pos;
			block_len += cur_len;
			subst_null(nextline, cur_len, ' ');
			g_string_append(str, nextline);
			cur_pos = str->str;
			strretchomp(nextline);
			/* log_print("IMAP4< %s\n", nextline); */
			debug_print("IMAP4< %s\n", nextline);
			g_free(nextline);
		} while (block_len < len);

		memcpy(dest, cur_pos, MIN(len, dest_len - 1));
		dest[MIN(len, dest_len - 1)] = '\0';
		cur_pos += len;
	}

	return cur_pos;
}

static gchar *imap_get_header(IMAPSession *session, gchar *cur_pos,
			      gchar **headers, GString *str)
{
	gchar *nextline;
	gchar buf[IMAPBUFSIZE];
	gint len;
	gint block_len = 0;

	*headers = NULL;

	g_return_val_if_fail(str != NULL, cur_pos);

	while (g_ascii_isspace(*cur_pos)) cur_pos++;
	if (*cur_pos == '~' && *(cur_pos + 1) == '{')
		cur_pos++;

	if (*cur_pos == '"') {
		cur_pos = strchr_cpy(cur_pos + 1, '"', buf, sizeof(buf));
		if (!cur_pos)
			return NULL;
		len = strlen(buf);
		*headers = g_strdup(buf);
		while (g_ascii_isspace(*cur_pos)) cur_pos++;
		return cur_pos;
	}

	g_return_val_if_fail(*cur_pos == '{', cur_pos);

	cur_pos = strchr_cpy(cur_pos + 1, '}', buf, sizeof(buf));
	len = atoi(buf);
	g_return_val_if_fail(len >= 0, cur_pos);

	g_string_truncate(str, 0);
	cur_pos = str->str;

	do {
		gint cur_len;

		cur_len = sock_getline(SESSION(session)->sock, &nextline);
		if (cur_len < 0)
			return cur_pos;
		block_len += cur_len;
		subst_null(nextline, cur_len, ' ');
		g_string_append(str, nextline);
		cur_pos = str->str;
		/* strretchomp(nextline); */
		/* debug_print("IMAP4< %s\n", nextline); */
		g_free(nextline);
	} while (block_len < len);

	debug_print("IMAP4< [contents of RFC822.HEADER]\n");

	*headers = g_strndup(cur_pos, len);
	cur_pos += len;

	while (g_ascii_isspace(*cur_pos)) cur_pos++;
	while (*cur_pos == '\0') {
		if (sock_getline(SESSION(session)->sock, &nextline) < 0)
			return cur_pos;
		g_string_assign(str, nextline);
		cur_pos = str->str;
		strretchomp(nextline);
		debug_print("IMAP4< %s\n", nextline);
		g_free(nextline);

		while (g_ascii_isspace(*cur_pos)) cur_pos++;
	}

	return cur_pos;
}

static MsgFlags imap_parse_flags(const gchar *flag_str)  
{
	const gchar *p = flag_str;
	MsgFlags flags = {0, 0};

	flags.perm_flags = MSG_UNREAD;

	while (*p != '\0') {
		if (g_ascii_strncasecmp(p, "\\Recent", 7) == 0 &&
		    MSG_IS_UNREAD(flags)) {
			MSG_SET_PERM_FLAGS(flags, MSG_NEW);
		} else if (g_ascii_strncasecmp(p, "\\Seen", 5) == 0) {
			MSG_UNSET_PERM_FLAGS(flags, MSG_NEW|MSG_UNREAD);
		} else if (g_ascii_strncasecmp(p, "\\Deleted", 8) == 0) {
			MSG_SET_PERM_FLAGS(flags, MSG_DELETED);
		} else if (g_ascii_strncasecmp(p, "\\Flagged", 8) == 0) {
			MSG_SET_PERM_FLAGS(flags, MSG_MARKED);
		} else if (g_ascii_strncasecmp(p, "\\Answered", 9) == 0) {
			MSG_SET_PERM_FLAGS(flags, MSG_REPLIED);
		} else if (g_ascii_strncasecmp(p, "$label", 6) == 0) {
			/* color labels */
			if (*(p + 6) >= '1' && *(p + 6) <= '7') {
				guint color = *(p + 6) - '1' + 1;
				MSG_UNSET_PERM_FLAGS(flags,
						     MSG_CLABEL_FLAG_MASK);
				MSG_SET_COLORLABEL_VALUE(flags, color);
			}
		}

		while (*p && !g_ascii_isspace(*p)) p++;
		while (g_ascii_isspace(*p)) p++;
	}

	return flags;
}

static IMAPFlags imap_parse_imap_flags(const gchar *flag_str)  
{
	const gchar *p = flag_str;
	IMAPFlags flags = 0;

	while (*p != '\0') {
		if (g_ascii_strncasecmp(p, "\\Seen", 5) == 0) {
			flags |= IMAP_FLAG_SEEN;
		} else if (g_ascii_strncasecmp(p, "\\Deleted", 8) == 0) {
			flags |= IMAP_FLAG_DELETED;
		} else if (g_ascii_strncasecmp(p, "\\Flagged", 8) == 0) {
			flags |= IMAP_FLAG_FLAGGED;
		} else if (g_ascii_strncasecmp(p, "\\Answered", 9) == 0) {
			flags |= IMAP_FLAG_ANSWERED;
		} else if (g_ascii_strncasecmp(p, "$label", 6) == 0) {
			/* color labels */
			if (*(p + 6) >= '1' && *(p + 6) <= '7') {
				guint color = *(p + 6) - '1' + 1;
				MSG_UNSET_FLAGS(flags, MSG_CLABEL_FLAG_MASK);
				IMAP_SET_COLORLABEL_VALUE(flags, color);
			}
		}

		while (*p && !g_ascii_isspace(*p)) p++;
		while (g_ascii_isspace(*p)) p++;
	}

	return flags;
}

static MsgInfo *imap_parse_envelope(IMAPSession *session, FolderItem *item,
				    GString *line_str)
{
	gchar buf[IMAPBUFSIZE];
	MsgInfo *msginfo = NULL;
	gchar *cur_pos;
	gint msgnum;
	guint32 uid = 0;
	size_t size = 0;
	MsgFlags flags = {0, 0}, imap_flags = {0, 0};

	g_return_val_if_fail(line_str != NULL, NULL);
	g_return_val_if_fail(line_str->str[0] == '*' &&
			     line_str->str[1] == ' ', NULL);

	MSG_SET_TMP_FLAGS(flags, MSG_IMAP);
	if (item->stype == F_QUEUE) {
		MSG_SET_TMP_FLAGS(flags, MSG_QUEUED);
	} else if (item->stype == F_DRAFT) {
		MSG_SET_TMP_FLAGS(flags, MSG_DRAFT);
	}

	cur_pos = line_str->str + 2;

#define PARSE_ONE_ELEMENT(ch)					\
{								\
	cur_pos = strchr_cpy(cur_pos, ch, buf, sizeof(buf));	\
	if (cur_pos == NULL) {					\
		g_warning("cur_pos == NULL\n");			\
		procmsg_msginfo_free(msginfo);			\
		return NULL;					\
	}							\
}

	PARSE_ONE_ELEMENT(' ');
	msgnum = atoi(buf);

	PARSE_ONE_ELEMENT(' ');
	g_return_val_if_fail(!strcmp(buf, "FETCH"), NULL);

	g_return_val_if_fail(*cur_pos == '(', NULL);
	cur_pos++;

	while (*cur_pos != '\0' && *cur_pos != ')') {
		while (*cur_pos == ' ') cur_pos++;

		if (!strncmp(cur_pos, "UID ", 4)) {
			cur_pos += 4;
			uid = strtoul(cur_pos, &cur_pos, 10);
		} else if (!strncmp(cur_pos, "FLAGS ", 6)) {
			cur_pos += 6;
			if (*cur_pos != '(') {
				g_warning("FLAGS: *cur_pos != '('\n");
				procmsg_msginfo_free(msginfo);
				return NULL;
			}
			cur_pos++;
			PARSE_ONE_ELEMENT(')');
			imap_flags = imap_parse_flags(buf);
		} else if (!strncmp(cur_pos, "RFC822.SIZE ", 12)) {
			cur_pos += 12;
			size = strtol(cur_pos, &cur_pos, 10);
		} else if (!strncmp(cur_pos, "RFC822.HEADER", 13)) {
			gchar *headers;

			cur_pos += 13;
			cur_pos = imap_get_header(session, cur_pos, &headers,
						  line_str);
			if (cur_pos == NULL) {
				g_warning("RFC822.HEADER: cur_pos == NULL\n");
				procmsg_msginfo_free(msginfo);
				return NULL;
			}
			if (!msginfo)
				msginfo = procheader_parse_str(headers, flags, FALSE);
			g_free(headers);
		} else {
			g_warning("invalid FETCH response: %s\n", cur_pos);
			break;
		}
	}

#undef PARSE_ONE_ELEMENT

	if (msginfo) {
		msginfo->msgnum = uid;
		msginfo->size = size;
		msginfo->flags.tmp_flags |= imap_flags.tmp_flags;
		msginfo->flags.perm_flags = imap_flags.perm_flags;
	}

	return msginfo;
}

static gint imap_msg_list_change_perm_flags(GSList *msglist, MsgPermFlags flags,
					    gboolean is_set)
{
	Folder *folder;
	IMAPSession *session;
	IMAPFlags iflags = 0;
	MsgInfo *msginfo;
	GSList *seq_list, *cur;
	gint ok = IMAP_SUCCESS;

	if (msglist == NULL) return IMAP_SUCCESS;

	msginfo = (MsgInfo *)msglist->data;
	g_return_val_if_fail(msginfo != NULL, -1);

	g_return_val_if_fail(MSG_IS_IMAP(msginfo->flags), -1);
	g_return_val_if_fail(msginfo->folder != NULL, -1);
	g_return_val_if_fail(msginfo->folder->folder != NULL, -1);

	folder = msginfo->folder->folder;
	g_return_val_if_fail(FOLDER_TYPE(folder) == F_IMAP, -1);

	session = imap_session_get(folder);
	if (!session) return -1;

	ok = imap_select(session, IMAP_FOLDER(folder), msginfo->folder->path,
			 NULL, NULL, NULL, NULL);
	if (ok != IMAP_SUCCESS)
		return ok;

	seq_list = imap_get_seq_set_from_msglist(msglist, 0);

	if (flags & MSG_MARKED)  iflags |= IMAP_FLAG_FLAGGED;
	if (flags & MSG_REPLIED) iflags |= IMAP_FLAG_ANSWERED;

	for (cur = seq_list; cur != NULL; cur = cur->next) {
		gchar *seq_set = (gchar *)cur->data;

		if (iflags) {
			ok = imap_set_message_flags(session, seq_set, iflags,
						    is_set);
			if (ok != IMAP_SUCCESS) break;
		}

		if (flags & MSG_UNREAD) {
			ok = imap_set_message_flags(session, seq_set,
						    IMAP_FLAG_SEEN, !is_set);
			if (ok != IMAP_SUCCESS) break;
		}
	}

	imap_seq_set_free(seq_list);

	return ok;
}

gint imap_msg_set_perm_flags(MsgInfo *msginfo, MsgPermFlags flags)
{
	GSList msglist;

	msglist.data = msginfo;
	msglist.next = NULL;

	return imap_msg_list_change_perm_flags(&msglist, flags, TRUE);
}

gint imap_msg_unset_perm_flags(MsgInfo *msginfo, MsgPermFlags flags)
{
	GSList msglist;

	msglist.data = msginfo;
	msglist.next = NULL;

	return imap_msg_list_change_perm_flags(&msglist, flags, FALSE);
}

gint imap_msg_list_set_perm_flags(GSList *msglist, MsgPermFlags flags)
{
	return imap_msg_list_change_perm_flags(msglist, flags, TRUE);
}

gint imap_msg_list_unset_perm_flags(GSList *msglist, MsgPermFlags flags)
{
	return imap_msg_list_change_perm_flags(msglist, flags, FALSE);
}

gint imap_msg_list_set_colorlabel_flags(GSList *msglist, guint color)
{
	Folder *folder;
	IMAPSession *session;
	IMAPFlags iflags = 0;
	MsgInfo *msginfo;
	GSList *seq_list, *cur;
	gint ok = IMAP_SUCCESS;

	if (msglist == NULL) return IMAP_SUCCESS;

	msginfo = (MsgInfo *)msglist->data;
	g_return_val_if_fail(msginfo != NULL, -1);

	g_return_val_if_fail(MSG_IS_IMAP(msginfo->flags), -1);
	g_return_val_if_fail(msginfo->folder != NULL, -1);
	g_return_val_if_fail(msginfo->folder->folder != NULL, -1);

	folder = msginfo->folder->folder;
	g_return_val_if_fail(FOLDER_TYPE(folder) == F_IMAP, -1);

	session = imap_session_get(folder);
	if (!session) return -1;

	ok = imap_select(session, IMAP_FOLDER(folder), msginfo->folder->path,
			 NULL, NULL, NULL, NULL);
	if (ok != IMAP_SUCCESS)
		return ok;

	seq_list = imap_get_seq_set_from_msglist(msglist, 0);

	IMAP_SET_COLORLABEL_VALUE(iflags, color);

	for (cur = seq_list; cur != NULL; cur = cur->next) {
		gchar *seq_set = (gchar *)cur->data;

		ok = imap_cmd_store(session, seq_set,
				    "-FLAGS.SILENT ($label1 $label2 $label3 $label4 $label5 $label6 $label7)");
		if (ok != IMAP_SUCCESS) break;

		if (iflags) {
			ok = imap_set_message_flags(session, seq_set, iflags,
						    TRUE);
			if (ok != IMAP_SUCCESS) break;
		}
	}

	imap_seq_set_free(seq_list);

	return ok;
}

static gchar *imap_get_flag_str(IMAPFlags flags)
{
	GString *str;
	gchar *ret;
	guint color;

	str = g_string_new(NULL);

	if (IMAP_IS_SEEN(flags))	g_string_append(str, "\\Seen ");
	if (IMAP_IS_ANSWERED(flags))	g_string_append(str, "\\Answered ");
	if (IMAP_IS_FLAGGED(flags))	g_string_append(str, "\\Flagged ");
	if (IMAP_IS_DELETED(flags))	g_string_append(str, "\\Deleted ");
	if (IMAP_IS_DRAFT(flags))	g_string_append(str, "\\Draft ");

	if ((color = IMAP_GET_COLORLABEL_VALUE(flags)) != 0) {
		g_string_append_printf(str, "$label%u", color);
	}

	if (str->len > 0 && str->str[str->len - 1] == ' ')
		g_string_truncate(str, str->len - 1);

	ret = str->str;
	g_string_free(str, FALSE);

	return ret;
}

static gint imap_set_message_flags(IMAPSession *session,
				   const gchar *seq_set,
				   IMAPFlags flags,
				   gboolean is_set)
{
	gchar *cmd;
	gchar *flag_str;
	gint ok;

	flag_str = imap_get_flag_str(flags);
	cmd = g_strconcat(is_set ? "+FLAGS.SILENT (" : "-FLAGS.SILENT (",
			  flag_str, ")", NULL);
	g_free(flag_str);

	ok = imap_cmd_store(session, seq_set, cmd);
	g_free(cmd);

	return ok;
}

static gint imap_select(IMAPSession *session, IMAPFolder *folder,
			const gchar *path,
			gint *exists, gint *recent, gint *unseen,
			guint32 *uid_validity)
{
	gchar *real_path;
	gint ok;
	gint exists_, recent_, unseen_;
	guint32 uid_validity_;

	if (!exists || !recent || !unseen || !uid_validity) {
		if (session->mbox && strcmp(session->mbox, path) == 0)
			return IMAP_SUCCESS;
		exists = &exists_;
		recent = &recent_;
		unseen = &unseen_;
		uid_validity = &uid_validity_;
	}

	g_free(session->mbox);
	session->mbox = NULL;

	real_path = imap_get_real_path(folder, path);
	ok = imap_cmd_select(session, real_path,
			     exists, recent, unseen, uid_validity);
	if (ok != IMAP_SUCCESS)
		log_warning(_("can't select folder: %s\n"), real_path);
	else
		session->mbox = g_strdup(path);
	g_free(real_path);

	return ok;
}

#define THROW(err) { ok = err; goto catch; }

static gint imap_status(IMAPSession *session, IMAPFolder *folder,
			const gchar *path,
			gint *messages, gint *recent,
			guint32 *uid_next, guint32 *uid_validity,
			gint *unseen)
{
	gchar *real_path;
	gchar *real_path_;
	gint ok;
	GPtrArray *argbuf = NULL;
	gchar *str;

	if (messages && recent && uid_next && uid_validity && unseen) {
		*messages = *recent = *uid_next = *uid_validity = *unseen = 0;
		argbuf = g_ptr_array_new();
	}

	real_path = imap_get_real_path(folder, path);
	QUOTE_IF_REQUIRED(real_path_, real_path);
	ok = imap_cmd_gen_send(session, "STATUS %s "
			       "(MESSAGES RECENT UIDNEXT UIDVALIDITY UNSEEN)",
			       real_path_);
	if (ok != IMAP_SUCCESS) {
		log_warning("error on sending imap command: STATUS\n");
		THROW(ok);
	}
	ok = imap_cmd_ok(session, argbuf);
	if (ok != IMAP_SUCCESS)
		log_warning(_("error on imap command: STATUS\n"));
	if (ok != IMAP_SUCCESS || !argbuf) THROW(ok);

	str = search_array_str(argbuf, "STATUS");
	if (!str) THROW(IMAP_ERROR);

	str = strrchr_with_skip_quote(str, '"', '(');
	if (!str) THROW(IMAP_ERROR);
	str++;
	while (*str != '\0' && *str != ')') {
		while (*str == ' ') str++;

		if (!strncmp(str, "MESSAGES ", 9)) {
			str += 9;
			*messages = strtol(str, &str, 10);
		} else if (!strncmp(str, "RECENT ", 7)) {
			str += 7;
			*recent = strtol(str, &str, 10);
		} else if (!strncmp(str, "UIDNEXT ", 8)) {
			str += 8;
			*uid_next = strtoul(str, &str, 10);
		} else if (!strncmp(str, "UIDVALIDITY ", 12)) {
			str += 12;
			*uid_validity = strtoul(str, &str, 10);
		} else if (!strncmp(str, "UNSEEN ", 7)) {
			str += 7;
			*unseen = strtol(str, &str, 10);
		} else {
			g_warning("invalid STATUS response: %s\n", str);
			break;
		}
	}

catch:
	g_free(real_path);
	if (argbuf) {
		ptr_array_free_strings(argbuf);
		g_ptr_array_free(argbuf, TRUE);
	}

	return ok;
}

#undef THROW

static gboolean imap_has_capability(IMAPSession	*session,
				    const gchar *capability)
{
	gchar **p;

	for (p = session->capability; *p != NULL; ++p) {
		if (!g_ascii_strcasecmp(*p, capability))
			return TRUE;
	}

	return FALSE;
}

static void imap_capability_free(IMAPSession *session)
{
	if (session->capability) {
		g_strfreev(session->capability);
		session->capability = NULL;
	}
}


/* low-level IMAP4rev1 commands */

#define THROW(err) { ok = err; goto catch; }

static gint imap_cmd_capability(IMAPSession *session)
{
	gint ok;
	GPtrArray *argbuf;
	gchar *capability;

	argbuf = g_ptr_array_new();

	if ((ok = imap_cmd_gen_send(session, "CAPABILITY")) != IMAP_SUCCESS)
		THROW(ok);
	if ((ok = imap_cmd_ok(session, argbuf)) != IMAP_SUCCESS) THROW(ok);

	capability = search_array_str(argbuf, "CAPABILITY ");
	if (!capability) THROW(IMAP_ERROR);

	capability += strlen("CAPABILITY ");

	imap_capability_free(session);
	session->capability = g_strsplit(capability, " ", -1);

catch:
	ptr_array_free_strings(argbuf);
	g_ptr_array_free(argbuf, TRUE);

	return ok;
}

#undef THROW

static gint imap_cmd_auth_plain(IMAPSession *session, const gchar *user,
				const gchar *pass)
{
	gchar *p;
	gchar *response;
	gchar *response64;
	gint ok;

	p = response = g_malloc(strlen(user) * 2 + 2 + strlen(pass) + 1);
	strcpy(p, user);
	p += strlen(user) + 1;
	strcpy(p, user);
	p += strlen(user) + 1;
	strcpy(p, pass);
	p += strlen(pass);

	response64 = g_malloc((p - response) * 2 + 1);
	base64_encode(response64, (guchar *)response, p - response);
	g_free(response);

	log_print("IMAP4> ****************\n");
	sock_puts(SESSION(session)->sock, response64);
	ok = imap_cmd_ok(session, NULL);
	if (ok != IMAP_SUCCESS)
		log_warning(_("IMAP4 authentication failed.\n"));
	g_free(response64);

	return ok;
}

static gint imap_cmd_auth_cram_md5(IMAPSession *session, const gchar *user,
				   const gchar *pass, const gchar *challenge64)
{
	gchar *challenge;
	gint challenge_len;
	gchar hexdigest[33];
	gchar *response;
	gchar *response64;
	gint ok;

	challenge = g_malloc(strlen(challenge64 + 2) + 1);
	challenge_len = base64_decode((guchar *)challenge, challenge64 + 2, -1);
	challenge[challenge_len] = '\0';
	log_print("IMAP< [Decoded: %s]\n", challenge);

	md5_hex_hmac(hexdigest, (guchar *)challenge, challenge_len,
		     (guchar *)pass, strlen(pass));
	g_free(challenge);

	response = g_strdup_printf("%s %s", user, hexdigest);
	log_print("IMAP> [Encoded: %s]\n", response);
	response64 = g_malloc((strlen(response) + 3) * 2 + 1);
	base64_encode(response64, (guchar *)response, strlen(response));
	g_free(response);

	log_print("IMAP> %s\n", response64);
	sock_puts(SESSION(session)->sock, response64);
	ok = imap_cmd_ok(session, NULL);
	if (ok != IMAP_SUCCESS)
		log_warning(_("IMAP4 authentication failed.\n"));

	return ok;
}

static gint imap_cmd_authenticate(IMAPSession *session, const gchar *user,
				  const gchar *pass, IMAPAuthType type)
{
	gchar *auth_type;
	gint ok;
	gchar *buf = NULL;

	g_return_val_if_fail((type == 0 || type == IMAP_AUTH_CRAM_MD5 ||
			      type == IMAP_AUTH_PLAIN), IMAP_ERROR);

	if (type == IMAP_AUTH_PLAIN)
		auth_type = "PLAIN";
	else
		auth_type = "CRAM-MD5";

	ok = imap_cmd_gen_send(session, "AUTHENTICATE %s", auth_type);
	if (ok != IMAP_SUCCESS) {
		g_free(buf);
		return ok;
	}
	ok = imap_cmd_gen_recv(session, &buf);
	if (ok != IMAP_SUCCESS || buf[0] != '+') {
		g_free(buf);
		return IMAP_ERROR;
	}

	if (type == IMAP_AUTH_PLAIN)
		ok = imap_cmd_auth_plain(session, user, pass);
	else
		ok = imap_cmd_auth_cram_md5(session, user, pass, buf);

	g_free(buf);

	return ok;
}

static gint imap_cmd_login(IMAPSession *session,
			   const gchar *user, const gchar *pass)
{
	gchar *user_, *pass_;
	gint ok;

	QUOTE_IF_REQUIRED(user_, user);
	QUOTE_IF_REQUIRED(pass_, pass);
	ok = imap_cmd_gen_send(session, "LOGIN %s %s", user_, pass_);
	if (ok == IMAP_SUCCESS)
		ok = imap_cmd_ok(session, NULL);
	if (ok != IMAP_SUCCESS)
		log_warning(_("IMAP4 login failed.\n"));

	return ok;
}

static gint imap_cmd_logout(IMAPSession *session)
{
	if (imap_cmd_gen_send(session, "LOGOUT") != IMAP_SUCCESS)
		return IMAP_ERROR;
	return imap_cmd_ok(session, NULL);
}

static gint imap_cmd_noop(IMAPSession *session)
{
	gint ret;

	ret = imap_cmd_gen_send(session, "NOOP");
	if (ret != IMAP_SUCCESS)
		return ret;
	return imap_cmd_ok(session, NULL);
}

#if USE_SSL
static gint imap_cmd_starttls(IMAPSession *session)
{
	if (imap_cmd_gen_send(session, "STARTTLS") != IMAP_SUCCESS)
		return IMAP_ERROR;
	return imap_cmd_ok(session, NULL);
}
#endif

#define THROW(err) { ok = err; goto catch; }

static gint imap_cmd_namespace(IMAPSession *session, gchar **ns_str)
{
	gint ok;
	GPtrArray *argbuf;
	gchar *str;

	argbuf = g_ptr_array_new();

	if ((ok = imap_cmd_gen_send(session, "NAMESPACE")) != IMAP_SUCCESS)
		THROW(ok);
	if ((ok = imap_cmd_ok(session, argbuf)) != IMAP_SUCCESS) THROW(ok);

	str = search_array_str(argbuf, "NAMESPACE");
	if (!str) THROW(IMAP_ERROR);

	*ns_str = g_strdup(str);

catch:
	ptr_array_free_strings(argbuf);
	g_ptr_array_free(argbuf, TRUE);

	return ok;
}

#undef THROW

static gint imap_cmd_list(IMAPSession *session, const gchar *ref,
			  const gchar *mailbox, GPtrArray *argbuf)
{
	gchar *ref_, *mailbox_;

	if (!ref) ref = "";
	if (!mailbox) mailbox = "";

	QUOTE_IF_REQUIRED(ref_, ref);
	QUOTE_IF_REQUIRED(mailbox_, mailbox);
	if (imap_cmd_gen_send(session, "LIST %s %s", ref_, mailbox_) != IMAP_SUCCESS)
		return IMAP_ERROR;

	return imap_cmd_ok(session, argbuf);
}

#define THROW goto catch

static gint imap_cmd_do_select(IMAPSession *session, const gchar *folder,
			       gboolean examine,
			       gint *exists, gint *recent, gint *unseen,
			       guint32 *uid_validity)
{
	gint ok;
	gchar *resp_str;
	GPtrArray *argbuf;
	gchar *select_cmd;
	gchar *folder_;
	guint uid_validity_;

	*exists = *recent = *unseen = *uid_validity = 0;
	argbuf = g_ptr_array_new();

	if (examine)
		select_cmd = "EXAMINE";
	else
		select_cmd = "SELECT";

	QUOTE_IF_REQUIRED(folder_, folder);
	if ((ok = imap_cmd_gen_send(session, "%s %s", select_cmd, folder_)) != IMAP_SUCCESS)
		THROW;

	if ((ok = imap_cmd_ok(session, argbuf)) != IMAP_SUCCESS) THROW;

	resp_str = search_array_contain_str(argbuf, "EXISTS");
	if (resp_str) {
		if (sscanf(resp_str,"%d EXISTS", exists) != 1) {
			g_warning("imap_cmd_select(): invalid EXISTS line.\n");
			THROW;
		}
	}

	resp_str = search_array_contain_str(argbuf, "RECENT");
	if (resp_str) {
		if (sscanf(resp_str, "%d RECENT", recent) != 1) {
			g_warning("imap_cmd_select(): invalid RECENT line.\n");
			THROW;
		}
	}

	resp_str = search_array_contain_str(argbuf, "UIDVALIDITY");
	if (resp_str) {
		if (sscanf(resp_str, "OK [UIDVALIDITY %u] ", &uid_validity_)
		    != 1) {
			g_warning("imap_cmd_select(): invalid UIDVALIDITY line.\n");
			THROW;
		}
		*uid_validity = uid_validity_;
	}

	resp_str = search_array_contain_str(argbuf, "UNSEEN");
	if (resp_str) {
		if (sscanf(resp_str, "OK [UNSEEN %d] ", unseen) != 1) {
			g_warning("imap_cmd_select(): invalid UNSEEN line.\n");
			THROW;
		}
	}

catch:
	ptr_array_free_strings(argbuf);
	g_ptr_array_free(argbuf, TRUE);

	return ok;
}

static gint imap_cmd_select(IMAPSession *session, const gchar *folder,
			    gint *exists, gint *recent, gint *unseen,
			    guint32 *uid_validity)
{
	return imap_cmd_do_select(session, folder, FALSE,
				  exists, recent, unseen, uid_validity);
}

static gint imap_cmd_examine(IMAPSession *session, const gchar *folder,
			     gint *exists, gint *recent, gint *unseen,
			     guint32 *uid_validity)
{
	return imap_cmd_do_select(session, folder, TRUE,
				  exists, recent, unseen, uid_validity);
}

#undef THROW

static gint imap_cmd_create(IMAPSession *session, const gchar *folder)
{
	gchar *folder_;

	QUOTE_IF_REQUIRED(folder_, folder);
	if (imap_cmd_gen_send(session, "CREATE %s", folder_) != IMAP_SUCCESS)
		return IMAP_ERROR;

	return imap_cmd_ok(session, NULL);
}

static gint imap_cmd_rename(IMAPSession *session, const gchar *old_folder,
			    const gchar *new_folder)
{
	gchar *old_folder_, *new_folder_;

	QUOTE_IF_REQUIRED(old_folder_, old_folder);
	QUOTE_IF_REQUIRED(new_folder_, new_folder);
	if (imap_cmd_gen_send(session, "RENAME %s %s", old_folder_, new_folder_) != IMAP_SUCCESS)
		return IMAP_ERROR;

	return imap_cmd_ok(session, NULL);
}

static gint imap_cmd_delete(IMAPSession *session, const gchar *folder)
{
	gchar *folder_;

	QUOTE_IF_REQUIRED(folder_, folder);
	if (imap_cmd_gen_send(session, "DELETE %s", folder_) != IMAP_SUCCESS)
		return IMAP_ERROR;

	return imap_cmd_ok(session, NULL);
}

static gint imap_cmd_subscribe(IMAPSession *session, const gchar *folder)
{
	gchar *folder_;

	QUOTE_IF_REQUIRED(folder_, folder);
	if (imap_cmd_gen_send(session, "SUBSCRIBE %s", folder_) != IMAP_SUCCESS)
		return IMAP_ERROR;

	return imap_cmd_ok(session, NULL);
}

#define THROW(err) { ok = err; goto catch; }

static gint imap_cmd_search(IMAPSession *session, const gchar *criteria,
			    GArray **result)
{
	gint ok;
	GPtrArray *argbuf;
	GArray *array;
	gchar *str;
	gchar *p, *ep;
	gint i;
	guint32 uid;

	g_return_val_if_fail(criteria != NULL, IMAP_ERROR);
	g_return_val_if_fail(result != NULL, IMAP_ERROR);

	argbuf = g_ptr_array_new();

	if ((ok = imap_cmd_gen_send(session, "UID SEARCH %s", criteria)) != IMAP_SUCCESS)
		THROW(ok);
	if ((ok = imap_cmd_ok(session, argbuf)) != IMAP_SUCCESS) THROW(ok);

	array = g_array_new(FALSE, FALSE, sizeof(guint32));

	for (i = 0; i < argbuf->len; i++) {
		str = g_ptr_array_index(argbuf, i);
		if (strncmp(str, "SEARCH", 6) != 0)
			continue;

		p = str + 6;
		while (*p != '\0') {
			uid = strtoul(p, &ep, 10);
			if (p < ep && uid > 0) {
				g_array_append_val(array, uid);
				p = ep;
			} else
				break;
		}
	}

	*result = array;

catch:
	ptr_array_free_strings(argbuf);
	g_ptr_array_free(argbuf, TRUE);

	return ok;
}

typedef struct _IMAPCmdFetchData
{
	guint32 uid;
	const gchar *filename;
} IMAPCmdFetchData;

#define THROW(err) { ok = err; goto catch; }

static gint imap_cmd_fetch_func(IMAPSession *session, gpointer data)
{
	const gchar *filename = ((IMAPCmdFetchData *)data)->filename;
	gint ok;
	gchar *buf;
	gchar *cur_pos;
	gchar size_str[32];
	glong size_num;
	gint ret;

	while ((ok = imap_cmd_gen_recv(session, &buf)) == IMAP_SUCCESS) {
		if (buf[0] != '*' || buf[1] != ' ') {
			g_free(buf);
			return IMAP_ERROR;
		}
		if (strstr(buf, "FETCH") != NULL && strstr(buf, "BODY") != NULL)
			break;
		g_free(buf);
	}
	if (ok != IMAP_SUCCESS)
		THROW(ok);

#define RETURN_ERROR_IF_FAIL(cond)			\
	if (!(cond)) {					\
		g_free(buf);				\
		ok = imap_cmd_ok_real(session, NULL);	\
		THROW(IMAP_ERROR);			\
	}

	cur_pos = strchr(buf, '{');
	RETURN_ERROR_IF_FAIL(cur_pos != NULL);
	cur_pos = strchr_cpy(cur_pos + 1, '}', size_str, sizeof(size_str));
	RETURN_ERROR_IF_FAIL(cur_pos != NULL);
	size_num = atol(size_str);
	RETURN_ERROR_IF_FAIL(size_num >= 0);

	RETURN_ERROR_IF_FAIL(*cur_pos == '\0');

#undef RETURN_ERROR_IF_FAIL

	g_free(buf);

	if ((ret = recv_bytes_write_to_file(SESSION(session)->sock,
					    size_num, filename)) != 0) {
		if (ret == -2)
			THROW(IMAP_SOCKET);
	}

	if (imap_cmd_gen_recv(session, &buf) != IMAP_SUCCESS)
		THROW(IMAP_ERROR);

	if (buf[0] == '\0' || buf[strlen(buf) - 1] != ')') {
		g_free(buf);
		THROW(IMAP_ERROR);
	}
	g_free(buf);

	ok = imap_cmd_ok_real(session, NULL);

	if (ret != 0)
		THROW(IMAP_ERROR);

catch:
	return ok;
}

#undef THROW

static gint imap_cmd_fetch(IMAPSession *session, guint32 uid,
			   const gchar *filename)
{
	gint ok;
	IMAPCmdFetchData fetch_data = {uid, filename};

	g_return_val_if_fail(filename != NULL, IMAP_ERROR);

	ok = imap_cmd_gen_send(session, "UID FETCH %u BODY.PEEK[]", uid);
	if (ok != IMAP_SUCCESS)
		return ok;

#if USE_THREADS
	ok = imap_thread_run(session, imap_cmd_fetch_func, &fetch_data);
#else
	ok = imap_cmd_fetch_func(session, &fetch_data);
#endif

	return ok;
}

static void imap_get_date_time(gchar *buf, size_t len, stime_t timer)
{
	static gchar monthstr[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
	struct tm *lt;
	gchar date_time[64];
	gchar tz[6];
	time_t timer_ = timer;

	lt = localtime(&timer_);
	if (lt && lt->tm_mon >= 0 && lt->tm_mon < 12) {
		strftime(date_time, sizeof(date_time), "%d-___-%Y %H:%M:%S",
			 lt);
		tzoffset_buf(tz, &timer);
		memcpy(date_time + 3, monthstr + lt->tm_mon * 3, 3);
		g_snprintf(buf, len, "%s %s", date_time, tz);
	}
}

static gint imap_cmd_append(IMAPSession *session, const gchar *destfolder,
			    const gchar *file, IMAPFlags flags,
			    guint32 *new_uid)
{
	gint ok;
	MsgInfo *msginfo;
	MsgFlags flags_ = {0, 0};
	gchar date_time[64] = "";
	gint size;
	gchar *destfolder_;
	gchar *flag_str;
	guint new_uid_;
	gchar *ret = NULL;
	gchar buf[BUFFSIZE];
	FILE *fp;
	FILE *tmp;
	size_t read_len;
	GPtrArray *argbuf;
	gchar *resp_str;

	g_return_val_if_fail(file != NULL, IMAP_ERROR);

	if ((fp = g_fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return -1;
	}

	/* use Date: header as received date */
	msginfo = procheader_parse_stream(fp, flags_, FALSE);
	imap_get_date_time(date_time, sizeof(date_time), msginfo->date_t);
	procmsg_msginfo_free(msginfo);

	rewind(fp);
	tmp = canonicalize_file_stream(fp, &size);
	fclose(fp);
	if (!tmp)
		return -1;

	QUOTE_IF_REQUIRED(destfolder_, destfolder);
	flag_str = imap_get_flag_str(flags);
	if (date_time[0])
		ok = imap_cmd_gen_send(session, "APPEND %s (%s) \"%s\" {%d}",
				       destfolder_, flag_str, date_time, size);
	else
		ok = imap_cmd_gen_send(session, "APPEND %s (%s) {%d}",
				       destfolder_, flag_str, size);
	g_free(flag_str);
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't append %s to %s\n"), file, destfolder_);
		fclose(tmp);
		return ok;
	}

	ok = imap_cmd_gen_recv(session, &ret);
	if (ok != IMAP_SUCCESS || ret[0] != '+') {
		log_warning(_("can't append %s to %s\n"), file, destfolder_);
		g_free(ret);
		fclose(tmp);
		return IMAP_ERROR;
	}
	g_free(ret);

	log_print("IMAP4> %s\n", _("(sending file...)"));

	while ((read_len = fread(buf, 1, sizeof(buf), tmp)) > 0) {
		if (read_len < sizeof(buf) && ferror(tmp))
			break;
		if (sock_write_all(SESSION(session)->sock, buf, read_len) < 0) {
			fclose(tmp);
			return -1;
		}
	}

	if (ferror(tmp)) {
		FILE_OP_ERROR(file, "fread");
		fclose(tmp);
		return -1;
	}

	sock_puts(SESSION(session)->sock, "");

	fclose(tmp);

	if (new_uid != NULL)
		*new_uid = 0;

	if (new_uid != NULL && session->uidplus) {
		argbuf = g_ptr_array_new();

		ok = imap_cmd_ok(session, argbuf);
		if (ok != IMAP_SUCCESS)
			log_warning(_("can't append message to %s\n"),
				    destfolder_);
		else if (argbuf->len > 0) {
			resp_str = g_ptr_array_index(argbuf, argbuf->len - 1);
			if (resp_str &&
			    sscanf(resp_str, "%*u OK [APPENDUID %*u %u]",
				   &new_uid_) == 1) {
				*new_uid = new_uid_;
			}
		}

		ptr_array_free_strings(argbuf);
		g_ptr_array_free(argbuf, TRUE);
	} else
		ok = imap_cmd_ok(session, NULL);

	return ok;
}

static gint imap_cmd_copy(IMAPSession *session, const gchar *seq_set,
			  const gchar *destfolder)
{
	gint ok;
	gchar *destfolder_;

	g_return_val_if_fail(destfolder != NULL, IMAP_ERROR);

	QUOTE_IF_REQUIRED(destfolder_, destfolder);
	ok = imap_cmd_gen_send(session, "UID COPY %s %s", seq_set, destfolder_);
	if (ok == IMAP_SUCCESS)
		ok = imap_cmd_ok(session, NULL);
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't copy %s to %s\n"), seq_set, destfolder_);
		return -1;
	}

	return ok;
}

gint imap_cmd_envelope(IMAPSession *session, const gchar *seq_set)
{
	return imap_cmd_gen_send
		(session, "UID FETCH %s (UID FLAGS RFC822.SIZE RFC822.HEADER)",
		 seq_set);
}

static gint imap_cmd_store(IMAPSession *session, const gchar *seq_set,
			   const gchar *sub_cmd)
{
	gint ok;

	ok = imap_cmd_gen_send(session, "UID STORE %s %s", seq_set, sub_cmd);
	if (ok == IMAP_SUCCESS)
		ok = imap_cmd_ok(session, NULL);
	if (ok != IMAP_SUCCESS) {
		log_warning(_("error while imap command: STORE %s %s\n"),
			    seq_set, sub_cmd);
		return ok;
	}

	return IMAP_SUCCESS;
}

static gint imap_cmd_expunge(IMAPSession *session)
{
	gint ok;

	ok = imap_cmd_gen_send(session, "EXPUNGE");
	if (ok == IMAP_SUCCESS)
		ok = imap_cmd_ok(session, NULL);
	if (ok != IMAP_SUCCESS) {
		log_warning(_("error while imap command: EXPUNGE\n"));
		return ok;
	}

	return IMAP_SUCCESS;
}

static gint imap_cmd_close(IMAPSession *session)
{
	gint ok;

	ok = imap_cmd_gen_send(session, "CLOSE");
	if (ok == IMAP_SUCCESS)
		ok = imap_cmd_ok(session, NULL);
	if (ok != IMAP_SUCCESS)
		log_warning(_("error while imap command: CLOSE\n"));

	return ok;
}

static gint imap_cmd_ok_real(IMAPSession *session, GPtrArray *argbuf)
{
	gint ok;
	gchar *buf;
	gint cmd_num;
	gchar cmd_status[IMAPBUFSIZE + 1];
	GString *str;
	gchar *p;
	gchar obuf[32];
	gint len;
	gchar *literal;

	str = g_string_sized_new(256);

	//g_usleep(800000);
	while ((ok = imap_cmd_gen_recv(session, &buf)) == IMAP_SUCCESS) {
		g_string_append(str, buf);

		if ((p = strrchr_with_skip_quote(buf, '"', '{'))) {
			/* literal */
			p = strchr_cpy(p + 1, '}', obuf, sizeof(obuf));
			len = atoi(obuf);
			if (len < 0 || p == NULL || *p != '\0') {
				g_free(buf);
				ok = IMAP_ERROR;
				break;
			}

			literal = recv_bytes(SESSION(session)->sock, len);
			if (!literal) {
				g_free(buf);
				ok = IMAP_SOCKET;
				break;
			}
			if (memchr(literal, '\n', len))
				log_print("IMAP4< (literal: %d bytes)\n", len);
			else
				log_print("IMAP4< %s\n", literal);

			g_string_append(str, "\r\n");
			g_string_append_len(str, literal, len);
			g_free(literal);
			g_free(buf);
			continue;
		}

		g_free(buf);

		if (str->str[0] == '*' && str->str[1] == ' ') {
			if (argbuf)
				g_ptr_array_add(argbuf, g_strdup(str->str + 2));

			g_string_truncate(str, 0);
			continue;
		} else if (sscanf(str->str, "%d %" Xstr(IMAPBUFSIZE) "s",
			   &cmd_num, cmd_status) < 2) {
			ok = IMAP_ERROR;
		} else if (cmd_num == session->cmd_count &&
			   !strcmp(cmd_status, "OK")) {
			if (argbuf)
				g_ptr_array_add(argbuf, g_strdup(str->str));
		} else {
			ok = IMAP_ERROR;
		}

		break;
	}

	g_string_free(str, TRUE);
	return ok;
}

#if USE_THREADS
static gint imap_cmd_ok_func(IMAPSession *session, gpointer data)
{
	GPtrArray *argbuf = (GPtrArray *)data;
	gint ok;

	ok = imap_cmd_ok_real(session, argbuf);
	return ok;
}
#endif

static gint imap_cmd_ok(IMAPSession *session, GPtrArray *argbuf)
{
#if USE_THREADS
	return imap_thread_run(session, imap_cmd_ok_func, argbuf);
#else
	return imap_cmd_ok_real(session, argbuf);
#endif
}

static gint imap_cmd_gen_send(IMAPSession *session, const gchar *format, ...)
{
	IMAPRealSession *real = (IMAPRealSession *)session;
	gchar buf[IMAPBUFSIZE];
	gchar tmp[IMAPBUFSIZE];
	gchar *p;
	va_list args;

	va_start(args, format);
	g_vsnprintf(tmp, sizeof(tmp), format, args);
	va_end(args);

#if USE_THREADS
	if (real->is_running) {
		g_warning("imap_cmd_gen_send: cannot send command because another command is already running.");
		return IMAP_EAGAIN;
	}
#endif

	session->cmd_count++;

	g_snprintf(buf, sizeof(buf), "%d %s\r\n", session->cmd_count, tmp);
	if (!g_ascii_strncasecmp(tmp, "LOGIN ", 6) &&
	    (p = strchr(tmp + 6, ' '))) {
		*p = '\0';
		log_print("IMAP4> %d %s ********\n", session->cmd_count, tmp);
	} else
		log_print("IMAP4> %d %s\n", session->cmd_count, tmp);

	sock_write_all(SESSION(session)->sock, buf, strlen(buf));

	return IMAP_SUCCESS;
}

static gint imap_cmd_gen_recv(IMAPSession *session, gchar **ret)
{
	gint len;

	if ((len = sock_getline(SESSION(session)->sock, ret)) < 0)
		return IMAP_SOCKET;

	strretchomp(*ret);

	if (len > 1000) {
		gchar *str;

		str = trim_string(*ret, 1000);
		log_print("IMAP4< %s\n", str);
		g_free(str);
	} else
		log_print("IMAP4< %s\n", *ret);

	session_set_access_time(SESSION(session));

	return IMAP_SUCCESS;
}

static gint imap_cmd_gen_recv_silent(IMAPSession *session, gchar **ret)
{
	gint len;

	if ((len = sock_getline(SESSION(session)->sock, ret)) < 0)
		return IMAP_SOCKET;

	strretchomp(*ret);

	session_set_access_time(SESSION(session));

	return IMAP_SUCCESS;
}


/* misc utility functions */

static gchar *strchr_cpy(const gchar *src, gchar ch, gchar *dest, gint len)
{
	gchar *tmp;

	dest[0] = '\0';
	tmp = strchr(src, ch);
	if (!tmp)
		return NULL;

	memcpy(dest, src, MIN(tmp - src, len - 1));
	dest[MIN(tmp - src, len - 1)] = '\0';

	return tmp + 1;
}

static gchar *get_quoted(const gchar *src, gchar ch, gchar *dest, gint len)
{
	const gchar *p = src;
	gint n = 0;

	g_return_val_if_fail(*p == ch, NULL);

	*dest = '\0';
	p++;

	while (*p != '\0' && *p != ch) {
		if (n < len - 1) {
			if (*p == '\\' && *(p + 1) != '\0')
				p++;
			*dest++ = *p++;
		} else
			p++;
		n++;
	}

	*dest = '\0';
	return (gchar *)(*p == ch ? p + 1 : p);
}

static gchar *search_array_contain_str(GPtrArray *array, gchar *str)
{
	gint i;

	for (i = 0; i < array->len; i++) {
		gchar *tmp;

		tmp = g_ptr_array_index(array, i);
		if (strstr(tmp, str) != NULL)
			return tmp;
	}

	return NULL;
}

static gchar *search_array_str(GPtrArray *array, gchar *str)
{
	gint i;
	gint len;

	len = strlen(str);

	for (i = 0; i < array->len; i++) {
		gchar *tmp;

		tmp = g_ptr_array_index(array, i);
		if (!strncmp(tmp, str, len))
			return tmp;
	}

	return NULL;
}

static void imap_path_separator_subst(gchar *str, gchar separator)
{
	gchar *p;
	gboolean in_escape = FALSE;

	if (!separator || separator == '/') return;

	for (p = str; *p != '\0'; p++) {
		if (*p == '/' && !in_escape)
			*p = separator;
		else if (*p == '&' && *(p + 1) != '-' && !in_escape)
			in_escape = TRUE;
		else if (*p == '-' && in_escape)
			in_escape = FALSE;
	}
}

static gchar *imap_modified_utf7_to_utf8(const gchar *mutf7_str)
{
	static iconv_t cd = (iconv_t)-1;
	static gboolean iconv_ok = TRUE;
	GString *norm_utf7;
	gchar *norm_utf7_p;
	size_t norm_utf7_len;
	const gchar *p;
	gchar *to_str, *to_p;
	size_t to_len;
	gboolean in_escape = FALSE;

	if (!iconv_ok) return g_strdup(mutf7_str);

	if (cd == (iconv_t)-1) {
		cd = iconv_open(CS_INTERNAL, CS_UTF_7);
		if (cd == (iconv_t)-1) {
			g_warning("iconv cannot convert UTF-7 to %s\n",
				  CS_INTERNAL);
			iconv_ok = FALSE;
			return g_strdup(mutf7_str);
		}
	}

	/* modified UTF-7 to normal UTF-7 conversion */
	norm_utf7 = g_string_new(NULL);

	for (p = mutf7_str; *p != '\0'; p++) {
		/* replace: '&'  -> '+',
			    "&-" -> '&',
			    "+"  -> "+-",
			    escaped ','  -> '/' */
		if (!in_escape && *p == '&') {
			if (*(p + 1) != '-') {
				g_string_append_c(norm_utf7, '+');
				in_escape = TRUE;
			} else {
				g_string_append_c(norm_utf7, '&');
				p++;
			}
		} else if (!in_escape && *p == '+') {
			g_string_append(norm_utf7, "+-");
		} else if (in_escape && *p == ',') {
			g_string_append_c(norm_utf7, '/');
		} else if (in_escape && *p == '-') {
			g_string_append_c(norm_utf7, '-');
			in_escape = FALSE;
		} else {
			g_string_append_c(norm_utf7, *p);
		}
	}

	/* somehow iconv() returns error when the last of the string is "+-" */
	g_string_append_c(norm_utf7, '\n');
	norm_utf7_p = norm_utf7->str;
	norm_utf7_len = norm_utf7->len;
	to_len = strlen(mutf7_str) * 5;
	to_p = to_str = g_malloc(to_len + 1);

	if (iconv(cd, (ICONV_CONST gchar **)&norm_utf7_p, &norm_utf7_len,
		  &to_p, &to_len) == -1) {
		g_warning(_("iconv cannot convert UTF-7 to %s\n"), CS_INTERNAL);
		g_string_free(norm_utf7, TRUE);
		g_free(to_str);
		return g_strdup(mutf7_str);
	}

	/* second iconv() call for flushing */
	iconv(cd, NULL, NULL, &to_p, &to_len);
	g_string_free(norm_utf7, TRUE);
	*to_p = '\0';
	strretchomp(to_str);

	return to_str;
}

static gchar *imap_utf8_to_modified_utf7(const gchar *from)
{
	static iconv_t cd = (iconv_t)-1;
	static gboolean iconv_ok = TRUE;
	gchar *norm_utf7, *norm_utf7_p;
	size_t from_len, norm_utf7_len;
	GString *to_str;
	const gchar *from_tmp;
	const gchar *p;
	gchar *to;
	gboolean in_escape = FALSE;

	if (!iconv_ok) return g_strdup(from);

	if (cd == (iconv_t)-1) {
		cd = iconv_open(CS_UTF_7, CS_INTERNAL);
		if (cd == (iconv_t)-1) {
			g_warning(_("iconv cannot convert %s to UTF-7\n"),
				  CS_INTERNAL);
			iconv_ok = FALSE;
			return g_strdup(from);
		}
	}

	/* UTF-8 to normal UTF-7 conversion */
	from_tmp = from;
	from_len = strlen(from);
	norm_utf7_len = from_len * 5;
	norm_utf7 = g_malloc(norm_utf7_len + 1);
	norm_utf7_p = norm_utf7;

	while (from_len > 0) {
		if (*from_tmp == '+') {
			*norm_utf7_p++ = '+';
			*norm_utf7_p++ = '-';
			norm_utf7_len -= 2;
			from_tmp++;
			from_len--;
		} else if (g_ascii_isprint(*from_tmp)) {
			/* printable ascii char */
			*norm_utf7_p = *from_tmp;
			norm_utf7_p++;
			norm_utf7_len--;
			from_tmp++;
			from_len--;
		} else {
			size_t conv_len = 0;

			/* unprintable char: convert to UTF-7 */
			p = from_tmp;
			while (!g_ascii_isprint(*p) && conv_len < from_len) {
				conv_len += g_utf8_skip[*(guchar *)p];
				p += g_utf8_skip[*(guchar *)p];
			}

			from_len -= conv_len;
			if (iconv(cd, (ICONV_CONST gchar **)&from_tmp,
				  &conv_len,
				  &norm_utf7_p, &norm_utf7_len) == -1) {
				g_warning("iconv cannot convert %s to UTF-7\n",
					  CS_INTERNAL);
				g_free(norm_utf7);
				return g_strdup(from);
			}

			/* second iconv() call for flushing */
			iconv(cd, NULL, NULL, &norm_utf7_p, &norm_utf7_len);
		}
	}

	*norm_utf7_p = '\0';
	to_str = g_string_new(NULL);
	for (p = norm_utf7; p < norm_utf7_p; p++) {
		/* replace: '&' -> "&-",
			    '+' -> '&',
			    "+-" -> '+',
			    BASE64 '/' -> ',' */
		if (!in_escape && *p == '&') {
			g_string_append(to_str, "&-");
		} else if (!in_escape && *p == '+') {
			if (*(p + 1) == '-') {
				g_string_append_c(to_str, '+');
				p++;
			} else {
				g_string_append_c(to_str, '&');
				in_escape = TRUE;
			}
		} else if (in_escape && *p == '/') {
			g_string_append_c(to_str, ',');
		} else if (in_escape && *p == '-') {
			g_string_append_c(to_str, '-');
			in_escape = FALSE;
		} else {
			g_string_append_c(to_str, *p);
		}
	}

	if (in_escape) {
		in_escape = FALSE;
		g_string_append_c(to_str, '-');
	}

	to = g_string_free(to_str, FALSE);
	g_free(norm_utf7);

	return to;
}

static GSList *imap_get_seq_set_from_msglist(GSList *msglist, gint limit)
{
	GString *str;
	GSList *sorted_list, *cur;
	guint first, last, next;
	gchar *ret_str;
	GSList *ret_list = NULL;
	gint count = 0;

	if (msglist == NULL)
		return NULL;

	str = g_string_sized_new(256);

	sorted_list = g_slist_copy(msglist);
	sorted_list = procmsg_sort_msg_list(sorted_list, SORT_BY_NUMBER,
					    SORT_ASCENDING);

	first = ((MsgInfo *)sorted_list->data)->msgnum;

	for (cur = sorted_list; cur != NULL; cur = cur->next) {
		++count;
		last = ((MsgInfo *)cur->data)->msgnum;
		if (cur->next)
			next = ((MsgInfo *)cur->next->data)->msgnum;
		else
			next = 0;

		if (limit > 0 && count >= limit) {
			if (str->len > 0)
				g_string_append_c(str, ',');
			if (first == last)
				g_string_sprintfa(str, "%u", first);
			else
				g_string_sprintfa(str, "%u:%u", first, last);

			first = next;

			ret_str = g_strdup(str->str);
			ret_list = g_slist_append(ret_list, ret_str);
			g_string_truncate(str, 0);
			count = 0;
			continue;
		}

		if (last + 1 != next || next == 0) {
			if (str->len > 0)
				g_string_append_c(str, ',');
			if (first == last)
				g_string_sprintfa(str, "%u", first);
			else
				g_string_sprintfa(str, "%u:%u", first, last);

			first = next;

			if (str->len > IMAP_CMD_LIMIT) {
				ret_str = g_strdup(str->str);
				ret_list = g_slist_append(ret_list, ret_str);
				g_string_truncate(str, 0);
			}
		}
	}

	if (str->len > 0) {
		ret_str = g_strdup(str->str);
		ret_list = g_slist_append(ret_list, ret_str);
	}

	g_slist_free(sorted_list);
	g_string_free(str, TRUE);

	return ret_list;
}

static gint imap_seq_set_get_count(const gchar *seq_set)
{
	gint count = 0;
	guint first, last;
	gchar *tmp, *p, *q;

	p = q = tmp = g_strdup(seq_set);

	while (*p) {
		if (*p == ',') {
			*p = '\0';
			if (sscanf(q, "%u:%u", &first, &last) == 2)
				count += last - first + 1;
			else if (sscanf(q, "%u", &first) == 1)
				count++;
			q = ++p;
		} else
			++p;
	}
	if (q != p) {
		if (sscanf(q, "%u:%u", &first, &last) == 2)
			count += last - first + 1;
		else if (sscanf(q, "%u", &first) == 1)
			count++;
	}

	g_free(tmp);

	return count;
}

static void imap_seq_set_free(GSList *seq_list)
{
	slist_free_strings(seq_list);
	g_slist_free(seq_list);
}

static GHashTable *imap_get_uid_table(GArray *array)
{
	GHashTable *table;
	gint i;
	guint32 uid;

	g_return_val_if_fail(array != NULL, NULL);

	table = g_hash_table_new(NULL, g_direct_equal);

	for (i = 0; i < array->len; i++) {
		uid = g_array_index(array, guint32, i);
		g_hash_table_insert(table, GUINT_TO_POINTER(uid),
				    GINT_TO_POINTER(i + 1));
	}

	return table;
}

static gboolean imap_rename_folder_func(GNode *node, gpointer data)
{
	FolderItem *item = node->data;
	gchar **paths = data;
	const gchar *oldpath = paths[0];
	const gchar *newpath = paths[1];
	gchar *base;
	gchar *new_itempath;
	gint oldpathlen;

	oldpathlen = strlen(oldpath);
	if (strncmp(oldpath, item->path, oldpathlen) != 0) {
		g_warning("path doesn't match: %s, %s\n", oldpath, item->path);
		return TRUE;
	}

	base = item->path + oldpathlen;
	while (*base == '/') base++;
	if (*base == '\0')
		new_itempath = g_strdup(newpath);
	else
		new_itempath = g_strconcat(newpath, "/", base, NULL);
	g_free(item->path);
	item->path = new_itempath;

	return FALSE;
}

#if USE_THREADS
static void imap_thread_run_proxy(gpointer push_data, gpointer data)
{
	IMAPRealSession *real = (IMAPRealSession *)data;

	debug_print("imap_thread_run_proxy (%p): calling thread_func\n", g_thread_self());
	real->retval = real->thread_func(IMAP_SESSION(real), real->thread_data);
	g_atomic_int_set(&real->flag, 1);
	g_main_context_wakeup(NULL);
	debug_print("imap_thread_run_proxy (%p): thread_func done\n", g_thread_self());
}

static gint imap_thread_run(IMAPSession	*session, IMAPThreadFunc func,
			    gpointer data)
{
	IMAPRealSession *real = (IMAPRealSession *)session;
	gint ret;

	if (real->is_running) {
		g_warning("imap_thread_run: thread is already running");
		return IMAP_ERROR;
	}

	if (!real->pool) {
		real->pool = g_thread_pool_new(imap_thread_run_proxy, real,
					       -1, FALSE, NULL);
		if (!real->pool)
			return IMAP_ERROR;
	}

	real->is_running = TRUE;
	real->thread_func = func;
	real->thread_data = data;
	real->flag = 0;
	real->retval = 0;

	g_thread_pool_push(real->pool, real, NULL);

	while (g_atomic_int_get(&real->flag) == 0)
		event_loop_iterate();

	real->is_running = FALSE;
	real->thread_func = NULL;
	real->thread_data = NULL;
	real->flag = 0;
	ret = real->retval;
	real->retval = 0;
	log_flush();

	return ret;
}

static gint imap_thread_run_progress(IMAPSession *session, IMAPThreadFunc func,
				     IMAPProgressFunc progress_func,
				     gpointer data)
{
	IMAPRealSession *real = (IMAPRealSession *)session;
	gint prev_count = 0;
	gint ret;

	if (real->is_running) {
		g_warning("imap_thread_run: thread is already running");
		return IMAP_ERROR;
	}

	if (!real->pool) {
		real->pool = g_thread_pool_new(imap_thread_run_proxy, real,
					       -1, FALSE, NULL);
		if (!real->pool)
			return IMAP_ERROR;
	}

	real->is_running = TRUE;
	real->thread_func = func;
	real->thread_data = data;
	real->flag = 0;
	real->retval = 0;
	real->prog_count = 0;
	real->prog_total = 0;

	g_thread_pool_push(real->pool, real, NULL);

	while (g_atomic_int_get(&real->flag) == 0) {
		event_loop_iterate();
		if (prev_count != real->prog_count && real->prog_total > 0) {
			progress_func(session, real->prog_count,
				      real->prog_total, data);
			prev_count = real->prog_count;
		}
	}

	real->is_running = FALSE;
	real->thread_func = NULL;
	real->thread_data = NULL;
	real->flag = 0;
	ret = real->retval;
	real->retval = 0;
	real->prog_count = 0;
	real->prog_total = 0;
	log_flush();

	return ret;
}
#endif /* USE_THREADS */

gboolean imap_is_session_active(IMAPFolder *folder)
{
#if USE_THREADS
	IMAPRealSession *real;

	g_return_val_if_fail(folder != NULL, FALSE);

	real = (IMAPRealSession *)(REMOTE_FOLDER(folder)->session);
	if (!real)
		return FALSE;

	return real->is_running;
#else
	return FALSE;
#endif
}
