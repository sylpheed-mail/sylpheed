/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2001 Hiroyuki Yamamoto
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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "intl.h"
#include "recv.h"
#include "socket.h"
#include "utils.h"

static RecvUIFunc	recv_ui_func;
static gpointer		recv_ui_func_data;

gint recv_write_to_file(SockInfo *sock, const gchar *filename)
{
	FILE *fp;
	gint ret;

	g_return_val_if_fail(filename != NULL, -1);

	if ((fp = fopen(filename, "wb")) == NULL) {
		FILE_OP_ERROR(filename, "fopen");
		recv_write(sock, NULL);
		return -1;
	}

	if (change_file_mode_rw(fp, filename) < 0)
		FILE_OP_ERROR(filename, "chmod");

	if ((ret = recv_write(sock, fp)) < 0) {
		fclose(fp);
		unlink(filename);
		return ret;
	}

	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(filename, "fclose");
		unlink(filename);
		return -1;
	}

	return 0;
}

gint recv_bytes_write_to_file(SockInfo *sock, glong size, const gchar *filename)
{
	FILE *fp;
	gint ret;

	g_return_val_if_fail(filename != NULL, -1);

	if ((fp = fopen(filename, "wb")) == NULL) {
		FILE_OP_ERROR(filename, "fopen");
		recv_write(sock, NULL);
		return -1;
	}

	if (change_file_mode_rw(fp, filename) < 0)
		FILE_OP_ERROR(filename, "chmod");

	if ((ret = recv_bytes_write(sock, size, fp)) < 0) {
		fclose(fp);
		unlink(filename);
		return ret;
	}

	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(filename, "fclose");
		unlink(filename);
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
	struct timeval tv_prev, tv_cur;

	gettimeofday(&tv_prev, NULL);

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
			gettimeofday(&tv_cur, NULL);
			/* if elapsed time from previous update is greater
			   than 50msec, update UI */
			if (tv_cur.tv_sec - tv_prev.tv_sec > 0 ||
			    tv_cur.tv_usec - tv_prev.tv_usec > UI_REFRESH_INTERVAL) {
				gboolean ret;
				ret = recv_ui_func(sock, count, bytes,
						   recv_ui_func_data);
				if (ret == FALSE) return -1;
				gettimeofday(&tv_prev, NULL);
			}
		}

		if (len > 1 && buf[len - 1] == '\n' && buf[len - 2] == '\r') {
			buf[len - 2] = '\n';
			buf[len - 1] = '\0';
			len--;
		}

		if (buf[0] == '.' && buf[1] == '.')
			memmove(buf, buf + 1, len--);

		if (!strncmp(buf, ">From ", 6))
			memmove(buf, buf + 1, len--);

		if (fp && fputs(buf, fp) == EOF) {
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
	glong count = 0;
	gchar *prev, *cur;

	if (size == 0)
		return 0;

	buf = g_malloc(size);

	do {
		gint read_count;

		read_count = sock_read(sock, buf + count, size - count);
		if (read_count < 0) {
			g_free(buf);
			return -2;
		}
		count += read_count;
	} while (count < size);

	/* +------------------+----------------+--------------------------+ *
	 * ^buf               ^prev            ^cur             buf+size-1^ */

	prev = buf;
	while ((cur = memchr(prev, '\r', size - (prev - buf))) != NULL) {
		if (cur == buf + size - 1) break;

		if (fwrite(prev, sizeof(gchar), cur - prev, fp) == EOF ||
		    fwrite("\n", sizeof(gchar), 1, fp) == EOF) {
			perror("fwrite");
			g_warning(_("Can't write to file.\n"));
			g_free(buf);
			return -1;
		}

		if (*(cur + 1) == '\n')
			prev = cur + 2;
		else
			prev = cur + 1;

		if (prev - buf >= size) break;
	}

	if (prev - buf < size && fwrite(buf, sizeof(gchar),
					size - (prev - buf), fp) == EOF) {
		perror("fwrite");
		g_warning(_("Can't write to file.\n"));
		g_free(buf);
		return -1;
	}

	g_free(buf);
	return 0;
}

void recv_set_ui_func(RecvUIFunc func, gpointer data)
{
	recv_ui_func = func;
	recv_ui_func_data = data;
}
