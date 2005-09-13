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

#include <glib.h>
#include <ctype.h>

static gboolean get_hex_value(guchar *out, gchar c1, gchar c2);
static void get_hex_str(gchar *out, guchar ch);

#define MAX_LINELEN	76

#define IS_LBREAK(p) \
	(*(p) == '\0' || *(p) == '\n' || (*(p) == '\r' && *((p) + 1) == '\n'))

#define SOFT_LBREAK_IF_REQUIRED(n)					\
	if (len + (n) > MAX_LINELEN ||					\
	    (len + (n) == MAX_LINELEN && (!IS_LBREAK(inp + 1)))) {	\
		*outp++ = '=';						\
		*outp++ = '\n';						\
		len = 0;						\
	}

void qp_encode_line(gchar *out, const guchar *in)
{
	const guchar *inp = in;
	gchar *outp = out;
	guchar ch;
	gint len = 0;

	while (*inp != '\0') {
		ch = *inp;

		if (IS_LBREAK(inp)) {
			*outp++ = '\n';
			len = 0;
			if (*inp == '\r')
				inp++;
			inp++;
		} else if (ch == '\t' || ch == ' ') {
			if (IS_LBREAK(inp + 1)) {
				SOFT_LBREAK_IF_REQUIRED(3);
				*outp++ = '=';
				get_hex_str(outp, ch);
				outp += 2;
				len += 3;
				inp++;
			} else {
				SOFT_LBREAK_IF_REQUIRED(1);
				*outp++ = *inp++;
				len++;
			}
		} else if ((ch >= 33 && ch <= 60) || (ch >= 62 && ch <= 126)) {
			SOFT_LBREAK_IF_REQUIRED(1);
			*outp++ = *inp++;
			len++;
		} else {
			SOFT_LBREAK_IF_REQUIRED(3);
			*outp++ = '=';
			get_hex_str(outp, ch);
			outp += 2;
			len += 3;
			inp++;
		}
	}

	if (len > 0)
		*outp++ = '\n';

	*outp = '\0';
}

gint qp_decode_line(gchar *str)
{
	gchar *inp = str, *outp = str;

	while (*inp != '\0') {
		if (*inp == '=') {
			if (inp[1] && inp[2] &&
			    get_hex_value((guchar *)outp, inp[1], inp[2])
			    == TRUE) {
				inp += 3;
			} else if (inp[1] == '\0' || g_ascii_isspace(inp[1])) {
				/* soft line break */
				break;
			} else {
				/* broken QP string */
				*outp = *inp++;
			}
		} else {
			*outp = *inp++;
		}
		outp++;
	}

	*outp = '\0';

	return outp - str;
}

gint qp_decode_q_encoding(guchar *out, const gchar *in, gint inlen)
{
	const gchar *inp = in;
	guchar *outp = out;

	if (inlen < 0)
		inlen = G_MAXINT;

	while (inp - in < inlen && *inp != '\0') {
		if (*inp == '=' && inp + 3 - in <= inlen) {
			if (get_hex_value(outp, inp[1], inp[2]) == TRUE) {
				inp += 3;
			} else {
				*outp = *inp++;
			}
		} else if (*inp == '_') {
			*outp = ' ';
			inp++;
		} else {
			*outp = *inp++;
		}
		outp++;
	}

	*outp = '\0';

	return outp - out;
}

gint qp_get_q_encoding_len(const guchar *str)
{
	const guchar *inp = str;
	gint len = 0;

	while (*inp != '\0') {
		if (*inp == 0x20)
			len++;
		else if (*inp == '=' || *inp == '?' || *inp == '_' ||
			 *inp < 32 || *inp > 127 || g_ascii_isspace(*inp))
			len += 3;
		else
			len++;

		inp++;
	}

	return len;
}

void qp_q_encode(gchar *out, const guchar *in)
{
	const guchar *inp = in;
	gchar *outp = out;

	while (*inp != '\0') {
		if (*inp == 0x20)
			*outp++ = '_';
		else if (*inp == '=' || *inp == '?' || *inp == '_' ||
			 *inp < 32 || *inp > 127 || g_ascii_isspace(*inp)) {
			*outp++ = '=';
			get_hex_str(outp, *inp);
			outp += 2;
		} else
			*outp++ = *inp;

		inp++;
	}

	*outp = '\0';
}

#define HEX_TO_INT(val, hex)			\
{						\
	gchar c = hex;				\
						\
	if ('0' <= c && c <= '9') {		\
		val = c - '0';			\
	} else if ('a' <= c && c <= 'f') {	\
		val = c - 'a' + 10;		\
	} else if ('A' <= c && c <= 'F') {	\
		val = c - 'A' + 10;		\
	} else {				\
		val = -1;			\
	}					\
}

static gboolean get_hex_value(guchar *out, gchar c1, gchar c2)
{
	gint hi, lo;

	HEX_TO_INT(hi, c1);
	HEX_TO_INT(lo, c2);

	if (hi == -1 || lo == -1)
		return FALSE;

	*out = (hi << 4) + lo;
	return TRUE;
}

#define INT_TO_HEX(hex, val)		\
{					\
	if ((val) < 10)			\
		hex = '0' + (val);	\
	else				\
		hex = 'A' + (val) - 10;	\
}

static void get_hex_str(gchar *out, guchar ch)
{
	gchar hex;

	INT_TO_HEX(hex, ch >> 4);
	*out++ = hex;
	INT_TO_HEX(hex, ch & 0x0f);
	*out++ = hex;
}
