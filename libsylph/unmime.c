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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <string.h>
#include <ctype.h>

#include "codeconv.h"
#include "base64.h"
#include "quoted-printable.h"

#define ENCODED_WORD_BEGIN	"=?"
#define ENCODED_WORD_END	"?="

/* Decodes headers based on RFC2045 and RFC2047. */

gchar *unmime_header(const gchar *encoded_str)
{
	const gchar *p = encoded_str;
	const gchar *eword_begin_p, *encoding_begin_p, *text_begin_p,
		    *eword_end_p;
	gchar charset[32];
	gchar encoding;
	gchar *conv_str;
	GString *outbuf;
	gchar *out_str;
	gsize out_len;

	outbuf = g_string_sized_new(strlen(encoded_str) * 2);

	while (*p != '\0') {
		gchar *decoded_text = NULL;
		gint len;

		eword_begin_p = strstr(p, ENCODED_WORD_BEGIN);
		if (!eword_begin_p) {
			g_string_append(outbuf, p);
			break;
		}
		encoding_begin_p = strchr(eword_begin_p + 2, '?');
		if (!encoding_begin_p) {
			g_string_append(outbuf, p);
			break;
		}
		text_begin_p = strchr(encoding_begin_p + 1, '?');
		if (!text_begin_p) {
			g_string_append(outbuf, p);
			break;
		}
		eword_end_p = strstr(text_begin_p + 1, ENCODED_WORD_END);
		if (!eword_end_p) {
			g_string_append(outbuf, p);
			break;
		}

		if (p == encoded_str) {
			g_string_append_len(outbuf, p, eword_begin_p - p);
			p = eword_begin_p;
		} else {
			/* ignore spaces between encoded words */
			const gchar *sp;

			for (sp = p; sp < eword_begin_p; sp++) {
				if (!g_ascii_isspace(*sp)) {
					g_string_append_len
						(outbuf, p, eword_begin_p - p);
					p = eword_begin_p;
					break;
				}
			}
		}

		len = MIN(sizeof(charset) - 1,
			  encoding_begin_p - (eword_begin_p + 2));
		memcpy(charset, eword_begin_p + 2, len);
		charset[len] = '\0';
		encoding = g_ascii_toupper(*(encoding_begin_p + 1));

		if (encoding == 'B') {
			decoded_text = g_malloc
				(eword_end_p - (text_begin_p + 1) + 1);
			len = base64_decode((guchar *)decoded_text,
					    text_begin_p + 1,
					    eword_end_p - (text_begin_p + 1));
			decoded_text[len] = '\0';
		} else if (encoding == 'Q') {
			decoded_text = g_malloc
				(eword_end_p - (text_begin_p + 1) + 1);
			len = qp_decode_q_encoding
				((guchar *)decoded_text, text_begin_p + 1,
				 eword_end_p - (text_begin_p + 1));
		} else {
			g_string_append_len(outbuf, p, eword_end_p + 2 - p);
			p = eword_end_p + 2;
			continue;
		}

		/* convert to UTF-8 */
		conv_str = conv_codeset_strdup(decoded_text, charset, NULL);
		if (!conv_str)
			conv_str = conv_utf8todisp(decoded_text, NULL);
		g_string_append(outbuf, conv_str);
		g_free(conv_str);

		g_free(decoded_text);

		p = eword_end_p + 2;
	}

	out_str = outbuf->str;
	out_len = outbuf->len;
	g_string_free(outbuf, FALSE);

	return g_realloc(out_str, out_len + 1);
}
