/* md5.h - MD5 Message-Digest Algorithm
 *	Copyright (C) 1995, 1996, 1998, 1999 Free Software Foundation, Inc.
 *
 * according to the definition of MD5 in RFC 1321 from April 1992.
 * NOTE: This is *not* the same file as the one from glibc
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _MD5_HDR_
#define _MD5_HDR_

#include "utils.h"

typedef struct {  /* Hmm, should be private */
    u32 A,B,C,D;
    u32  nblocks;
    unsigned char buf[64];
    int  count;
    int  finalized;
} MD5_CONTEXT;

void md5_init(MD5_CONTEXT *ctx);
void md5_update(MD5_CONTEXT *hd, const unsigned char *inbuf, size_t inlen);
void md5_final(unsigned char *digest, MD5_CONTEXT *ctx);

void md5_hex_digest(char *hexdigest, const unsigned char *s);

void md5_hmac(unsigned char *digest,
              const unsigned char* text, int text_len,
              const unsigned char* key, int key_len);
void md5_hex_hmac(char *hexdigest,
                  const unsigned char* text, int text_len,
                  const unsigned char* key, int key_len);

#endif /* _MD5_HDR_ */

