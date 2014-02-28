/*
 * LibSylph -- E-Mail client library
 * Copyright (C) 1999-2014 Hiroyuki Yamamoto
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

#include <glib.h>

#ifdef G_OS_WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#endif

#include "socket.h"
#include "socks.h"
#include "utils.h"


SocksInfo *socks_info_new(SocksType type, const gchar *proxy_host,
			  gushort proxy_port, const gchar *proxy_name,
			  const gchar *proxy_pass)
{
	SocksInfo *socks_info;

	socks_info = g_new0(SocksInfo, 1);
	socks_info->type = type;
	socks_info->proxy_host = g_strdup(proxy_host);
	socks_info->proxy_port = proxy_port;
	socks_info->proxy_name = g_strdup(proxy_name);
	socks_info->proxy_pass = g_strdup(proxy_pass);

	return socks_info;
}

void socks_info_free(SocksInfo *socks_info)
{
	if (!socks_info)
		return;
	g_free(socks_info->proxy_host);
	g_free(socks_info->proxy_name);
	g_free(socks_info->proxy_pass);
	g_free(socks_info);
}

gint socks_connect(SockInfo *sock, const gchar *hostname, gushort port,
		   SocksInfo *socks_info)
{
	g_return_val_if_fail(sock != NULL, -1);
	g_return_val_if_fail(hostname != NULL, -1);
	g_return_val_if_fail(socks_info != NULL, -1);

	debug_print("socks_connect: connect to %s:%u via %s:%u\n",
		    hostname, port,
		    socks_info->proxy_host, socks_info->proxy_port);

	if (socks_info->type == SOCKS_SOCKS5)
		return socks5_connect(sock, hostname, port,
				      socks_info->proxy_name,
				      socks_info->proxy_pass);
	else if (socks_info->type == SOCKS_SOCKS4)
		return socks4_connect(sock, hostname, port);
	else
		g_warning("socks_connect: unknown SOCKS type: %d\n",
			  socks_info->type);

	return -1;
}

gint socks4_connect(SockInfo *sock, const gchar *hostname, gushort port)
{
	guchar socks_req[1024];
	struct hostent *hp;

	g_return_val_if_fail(sock != NULL, -1);
	g_return_val_if_fail(hostname != NULL, -1);

	debug_print("socks4_connect: connect to %s:%u\n", hostname, port);

	socks_req[0] = 4;
	socks_req[1] = 1;
	*((gushort *)(socks_req + 2)) = htons(port);

	/* lookup */
	if ((hp = my_gethostbyname(hostname)) == NULL) {
		g_warning("socks4_connect: cannot lookup host: %s", hostname);
		return -1;
	}
	if (hp->h_length != 4) {
		g_warning("socks4_connect: invalid address length for host: %s", hostname);
		return -1;
	}
	memcpy(socks_req + 4, (guchar *)hp->h_addr, 4);

	/* userid (empty) */
	socks_req[8] = 0;

	if (sock_write_all(sock, (gchar *)socks_req, 9) != 9) {
		g_warning("socks4_connect: SOCKS4 initial request write failed");
		return -1;
	}

	if (sock_read(sock, (gchar *)socks_req, 8) != 8) {
		g_warning("socks4_connect: SOCKS4 response read failed");
		return -1;
	}
	if (socks_req[0] != 0) {
		g_warning("socks4_connect: SOCKS4 response has invalid version");
		return -1;
	}
	if (socks_req[1] != 90) {
		g_warning("socks4_connect: SOCKS4 connection to %u.%u.%u.%u:%u failed. (%u)", socks_req[4], socks_req[5], socks_req[6], socks_req[7], ntohs(*(gushort *)(socks_req + 2)), socks_req[1]);
		return -1;
	}

	/* replace sock->hostname with endpoint */
	if (sock->hostname != hostname) {
		g_free(sock->hostname);
		sock->hostname = g_strdup(hostname);
		sock->port = port;
	}

	debug_print("socks4_connect: SOCKS4 connection to %s:%u successful.\n", hostname, port);

	return 0;
}

gint socks5_connect(SockInfo *sock, const gchar *hostname, gushort port,
		    const gchar *proxy_name, const gchar *proxy_pass)
{
	guchar socks_req[1024];
	size_t len;
	size_t size;

	g_return_val_if_fail(sock != NULL, -1);
	g_return_val_if_fail(hostname != NULL, -1);

	debug_print("socks5_connect: connect to %s:%u\n", hostname, port);

	len = strlen(hostname);
	if (len > 255) {
		g_warning("socks5_connect: hostname too long");
		return -1;
	}

	socks_req[0] = 5;
	socks_req[1] = proxy_name ? 2 : 1;
	socks_req[2] = 0;
	socks_req[3] = 2;

	if (sock_write_all(sock, (gchar *)socks_req, 2 + socks_req[1]) != 2 + socks_req[1]) {
		g_warning("socks5_connect: SOCKS5 initial request write failed");
		return -1;
	}

	if (sock_read(sock, (gchar *)socks_req, 2) != 2) {
		g_warning("socks5_connect: SOCKS5 response read failed");
		return -1;
	}
	if (socks_req[0] != 5) {
		g_warning("socks5_connect: SOCKS5 response has invalid version");
		return -1;
	}
	if (socks_req[1] == 2) {
		/* auth */
		size_t userlen, passlen;
		gint reqlen;

		if (proxy_name && proxy_pass) {
			userlen = strlen(proxy_name);
			passlen = strlen(proxy_pass);
		} else
			userlen = passlen = 0;

		socks_req[0] = 1;
		socks_req[1] = (guchar)userlen;
		if (proxy_name && userlen > 0)
			memcpy(socks_req + 2, proxy_name, userlen);
		socks_req[2 + userlen] = (guchar)passlen;
		if (proxy_pass && passlen > 0)
			memcpy(socks_req + 2 + userlen + 1, proxy_pass, passlen);

		reqlen = 2 + userlen + 1 + passlen;
		if (sock_write_all(sock, (gchar *)socks_req, reqlen) != reqlen) {
			g_warning("socks5_connect: SOCKS5 auth write failed");
			return -1;
		}
		if (sock_read(sock, (gchar *)socks_req, 2) != 2) { 
			g_warning("socks5_connect: SOCKS5 auth response read failed");
			return -1;
		}
		if (socks_req[1] != 0) {
			g_warning("socks5_connect: SOCKS5 authentication failed: user: %s (%u %u)", proxy_name ? proxy_name : "(none)", socks_req[0], socks_req[1]);
			return -1;
		}
	} else if (socks_req[1] != 0) {
		g_warning("socks5_connect: SOCKS5 reply (%u) error", socks_req[1]);
		return -1;
	}

	socks_req[0] = 5;
	socks_req[1] = 1;
	socks_req[2] = 0;

	socks_req[3] = 3;
	socks_req[4] = (guchar)len;
	memcpy(socks_req + 5, hostname, len);
	*((gushort *)(socks_req + 5 + len)) = htons(port);

	if (sock_write_all(sock, (gchar *)socks_req, 5 + len + 2) != 5 + len + 2) {
		g_warning("socks5_connect: SOCKS5 connect request write failed");
		return -1;
	}

	if (sock_read(sock, (gchar *)socks_req, 10) != 10) {
		g_warning("socks5_connect: SOCKS5 connect request response read failed");
		return -1;
	}
	if (socks_req[0] != 5) {
		g_warning("socks5_connect: SOCKS5 response has invalid version");
		return -1;
	}
	if (socks_req[1] != 0) {
		g_warning("socks5_connect: SOCKS5 connection to %u.%u.%u.%u:%u failed. (%u)", socks_req[4], socks_req[5], socks_req[6], socks_req[7], ntohs(*(gushort *)(socks_req + 8)), socks_req[1]);
		return -1;
	}

	size = 10;
	if (socks_req[3] == 3)
		size = 5 + socks_req[4] + 2;
	else if (socks_req[3] == 4)
		size = 4 + 16 + 2;
	if (size > 10) {
		size -= 10;
		if (sock_read(sock, (gchar *)socks_req + 10, size) != size) {
			g_warning("socks5_connect: SOCKS5 connect request response read failed");
			return -1;
		}
	}

	/* replace sock->hostname with endpoint */
	if (sock->hostname != hostname) {
		g_free(sock->hostname);
		sock->hostname = g_strdup(hostname);
		sock->port = port;
	}

	debug_print("socks5_connect: SOCKS5 connection to %s:%u successful.\n", hostname, port);

	return 0;
}
