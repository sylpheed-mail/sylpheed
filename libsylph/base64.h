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

#ifndef __BASE64_H__
#define __BASE64_H__

#include <glib.h>

typedef struct _Base64Decoder	Base64Decoder;

struct _Base64Decoder
{
	gint buf_len;
	gchar buf[4];
};

void base64_encode	(gchar		*out,
			 const guchar	*in,
			 gint		 inlen);
gint base64_decode	(guchar		*out,
			 const gchar	*in,
			 gint		 inlen);

Base64Decoder *base64_decoder_new	(void);
void	       base64_decoder_free	(Base64Decoder	*decoder);
gint	       base64_decoder_decode	(Base64Decoder	*decoder,
					 const gchar	*in,
					 guchar		*out);

#endif /* __BASE64_H__ */
