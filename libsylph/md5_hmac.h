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

#ifndef __MD5_HMAC_H__
#define __MD5_HMAC_H__

#include <glib.h>

void md5_hmac		(guchar *digest,
			 const guchar* text, gint text_len,
			 const guchar* key, gint key_len);
void md5_hex_hmac	(gchar *hexdigest,
			 const guchar* text, gint text_len,
			 const guchar* key, gint key_len);

#endif /* __MD5_HMAC_H__ */

