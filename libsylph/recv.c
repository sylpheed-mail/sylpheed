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

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "recv.h"
#include "socket.h"
#include "utils.h"

static RecvUIFunc	recv_ui_func;
static gpointer		recv_ui_func_data;


gchar *recv_bytes(SockInfo *sock, glong size)
{
	gchar *buf;
	glong count = 0;

	if (size == 0)
		return NULL;

	buf = g_malloc(size + 1);

	do {
		gint read_count;

		read_count = sock_read(sock, buf + count,
				       MIN(BUFFSIZE, size - count));
		if (read_count <= 0) {
			g_free(buf);
			return NULL;
		}
		count += read_count;
	} while (count < size);

	buf[size] = '\0';

	return buf;
}

gint recv_write_to_file(SockInfo *sock, const gchar *filename)
{
	FILE *fp;
	gint ret;

	g_return_val_if_fail(filename != NULL, -1);

	if ((fp = g_fopen(filename, "wb")) == NULL) {
		FILE_OP_ERROR(filename, "fopen");
		recv_write(sock, NULL);
		return -1;
	}

	if (change_file_mode_rw(fp, filename) < 0)
		FILE_OP_ERROR(filename, "chmod");

	if ((ret = recv_write(sock, fp)) < 0) {
		fclose(fp);
		g_unlink(filename);
		return ret;
	}

	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(filename, "fclose");
		g_unlink(filename);
		return -1;
	}

	return 0;
}

gint recv_bytes_write_to_file(SockInfo *sock, glong size, const gchar *filename)
{
	FILE *fp;
	gint ret;

	g_return_val_if_fail(filename != NULL, -1);

	if ((fp = g_fopen(filename, "wb")) == NULL) {
		FILE_OP_ERROR(filename, "fopen");
		return recv_bytes_write(sock, size, NULL);
	}

	if (change_file_mode_rw(fp, filename) < 0)
		FILE_OP_ERROR(filename, "chmod");

	if ((ret = recv_bytes_write(sock, size, fp)) < 0) {
		fclose(fp);
		g_unlink(filename);
		return ret;
	}

	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(filename, "fclose");
		g_unlink(filename);
		return -1;
	}

	return 0;
}

gint recv_write(SockInfo *sock, FILE *fp)
{
	gchar buf[BUFFSIZE];
	gint len;
	gint count = 0;
	gint bytes = 0;
	gchar *p;
	GTimeVal tv_prev, tv_cur;

	g_get_current_time(&tv_prev);

	for (;;) {
		if (sock_gets(sock, buf, sizeof(buf)) < 0) {
			g_warning(_("error occurred while retrieving data.\n"));
			return -2;
		}

		len = strlen(buf);
		if (len > 1 && buf[0] == '.' && buf[1] == '\r') {
			if (recv_ui_func)
				recv_ui_func(sock, count, bytes,
					     recv_ui_func_data);
			break;
		}
		count++;
		bytes += len;

		if (recv_ui_func) {
			g_get_current_time(&tv_cur);
			/* if elapsed time from previous update is greater
			   than 50msec, update UI */
			if (tv_cur.tv_sec - tv_prev.tv_sec > 0 ||
			    tv_cur.tv_usec - tv_prev.tv_usec > UI_REFRESH_INTERVAL) {
				gboolean ret;
				ret = recv_ui_func(sock, count, bytes,
						   recv_ui_func_data);
				if (ret == FALSE) return -1;
				g_get_current_time(&tv_prev);
			}
		}

		p = buf;

		if (len > 1 && buf[len - 1] == '\n' && buf[len - 2] == '\r') {
			buf[len - 2] = '\n';
			buf[len - 1] = '\0';
		}

		if (buf[0] == '.' && buf[1] == '.')
			p++;
		else if (!strncmp(buf, ">From ", 6))
			p++;

		if (fp && fputs(p, fp) == EOF) {
			perror("fputs");
			g_warning(_("Can't write to file.\n"));
			fp = NULL;
		}
	}

	if (!fp) return -1;

	return 0;
}

gint recv_bytes_write(SockInfo *sock, glong size, FILE *fp)
{
	gchar *buf;
	gchar *prev, *cur;

	if (size == 0)
		return 0;

	buf = recv_bytes(sock, size);
	if (!buf)
		return -2;

	/* +------------------+----------------+--------------------------+ *
	 * ^buf               ^prev            ^cur             buf+size-1^ */

	prev = buf;
	while ((cur = memchr(prev, '\r', size - (prev - buf))) != NULL) {
		if (cur == buf + size - 1) break;

		if (fp && (fwrite(prev, sizeof(gchar), cur - prev, fp) == EOF ||
		           fwrite("\n", sizeof(gchar), 1, fp) == EOF)) {
			perror("fwrite");
			g_warning(_("Can't write to file.\n"));
			fp = NULL;
		}

		if (*(cur + 1) == '\n')
			prev = cur + 2;
		else
			prev = cur + 1;

		if (prev - buf >= size) break;
	}

	if (prev - buf < size && fp &&
	    fwrite(prev, sizeof(gchar), size - (prev - buf), fp) == EOF) {
		perror("fwrite");
		g_warning(_("Can't write to file.\n"));
		fp = NULL;
	}

	g_free(buf);

	if (!fp) return -1;

	return 0;
}

void recv_set_ui_func(RecvUIFunc func, gpointer data)
{
	recv_ui_func = func;
	recv_ui_func_data = data;
}
