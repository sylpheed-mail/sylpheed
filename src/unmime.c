/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2005 Hiroyuki Yamamoto
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

#include <glib.h>
#include <string.h>
#include <ctype.h>

#include "codeconv.h"
#include "base64.h"
#include "quoted-printable.h"

#define ENCODED_WORD_BEGIN	"=?"
#define ENCODED_WORD_END	"?="

/* Decodes headers based on RFC2045 and RFC2047. */

void unmime_header(gchar *out, const gchar *str)
{
	const gchar *p = str;
	gchar *outp = out;
	const gchar *eword_begin_p, *encoding_begin_p, *text_begin_p,
		    *eword_end_p;
	gchar charset[32];
	gchar encoding;
	gchar *conv_str;
	gint len;

	while (*p != '\0') {
		gchar *decoded_text = NULL;

		eword_begin_p = strstr(p, ENCODED_WORD_BEGIN);
		if (!eword_begin_p) {
			strcpy(outp, p);
			return;
		}
		encoding_begin_p = strchr(eword_begin_p + 2, '?');
		if (!encoding_begin_p) {
			strcpy(outp, p);
			return;
		}
		text_begin_p = strchr(encoding_begin_p + 1, '?');
		if (!text_begin_p) {
			strcpy(outp, p);
			return;
		}
		eword_end_p = strstr(text_begin_p + 1, ENCODED_WORD_END);
		if (!eword_end_p) {
			strcpy(outp, p);
			return;
		}

		if (p == str) {
			memcpy(outp, p, eword_begin_p - p);
			outp += eword_begin_p - p;
			p = eword_begin_p;
		} else {
			/* ignore spaces between encoded words */
			const gchar *sp;

			for (sp = p; sp < eword_begin_p; sp++) {
				if (!isspace(*(const guchar *)sp)) {
					memcpy(outp, p, eword_begin_p - p);
					outp += eword_begin_p - p;
					p = eword_begin_p;
					break;
				}
			}
		}

		len = MIN(sizeof(charset) - 1,
			  encoding_begin_p - (eword_begin_p + 2));
		memcpy(charset, eword_begin_p + 2, len);
		charset[len] = '\0';
		encoding = toupper(*(encoding_begin_p + 1));

		if (encoding == 'B') {
			decoded_text = g_malloc
				(eword_end_p - (text_begin_p + 1) + 1);
			len = base64_decode(decoded_text, text_begin_p + 1,
					    eword_end_p - (text_begin_p + 1));
			decoded_text[len] = '\0';
		} else if (encoding == 'Q') {
			decoded_text = g_malloc
				(eword_end_p - (text_begin_p + 1) + 1);
			len = qp_decode_q_encoding
				(decoded_text, text_begin_p + 1,
				 eword_end_p - (text_begin_p + 1));
		} else {
			memcpy(outp, p, eword_end_p + 2 - p);
			outp += eword_end_p + 2 - p;
			p = eword_end_p + 2;
			continue;
		}

		/* convert to UTF-8 */
		conv_str = conv_codeset_strdup(decoded_text, charset, NULL);
		if (conv_str) {
			len = strlen(conv_str);
			memcpy(outp, conv_str, len);
			g_free(conv_str);
		} else {
			len = strlen(decoded_text);
			conv_utf8todisp(outp, len + 1, decoded_text);
		}
		outp += len;

		g_free(decoded_text);

		p = eword_end_p + 2;
	}

	*outp = '\0';
}
