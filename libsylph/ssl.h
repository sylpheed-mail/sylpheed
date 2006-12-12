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

#ifndef __SSL_H__
#define __SSL_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#if USE_SSL

#include <glib.h>
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "socket.h"

typedef enum {
	SSL_METHOD_SSLv23,
	SSL_METHOD_TLSv1
} SSLMethod;

typedef enum {
	SSL_NONE,
	SSL_TUNNEL,
	SSL_STARTTLS
} SSLType;

typedef gint (*SSLVerifyFunc)		(SockInfo	*sockinfo,
					 const gchar	*hostname,
					 X509		*server_cert,
					 glong		 verify_result);

void ssl_init				(void);
void ssl_done				(void);
gboolean ssl_init_socket		(SockInfo	*sockinfo);
gboolean ssl_init_socket_with_method	(SockInfo	*sockinfo,
					 SSLMethod	 method);
void ssl_done_socket			(SockInfo	*sockinfo);

void ssl_set_verify_func		(SSLVerifyFunc	 func);

#endif /* USE_SSL */

#endif /* __SSL_H__ */
