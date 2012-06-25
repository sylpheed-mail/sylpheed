/*
 * LibSylph -- E-Mail client library
 * Copyright (C) 1999-2012 Hiroyuki Yamamoto
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include "session.h"
#include "utils.h"

typedef struct _SessionPrivData	SessionPrivData;

struct _SessionPrivData {
	Session *session;
	SocksInfo *socks_info;
	SessionErrorValue error_val;
	gpointer data;
};

static GList *priv_list = NULL;

static SessionPrivData *session_get_priv(Session	*session);

static gint session_connect_cb		(SockInfo	*sock,
					 gpointer	 data);
static gint session_close		(Session	*session);

static gboolean session_timeout_cb	(gpointer	 data);

#ifdef G_OS_WIN32
static gboolean session_ping_cb		(gpointer	 data);
#endif

static gboolean session_recv_msg_idle_cb	(gpointer	 data);
static gboolean session_recv_data_idle_cb	(gpointer	 data);

static gboolean session_recv_data_as_file_idle_cb	(gpointer	 data);

static gboolean session_read_msg_cb	(SockInfo	*source,
					 GIOCondition	 condition,
					 gpointer	 data);
static gboolean session_read_data_cb	(SockInfo	*source,
					 GIOCondition	 condition,
					 gpointer	 data);

static gboolean session_read_data_as_file_cb	(SockInfo	*source,
						 GIOCondition	 condition,
						 gpointer	 data);

static gboolean session_write_msg_cb	(SockInfo	*source,
					 GIOCondition	 condition,
					 gpointer	 data);
static gboolean session_write_data_cb	(SockInfo	*source,
					 GIOCondition	 condition,
					 gpointer	 data);


void session_init(Session *session)
{
	SessionPrivData *priv;

	session->type = SESSION_UNKNOWN;
	session->sock = NULL;
	session->server = NULL;
	session->port = 0;
#if USE_SSL
	session->ssl_type = SSL_NONE;
#endif
	session->nonblocking = TRUE;
	session->state = SESSION_READY;
	session->last_access_time = time(NULL);

	g_get_current_time(&session->tv_prev);

	session->conn_id = 0;

	session->io_tag = 0;

	session->read_buf_p = session->read_buf;
	session->read_buf_len = 0;

	session->read_msg_buf = g_string_sized_new(1024);
	session->read_data_buf = g_byte_array_new();
	session->read_data_terminator = NULL;

	session->read_data_fp = NULL;
	session->read_data_pos = 0;

	session->preread_len = 0;

	session->write_buf = NULL;
	session->write_buf_p = NULL;
	session->write_buf_len = 0;

	session->write_data_fp = NULL;
	session->write_data_pos = 0;
	session->write_data_len = 0;

	session->timeout_tag = 0;
	session->timeout_interval = 0;

	session->idle_tag = 0;
	session->ping_tag = 0;

	session->data = NULL;

	priv = g_new0(SessionPrivData, 1);
	priv->session = session;
	priv->socks_info = NULL;
	priv->error_val = SESSION_ERROR_OK;
	priv_list = g_list_prepend(priv_list, priv);
}

static SessionPrivData *session_get_priv(Session *session)
{
	SessionPrivData *priv;
	GList *cur;

	g_return_val_if_fail(session != NULL, NULL);

	for (cur = priv_list; cur != NULL; cur = cur->next) {
		priv = (SessionPrivData *)cur->data;
		if (priv->session == session)
                        return priv;
        }

        return NULL;
}

gint session_connect(Session *session, const gchar *server, gushort port)
{
	return session_connect_full(session, server, port, NULL);
}

gint session_connect_full(Session *session, const gchar *server, gushort port,
			  SocksInfo *socks_info)
{
	SessionPrivData *priv;
#ifndef G_OS_UNIX
	SockInfo *sock = NULL;
#endif
	g_return_val_if_fail(session != NULL, -1);
	g_return_val_if_fail(server != NULL, -1);
	g_return_val_if_fail(port > 0, -1);

	priv = session_get_priv(session);
	g_return_val_if_fail(priv != NULL, -1);
	priv->socks_info = socks_info;

	if (session->server != server) {
		g_free(session->server);
		session->server = g_strdup(server);
	}
	session->port = port;

	if (socks_info) {
		g_return_val_if_fail(socks_info->proxy_host != NULL, -1);
		server = socks_info->proxy_host;
		port = socks_info->proxy_port;
	}

#ifdef G_OS_UNIX
	session->conn_id = sock_connect_async(server, port, session_connect_cb,
					      session);
	if (session->conn_id < 0) {
		g_warning("can't connect to server.");
		session->state = SESSION_ERROR;
		priv->error_val = SESSION_ERROR_CONNFAIL;
		return -1;
	}
#elif USE_THREADS
	session->conn_id = sock_connect_async_thread(server, port);
	if (session->conn_id < 0) {
		g_warning("can't connect to server.");
		session->state = SESSION_ERROR;
		priv->error_val = SESSION_ERROR_CONNFAIL;
		return -1;
	}
	if (sock_info_connect_async_thread_wait(session->conn_id, &sock) < 0) {
		session_connect_cb(sock, session);
		if (sock)
			sock_close(sock);
		return -1;
	}
#else /* !USE_THREADS */
	sock = sock_new(server, port);
	if (sock_info_connect(sock) < 0) {
		session_connect_cb(sock, session);
		sock_close(sock);
		return -1;
	}
#endif

#ifdef G_OS_UNIX
	return 0;
#else
	return session_connect_cb(sock, session);
#endif
}

static gint session_connect_cb(SockInfo *sock, gpointer data)
{
	Session *session = SESSION(data);
	SessionPrivData *priv;

	priv = session_get_priv(session);
	session->conn_id = 0;

	if (!sock) {
		g_warning("can't connect to server.");
		session->state = SESSION_ERROR;
		priv->error_val = SESSION_ERROR_CONNFAIL;
		return -1;
	}
	if (sock->state == CONN_LOOKUPFAILED) {
		g_warning("DNS lookup failed.");
		session->state = SESSION_ERROR;
		priv->error_val = SESSION_ERROR_LOOKUP;
		return -1;
	} else if (sock->state != CONN_ESTABLISHED) {
		g_warning("can't connect to server (ConnectionState: %d).",
			  sock->state);
		session->state = SESSION_ERROR;
		priv->error_val = SESSION_ERROR_CONNFAIL;
		return -1;
	}

	session->sock = sock;

	if (priv->socks_info) {
		sock_set_nonblocking_mode(sock, FALSE);
		if (socks_connect(sock, session->server, session->port,
				  priv->socks_info) < 0) {
			g_warning("can't establish SOCKS connection.");
			session->state = SESSION_ERROR;
			priv->error_val = SESSION_ERROR_CONNFAIL;
			return -1;
		}
	}

#if USE_SSL
	if (session->ssl_type == SSL_TUNNEL) {
		sock_set_nonblocking_mode(sock, FALSE);
		if (!ssl_init_socket(sock)) {
			g_warning("can't initialize SSL.");
			session->state = SESSION_ERROR;
			priv->error_val = SESSION_ERROR_SOCKET;
			return -1;
		}
	}
#endif

	debug_print("session (%p): connected\n", session);

	sock_set_nonblocking_mode(sock, session->nonblocking);

	session->state = SESSION_RECV;
	priv->error_val = SESSION_ERROR_OK;
	session->io_tag = sock_add_watch(session->sock, G_IO_IN,
					 session_read_msg_cb,
					 session);

#ifdef G_OS_WIN32
	session->ping_tag = g_timeout_add_full(G_PRIORITY_LOW, 1000, session_ping_cb, session, NULL);
#endif

	return 0;
}

gint session_disconnect(Session *session)
{
	session_close(session);
	return 0;
}

void session_destroy(Session *session)
{
	SessionPrivData *priv;

	g_return_if_fail(session != NULL);
	g_return_if_fail(session->destroy != NULL);

	session_close(session);
	session->destroy(session);
	g_free(session->server);
	g_string_free(session->read_msg_buf, TRUE);
	g_byte_array_free(session->read_data_buf, TRUE);
	g_free(session->read_data_terminator);
	if (session->read_data_fp)
		fclose(session->read_data_fp);
	g_free(session->write_buf);

	priv = session_get_priv(session);
	if (priv) {
		priv_list = g_list_remove(priv_list, priv);
		socks_info_free(priv->socks_info);
		g_free(priv);
	}

	debug_print("session (%p): destroyed\n", session);

	g_free(session);
}

gboolean session_is_connected(Session *session)
{
	return (session->state == SESSION_READY ||
		session->state == SESSION_SEND ||
		session->state == SESSION_RECV);
}

SessionErrorValue session_get_error(Session *session)
{
	SessionPrivData *priv;

	priv = session_get_priv(session);
	if (priv)
		return priv->error_val;
	else
		return SESSION_ERROR_ERROR;
}

void session_set_access_time(Session *session)
{
	session->last_access_time = time(NULL);
}

void session_set_timeout(Session *session, guint interval)
{
	if (session->timeout_tag > 0)
		g_source_remove(session->timeout_tag);

	session->timeout_interval = interval;
	if (interval > 0)
		session->timeout_tag =
			g_timeout_add_full(G_PRIORITY_LOW, interval, session_timeout_cb, session, NULL);
	else
		session->timeout_tag = 0;
}

static gboolean session_timeout_cb(gpointer data)
{
	Session *session = SESSION(data);
	SessionPrivData *priv;

	g_warning("session timeout.\n");

	if (session->io_tag > 0) {
		g_source_remove(session->io_tag);
		session->io_tag = 0;
	}

	session->timeout_tag = 0;
	session->state = SESSION_TIMEOUT;
	priv = session_get_priv(session);
	priv->error_val = SESSION_ERROR_TIMEOUT;

	return FALSE;
}

#ifdef G_OS_WIN32
/* hack for state machine freeze problem in GLib >= 2.8.x */
static gboolean session_ping_cb(gpointer data)
{
	Session *session = SESSION(data);
	SockInfo *sock = session->sock;

	if (!session_is_connected(session))
		return FALSE;

	if (session->io_tag > 0 && sock && sock->callback) {
		GTimeVal tv_cur, tv_result;

		g_get_current_time(&tv_cur);
		tv_result.tv_sec = tv_cur.tv_sec - session->tv_prev.tv_sec;
		tv_result.tv_usec = tv_cur.tv_usec - session->tv_prev.tv_usec;
		if (tv_result.tv_usec < 0) {
			tv_result.tv_sec--;
			tv_result.tv_usec += G_USEC_PER_SEC;
		}
		if (tv_result.tv_sec * G_USEC_PER_SEC + tv_result.tv_usec >
		    G_USEC_PER_SEC) {
			SockFlags save_flags;

			debug_print("state machine freeze for 1 second detected, forcing dispatch.\n");
			save_flags = sock->flags;
			SOCK_UNSET_FLAGS(sock->flags, SYL_SOCK_CHECK_IO);
			sock->callback(sock, sock->condition, sock->data);
			sock->flags = save_flags;
		}
	}

	return TRUE;
}
#endif

void session_set_recv_message_notify(Session *session,
				     RecvMsgNotify notify_func, gpointer data)
{
	session->recv_msg_notify = notify_func;
	session->recv_msg_notify_data = data;
}

void session_set_recv_data_progressive_notify
					(Session *session,
					 RecvDataProgressiveNotify notify_func,
					 gpointer data)
{
	session->recv_data_progressive_notify = notify_func,
	session->recv_data_progressive_notify_data = data;
}

void session_set_recv_data_notify(Session *session, RecvDataNotify notify_func,
				  gpointer data)
{
	session->recv_data_notify = notify_func;
	session->recv_data_notify_data = data;
}

void session_set_send_data_progressive_notify
					(Session *session,
					 SendDataProgressiveNotify notify_func,
					 gpointer data)
{
	session->send_data_progressive_notify = notify_func;
	session->send_data_progressive_notify_data = data;
}

void session_set_send_data_notify(Session *session, SendDataNotify notify_func,
				  gpointer data)
{
	session->send_data_notify = notify_func;
	session->send_data_notify_data = data;
}

static gint session_close(Session *session)
{
	g_return_val_if_fail(session != NULL, -1);

#ifdef G_OS_UNIX
	if (session->conn_id > 0) {
		sock_connect_async_cancel(session->conn_id);
		session->conn_id = 0;
		debug_print("session (%p): connection cancelled\n", session);
	}
#endif

	session_set_timeout(session, 0);

	if (session->idle_tag > 0) {
		g_source_remove(session->idle_tag);
		session->idle_tag = 0;
	}

#ifdef G_OS_WIN32
	if (session->ping_tag > 0) {
		g_source_remove(session->ping_tag);
		session->ping_tag = 0;
	}
#endif

	if (session->io_tag > 0) {
		g_source_remove(session->io_tag);
		session->io_tag = 0;
	}

	if (session->sock) {
		sock_close(session->sock);
		session->sock = NULL;
		session->state = SESSION_DISCONNECTED;
		debug_print("session (%p): closed\n", session);
	}

	return 0;
}

#if USE_SSL
gint session_start_tls(Session *session)
{
	gboolean nb_mode;

	nb_mode = sock_is_nonblocking_mode(session->sock);

	sock_set_nonblocking_mode(session->sock, FALSE);

	if (!ssl_init_socket_with_method(session->sock, SSL_METHOD_TLSv1)) {
		g_warning("can't start TLS session.\n");
		if (nb_mode)
			sock_set_nonblocking_mode(session->sock, TRUE);
		return -1;
	}

	sock_set_nonblocking_mode(session->sock, session->nonblocking);

	return 0;
}
#endif

gint session_send_msg(Session *session, SessionMsgType type, const gchar *msg)
{
	gboolean ret;

	g_return_val_if_fail(session->sock != NULL, -1);
	g_return_val_if_fail(session->write_buf == NULL, -1);
	g_return_val_if_fail(msg != NULL, -1);
	g_return_val_if_fail(msg[0] != '\0', -1);

	session->state = SESSION_SEND;
	session->write_buf = g_strconcat(msg, "\r\n", NULL);
	session->write_buf_p = session->write_buf;
	session->write_buf_len = strlen(msg) + 2;

	ret = session_write_msg_cb(session->sock, G_IO_OUT, session);

	if (ret == TRUE)
		session->io_tag = sock_add_watch(session->sock, G_IO_OUT,
						 session_write_msg_cb, session);
	else if (session->state == SESSION_ERROR)
		return -1;

	return 0;
}

gint session_recv_msg(Session *session)
{
	g_return_val_if_fail(session->sock != NULL, -1);
	g_return_val_if_fail(session->read_msg_buf->len == 0, -1);

	session->state = SESSION_RECV;

	if (session->read_buf_len > 0)
		session->idle_tag = g_idle_add(session_recv_msg_idle_cb,
					       session);
	else
		session->io_tag = sock_add_watch(session->sock, G_IO_IN,
						 session_read_msg_cb, session);

	return 0;
}

static gboolean session_recv_msg_idle_cb(gpointer data)
{
	Session *session = SESSION(data);
	gboolean ret;

#if GLIB_CHECK_VERSION(2, 12, 0)
	if (g_source_is_destroyed(g_main_current_source()))
		return FALSE;
#endif

	session->idle_tag = 0;
	ret = session_read_msg_cb(session->sock, G_IO_IN, session);

	if (ret == TRUE)
		session->io_tag = sock_add_watch(session->sock, G_IO_IN,
						 session_read_msg_cb, session);

	return FALSE;
}

gint session_send_data(Session *session, FILE *data_fp, guint size)
{
	gboolean ret;

	g_return_val_if_fail(session->sock != NULL, -1);
	g_return_val_if_fail(session->write_data_fp == NULL, -1);
	g_return_val_if_fail(data_fp != NULL, -1);
	g_return_val_if_fail(size != 0, -1);

	session->state = SESSION_SEND;

	session->write_data_fp = data_fp;
	session->write_data_pos = 0;
	session->write_data_len = size;
	g_get_current_time(&session->tv_prev);

#ifdef G_OS_WIN32
	sock_set_nonblocking_mode(session->sock, FALSE);
#endif

	ret = session_write_data_cb(session->sock, G_IO_OUT, session);

	if (ret == TRUE)
#ifdef G_OS_WIN32
		session->io_tag = sock_add_watch_poll(session->sock, G_IO_OUT,
						      session_write_data_cb,
						      session);
#else
		session->io_tag = sock_add_watch(session->sock, G_IO_OUT,
						 session_write_data_cb,
						 session);
#endif
	else if (session->state == SESSION_ERROR)
		return -1;

	return 0;
}

gint session_recv_data(Session *session, guint size, const gchar *terminator)
{
	g_return_val_if_fail(session->sock != NULL, -1);
	g_return_val_if_fail(session->read_data_buf->len == 0, -1);

	session->state = SESSION_RECV;

	g_free(session->read_data_terminator);
	session->read_data_terminator = g_strdup(terminator);
	g_get_current_time(&session->tv_prev);

	if (session->read_buf_len > 0)
		session->idle_tag = g_idle_add(session_recv_data_idle_cb,
					       session);
	else
		session->io_tag = sock_add_watch(session->sock, G_IO_IN,
						 session_read_data_cb, session);

	return 0;
}

static gboolean session_recv_data_idle_cb(gpointer data)
{
	Session *session = SESSION(data);
	gboolean ret;

#if GLIB_CHECK_VERSION(2, 12, 0)
	if (g_source_is_destroyed(g_main_current_source()))
		return FALSE;
#endif

	session->idle_tag = 0;
	ret = session_read_data_cb(session->sock, G_IO_IN, session);

	if (ret == TRUE)
		session->io_tag = sock_add_watch(session->sock, G_IO_IN,
						 session_read_data_cb, session);

	return FALSE;
}

gint session_recv_data_as_file(Session *session, guint size,
			       const gchar *terminator)
{
	g_return_val_if_fail(session->sock != NULL, -1);
	g_return_val_if_fail(session->read_data_pos == 0, -1);
	g_return_val_if_fail(session->read_data_fp == NULL, -1);

	session->state = SESSION_RECV;

	g_free(session->read_data_terminator);
	session->read_data_terminator = g_strdup(terminator);
	g_get_current_time(&session->tv_prev);

	session->read_data_fp = my_tmpfile();
	if (!session->read_data_fp) {
		FILE_OP_ERROR("session_recv_data_as_file", "my_tmpfile");
		return -1;
	}

	if (session->read_buf_len > 0)
		session->idle_tag =
			g_idle_add(session_recv_data_as_file_idle_cb, session);
	else
		session->io_tag = sock_add_watch(session->sock, G_IO_IN,
						 session_read_data_as_file_cb,
						 session);

	return 0;
}

static gboolean session_recv_data_as_file_idle_cb(gpointer data)
{
	Session *session = SESSION(data);
	gboolean ret;

#if GLIB_CHECK_VERSION(2, 12, 0)
	if (g_source_is_destroyed(g_main_current_source()))
		return FALSE;
#endif

	session->idle_tag = 0;
	ret = session_read_data_as_file_cb(session->sock, G_IO_IN, session);

	if (ret == TRUE)
		session->io_tag = sock_add_watch(session->sock, G_IO_IN,
						 session_read_data_as_file_cb,
						 session);

	return FALSE;
}

static gboolean session_read_msg_cb(SockInfo *source, GIOCondition condition,
				    gpointer data)
{
	Session *session = SESSION(data);
	SessionPrivData *priv;
	gchar buf[SESSION_BUFFSIZE];
	gint line_len;
	gchar *newline;
	gchar *msg;
	gint ret;

	g_return_val_if_fail(condition == G_IO_IN, FALSE);

	if (session->read_buf_len == 0) {
		gint read_len;

		read_len = sock_read(session->sock, session->read_buf,
				     SESSION_BUFFSIZE - 1);

		if (read_len == 0) {
			g_warning("sock_read: received EOF\n");
			session->state = SESSION_EOF;
			return FALSE;
		}

		if (read_len < 0) {
			switch (errno) {
			case EAGAIN:
				return TRUE;
			default:
				g_warning("%s: sock_read: %s\n", G_STRFUNC, g_strerror(errno));
				session->state = SESSION_ERROR;
				priv = session_get_priv(session);
				priv->error_val = SESSION_ERROR_SOCKET;
				return FALSE;
			}
		}

		session->read_buf_len = read_len;
	}

	session_set_timeout(session, session->timeout_interval);

	if ((newline = memchr(session->read_buf_p, '\n', session->read_buf_len))
		!= NULL)
		line_len = newline - session->read_buf_p + 1;
	else
		line_len = session->read_buf_len;

	if (line_len == 0)
		return TRUE;

	memcpy(buf, session->read_buf_p, line_len);
	buf[line_len] = '\0';

	g_string_append(session->read_msg_buf, buf);

	session->read_buf_len -= line_len;
	if (session->read_buf_len == 0)
		session->read_buf_p = session->read_buf;
	else
		session->read_buf_p += line_len;

	/* incomplete read */
	if (buf[line_len - 1] != '\n')
		return TRUE;

	/* complete */
	if (session->io_tag > 0) {
		g_source_remove(session->io_tag);
		session->io_tag = 0;
	}

	/* callback */
	msg = g_strdup(session->read_msg_buf->str);
	strretchomp(msg);
	g_string_truncate(session->read_msg_buf, 0);

	ret = session->recv_msg(session, msg);
	if (session->recv_msg_notify)
		session->recv_msg_notify(session, msg,
					 session->recv_msg_notify_data);

	g_free(msg);

	if (ret < 0) {
		session->state = SESSION_ERROR;
		priv = session_get_priv(session);
		priv->error_val = SESSION_ERROR_SOCKET;
	}

	return FALSE;
}

static gboolean session_read_data_cb(SockInfo *source, GIOCondition condition,
				     gpointer data)
{
	Session *session = SESSION(data);
	SessionPrivData *priv;
	GByteArray *data_buf;
	gint terminator_len;
	gboolean complete = FALSE;
	guint data_len;
	gint ret;

	g_return_val_if_fail(condition == G_IO_IN, FALSE);

	if (session->read_buf_len == 0) {
		gint read_len;

		read_len = sock_read(session->sock, session->read_buf,
				     SESSION_BUFFSIZE);

		if (read_len == 0) {
			g_warning("sock_read: received EOF\n");
			session->state = SESSION_EOF;
			return FALSE;
		}

		if (read_len < 0) {
			switch (errno) {
			case EAGAIN:
				return TRUE;
			default:
				g_warning("%s: sock_read: %s\n", G_STRFUNC, g_strerror(errno));
				session->state = SESSION_ERROR;
				priv = session_get_priv(session);
				priv->error_val = SESSION_ERROR_SOCKET;
				return FALSE;
			}
		}

		session->read_buf_len = read_len;
	}

	session_set_timeout(session, session->timeout_interval);

	data_buf = session->read_data_buf;
	terminator_len = strlen(session->read_data_terminator);

	if (session->read_buf_len == 0)
		return TRUE;

	g_byte_array_append(data_buf, (guchar *)session->read_buf_p,
			    session->read_buf_len);

	session->read_buf_len = 0;
	session->read_buf_p = session->read_buf;

	/* check if data is terminated */
	if (data_buf->len >= terminator_len) {
		if (memcmp(data_buf->data, session->read_data_terminator,
			   terminator_len) == 0)
			complete = TRUE;
		else if (data_buf->len >= terminator_len + 2 &&
			 memcmp(data_buf->data + data_buf->len -
				(terminator_len + 2), "\r\n", 2) == 0 &&
			 memcmp(data_buf->data + data_buf->len -
				terminator_len, session->read_data_terminator,
				terminator_len) == 0)
			complete = TRUE;
	}

	/* incomplete read */
	if (!complete) {
		GTimeVal tv_cur;

		g_get_current_time(&tv_cur);
		if (tv_cur.tv_sec - session->tv_prev.tv_sec > 0 ||
		    tv_cur.tv_usec - session->tv_prev.tv_usec >
		    UI_REFRESH_INTERVAL) {
			if (session->recv_data_progressive_notify)
				session->recv_data_progressive_notify
					(session, data_buf->len, 0,
					 session->recv_data_progressive_notify_data);
			g_get_current_time(&session->tv_prev);
		}
		return TRUE;
	}

	/* complete */
	if (session->io_tag > 0) {
		g_source_remove(session->io_tag);
		session->io_tag = 0;
	}

	data_len = data_buf->len - terminator_len;

	/* callback */
	ret = session->recv_data_finished(session, (guchar *)data_buf->data,
					  data_len);

	g_byte_array_set_size(data_buf, 0);

	if (session->recv_data_notify)
		session->recv_data_notify(session, data_len,
					  session->recv_data_notify_data);

	if (ret < 0) {
		session->state = SESSION_ERROR;
		priv = session_get_priv(session);
		priv->error_val = SESSION_ERROR_SOCKET;
	}

	return FALSE;
}

#define READ_BUF_LEFT() \
	(SESSION_BUFFSIZE - (session->read_buf_p - session->read_buf) - \
	 session->read_buf_len)
#define PREREAD_SIZE	8

static gboolean session_read_data_as_file_cb(SockInfo *source,
					     GIOCondition condition,
					     gpointer data)
{
	Session *session = SESSION(data);
	SessionPrivData *priv;
	gint terminator_len;
	gchar *data_begin_p;
	gint buf_data_len;
	gboolean complete = FALSE;
	gint read_len;
	gint write_len;
	gint ret;

	g_return_val_if_fail(condition == G_IO_IN, FALSE);

	if (session->read_buf_len == 0) {
		read_len = sock_read(session->sock, session->read_buf_p,
				     READ_BUF_LEFT());

		if (read_len == 0) {
			g_warning("sock_read: received EOF\n");
			session->state = SESSION_EOF;
			return FALSE;
		}

		if (read_len < 0) {
			switch (errno) {
			case EAGAIN:
				return TRUE;
			default:
				g_warning("%s: sock_read: %s\n", G_STRFUNC, g_strerror(errno));
				session->state = SESSION_ERROR;
				priv = session_get_priv(session);
				priv->error_val = SESSION_ERROR_SOCKET;
				return FALSE;
			}
		}

		session->read_buf_len = read_len;
	}

	session_set_timeout(session, session->timeout_interval);

	terminator_len = strlen(session->read_data_terminator);

	if (session->read_buf_len == 0)
		return TRUE;

	/* +---------------buf_data_len---------------+
	 * +--preread_len--+-------read_buf_len-------+
	 * +---------------+--------------------------+-------------------+ *
	 * ^data_begin_p   ^read_buf_p
	 * ^read_buf
	 */

	data_begin_p = session->read_buf_p - session->preread_len;
	buf_data_len = session->preread_len + session->read_buf_len;

	/* check if data is terminated */
	if (buf_data_len >= terminator_len) {
		if (session->read_data_pos == 0 &&
		    buf_data_len == terminator_len &&
		    memcmp(data_begin_p, session->read_data_terminator,
			   terminator_len) == 0)
			complete = TRUE;
		else if (buf_data_len >= terminator_len + 2 &&
			 memcmp(data_begin_p + buf_data_len -
				(terminator_len + 2), "\r\n", 2) == 0 &&
			 memcmp(data_begin_p + buf_data_len -
				terminator_len, session->read_data_terminator,
				terminator_len) == 0)
			complete = TRUE;
	}

	/* incomplete read */
	if (!complete) {
		GTimeVal tv_cur;

		if (buf_data_len <= PREREAD_SIZE) {
			if (data_begin_p > session->read_buf) {
				g_memmove(session->read_buf, data_begin_p,
					  buf_data_len);
				data_begin_p = session->read_buf;
				session->read_buf_p = session->read_buf +
					session->preread_len;
			}
			session->read_buf_p += session->read_buf_len;
			session->preread_len = buf_data_len;
			session->read_buf_len = 0;
			return TRUE;
		}

		if (READ_BUF_LEFT() >= (SESSION_BUFFSIZE / 2)) {
			session->read_buf_p += session->read_buf_len;
			session->preread_len = buf_data_len;
			session->read_buf_len = 0;
			return TRUE;
		}

		write_len = buf_data_len - PREREAD_SIZE;
		if (fwrite(data_begin_p, write_len, 1,
			   session->read_data_fp) < 1) {
			g_warning("session_read_data_as_file_cb: "
				  "writing data to file failed\n");
			session->state = SESSION_ERROR;
			priv = session_get_priv(session);
			priv->error_val = SESSION_ERROR_IO;
			return FALSE;
		}
		session->read_data_pos += write_len;

		g_memmove(session->read_buf, data_begin_p + write_len,
			  PREREAD_SIZE);
		session->read_buf_p = session->read_buf + PREREAD_SIZE;
		session->preread_len = PREREAD_SIZE;
		session->read_buf_len = 0;

		g_get_current_time(&tv_cur);
		if (tv_cur.tv_sec - session->tv_prev.tv_sec > 0 ||
		    tv_cur.tv_usec - session->tv_prev.tv_usec >
		    UI_REFRESH_INTERVAL) {
			if (session->recv_data_progressive_notify)
				session->recv_data_progressive_notify
					(session, session->read_data_pos, 0,
					 session->recv_data_progressive_notify_data);
			g_get_current_time(&session->tv_prev);
		}

		return TRUE;
	}

	/* complete */
	if (session->io_tag > 0) {
		g_source_remove(session->io_tag);
		session->io_tag = 0;
	}

	write_len = buf_data_len - terminator_len;
	if (write_len > 0 && fwrite(data_begin_p, write_len, 1,
				    session->read_data_fp) < 1) {
		g_warning("session_read_data_as_file_cb: "
			  "writing data to file failed\n");
		session->state = SESSION_ERROR;
		priv = session_get_priv(session);
		priv->error_val = SESSION_ERROR_IO;
		return FALSE;
	}
	session->read_data_pos += write_len;

	if (fflush(session->read_data_fp) == EOF) {
		perror("fflush");
		g_warning("session_read_data_as_file_cb: "
			  "writing data to file failed\n");
		session->state = SESSION_ERROR;
		priv = session_get_priv(session);
		priv->error_val = SESSION_ERROR_IO;
		return FALSE;
	}
	rewind(session->read_data_fp);

	session->preread_len = 0;
	session->read_buf_len = 0;
	session->read_buf_p = session->read_buf;

	/* callback */
	ret = session->recv_data_as_file_finished
		(session, session->read_data_fp, session->read_data_pos);

	fclose(session->read_data_fp);
	session->read_data_fp = NULL;

	if (session->recv_data_notify)
		session->recv_data_notify(session, session->read_data_pos,
					  session->recv_data_notify_data);

	session->read_data_pos = 0;

	if (ret < 0) {
		session->state = SESSION_ERROR;
		priv = session_get_priv(session);
		priv->error_val = SESSION_ERROR_IO;
	}

	return FALSE;
}

static gint session_write_buf(Session *session)
{
	gint write_len;
	gint to_write_len;
	SessionPrivData *priv;

	g_return_val_if_fail(session->write_buf != NULL, -1);
	g_return_val_if_fail(session->write_buf_p != NULL, -1);
	g_return_val_if_fail(session->write_buf_len > 0, -1);

	to_write_len = session->write_buf_len -
		(session->write_buf_p - session->write_buf);
	to_write_len = MIN(to_write_len, SESSION_BUFFSIZE);

	write_len = sock_write(session->sock, session->write_buf_p,
			       to_write_len);

	if (write_len < 0) {
		switch (errno) {
		case EAGAIN:
			write_len = 0;
			break;
		default:
			g_warning("sock_write: %s\n", g_strerror(errno));
			session->state = SESSION_ERROR;
			priv = session_get_priv(session);
			priv->error_val = SESSION_ERROR_SOCKET;
			return -1;
		}
	}

	/* incomplete write */
	if (session->write_buf_p - session->write_buf + write_len <
	    session->write_buf_len) {
		session->write_buf_p += write_len;
		return 1;
	}

	g_free(session->write_buf);
	session->write_buf = NULL;
	session->write_buf_p = NULL;
	session->write_buf_len = 0;

	return 0;
}

#define WRITE_DATA_BUFFSIZE	8192

static gint session_write_data(Session *session, gint *nwritten)
{
	gchar buf[WRITE_DATA_BUFFSIZE];
	gint write_len;
	gint to_write_len;
	SessionPrivData *priv;

	g_return_val_if_fail(session->write_data_fp != NULL, -1);
	g_return_val_if_fail(session->write_data_pos >= 0, -1);
	g_return_val_if_fail(session->write_data_len > 0, -1);

	to_write_len = session->write_data_len - session->write_data_pos;
	to_write_len = MIN(to_write_len, WRITE_DATA_BUFFSIZE);
	if (fread(buf, to_write_len, 1, session->write_data_fp) < 1) {
		g_warning("session_write_data: reading data from file failed\n");
		session->state = SESSION_ERROR;
		priv = session_get_priv(session);
		priv->error_val = SESSION_ERROR_IO;
		return -1;
	}

	write_len = sock_write(session->sock, buf, to_write_len);

	if (write_len < 0) {
		switch (errno) {
		case EAGAIN:
			write_len = 0;
			break;
		default:
			g_warning("sock_write: %s\n", g_strerror(errno));
			session->state = SESSION_ERROR;
			priv = session_get_priv(session);
			priv->error_val = SESSION_ERROR_SOCKET;
			*nwritten = write_len;
			return -1;
		}
	}

	*nwritten = write_len;

	/* incomplete write */
	if (session->write_data_pos + write_len < session->write_data_len) {
		session->write_data_pos += write_len;
		if (write_len < to_write_len) {
			if (fseek(session->write_data_fp,
				  session->write_data_pos, SEEK_SET) < 0) {
				g_warning("session_write_data: file seek failed\n");
				session->state = SESSION_ERROR;
				priv = session_get_priv(session);
				priv->error_val = SESSION_ERROR_IO;
				return -1;
			}
		}
		return 1;
	}

	session->write_data_fp = NULL;
	session->write_data_pos = 0;
	session->write_data_len = 0;

	return 0;
}

static gboolean session_write_msg_cb(SockInfo *source, GIOCondition condition,
				     gpointer data)
{
	Session *session = SESSION(data);
	SessionPrivData *priv;
	gint ret;

	g_return_val_if_fail(condition == G_IO_OUT, FALSE);
	g_return_val_if_fail(session->write_buf != NULL, FALSE);
	g_return_val_if_fail(session->write_buf_p != NULL, FALSE);
	g_return_val_if_fail(session->write_buf_len > 0, FALSE);

	ret = session_write_buf(session);

	if (ret < 0) {
		session->state = SESSION_ERROR;
		priv = session_get_priv(session);
		if (priv->error_val == SESSION_ERROR_OK)
			priv->error_val = SESSION_ERROR_IO;
		return FALSE;
	} else if (ret > 0)
		return TRUE;

	if (session->io_tag > 0) {
		g_source_remove(session->io_tag);
		session->io_tag = 0;
	}

	session_recv_msg(session);

	return FALSE;
}

static gboolean session_write_data_cb(SockInfo *source,
				      GIOCondition condition, gpointer data)
{
	Session *session = SESSION(data);
	SessionPrivData *priv;
	guint write_data_len;
	gint write_len;
	gint ret;

	g_return_val_if_fail(condition == G_IO_OUT, FALSE);
	g_return_val_if_fail(session->write_data_fp != NULL, FALSE);
	g_return_val_if_fail(session->write_data_pos >= 0, FALSE);
	g_return_val_if_fail(session->write_data_len > 0, FALSE);

	write_data_len = session->write_data_len;

	ret = session_write_data(session, &write_len);

	if (ret < 0) {
		session->state = SESSION_ERROR;
		priv = session_get_priv(session);
		if (priv->error_val == SESSION_ERROR_OK)
			priv->error_val = SESSION_ERROR_IO;
		return FALSE;
	} else if (ret > 0) {
		GTimeVal tv_cur;

		g_get_current_time(&tv_cur);
		if (tv_cur.tv_sec - session->tv_prev.tv_sec > 0 ||
		    tv_cur.tv_usec - session->tv_prev.tv_usec >
		    UI_REFRESH_INTERVAL) {
			session_set_timeout(session, session->timeout_interval);
			if (session->send_data_progressive_notify)
				session->send_data_progressive_notify
					(session,
					 session->write_data_pos,
					 write_data_len,
					 session->send_data_progressive_notify_data);
			g_get_current_time(&session->tv_prev);
		}
		return TRUE;
	}

	if (session->io_tag > 0) {
		g_source_remove(session->io_tag);
		session->io_tag = 0;
	}

	/* callback */
	ret = session->send_data_finished(session, write_data_len);
	if (session->send_data_notify)
		session->send_data_notify(session, write_data_len,
					  session->send_data_notify_data);

#ifdef G_OS_WIN32
	sock_set_nonblocking_mode(session->sock, session->nonblocking);
#endif

	return FALSE;
}
