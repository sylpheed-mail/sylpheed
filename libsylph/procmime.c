/*
 * LibSylph -- E-Mail client library
 * Copyright (C) 1999-2017 Hiroyuki Yamamoto
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

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <ctype.h>

#include "procmime.h"
#include "procheader.h"
#include "base64.h"
#include "quoted-printable.h"
#include "uuencode.h"
#include "html.h"
#include "codeconv.h"
#include "utils.h"
#include "prefs_common.h"

#undef MIME_DEBUG
/* #define MIME_DEBUG */
#ifdef MIME_DEBUG
#  define mime_debug_print	debug_print
#else
#  define mime_debug_print	1 ? (void)0 : debug_print
#endif

#define MAX_MIME_LEVEL	64

static GHashTable *procmime_get_mime_type_table	(void);
static GList *procmime_get_mime_type_list	(const gchar *file);


MimeInfo *procmime_mimeinfo_new(void)
{
	MimeInfo *mimeinfo;

	mimeinfo = g_new0(MimeInfo, 1);
	mimeinfo->mime_type     = MIME_UNKNOWN;
	mimeinfo->encoding_type = ENC_UNKNOWN;

	return mimeinfo;
}

void procmime_mimeinfo_free_all(MimeInfo *mimeinfo)
{
	while (mimeinfo != NULL) {
		MimeInfo *next;

		g_free(mimeinfo->encoding);
		g_free(mimeinfo->content_type);
		g_free(mimeinfo->charset);
		g_free(mimeinfo->name);
		g_free(mimeinfo->boundary);
		g_free(mimeinfo->content_disposition);
		g_free(mimeinfo->filename);

		g_free(mimeinfo->sigstatus);
		g_free(mimeinfo->sigstatus_full);

		procmime_mimeinfo_free_all(mimeinfo->sub);
		procmime_mimeinfo_free_all(mimeinfo->children);
		procmime_mimeinfo_free_all(mimeinfo->plaintext);

		next = mimeinfo->next;
		g_free(mimeinfo);
		mimeinfo = next;
	}
}

MimeInfo *procmime_mimeinfo_insert(MimeInfo *parent, MimeInfo *mimeinfo)
{
	MimeInfo *child = parent->children;

	if (!child)
		parent->children = mimeinfo;
	else {
		while (child->next != NULL)
			child = child->next;

		child->next = mimeinfo;
	}

	mimeinfo->parent = parent;
	mimeinfo->level = parent->level + 1;

	return mimeinfo;
}

#if 0
void procmime_mimeinfo_replace(MimeInfo *old, MimeInfo *new)
{
	MimeInfo *parent = old->parent;
	MimeInfo *child;

	g_return_if_fail(parent != NULL);
	g_return_if_fail(new->next == NULL);

	for (child = parent->children; child && child != old;
	     child = child->next)
		;
	if (!child) {
		g_warning("oops: parent can't find it's own child");
		return;
	}
	procmime_mimeinfo_free_all(old);

	if (child == parent->children) {
		new->next = parent->children->next;
		parent->children = new;
	} else {
		new->next = child->next;
		child = new;
	}
}
#endif

MimeInfo *procmime_mimeinfo_next(MimeInfo *mimeinfo)
{
	if (!mimeinfo) return NULL;

	if (mimeinfo->children)
		return mimeinfo->children;
	if (mimeinfo->sub)
		return mimeinfo->sub;
	if (mimeinfo->next)
		return mimeinfo->next;

	if (mimeinfo->main) {
		mimeinfo = mimeinfo->main;
		if (mimeinfo->next)
			return mimeinfo->next;
	}

	for (mimeinfo = mimeinfo->parent; mimeinfo != NULL;
	     mimeinfo = mimeinfo->parent) {
		if (mimeinfo->next)
			return mimeinfo->next;
		if (mimeinfo->main) {
			mimeinfo = mimeinfo->main;
			if (mimeinfo->next)
				return mimeinfo->next;
		}
	}

	return NULL;
}

#if 0
void procmime_dump_mimeinfo(MimeInfo *mimeinfo)
{
	gint i;

	g_print("\n");

	for (; mimeinfo != NULL; mimeinfo = procmime_mimeinfo_next(mimeinfo)) {
		for (i = 0; i < mimeinfo->level; i++)
			g_print("  ");
		g_print("%s%s\n", mimeinfo->main ? "sub: " : "",
			mimeinfo->content_type);
	}
}
#endif

MimeInfo *procmime_scan_message(MsgInfo *msginfo)
{
	FILE *fp;
	MimeInfo *mimeinfo;

	g_return_val_if_fail(msginfo != NULL, NULL);

	if ((fp = procmsg_open_message_decrypted(msginfo, &mimeinfo)) == NULL)
		return NULL;

	if (mimeinfo) {
		mimeinfo->size = msginfo->size;
		mimeinfo->content_size = get_left_file_size(fp);
		if (mimeinfo->encoding_type == ENC_BASE64)
			mimeinfo->content_size = mimeinfo->content_size / 4 * 3;
		if (mimeinfo->mime_type == MIME_MULTIPART ||
		    mimeinfo->mime_type == MIME_MESSAGE_RFC822)
			procmime_scan_multipart_message(mimeinfo, fp);
	}

	fclose(fp);

	return mimeinfo;
}

MimeInfo *procmime_scan_message_stream(FILE *fp)
{
	MimeInfo *mimeinfo;
	glong fpos;

	g_return_val_if_fail(fp != NULL, NULL);

	if (fseek(fp, 0L, SEEK_SET) < 0) {
		FILE_OP_ERROR("procmime_scan_message_stream()", "fseek");
		return NULL;
	}

	mimeinfo = procmime_scan_mime_header(fp);

	if (mimeinfo) {
		fpos = ftell(fp);
		mimeinfo->content_size = get_left_file_size(fp);
		mimeinfo->size = fpos + mimeinfo->content_size;
		if (mimeinfo->encoding_type == ENC_BASE64)
			mimeinfo->content_size = mimeinfo->content_size / 4 * 3;
		if (mimeinfo->mime_type == MIME_MULTIPART ||
		    mimeinfo->mime_type == MIME_MESSAGE_RFC822)
			procmime_scan_multipart_message(mimeinfo, fp);
	}

	return mimeinfo;
}

void procmime_scan_multipart_message(MimeInfo *mimeinfo, FILE *fp)
{
	gchar *p;
	gchar *boundary;
	gint boundary_len = 0;
	gchar *buf;
	glong fpos, prev_fpos;

	g_return_if_fail(mimeinfo != NULL);
	g_return_if_fail(mimeinfo->mime_type == MIME_MULTIPART ||
			 mimeinfo->mime_type == MIME_MESSAGE_RFC822);

	if (mimeinfo->mime_type == MIME_MULTIPART) {
		g_return_if_fail(mimeinfo->boundary != NULL);
		g_return_if_fail(mimeinfo->sub == NULL);
	}
	g_return_if_fail(fp != NULL);

	buf = g_malloc(BUFFSIZE);

	boundary = mimeinfo->boundary;

	if (boundary) {
		boundary_len = strlen(boundary);

		/* look for first boundary */
		while ((p = fgets(buf, BUFFSIZE, fp)) != NULL)
			if (IS_BOUNDARY(buf, boundary, boundary_len)) break;
		if (!p) {
			g_free(buf);
			return;
		}
	} else if (mimeinfo->parent && mimeinfo->parent->boundary) {
		boundary = mimeinfo->parent->boundary;
		boundary_len = strlen(boundary);
	}

	if ((fpos = ftell(fp)) < 0) {
		perror("ftell");
		g_free(buf);
		return;
	}

	mime_debug_print("==== enter part\n");
	mime_debug_print("level = %d\n", mimeinfo->level);

	for (;;) {
		MimeInfo *partinfo;
		gboolean eom = FALSE;
		glong content_pos;
		gboolean is_base64;
		gint len;
		guint b64_content_len = 0;
		gint b64_pad_len = 0;

		prev_fpos = fpos;
		mime_debug_print("prev_fpos: %ld\n", fpos);

		/* scan part header */
		if (mimeinfo->mime_type == MIME_MESSAGE_RFC822) {
			MimeInfo *sub;

			mimeinfo->sub = sub = procmime_scan_mime_header(fp);
			if (!sub) break;

			mime_debug_print("message/rfc822 part (content-type: %s)\n",
					 sub->content_type);
			sub->level = mimeinfo->level + 1;
			sub->parent = mimeinfo->parent;
			sub->main = mimeinfo;

			partinfo = sub;
		} else {
			partinfo = procmime_scan_mime_header(fp);
			if (!partinfo) break;
			procmime_mimeinfo_insert(mimeinfo, partinfo);
			mime_debug_print("content-type: %s\n",
					 partinfo->content_type);
			if (partinfo->filename)
				mime_debug_print("filename: %s\n", partinfo->filename);
			else if (partinfo->name)
				mime_debug_print("name: %s\n", partinfo->name);
		}

		/* begin content */
		content_pos = ftell(fp);
		mime_debug_print("content_pos: %ld\n", content_pos);

		if (partinfo->mime_type == MIME_MULTIPART ||
		    partinfo->mime_type == MIME_MESSAGE_RFC822) {
			if (partinfo->level < MAX_MIME_LEVEL) {
				mime_debug_print("\n");
				mime_debug_print("enter to child part:\n");
				procmime_scan_multipart_message(partinfo, fp);
			}
		}

		/* look for next boundary */
		buf[0] = '\0';
		is_base64 = partinfo->encoding_type == ENC_BASE64;
		while ((p = fgets(buf, BUFFSIZE, fp)) != NULL) {
			if (IS_BOUNDARY(buf, boundary, boundary_len)) {
				if (buf[2 + boundary_len]     == '-' &&
				    buf[2 + boundary_len + 1] == '-')
					eom = TRUE;
				break;
			} else if (is_base64) {
				const gchar *s;
				for (s = buf; *s && *s != '\r' && *s != '\n';
				     ++s)
					if (*s == '=')
						++b64_pad_len;
				b64_content_len += s - buf;
			}
		}
		if (p == NULL) {
			/* broken MIME, or single part MIME message */
			buf[0] = '\0';
			eom = TRUE;
		}
		mime_debug_print("boundary: %s\n", buf);

		fpos = ftell(fp);
		mime_debug_print("fpos: %ld\n", fpos);

		len = strlen(buf);
		partinfo->size = fpos - prev_fpos - len;
		if (is_base64)
			partinfo->content_size =
				b64_content_len / 4 * 3 - b64_pad_len;
		else
			partinfo->content_size = fpos - content_pos - len;
		mime_debug_print("partinfo->size: %d\n", partinfo->size);
		mime_debug_print("partinfo->content_size: %d\n",
				 partinfo->content_size);
		if (partinfo->sub && !partinfo->sub->sub &&
		    !partinfo->sub->children) {
			partinfo->sub->size =
				fpos - partinfo->sub->fpos - strlen(buf);
			mime_debug_print("partinfo->sub->size: %d\n",
					 partinfo->sub->size);
		}

		if (mimeinfo->mime_type == MIME_MESSAGE_RFC822) {
			if (len > 0 && fseek(fp, fpos - len, SEEK_SET) < 0)
				perror("fseek");
			break;
		}

		if (eom) break;
	}

	g_free(buf);
	mime_debug_print("==== leave part\n");
}

void procmime_scan_encoding(MimeInfo *mimeinfo, const gchar *encoding)
{
	gchar *enc;

	g_free(mimeinfo->encoding);
	enc = mimeinfo->encoding = g_strstrip(g_strdup(encoding));

	if (!g_ascii_strncasecmp(enc, "7bit", 4))
		mimeinfo->encoding_type = ENC_7BIT;
	else if (!g_ascii_strncasecmp(enc, "8bit", 4))
		mimeinfo->encoding_type = ENC_8BIT;
	else if (!g_ascii_strncasecmp(enc, "quoted-printable", 16))
		mimeinfo->encoding_type = ENC_QUOTED_PRINTABLE;
	else if (!g_ascii_strncasecmp(enc, "base64", 6))
		mimeinfo->encoding_type = ENC_BASE64;
	else if (!g_ascii_strncasecmp(enc, "x-uuencode", 10))
		mimeinfo->encoding_type = ENC_X_UUENCODE;
	else
		mimeinfo->encoding_type = ENC_UNKNOWN;

}

void procmime_scan_content_type(MimeInfo *mimeinfo, const gchar *content_type)
{
	g_free(mimeinfo->content_type);
	g_free(mimeinfo->charset);
	g_free(mimeinfo->name);
	g_free(mimeinfo->boundary);
	mimeinfo->content_type = NULL;
	mimeinfo->charset      = NULL;
	mimeinfo->name         = NULL;
	mimeinfo->boundary     = NULL;

	procmime_scan_content_type_str(content_type, &mimeinfo->content_type,
				       &mimeinfo->charset, &mimeinfo->name,
				       &mimeinfo->boundary);

	mimeinfo->mime_type = procmime_scan_mime_type(mimeinfo->content_type);
	if (mimeinfo->mime_type == MIME_MULTIPART && !mimeinfo->boundary)
		mimeinfo->mime_type = MIME_TEXT;
}

typedef struct
{
	gchar *name;
	gchar *value;
} MimeParam;

typedef struct
{
	gchar *hvalue;
	GSList *plist;
} MimeParams;

static gchar *procmime_find_parameter_delimiter(const gchar *param,
						const gchar **eq)
{
	register const gchar *p = param;
	gboolean quoted = FALSE;
	const gchar *delim = NULL;

	while (*p) {
		if (*p == '=')
			break;
		else if (*p == ';' || *p == '\r' || *p == '\n') {
			delim = p;
			break;
		}
		++p;
	}
	if (*p != '=') {
		*eq = NULL;
		return (gchar *)delim;
	}
	*eq = p;

	++p;
	while (g_ascii_isspace(*p))
		++p;
	if (*p == '"') {
		quoted = TRUE;
		++p;
	}

	while (*p) {
		if (quoted == TRUE) {
			if (*p == '"')
				quoted = FALSE;
		} else if (*p == ';' || *p == '\r' || *p == '\n') {
			delim = p;
			break;
		}
		++p;
	}

	return (gchar *)delim;
}

static gchar *procmime_convert_value(const gchar *value, const gchar *charset)
{
	if (charset) {
		gchar *utf8_value;

		utf8_value = conv_codeset_strdup(value, charset, CS_INTERNAL);
		if (utf8_value)
			return utf8_value;
	}

	return g_strdup(value);
}

static MimeParams *procmime_parse_mime_parameter(const gchar *str)
{
	ConvADType ad_type;
	gchar *tmp_param = NULL;
	gchar *hvalue;
	gchar *param, *name, *value;
	gchar *charset = NULL, *lang = NULL;
	const gchar *p, *delim;
	gint count, prev_count;
	gchar *cont_name;
	gchar *cont_value;
	MimeParam *mparam;
	MimeParams *mparams;
	GSList *plist = NULL;

	if ((p = strchr(str, ';')))
		hvalue = g_strndup(str, p - str);
	else
		hvalue = g_strdup(str);

	g_strstrip(hvalue);

	mparams = g_new(MimeParams, 1);
	mparams->hvalue = hvalue;
	mparams->plist = NULL;

	if (!p)
		return mparams;
	++p;

	/* workaround for raw-JIS filename (Eudora etc.) */
	ad_type = conv_get_autodetect_type();
	if ((ad_type == C_AD_JAPANESE ||
	     (ad_type == C_AD_BY_LOCALE && conv_is_ja_locale())) &&
	    strstr(p, "\033$") != NULL) {
		CodeConvFunc conv_func;
		conv_func = conv_get_code_conv_func(NULL, NULL);
		tmp_param = conv_func(p, NULL);
		p = tmp_param;
		debug_print("procmime_parse_mime_parameter(): raw-JIS header body detected: %s\n", str);
	}

	count = prev_count = -1;
	cont_name = cont_value = NULL;

	for (;;) {
		gboolean encoded = FALSE;
		gchar *begin;
		gchar *dec_value;
		const gchar *eq;
		gchar *ast = NULL;

		while (*p == ';' || g_ascii_isspace(*p))
			++p;
		if (*p == '\0')
			break;

		delim = procmime_find_parameter_delimiter(p, &eq);
		if (!eq)
			break;
		if (delim)
			param = g_strndup(p, delim - p);
		else
			param = g_strdup(p);

		name = g_strndup(p, eq - p);
		g_strchomp(name);
		if (*name != '*' && (ast = strchr(name, '*'))) {
			const gchar *next = ast + 1;

			if (*next == '\0') {
				encoded = TRUE;
			} else if (g_ascii_isdigit(*next)) {
				count = atoi(next);
				while (g_ascii_isdigit(*next))
					++next;
				if (*next == '*')
					encoded = TRUE;
				if (prev_count + 1 != count) {
					g_warning("procmime_parse_mime_parameter(): invalid count: %s\n", str);
					g_free(name);
					g_free(param);
					break;
				}
			} else {
				g_warning("procmime_parse_mime_parameter(): invalid name: %s\n", str);
				g_free(name);
				g_free(param);
				break;
			}

			*ast = '\0';
		}

		value = g_strdup(param + (eq - p) + 1);
		g_strstrip(value);
		if (*value == '"')
			extract_quote(value, '"');

		begin = value;

		if (encoded) {
			gchar *sq1, *sq2;

			if ((sq1 = strchr(value, '\''))) {
				if (sq1 > value) {
					if (charset)
						g_free(charset);
					charset = g_strndup(value, sq1 - value);
				}
				if ((sq2 = strchr(sq1 + 1, '\''))) {
					if (sq2 > sq1 + 1) {
						if (lang)
							g_free(lang);
						lang = g_strndup(sq1 + 1,
								 sq2 - sq1 - 1);
					}
					begin = sq2 + 1;
				}
			}
		}

#define CONCAT_CONT_VALUE(s)				\
{							\
	if (cont_value) {				\
		gchar *tmp;				\
		tmp = g_strconcat(cont_value, s, NULL);	\
		g_free(cont_value);			\
		cont_value = tmp;			\
	} else						\
		cont_value = g_strdup(s);		\
}

		if (count >= 0) {
			if (count > 0 && cont_name) {
				if (strcmp(cont_name, name) != 0) {
					g_warning("procmime_parse_mime_parameter(): mismatch parameter name: %s\n", str);
					g_free(name);
					g_free(value);
					g_free(param);
					break;
				}
			} else
				cont_name = g_strdup(name);

			if (encoded) {
				dec_value = g_malloc(strlen(begin) + 1);
				decode_xdigit_encoded_str(dec_value, begin);
				CONCAT_CONT_VALUE(dec_value);
				g_free(dec_value);
			} else {
				CONCAT_CONT_VALUE(begin);
			}
		}

#undef CONCAT_CONT_VALUE

		if (count == -1 && cont_name && cont_value) {
			mparam = g_new(MimeParam, 1);
			mparam->name = cont_name;
			cont_name = NULL;
			mparam->value = procmime_convert_value
				(cont_value, charset);
			g_free(cont_value);
			cont_value = NULL;
			plist = g_slist_prepend(plist, mparam);
		}

		if (count == -1) {
			mparam = g_new(MimeParam, 1);
			mparam->name = name;
			if (encoded) {
				dec_value = g_malloc(strlen(begin) + 1);
				decode_xdigit_encoded_str(dec_value, begin);
				mparam->value = procmime_convert_value
					(dec_value, charset);
				g_free(dec_value);
			} else {
				if (!ast &&
				    (!g_ascii_strcasecmp(name, "name") ||
				     !g_ascii_strcasecmp(name, "filename")))
					mparam->value =
						conv_unmime_header(begin, NULL);
				else
					mparam->value = g_strdup(begin);
			}
			name = NULL;
			plist = g_slist_prepend(plist, mparam);
		}

		g_free(name);
		g_free(value);
		g_free(param);

		prev_count = count;
		count = -1;

		if (delim)
			p = delim + 1;
		else
			break;
	}

	if (cont_name && cont_value) {
		mparam = g_new(MimeParam, 1);
		mparam->name = cont_name;
		cont_name = NULL;
		mparam->value = procmime_convert_value(cont_value, charset);
		plist = g_slist_prepend(plist, mparam);
	}

	g_free(cont_name);
	g_free(cont_value);
	g_free(lang);
	g_free(charset);
	if (tmp_param)
		g_free(tmp_param);

	plist = g_slist_reverse(plist);
	mparams->plist = plist;

	return mparams;
}

static void procmime_mime_params_free(MimeParams *mparams)
{
	GSList *cur;

	if (!mparams)
		return;

	g_free(mparams->hvalue);
	for (cur = mparams->plist; cur != NULL; cur = cur->next) {
		MimeParam *mparam = (MimeParam *)cur->data;
		g_free(mparam->name);
		g_free(mparam->value);
		g_free(mparam);
	}
	g_slist_free(mparams->plist);
	g_free(mparams);
}

void procmime_scan_content_type_str(const gchar *content_type,
				    gchar **mime_type, gchar **charset,
				    gchar **name, gchar **boundary)
{
	MimeParams *mparams;
	GSList *cur;

	mparams = procmime_parse_mime_parameter(content_type);

	if (mime_type)
		*mime_type = g_strdup(mparams->hvalue);

	for (cur = mparams->plist; cur != NULL; cur = cur->next) {
		MimeParam *param = (MimeParam *)cur->data;
		if (charset && !g_ascii_strcasecmp(param->name, "charset")) {
			*charset = g_strdup(param->value);
			eliminate_parenthesis(*charset, '(', ')');
			g_strstrip(*charset);
			charset = NULL;
		} else if (name && !g_ascii_strcasecmp(param->name, "name")) {
			*name = g_strdup(param->value);
			name = NULL;
		} else if (boundary &&
			   !g_ascii_strcasecmp(param->name, "boundary")) {
			*boundary = g_strdup(param->value);
			boundary = NULL;
		}
	}

	procmime_mime_params_free(mparams);
}

void procmime_scan_content_type_partial(const gchar *content_type,
					gint *total, gchar **part_id,
					gint *number)
{
	MimeParams *mparams;
	GSList *cur;
	gchar *id_str = NULL;
	gint t = 0, n = 0;

	*total = 0;
	*part_id = NULL;
	*number = 0;

	mparams = procmime_parse_mime_parameter(content_type);

	if (!mparams->hvalue ||
	    g_ascii_strcasecmp(mparams->hvalue, "message/partial") != 0) {
		procmime_mime_params_free(mparams);
		return;
	}

	for (cur = mparams->plist; cur != NULL; cur = cur->next) {
		MimeParam *param = (MimeParam *)cur->data;
		if (!g_ascii_strcasecmp(param->name, "total")) {
			t = atoi(param->value);
		} else if (!id_str && !g_ascii_strcasecmp(param->name, "id")) {
			id_str = g_strdup(param->value);
		} else if (!g_ascii_strcasecmp(param->name, "number")) {
			n = atoi(param->value);
		}
	}

	procmime_mime_params_free(mparams);

	if (n > 0 && (t == 0 || t >= n) && id_str) {
		*total = t;
		*part_id = id_str;
		*number = n;
	} else {
		g_free(id_str);
	}
}

void procmime_scan_content_disposition(MimeInfo *mimeinfo,
				       const gchar *content_disposition)
{
	MimeParams *mparams;
	GSList *cur;

	mparams = procmime_parse_mime_parameter(content_disposition);

	mimeinfo->content_disposition = g_strdup(mparams->hvalue);

	for (cur = mparams->plist; cur != NULL; cur = cur->next) {
		MimeParam *param = (MimeParam *)cur->data;
		if (!g_ascii_strcasecmp(param->name, "filename")) {
			mimeinfo->filename = g_strdup(param->value);
			break;
		}
	}

	procmime_mime_params_free(mparams);
}

enum
{
	H_CONTENT_TRANSFER_ENCODING = 0,
	H_CONTENT_TYPE		    = 1,
	H_CONTENT_DISPOSITION	    = 2
};

MimeInfo *procmime_scan_mime_header(FILE *fp)
{
	static HeaderEntry hentry[] = {{"Content-Transfer-Encoding:",
							  NULL, FALSE},
				       {"Content-Type:", NULL, TRUE},
				       {"Content-Disposition:",
							  NULL, TRUE},
				       {NULL,		  NULL, FALSE}};
	gchar buf[BUFFSIZE];
	gint hnum;
	HeaderEntry *hp;
	MimeInfo *mimeinfo;

	g_return_val_if_fail(fp != NULL, NULL);

	mimeinfo = procmime_mimeinfo_new();
	mimeinfo->mime_type = MIME_TEXT;
	mimeinfo->encoding_type = ENC_7BIT;
	mimeinfo->fpos = ftell(fp);

	while ((hnum = procheader_get_one_field(buf, sizeof(buf), fp, hentry))
	       != -1) {
		hp = hentry + hnum;

		if (H_CONTENT_TRANSFER_ENCODING == hnum) {
			procmime_scan_encoding
				(mimeinfo, buf + strlen(hp->name));
		} else if (H_CONTENT_TYPE == hnum) {
			procmime_scan_content_type
				(mimeinfo, buf + strlen(hp->name));
		} else if (H_CONTENT_DISPOSITION == hnum) {
			procmime_scan_content_disposition
				(mimeinfo, buf + strlen(hp->name));
		}
	}

	if (mimeinfo->mime_type == MIME_APPLICATION_OCTET_STREAM &&
	    (mimeinfo->filename || mimeinfo->name)) {
		const gchar *type;
		type = procmime_get_mime_type
			(mimeinfo->filename ? mimeinfo->filename
			 : mimeinfo->name);
		if (type)
			mimeinfo->mime_type = procmime_scan_mime_type(type);
	}

	if (!mimeinfo->content_type)
		mimeinfo->content_type = g_strdup("text/plain");

	return mimeinfo;
}

static gint procmime_normalize_lbreak(FILE *infp, FILE *outfp)
{
	gchar buf[BUFFSIZE];
	gint len;

	g_return_val_if_fail(infp != NULL, -1);
	g_return_val_if_fail(outfp != NULL, -1);

	while (fgets(buf, sizeof(buf), infp) != NULL) {
		len = strlen(buf);
		if (len == sizeof(buf) - 1 && buf[len - 1] != '\n') {
			if (buf[len - 1] == '\r') {
				ungetc('\r', infp);
				buf[len - 1] = '\0';
			}
			fputs(buf, outfp);
			continue;
		}
#ifdef G_OS_WIN32
		strretchomp(buf);
		fputs(buf, outfp);
		fputs("\r\n", outfp);
#else
		strcrchomp(buf);
		fputs(buf, outfp);
#endif
	}

	return 0;
}

FILE *procmime_decode_content(FILE *outfp, FILE *infp, MimeInfo *mimeinfo)
{
	gchar buf[BUFFSIZE];
	gchar *boundary = NULL;
	gint boundary_len = 0;
	gboolean tmp_file = FALSE;
	gboolean normalize_lbreak = FALSE;
	ContentType content_type;

	g_return_val_if_fail(infp != NULL, NULL);
	g_return_val_if_fail(mimeinfo != NULL, NULL);

	if (!outfp) {
		outfp = my_tmpfile();
		if (!outfp) {
			perror("tmpfile");
			return NULL;
		}
		tmp_file = TRUE;
	}

	if (mimeinfo->parent && mimeinfo->parent->boundary) {
		boundary = mimeinfo->parent->boundary;
		boundary_len = strlen(boundary);
	}

	content_type = procmime_scan_mime_type(mimeinfo->content_type);
	if (content_type == MIME_TEXT ||
	    content_type == MIME_TEXT_HTML) {
		normalize_lbreak = TRUE;
	}

	if (mimeinfo->encoding_type == ENC_QUOTED_PRINTABLE) {
		FILE *tmpfp = outfp;
		gchar prev_empty_line[3] = "";

		if (normalize_lbreak) {
			tmpfp = my_tmpfile();
			if (!tmpfp) {
				perror("tmpfile");
				if (tmp_file) fclose(outfp);
				return NULL;
			}
		}

		while (fgets(buf, sizeof(buf), infp) != NULL &&
		       (!boundary ||
			!IS_BOUNDARY(buf, boundary, boundary_len))) {
			gint len;

			if (prev_empty_line[0]) {
				fputs(prev_empty_line, tmpfp);
				prev_empty_line[0] = '\0';
			}

			if (buf[0] == '\n' ||
			    (buf[0] == '\r' && buf[1] == '\n'))
				strcpy(prev_empty_line, buf);
			else {
				len = qp_decode_line(buf);
				fwrite(buf, len, 1, tmpfp);
			}
		}
		if (!boundary && prev_empty_line[0])
			fputs(prev_empty_line, tmpfp);

		if (normalize_lbreak) {
			if (fflush(tmpfp) == EOF) {
				perror("fflush");
				fclose(tmpfp);
				if (tmp_file) fclose(outfp);
				return NULL;
			}
			rewind(tmpfp);
			procmime_normalize_lbreak(tmpfp, outfp);
			fclose(tmpfp);
		}
	} else if (mimeinfo->encoding_type == ENC_BASE64) {
		gchar outbuf[BUFFSIZE];
		gint len;
		Base64Decoder *decoder;
		FILE *tmpfp = outfp;

		if (normalize_lbreak) {
			tmpfp = my_tmpfile();
			if (!tmpfp) {
				perror("tmpfile");
				if (tmp_file) fclose(outfp);
				return NULL;
			}
		}

		decoder = base64_decoder_new();
		while (fgets(buf, sizeof(buf), infp) != NULL &&
		       (!boundary ||
			!IS_BOUNDARY(buf, boundary, boundary_len))) {
			len = base64_decoder_decode(decoder, buf,
						    (guchar *)outbuf);
			if (len < 0) {
				g_warning("Bad BASE64 content\n");
				break;
			}
			fwrite(outbuf, sizeof(gchar), len, tmpfp);
		}
		base64_decoder_free(decoder);

		if (normalize_lbreak) {
			if (fflush(tmpfp) == EOF) {
				perror("fflush");
				fclose(tmpfp);
				if (tmp_file) fclose(outfp);
				return NULL;
			}
			rewind(tmpfp);
			procmime_normalize_lbreak(tmpfp, outfp);
			fclose(tmpfp);
		}
	} else if (mimeinfo->encoding_type == ENC_X_UUENCODE) {
		gchar outbuf[BUFFSIZE];
		gint len;
		gboolean flag = FALSE;

		while (fgets(buf, sizeof(buf), infp) != NULL &&
		       (!boundary ||
			!IS_BOUNDARY(buf, boundary, boundary_len))) {
			if(!flag && strncmp(buf,"begin ", 6)) continue;

			if (flag) {
				len = fromuutobits(outbuf, buf);
				if (len <= 0) {
					if (len < 0) 
						g_warning("Bad UUENCODE content(%d)\n", len);
					break;
				}
				fwrite(outbuf, sizeof(gchar), len, outfp);
			} else
				flag = TRUE;
		}
	} else {
		gchar prev_empty_line[3] = "";
		gint len;
		gboolean cont_line = FALSE;

		while (fgets(buf, sizeof(buf), infp) != NULL &&
		       (!boundary ||
			!IS_BOUNDARY(buf, boundary, boundary_len))) {
			if (prev_empty_line[0]) {
				fputs(prev_empty_line, outfp);
				prev_empty_line[0] = '\0';
			}

			len = strlen(buf);
			if (len == sizeof(buf) - 1 &&
			    buf[len - 1] != '\n') {
				if (buf[len - 1] == '\r') {
					ungetc('\r', infp);
					buf[len - 1] = '\0';
				}
				fputs(buf, outfp);
				cont_line = TRUE;
				continue;
			}

			if (normalize_lbreak) {
#ifdef G_OS_WIN32
				strretchomp(buf);
				if (!cont_line && buf[0] == '\0')
					strcpy(prev_empty_line, "\r\n");
				else {
					fputs(buf, outfp);
					fputs("\r\n", outfp);
				}
#else
				strcrchomp(buf);
				if (!cont_line && buf[0] == '\n')
					strcpy(prev_empty_line, "\n");
				else
					fputs(buf, outfp);
#endif
			} else {
				if (!cont_line &&
				    (buf[0] == '\n' ||
				     (buf[0] == '\r' && buf[1] == '\n')))
					strcpy(prev_empty_line, buf);
				else
					fputs(buf, outfp);
			}

			cont_line = FALSE;
		}
		if (!boundary && prev_empty_line[0])
			fputs(prev_empty_line, outfp);
	}

	if (fflush(outfp) == EOF)
		perror("fflush");
	if (ferror(outfp) != 0) {
		g_warning("procmime_decode_content(): Can't write to temporary file\n");
		if (tmp_file) fclose(outfp);
		return NULL;
	}

	if (tmp_file) rewind(outfp);
	return outfp;
}

gint procmime_get_part(const gchar *outfile, const gchar *infile,
		       MimeInfo *mimeinfo)
{
	FILE *infp;
	gint ret;

	g_return_val_if_fail(outfile != NULL, -1);
	g_return_val_if_fail(infile != NULL, -1);
	g_return_val_if_fail(mimeinfo != NULL, -1);

	if ((infp = g_fopen(infile, "rb")) == NULL) {
		FILE_OP_ERROR(infile, "fopen");
		return -1;
	}
	ret = procmime_get_part_fp(outfile, infp, mimeinfo);
	fclose(infp);

	return ret;
}

gint procmime_get_part_fp(const gchar *outfile, FILE *infp, MimeInfo *mimeinfo)
{
	FILE *outfp;
	gchar buf[BUFFSIZE];

	g_return_val_if_fail(outfile != NULL, -1);
	g_return_val_if_fail(infp != NULL, -1);
	g_return_val_if_fail(mimeinfo != NULL, -1);

	if (fseek(infp, mimeinfo->fpos, SEEK_SET) < 0) {
		FILE_OP_ERROR("procmime_get_part_fp()", "fseek");
		return -1;
	}
	if ((outfp = g_fopen(outfile, "wb")) == NULL) {
		FILE_OP_ERROR(outfile, "fopen");
		return -1;
	}

	while (fgets(buf, sizeof(buf), infp) != NULL)
		if (buf[0] == '\r' || buf[0] == '\n') break;

	if (procmime_decode_content(outfp, infp, mimeinfo) == NULL) {
		fclose(outfp);
		g_unlink(outfile);
		return -1;
	}

	if (fclose(outfp) == EOF) {
		FILE_OP_ERROR(outfile, "fclose");
		g_unlink(outfile);
		return -1;
	}

	return 0;
}

FILE *procmime_get_part_fp_fp(FILE *outfp, FILE *infp, MimeInfo *mimeinfo)
{
	gchar buf[BUFFSIZE];

	g_return_val_if_fail(infp != NULL, NULL);
	g_return_val_if_fail(mimeinfo != NULL, NULL);

	if (fseek(infp, mimeinfo->fpos, SEEK_SET) < 0) {
		FILE_OP_ERROR("procmime_get_part_fp()", "fseek");
		return NULL;
	}

	while (fgets(buf, sizeof(buf), infp) != NULL)
		if (buf[0] == '\r' || buf[0] == '\n') break;

	if ((outfp = procmime_decode_content(outfp, infp, mimeinfo)) == NULL) {
		return NULL;
	}

	return outfp;
}

gint procmime_get_all_parts(const gchar *dir, const gchar *infile,
			    MimeInfo *mimeinfo)
{
	FILE *fp;
	MimeInfo *partinfo;
	gchar *base, *filename;

	g_return_val_if_fail(dir != NULL, -1);
	g_return_val_if_fail(infile != NULL, -1);
	g_return_val_if_fail(mimeinfo != NULL, -1);

	if (!is_dir_exist(dir)) {
		g_warning("%s: directory not exist.\n", dir);
		return -1;
	}

	if ((fp = g_fopen(infile, "rb")) == NULL) {
		FILE_OP_ERROR(infile, "fopen");
		return -1;
	}

	for (partinfo = mimeinfo; partinfo != NULL;
	     partinfo = procmime_mimeinfo_next(partinfo)) {
		if (partinfo->filename || partinfo->name) {
			gint count = 1;

			base = procmime_get_part_file_name(partinfo);
			filename = g_strconcat(dir, G_DIR_SEPARATOR_S, base,
					       NULL);

			while (is_file_entry_exist(filename)) {
				gchar *base_alt;

				base_alt = get_alt_filename(base, count++);
				g_free(filename);
				filename = g_strconcat
					(dir, G_DIR_SEPARATOR_S, base_alt,
					 NULL);
				g_free(base_alt);
			}

			procmime_get_part_fp(filename, fp, partinfo);

			g_free(filename);
			g_free(base);
		}
	}

	fclose(fp);

	return 0;
}

FILE *procmime_get_text_content(MimeInfo *mimeinfo, FILE *infp,
				const gchar *encoding)
{
	FILE *tmpfp, *outfp;
	const gchar *src_encoding;
	gboolean conv_fail = FALSE;
	gchar buf[BUFFSIZE];

	g_return_val_if_fail(mimeinfo != NULL, NULL);
	g_return_val_if_fail(infp != NULL, NULL);
	g_return_val_if_fail(mimeinfo->mime_type == MIME_TEXT ||
			     mimeinfo->mime_type == MIME_TEXT_HTML, NULL);

	if (fseek(infp, mimeinfo->fpos, SEEK_SET) < 0) {
		perror("fseek");
		return NULL;
	}

	while (fgets(buf, sizeof(buf), infp) != NULL)
		if (buf[0] == '\r' || buf[0] == '\n') break;

	tmpfp = procmime_decode_content(NULL, infp, mimeinfo);
	if (!tmpfp)
		return NULL;

	if ((outfp = my_tmpfile()) == NULL) {
		perror("tmpfile");
		fclose(tmpfp);
		return NULL;
	}

	src_encoding = prefs_common.force_charset ? prefs_common.force_charset
		: mimeinfo->charset ? mimeinfo->charset
		: prefs_common.default_encoding;

	if (mimeinfo->mime_type == MIME_TEXT) {
		while (fgets(buf, sizeof(buf), tmpfp) != NULL) {
			gchar *str;

			str = conv_codeset_strdup(buf, src_encoding, encoding);
			if (str) {
				fputs(str, outfp);
				g_free(str);
			} else {
				conv_fail = TRUE;
				fputs(buf, outfp);
			}
		}
	} else if (mimeinfo->mime_type == MIME_TEXT_HTML) {
		HTMLParser *parser;
		CodeConverter *conv;
		const gchar *str;

		conv = conv_code_converter_new(src_encoding, encoding);
		parser = html_parser_new(tmpfp, conv);
		while ((str = html_parse(parser)) != NULL) {
			fputs(str, outfp);
		}
		html_parser_destroy(parser);
		conv_code_converter_destroy(conv);
	}

	if (conv_fail)
		g_warning(_("procmime_get_text_content(): Code conversion failed.\n"));

	fclose(tmpfp);
	if (fflush(outfp) == EOF) {
		perror("fflush");
		fclose(outfp);
		return NULL;
	}
	rewind(outfp);

	return outfp;
}

/* search the first text part of (multipart) MIME message,
   decode, convert it and output to outfp. */
FILE *procmime_get_first_text_content(MsgInfo *msginfo, const gchar *encoding)
{
	FILE *infp, *outfp = NULL;
	MimeInfo *mimeinfo, *partinfo;

	g_return_val_if_fail(msginfo != NULL, NULL);

	mimeinfo = procmime_scan_message(msginfo);
	if (!mimeinfo) return NULL;

	if ((infp = procmsg_open_message(msginfo)) == NULL) {
		procmime_mimeinfo_free_all(mimeinfo);
		return NULL;
	}

	partinfo = mimeinfo;
	while (partinfo && partinfo->mime_type != MIME_TEXT)
		partinfo = procmime_mimeinfo_next(partinfo);
	if (!partinfo) {
		partinfo = mimeinfo;
		while (partinfo && partinfo->mime_type != MIME_TEXT_HTML)
			partinfo = procmime_mimeinfo_next(partinfo);
	}

	if (partinfo)
		outfp = procmime_get_text_content(partinfo, infp, encoding);

	fclose(infp);
	procmime_mimeinfo_free_all(mimeinfo);

	return outfp;
}

gboolean procmime_find_string_part(MimeInfo *mimeinfo, const gchar *filename,
				   const gchar *str, StrFindFunc find_func)
{

	FILE *infp, *outfp;
	gchar buf[BUFFSIZE];

	g_return_val_if_fail(mimeinfo != NULL, FALSE);
	g_return_val_if_fail(mimeinfo->mime_type == MIME_TEXT ||
			     mimeinfo->mime_type == MIME_TEXT_HTML, FALSE);
	g_return_val_if_fail(str != NULL, FALSE);
	g_return_val_if_fail(find_func != NULL, FALSE);

	if ((infp = g_fopen(filename, "rb")) == NULL) {
		FILE_OP_ERROR(filename, "fopen");
		return FALSE;
	}

	outfp = procmime_get_text_content(mimeinfo, infp, NULL);
	fclose(infp);

	if (!outfp)
		return FALSE;

	while (fgets(buf, sizeof(buf), outfp) != NULL) {
		strretchomp(buf);
		if (find_func(buf, str)) {
			fclose(outfp);
			return TRUE;
		}
	}

	fclose(outfp);

	return FALSE;
}

gboolean procmime_find_string(MsgInfo *msginfo, const gchar *str,
			      StrFindFunc find_func)
{
	MimeInfo *mimeinfo;
	MimeInfo *partinfo;
	gchar *filename;
	gboolean found = FALSE;

	g_return_val_if_fail(msginfo != NULL, FALSE);
	g_return_val_if_fail(str != NULL, FALSE);
	g_return_val_if_fail(find_func != NULL, FALSE);

	filename = procmsg_get_message_file(msginfo);
	if (!filename) return FALSE;
	mimeinfo = procmime_scan_message(msginfo);

	for (partinfo = mimeinfo; partinfo != NULL;
	     partinfo = procmime_mimeinfo_next(partinfo)) {
		if (partinfo->mime_type == MIME_TEXT ||
		    partinfo->mime_type == MIME_TEXT_HTML) {
			if (procmime_find_string_part
				(partinfo, filename, str, find_func) == TRUE) {
				found = TRUE;
				break;
			}
		}
	}

	procmime_mimeinfo_free_all(mimeinfo);
	g_free(filename);

	return found;
}

gchar *procmime_get_part_file_name(MimeInfo *mimeinfo)
{
	gchar *base;
	const gchar *base_;

	base_ = mimeinfo->filename ? mimeinfo->filename
		: mimeinfo->name ? mimeinfo->name : "mimetmp";
	base_ = g_basename(base_);
	if (*base_ == '\0') base_ = "mimetmp";
	base = conv_filename_from_utf8(base_);
	subst_for_filename(base);

	return base;
}

gchar *procmime_get_tmp_file_name(MimeInfo *mimeinfo)
{
	static guint32 id = 0;
	gchar *base;
	gchar *filename = NULL;
	gchar f_prefix[10];

	g_return_val_if_fail(mimeinfo != NULL, NULL);

	if (MIME_TEXT_HTML == mimeinfo->mime_type)
		base = g_strdup("mimetmp.html");
	else
		base = procmime_get_part_file_name(mimeinfo);

	do {
		g_snprintf(f_prefix, sizeof(f_prefix), "%08x.", id++);
		if (filename)
			g_free(filename);
		filename = g_strconcat(get_mime_tmp_dir(), G_DIR_SEPARATOR_S,
				       f_prefix, base, NULL);
	} while (is_file_entry_exist(filename));

	g_free(base);

	debug_print("procmime_get_tmp_file_name: %s\n", filename);

	return filename;
}

gchar *procmime_get_tmp_file_name_for_user(MimeInfo *mimeinfo)
{
	gchar *base;
	gchar *filename;
	gint count = 1;

	g_return_val_if_fail(mimeinfo != NULL, NULL);

	if (MIME_TEXT_HTML == mimeinfo->mime_type)
		base = g_strdup("mimetmp.html");
	else
		base = procmime_get_part_file_name(mimeinfo);

	filename = g_strconcat(get_mime_tmp_dir(), G_DIR_SEPARATOR_S,
			       base, NULL);
	while (is_file_entry_exist(filename)) {
		gchar *base_alt;

		base_alt = get_alt_filename(base, count++);
		g_free(filename);
		filename = g_strconcat(get_mime_tmp_dir(), G_DIR_SEPARATOR_S,
				       base_alt, NULL);
		g_free(base_alt);
	}

	g_free(base);

	debug_print("procmime_get_tmp_file_name_for_user: %s\n", filename);

	return filename;
}

ContentType procmime_scan_mime_type(const gchar *mime_type)
{
	ContentType type;

	if (!g_ascii_strncasecmp(mime_type, "text/html", 9))
		type = MIME_TEXT_HTML;
	else if (!g_ascii_strncasecmp(mime_type, "text/", 5))
		type = MIME_TEXT;
	else if (!g_ascii_strncasecmp(mime_type, "message/rfc822", 14))
		type = MIME_MESSAGE_RFC822;
	else if (!g_ascii_strncasecmp(mime_type, "message/", 8))
		type = MIME_TEXT;
	else if (!g_ascii_strncasecmp(mime_type, "application/octet-stream",
				      24))
		type = MIME_APPLICATION_OCTET_STREAM;
	else if (!g_ascii_strncasecmp(mime_type, "application/", 12))
		type = MIME_APPLICATION;
	else if (!g_ascii_strncasecmp(mime_type, "multipart/", 10))
		type = MIME_MULTIPART;
	else if (!g_ascii_strncasecmp(mime_type, "image/", 6))
		type = MIME_IMAGE;
	else if (!g_ascii_strncasecmp(mime_type, "audio/", 6))
		type = MIME_AUDIO;
	else if (!g_ascii_strncasecmp(mime_type, "video/", 6))
		type = MIME_VIDEO;
	else if (!g_ascii_strcasecmp(mime_type, "text"))
		type = MIME_TEXT;
	else
		type = MIME_UNKNOWN;

	return type;
}

static GList *mime_type_list = NULL;

gchar *procmime_get_mime_type(const gchar *filename)
{
	static GHashTable *mime_type_table = NULL;
	MimeType *mime_type;
	const gchar *p;
	gchar ext[64];
	static gboolean no_mime_type_table = FALSE;

	if (no_mime_type_table)
		return NULL;

	if (!mime_type_table) {
		mime_type_table = procmime_get_mime_type_table();
		if (!mime_type_table) {
			no_mime_type_table = TRUE;
			return NULL;
		}
	}

	filename = g_basename(filename);
	p = strrchr(filename, '.');
	if (!p) return NULL;

	strncpy2(ext, p + 1, sizeof(ext));
	g_strdown(ext);
	mime_type = g_hash_table_lookup(mime_type_table, ext);
	if (mime_type) {
		gchar *str;

		str = g_strconcat(mime_type->type, "/", mime_type->sub_type,
				  NULL);
		return str;
	}

	return NULL;
}

static GHashTable *procmime_get_mime_type_table(void)
{
	GHashTable *table = NULL;
	GList *cur;
	MimeType *mime_type;
	gchar **exts;

	if (!mime_type_list) {
		GList *list;
		gchar *dir;

#ifdef G_OS_WIN32
		dir = g_strconcat(get_startup_dir(),
				  G_DIR_SEPARATOR_S "etc" G_DIR_SEPARATOR_S
				  "mime.types", NULL);
		mime_type_list = procmime_get_mime_type_list(dir);
		g_free(dir);
#else
		mime_type_list =
			procmime_get_mime_type_list(SYSCONFDIR "/mime.types");
		if (!mime_type_list)
			mime_type_list =
				procmime_get_mime_type_list("/etc/mime.types");
#endif
		dir = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
				  "mime.types", NULL);
		list = procmime_get_mime_type_list(dir);
		g_free(dir);
		mime_type_list = g_list_concat(mime_type_list, list);

		if (!mime_type_list) {
			debug_print("mime.types not found\n");
			return NULL;
		}
	}

	table = g_hash_table_new(g_str_hash, g_str_equal);

	for (cur = mime_type_list; cur != NULL; cur = cur->next) {
		gint i;
		gchar *key;

		mime_type = (MimeType *)cur->data;

		if (!mime_type->extension) continue;

		exts = g_strsplit(mime_type->extension, " ", 16);
		for (i = 0; exts[i] != NULL; i++) {
			/* make the key case insensitive */
			g_strdown(exts[i]);
			/* use previously dup'd key on overwriting */
			if (g_hash_table_lookup(table, exts[i]))
				key = exts[i];
			else
				key = g_strdup(exts[i]);
			g_hash_table_insert(table, key, mime_type);
		}
		g_strfreev(exts);
	}

	return table;
}

static GList *procmime_get_mime_type_list(const gchar *file)
{
	GList *list = NULL;
	FILE *fp;
	gchar buf[BUFFSIZE];
	gchar *p;
	gchar *delim;
	MimeType *mime_type;

	if ((fp = g_fopen(file, "rb")) == NULL) return NULL;

	debug_print("Reading %s ...\n", file);

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		p = strchr(buf, '#');
		if (p) *p = '\0';
		g_strstrip(buf);

		p = buf;
		while (*p && !g_ascii_isspace(*p)) p++;
		if (*p) {
			*p = '\0';
			p++;
		}
		delim = strchr(buf, '/');
		if (delim == NULL) continue;
		*delim = '\0';

		mime_type = g_new(MimeType, 1);
		mime_type->type = g_strdup(buf);
		mime_type->sub_type = g_strdup(delim + 1);

		while (*p && g_ascii_isspace(*p)) p++;
		if (*p)
			mime_type->extension = g_strdup(p);
		else
			mime_type->extension = NULL;

		list = g_list_append(list, mime_type);
	}

	fclose(fp);

	if (!list)
		g_warning("Can't read mime.types\n");

	return list;
}

static GList *mailcap_list = NULL;

static GList *procmime_parse_mailcap(const gchar *file)
{
	GList *list = NULL;
	FILE *fp;
	gchar buf[BUFFSIZE];
	MailCap *mailcap;

	if ((fp = g_fopen(file, "rb")) == NULL) return NULL;

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		gint i;
		gchar *p;
		gchar **strv;

		p = strchr(buf, '#');
		if (p) *p = '\0';
		g_strstrip(buf);

		strv = strsplit_with_quote(buf, ";", 0);
		if (!strv)
			continue;

		for (i = 0; strv[i] != NULL; ++i)
			g_strstrip(strv[i]);

		if (!strv[0] || *strv[0] == '\0' ||
		    !strv[1] || *strv[1] == '\0') {
			g_strfreev(strv);
			continue;
		}

		mailcap = g_new(MailCap, 1);
		mailcap->mime_type = g_strdup(strv[0]);
		mailcap->cmdline_fmt = g_strdup(strv[1]);
		mailcap->needs_terminal = FALSE;

		for (i = 0; strv[i] != NULL; ++i) {
			if (strcmp(strv[i], "needsterminal") == 0)
				mailcap->needs_terminal = TRUE;
		}

		g_strfreev(strv);

		list = g_list_append(list, mailcap);
	}

	fclose(fp);

	return list;
}

gint procmime_execute_open_file(const gchar *file, const gchar *mime_type)
{
	gchar *mime_type_ = NULL;
	GList *cur;
	MailCap *mailcap;
	gchar *cmdline;
	gint ret = -1;
	static gboolean mailcap_list_init = FALSE;

	g_return_val_if_fail(file != NULL, -1);

	if (!mime_type ||
	    g_ascii_strcasecmp(mime_type, "application/octet-stream") == 0) {
		gchar *tmp;
		tmp = procmime_get_mime_type(file);
		if (!tmp)
			return -1;
		mime_type_ = g_ascii_strdown(tmp, -1);
		g_free(tmp);
	} else
		mime_type_ = g_ascii_strdown(mime_type, -1);

	if (!mailcap_list_init && !mailcap_list) {
		GList *list;
		gchar *path;

		path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, "mailcap",
				  NULL);
		mailcap_list = procmime_parse_mailcap(path);
		g_free(path);
#ifdef G_OS_WIN32
		path = g_strconcat(get_startup_dir(), G_DIR_SEPARATOR_S "etc"
				   G_DIR_SEPARATOR_S "mailcap", NULL);
		list = procmime_parse_mailcap(path);
		g_free(path);
#else
		if (!mailcap_list) {
			path = g_strconcat(get_home_dir(), G_DIR_SEPARATOR_S,
					   ".mailcap", NULL);
			mailcap_list = procmime_parse_mailcap(path);
			g_free(path);
		}
		list = procmime_parse_mailcap(SYSCONFDIR "/mailcap");
		if (!list)
			list = procmime_parse_mailcap("/etc/mailcap");
#endif
		mailcap_list = g_list_concat(mailcap_list, list);

		mailcap_list_init = TRUE;
	}

	for (cur = mailcap_list; cur != NULL; cur = cur->next) {
		mailcap = (MailCap *)cur->data;

		if (!g_pattern_match_simple(mailcap->mime_type, mime_type_))
			continue;
		if (mailcap->needs_terminal)
			continue;

		if (str_find_format_times(mailcap->cmdline_fmt, 's') == 1)
			cmdline = g_strdup_printf(mailcap->cmdline_fmt, file);
		else
			cmdline = g_strconcat(mailcap->cmdline_fmt, " \"", file,
					      "\"", NULL);
		ret = execute_command_line(cmdline, TRUE);
		g_free(cmdline);
		break;
	}

	g_free(mime_type_);

	return ret;
}

EncodingType procmime_get_encoding_for_charset(const gchar *charset)
{
	if (!charset)
		return ENC_8BIT;
	else if (!g_ascii_strncasecmp(charset, "ISO-2022-", 9) ||
		 !g_ascii_strcasecmp(charset, "US-ASCII"))
		return ENC_7BIT;
	else if (!g_ascii_strcasecmp(charset, "ISO-8859-5") ||
		 !g_ascii_strncasecmp(charset, "KOI8-", 5) ||
		 !g_ascii_strcasecmp(charset, "Windows-1251"))
		return ENC_8BIT;
	else if (!g_ascii_strncasecmp(charset, "ISO-8859-", 9))
		return ENC_QUOTED_PRINTABLE;
	else
		return ENC_8BIT;
}

EncodingType procmime_get_encoding_for_text_file(const gchar *file)
{
	FILE *fp;
	guchar buf[BUFFSIZE];
	size_t len;
	size_t octet_chars = 0;
	size_t total_len = 0;
	gfloat octet_percentage;

	if ((fp = g_fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return ENC_UNKNOWN;
	}

	while ((len = fread(buf, sizeof(guchar), sizeof(buf), fp)) > 0) {
		guchar *p;
		gint i;

		for (p = buf, i = 0; i < len; ++p, ++i) {
			if ((*p & 0x80) || *p == 0x00 || *p == 0x7f) {
				++octet_chars;
			}
		}
		total_len += len;
	}

	fclose(fp);

	if (total_len > 0)
		octet_percentage = (gfloat)octet_chars / (gfloat)total_len;
	else
		octet_percentage = 0.0;

	debug_print("procmime_get_encoding_for_text_file(): "
		    "8bit chars: %d / %d (%f%%)\n", octet_chars, total_len,
		    100.0 * octet_percentage);

	if (octet_percentage > 0.20) {
		debug_print("using BASE64\n");
		return ENC_BASE64;
	} else if (octet_chars > 0) {
		debug_print("using quoted-printable\n");
		return ENC_QUOTED_PRINTABLE;
	} else {
		debug_print("using 7bit\n");
		return ENC_7BIT;
	}
}

EncodingType procmime_get_encoding_for_str(const gchar *str)
{
	const guchar *p;
	size_t octet_chars = 0;
	size_t total_len = 0;
	gfloat octet_percentage;

	total_len = strlen(str);

	for (p = (const guchar *)str; *p != '\0'; ++p) {
		if ((*p & 0x80) || *p == 0x7f) {
			++octet_chars;
		}
	}

	if (total_len > 0)
		octet_percentage = (gfloat)octet_chars / (gfloat)total_len;
	else
		octet_percentage = 0.0;

	debug_print("procmime_get_encoding_for_str(): "
		    "8bit chars: %d / %d (%f%%)\n", octet_chars, total_len,
		    100.0 * octet_percentage);

	if (octet_percentage > 0.20) {
		debug_print("using BASE64\n");
		return ENC_BASE64;
	} else if (octet_chars > 0) {
		debug_print("using quoted-printable\n");
		return ENC_QUOTED_PRINTABLE;
	} else {
		debug_print("using 7bit\n");
		return ENC_7BIT;
	}
}

const gchar *procmime_get_encoding_str(EncodingType encoding)
{
	static const gchar *encoding_str[] = {
		"7bit", "8bit", "quoted-printable", "base64", "x-uuencode",
		NULL
	};

	if (encoding >= ENC_7BIT && encoding <= ENC_UNKNOWN)
		return encoding_str[encoding];
	else
		return NULL;
}
