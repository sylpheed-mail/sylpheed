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

#ifndef __RECV_H__
#define __RECV_H__

#include <glib.h>

#include "socket.h"

typedef gboolean (*RecvUIFunc)	(SockInfo	*sock,
				 gint		 count,
				 gint		 read_bytes,
				 gpointer	 data);

gchar *recv_bytes		(SockInfo	*sock,
				 glong		 size);

gint recv_write_to_file		(SockInfo	*sock,
				 const gchar	*filename);
gint recv_bytes_write_to_file	(SockInfo	*sock,
				 glong		 size,
				 const gchar	*filename);
gint recv_write			(SockInfo	*sock,
				 FILE		*fp);
gint recv_bytes_write		(SockInfo	*sock,
				 glong		 size,
				 FILE		*fp);

void recv_set_ui_func		(RecvUIFunc	 func,
				 gpointer	 data);

#endif /* __RECV_H__ */
