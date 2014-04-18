/*
 * LibSylph -- E-Mail client library
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto
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

#ifndef __POP_H__
#define __POP_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <time.h>

#include "session.h"
#include "prefs_account.h"
#include "utils.h"

typedef struct _Pop3MsgInfo	Pop3MsgInfo;
typedef struct _Pop3Session	Pop3Session;

#define POP3_SESSION(obj)	((Pop3Session *)obj)

typedef enum {
	POP3_READY,
	POP3_GREETING,
	POP3_STLS,
	POP3_GETAUTH_USER,
	POP3_GETAUTH_PASS,
	POP3_GETAUTH_APOP,
	POP3_GETRANGE_STAT,
	POP3_GETRANGE_LAST,
	POP3_GETRANGE_UIDL,
	POP3_GETRANGE_UIDL_RECV,
	POP3_GETSIZE_LIST,
	POP3_GETSIZE_LIST_RECV,
	POP3_RETR,
	POP3_RETR_RECV,
	POP3_DELETE,
	POP3_LOGOUT,
	POP3_DONE,
	POP3_ERROR,

	N_POP3_STATE
} Pop3State;

typedef enum {
	PS_SUCCESS	= 0,	/* command successful */
	PS_NOMAIL	= 1,	/* no mail available */
	PS_SOCKET	= 2,	/* socket I/O woes */
	PS_AUTHFAIL	= 3,	/* user authorization failed */
	PS_PROTOCOL	= 4,	/* protocol violation */
	PS_SYNTAX	= 5,	/* command-line syntax error */
	PS_IOERR	= 6,	/* file I/O error */
	PS_ERROR	= 7,	/* protocol error */
	PS_EXCLUDE	= 8,	/* client-side exclusion error */
	PS_LOCKBUSY	= 9,	/* server responded lock busy */
	PS_SMTP		= 10,	/* SMTP error */
	PS_DNS		= 11,	/* fatal DNS error */
	PS_BSMTP	= 12,	/* output batch could not be opened */
	PS_MAXFETCH	= 13,	/* poll ended by fetch limit */

	PS_NOTSUPPORTED	= 14,	/* command not supported */

	/* leave space for more codes */

	PS_CONTINUE	= 128	/* more responses may follow */
} Pop3ErrorValue;

typedef enum {
	RECV_TIME_NONE     = 0,
	RECV_TIME_RECEIVED = 1,
	RECV_TIME_KEEP     = 2,
	RECV_TIME_DELETE   = 3
} RecvTime;

typedef enum {
	DROP_OK = 0,
	DROP_DONT_RECEIVE = 1,
	DROP_DELETE = 2,
	DROP_ERROR = -1
} Pop3DropValue;

struct _Pop3MsgInfo
{
	gint size;
	gchar *uidl;
	stime_t recv_time;
	guint received : 1;
	guint deleted  : 1;
};

struct _Pop3Session
{
	Session session;

	Pop3State state;

	PrefsAccount *ac_prefs;

	gchar *greeting;
	gchar *user;
	gchar *pass;
	gint count;
	gint64 total_bytes;
	gint cur_msg;
	gint cur_total_num;
	gint64 cur_total_bytes;
	gint64 cur_total_recv_bytes;
	gint skipped_num;

	Pop3MsgInfo *msg;

	GHashTable *uidl_table;

	gboolean auth_only;

	gboolean new_msg_exist;
	gboolean uidl_is_valid;

	stime_t current_time;

	Pop3ErrorValue error_val;
	gchar *error_msg;

	gpointer data;

	/* virtual method to drop message */
	gint (*drop_message)	(Pop3Session	*session,
				 const gchar	*file);
};

#define POPBUFSIZE	512
/* #define IDLEN	128 */
#define IDLEN		POPBUFSIZE

Session *pop3_session_new	(PrefsAccount	*account);

GHashTable *pop3_get_uidl_table	(PrefsAccount	*account);
gint pop3_write_uidl_list	(Pop3Session	*session);

#endif /* __POP_H__ */
