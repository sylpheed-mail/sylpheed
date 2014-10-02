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

#ifndef __SOCKET_H__
#define __SOCKET_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#if HAVE_NETDB_H
#  include <netdb.h>
#endif
#ifdef G_OS_WIN32
#  include <winsock2.h>
#endif

typedef struct _SockInfo	SockInfo;

#if USE_SSL
#  include "ssl.h"
#endif

typedef enum
{
	CONN_READY,
	CONN_LOOKUPSUCCESS,
	CONN_ESTABLISHED,
	CONN_LOOKUPFAILED,
	CONN_FAILED
} ConnectionState;

typedef enum
{
	SYL_SOCK_NONBLOCK	= 1 << 0,
	SYL_SOCK_CHECK_IO	= 1 << 1
} SockFlags;

#define SOCK_SET_FLAGS(flags, set)	{ (flags) |= (set); }
#define SOCK_UNSET_FLAGS(flags, set)	{ (flags) &= ~(set); }
#define SOCK_IS_NONBLOCK(flags)		((flags & SYL_SOCK_NONBLOCK) != 0)
#define SOCK_IS_CHECK_IO(flags)		((flags & SYL_SOCK_CHECK_IO) != 0)

typedef gint (*SockConnectFunc)		(SockInfo	*sock,
					 gpointer	 data);
typedef gboolean (*SockFunc)		(SockInfo	*sock,
					 GIOCondition	 condition,
					 gpointer	 data);

struct _SockInfo
{
	gint sock;
#if USE_SSL
	SSL *ssl;
#else
	gpointer ssl;
#endif
	GIOChannel *sock_ch;

	gchar *hostname;
	gushort port;
	ConnectionState state;
	SockFlags flags;
	gpointer data;

	SockFunc callback;
	GIOCondition condition;
};

gint sock_init				(void);
gint sock_cleanup			(void);

gint sock_set_io_timeout		(guint sec);

SockInfo *sock_new			(const gchar *hostname, gushort port);

gint sock_set_nonblocking_mode		(SockInfo *sock, gboolean nonblock);
gboolean sock_is_nonblocking_mode	(SockInfo *sock);

gboolean sock_has_read_data		(SockInfo *sock);

guint sock_add_watch			(SockInfo *sock, GIOCondition condition,
					 SockFunc func, gpointer data);
guint sock_add_watch_poll		(SockInfo *sock, GIOCondition condition,
					 SockFunc func, gpointer data);

struct hostent *my_gethostbyname	(const gchar *hostname);

SockInfo *sock_connect			(const gchar *hostname, gushort port);
#ifdef G_OS_UNIX
gint sock_connect_async			(const gchar *hostname, gushort port,
					 SockConnectFunc func, gpointer data);
gint sock_connect_async_cancel		(gint id);
#endif
#if USE_THREADS
gint sock_connect_async_thread		(const gchar *hostname, gushort port);
gint sock_connect_async_thread_wait	(gint id, SockInfo **sock);
#endif

gint sock_info_connect			(SockInfo *sock);
#ifdef G_OS_UNIX
gint sock_info_connect_async		(SockInfo *sock,
					 SockConnectFunc func, gpointer data);
#endif
#if USE_THREADS
gint sock_info_connect_async_thread	(SockInfo *sock);
gint sock_info_connect_async_thread_wait(gint id, SockInfo **sock);
#endif

/* Basic I/O functions */
gint sock_printf	(SockInfo *sock, const gchar *format, ...)
			 G_GNUC_PRINTF(2, 3);
gint sock_read		(SockInfo *sock, gchar *buf, gint len);
gint sock_write		(SockInfo *sock, const gchar *buf, gint len);
gint sock_write_all	(SockInfo *sock, const gchar *buf, gint len);
gint sock_gets		(SockInfo *sock, gchar *buf, gint len);
gint sock_getline	(SockInfo *sock, gchar **line);
gint sock_puts		(SockInfo *sock, const gchar *buf);
gint sock_peek		(SockInfo *sock, gchar *buf, gint len);
gint sock_close		(SockInfo *sock);

/* Functions to directly work on FD.  They are needed for pipes */
gint fd_connect_inet	(gushort port);
gint fd_open_inet	(gushort port);
gint fd_connect_unix	(const gchar *path);
gint fd_open_unix	(const gchar *path);
gint fd_accept		(gint sock);

gint fd_read		(gint sock, gchar *buf, gint len);
gint fd_write		(gint sock, const gchar *buf, gint len);
gint fd_write_all	(gint sock, const gchar *buf, gint len);
gint fd_gets		(gint sock, gchar *buf, gint len);
gint fd_getline		(gint sock, gchar **line);
gint fd_close		(gint sock);

/* Functions for SSL */
#if USE_SSL
gint ssl_read		(SSL *ssl, gchar *buf, gint len);
gint ssl_write		(SSL *ssl, const gchar *buf, gint len);
gint ssl_write_all	(SSL *ssl, const gchar *buf, gint len);
gint ssl_gets		(SSL *ssl, gchar *buf, gint len);
gint ssl_getline	(SSL *ssl, gchar **line);
gint ssl_peek		(SSL *ssl, gchar *buf, gint len);
#endif

#endif /* __SOCKET_H__ */
