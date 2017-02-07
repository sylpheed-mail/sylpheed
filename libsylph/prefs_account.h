/*
 * LibSylph -- E-Mail client library
 * Copyright (C) 1999-2017 Hiroyuki Yamamoto
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

#ifndef __PREFS_ACCOUNT_H__
#define __PREFS_ACCOUNT_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>

typedef struct _PrefsAccount	PrefsAccount;

#include "folder.h"
#include "smtp.h"
#include "prefs.h"

typedef enum {
	A_POP3,
	A_APOP,	/* deprecated */
	A_RPOP,	/* deprecated */
	A_IMAP4,
	A_NNTP,
	A_LOCAL
} RecvProtocol;

typedef enum {
	SIG_FILE,
	SIG_COMMAND,
	SIG_DIRECT
} SigType;

typedef enum {
	SIGN_KEY_DEFAULT,
	SIGN_KEY_BY_FROM,
	SIGN_KEY_CUSTOM
} SignKeyType;

struct _PrefsAccount
{
	gchar *account_name;

	/* Personal info */
	gchar *name;
	gchar *address;
	gchar *organization;

	/* Server info */
	RecvProtocol protocol;
	gchar *recv_server;
	gchar *smtp_server;
	gchar *nntp_server;
	gboolean use_nntp_auth;
	gchar *userid;
	gchar *passwd;

	/* SSL */
	gint ssl_pop;
	gint ssl_imap;
	gint ssl_nntp;
	gint ssl_smtp;
	gboolean use_nonblocking_ssl;

	/* Temporarily preserved password */
	gchar *tmp_pass;

	/* Receive */
	gboolean use_apop_auth;
	gboolean rmmail;
	gint msg_leave_time;
	gboolean getall;
	gboolean enable_size_limit;
	gint size_limit;
	gboolean filter_on_recv;
	gchar *inbox;

	gboolean imap_check_inbox_only;
	gboolean imap_filter_inbox_on_recv;
	gint imap_auth_type;

	gint max_nntp_articles;

	gboolean recv_at_getall;

	/* Send */
	gboolean add_date;
	gboolean gen_msgid;
	gboolean add_customhdr;
	gboolean use_smtp_auth;
	SMTPAuthType smtp_auth_type;
	gchar *smtp_userid;
	gchar *smtp_passwd;

	/* Temporarily preserved password */
	gchar *tmp_smtp_pass;

	gboolean pop_before_smtp;

	GSList *customhdr_list;

	/* Compose */
	SigType sig_type;
	gchar *sig_path;
	gchar *sig_text;
	gboolean  set_autocc;
	gchar    *auto_cc;
	gboolean  set_autobcc;
	gchar    *auto_bcc;
	gboolean  set_autoreplyto;
	gchar    *auto_replyto;

	/* Privacy */
	gboolean default_sign;
	gboolean default_encrypt;
	gboolean ascii_armored;
	gboolean clearsign;
	gboolean encrypt_reply;

	SignKeyType sign_key;
	gchar *sign_key_id;

	/* Advanced */
	gboolean  set_smtpport;
	gushort   smtpport;
	gboolean  set_popport;
	gushort   popport;
	gboolean  set_imapport;
	gushort   imapport;
	gboolean  set_nntpport;
	gushort   nntpport;
	gboolean  set_domain;
	gchar    *domain;

	gchar *imap_dir;
	gboolean imap_clear_cache_on_exit;

	gboolean set_sent_folder;
	gchar *sent_folder;
	gboolean set_draft_folder;
	gchar *draft_folder;
	gboolean set_queue_folder;
	gchar *queue_folder;
	gboolean set_trash_folder;
	gchar *trash_folder;

	/* Default or not */
	gboolean is_default;
	/* Unique account ID */
	gint account_id;

	RemoteFolder *folder;

	/* Compose */
	gboolean  sig_before_quote;

	/* SOCKS proxy */
	gboolean use_socks;
	gboolean use_socks_for_recv;
	gboolean use_socks_for_send;
	gint socks_type;
	gchar *proxy_host;
	gushort proxy_port;
	gboolean use_proxy_auth;
	gchar *proxy_name;
	gchar *proxy_pass;

	/* Privacy */
	gboolean encrypt_to_self;

	/* Compose */
	gchar *sig_names[10];
	gchar *sig_texts[10];
};

PrefsAccount *prefs_account_new		(void);

PrefsAccount *prefs_account_get_tmp_prefs	(void);
void prefs_account_set_tmp_prefs		(PrefsAccount	*ac_prefs);
void prefs_account_apply_tmp_prefs		(PrefsAccount	*ac_prefs);
PrefParam *prefs_account_get_params		(void);

void prefs_account_read_config		(PrefsAccount	*ac_prefs,
					 const gchar	*label);
void prefs_account_write_config_all	(GList		*account_list);

void prefs_account_free			(PrefsAccount	*ac_prefs);

#endif /* __PREFS_ACCOUNT_H__ */
