/*
 * LibSylph -- E-Mail client library
 * Copyright (C) 1999-2005 Hiroyuki Yamamoto
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

#ifndef __SESSION_H__
#define __SESSION_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>

#include <time.h>
#include <unistd.h>

#include "socket.h"

#define SESSION_BUFFSIZE	4096

typedef struct _Session	Session;

#define SESSION(obj)	((Session *)obj)

typedef enum {
	SESSION_UNKNOWN,
	SESSION_IMAP,
	SESSION_NEWS,
	SESSION_SMTP,
	SESSION_POP3
} SessionType;

typedef enum {
	SESSION_READY,
	SESSION_SEND,
	SESSION_RECV,
	SESSION_EOF,
	SESSION_TIMEOUT,
	SESSION_ERROR,
	SESSION_DISCONNECTED
} SessionState;

typedef enum
{
	SESSION_MSG_NORMAL,
	SESSION_MSG_SEND_DATA,
	SESSION_MSG_RECV_DATA,
	SESSION_MSG_CONTROL,
	SESSION_MSG_ERROR,
	SESSION_MSG_UNKNOWN
} SessionMsgType;

typedef gint (*RecvMsgNotify)			(Session	*session,
						 const gchar	*msg,
						 gpointer	 user_data);
typedef gint (*RecvDataProgressiveNotify)	(Session	*session,
						 guint		 cur_len,
						 guint		 total_len,
						 gpointer	 user_data);
typedef gint (*RecvDataNotify)			(Session	*session,
						 guint		 len,
						 gpointer	 user_data);
typedef gint (*SendDataProgressiveNotify)	(Session	*session,
						 guint		 cur_len,
						 guint		 total_len,
						 gpointer	 user_data);
typedef gint (*SendDataNotify)			(Session	*session,
						 guint		 len,
						 gpointer	 user_data);

struct _Session
{
	SessionType type;

	SockInfo *sock;

	gchar *server;
	gushort port;

#if USE_SSL
	SSLType ssl_type;
#endif

	gboolean nonblocking;

	SessionState state;

	time_t last_access_time;
	GTimeVal tv_prev;

	gint conn_id;

	gint io_tag;

	gchar read_buf[SESSION_BUFFSIZE];
	gchar *read_buf_p;
	gint read_buf_len;

	GString *read_msg_buf;
	GByteArray *read_data_buf;
	gchar *read_data_terminator;

	/* buffer for short messages */
	gchar *write_buf;
	gchar *write_buf_p;
	gint write_buf_len;

	/* buffer for large data */
	FILE *write_data_fp;
	gint write_data_pos;
	gint write_data_len;

	guint timeout_tag;
	guint timeout_interval;

	gpointer data;

	/* virtual methods to parse server responses */
	gint (*recv_msg)		(Session	*session,
					 const gchar	*msg);

	gint (*send_data_finished)	(Session	*session,
					 guint		 len);
	gint (*recv_data_finished)	(Session	*session,
					 guchar		*data,
					 guint		 len);

	void (*destroy)			(Session	*session);

	/* notification functions */
	RecvMsgNotify			recv_msg_notify;
	RecvDataProgressiveNotify	recv_data_progressive_notify;
	RecvDataNotify			recv_data_notify;
	SendDataProgressiveNotify	send_data_progressive_notify;
	SendDataNotify			send_data_notify;

	gpointer recv_msg_notify_data;
	gpointer recv_data_progressive_notify_data;
	gpointer recv_data_notify_data;
	gpointer send_data_progressive_notify_data;
	gpointer send_data_notify_data;
};

void session_init		(Session	*session);
gint session_connect		(Session	*session,
				 const gchar	*server,
				 gushort	 port);
gint session_disconnect		(Session	*session);
void session_destroy		(Session	*session);
gboolean session_is_connected	(Session	*session);

void session_set_access_time	(Session	*session);

void session_set_timeout	(Session	*session,
				 guint		 interval);

void session_set_recv_message_notify	(Session	*session,
					 RecvMsgNotify	 notify_func,
					 gpointer	 data);
void session_set_recv_data_progressive_notify
					(Session	*session,
					 RecvDataProgressiveNotify notify_func,
					 gpointer	 data);
void session_set_recv_data_notify	(Session	*session,
					 RecvDataNotify	 notify_func,
					 gpointer	 data);
void session_set_send_data_progressive_notify
					(Session	*session,
					 SendDataProgressiveNotify notify_func,
					 gpointer	 data);
void session_set_send_data_notify	(Session	*session,
					 SendDataNotify	 notify_func,
					 gpointer	 data);

#if USE_SSL
gint session_start_tls	(Session	*session);
#endif

gint session_send_msg	(Session	*session,
			 SessionMsgType	 type,
			 const gchar	*msg);
gint session_recv_msg	(Session	*session);
gint session_send_data	(Session	*session,
			 FILE		*data_fp,
			 guint		 size);
gint session_recv_data	(Session	*session,
			 guint		 size,
			 const gchar	*terminator);

#endif /* __SESSION_H__ */
