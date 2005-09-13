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

#include <ctype.h>

#define UUDECODE(c) (c=='`' ? 0 : c - ' ')
#define N64(i) (i & ~63)

const char uudigit[64] =
{
  '`', '!', '"', '#', '$', '%', '&', '\'',
  '(', ')', '*', '+', ',', '-', '.', '/',
  '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', ':', ';', '<', '=', '>', '?',
  '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
  'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
  'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
  'X', 'Y', 'Z', '[', '\\', ']', '^', '_'
};

int touufrombits(unsigned char *out, const unsigned char *in, int inlen)
{
    int len;

    if (inlen > 45) return -1;
    len = (inlen * 4 + 2) / 3 + 1;
    *out++ = uudigit[inlen];

    for (; inlen >= 3; inlen -= 3) {
	*out++ = uudigit[in[0] >> 2];
	*out++ = uudigit[((in[0] << 4) & 0x30) | (in[1] >> 4)];
	*out++ = uudigit[((in[1] << 2) & 0x3c) | (in[2] >> 6)];
	*out++ = uudigit[in[2] & 0x3f];
	in += 3;
    }

    if (inlen > 0) {
	*out++ = uudigit[(in[0] >> 2)];
	if (inlen == 1) {
	    *out++ = uudigit[((in[0] << 4) & 0x30)];
	} else {
	    *out++ = uudigit[(((in[0] << 4) & 0x30) | (in[1] >> 4))] ;	    
	    *out++ = uudigit[((in[1] << 2) & 0x3c)];
	}
    }
    *out = '\0';

    return len;
}

int fromuutobits(char *out, const char *in)
{
    int len, outlen, inlen;
    register unsigned char digit1, digit2;

    outlen = UUDECODE(in[0]);
    in += 1;
    if(outlen < 0 || outlen > 45)
	return -2;
    if(outlen == 0)
	return 0;
    inlen = (outlen * 4 + 2) / 3;
    len = 0;

    for( ; inlen>0; inlen-=4) {
	digit1 = UUDECODE(in[0]);
	if (N64(digit1)) return -1;
	digit2 = UUDECODE(in[1]);
	if (N64(digit2)) return -1;
	out[len++] = (digit1 << 2) | (digit2 >> 4);
	if (inlen > 2) {
	    digit1 = UUDECODE(in[2]);
	    if (N64(digit1)) return -1;
	    out[len++] = (digit2 << 4) | (digit1 >> 2);
	    if (inlen > 3) {
		digit2 = UUDECODE(in[3]);
		if (N64(digit2)) return -1;
		out[len++] = (digit1 << 6) | digit2;
	    }
	}
	in += 4;
    }

    return len == outlen ? len : -3;
}
