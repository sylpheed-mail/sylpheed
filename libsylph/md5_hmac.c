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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <string.h>

#include "md5.h"
#include "md5_hmac.h"

/*
** Function: md5_hmac_get
** taken from the file rfc2104.txt
** originally written by Martin Schaaf <mascha@ma-scha.de>
** rewritten by Hiroyuki Yamamoto <hiro-y@kcn.ne.jp>
*/
static SMD5*
md5_hmac_get(const guchar *text, gint text_len,
	     const guchar *key, gint key_len)
{
	SMD5 *md5;
	guchar k_ipad[64];    /* inner padding -
			       * key XORd with ipad
			       */
	guchar k_opad[64];    /* outer padding -
			       * key XORd with opad
			       */
	guchar digest[S_GNET_MD5_HASH_LENGTH];
	gint i;

	/* start out by storing key in pads */
	memset(k_ipad, 0, sizeof k_ipad);
	memset(k_opad, 0, sizeof k_opad);

	if (key_len > 64) {
		/* if key is longer than 64 bytes reset it to key=MD5(key) */
		SMD5 *tmd5;

		tmd5 = s_gnet_md5_new(key, key_len);
		memcpy(k_ipad, s_gnet_md5_get_digest(tmd5),
		       S_GNET_MD5_HASH_LENGTH);
		memcpy(k_opad, s_gnet_md5_get_digest(tmd5),
		       S_GNET_MD5_HASH_LENGTH);
		s_gnet_md5_delete(tmd5);
	} else {
		memcpy(k_ipad, key, key_len);
		memcpy(k_opad, key, key_len);
	}

	/*
	 * the HMAC_MD5 transform looks like:
	 *
	 * MD5(K XOR opad, MD5(K XOR ipad, text))
	 *
	 * where K is an n byte key
	 * ipad is the byte 0x36 repeated 64 times
	 * opad is the byte 0x5c repeated 64 times
	 * and text is the data being protected
	 */


	/* XOR key with ipad and opad values */
	for (i = 0; i < 64; i++) {
		k_ipad[i] ^= 0x36;
		k_opad[i] ^= 0x5c;
	}

	/*
	 * perform inner MD5
	 */
	md5 = s_gnet_md5_new_incremental();	/* init context for 1st
						 * pass */
	s_gnet_md5_update(md5, k_ipad, 64);	/* start with inner pad */
	s_gnet_md5_update(md5, text, text_len);	/* then text of datagram */
	s_gnet_md5_final(md5);			/* finish up 1st pass */
	memcpy(digest, s_gnet_md5_get_digest(md5), S_GNET_MD5_HASH_LENGTH);
	s_gnet_md5_delete(md5);

	/*
	 * perform outer MD5
	 */
	md5 = s_gnet_md5_new_incremental();	/* init context for 2nd
						 * pass */
	s_gnet_md5_update(md5, k_opad, 64);	/* start with outer pad */
	s_gnet_md5_update(md5, digest, 16);	/* then results of 1st
						 * hash */
	s_gnet_md5_final(md5);			/* finish up 2nd pass */

	return md5;
}

void
md5_hmac(guchar *digest,
	 const guchar *text, gint text_len,
	 const guchar *key, gint key_len)
{
	SMD5 *md5;

	md5 = md5_hmac_get(text, text_len, key, key_len);
	memcpy(digest, s_gnet_md5_get_digest(md5), S_GNET_MD5_HASH_LENGTH);
	s_gnet_md5_delete(md5);
}

void
md5_hex_hmac(gchar *hexdigest,
	     const guchar *text, gint text_len,
	     const guchar *key, gint key_len)
{
	SMD5 *md5;

	md5 = md5_hmac_get(text, text_len, key, key_len);
	s_gnet_md5_copy_string(md5, hexdigest);
	hexdigest[S_GNET_MD5_HASH_LENGTH * 2] = '\0';
	s_gnet_md5_delete(md5);
}
