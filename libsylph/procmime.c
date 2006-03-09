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

		g_free(mimeinfo->plaintextfile);
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

void procmime_scan_multipart_message(MimeInfo *mimeinfo, FILE *fp)
{
	gchar *p;
	gchar *boundary;
	gint boundary_len = 0;
	gchar buf[BUFFSIZE];
	glong fpos, prev_fpos;

	g_return_if_fail(mimeinfo != NULL);
	g_return_if_fail(mimeinfo->mime_type == MIME_MULTIPART ||
			 mimeinfo->mime_type == MIME_MESSAGE_RFC822);

	if (mimeinfo->mime_type == MIME_MULTIPART) {
		g_return_if_fail(mimeinfo->boundary != NULL);
		g_return_if_fail(mimeinfo->sub == NULL);
	}
	g_return_if_fail(fp != NULL);

	boundary = mimeinfo->boundary;

	if (boundary) {
		boundary_len = strlen(boundary);

		/* look for first boundary */
		while ((p = fgets(buf, sizeof(buf), fp)) != NULL)
			if (IS_BOUNDARY(buf, boundary, boundary_len)) break;
		if (!p) return;
	} else if (mimeinfo->parent && mimeinfo->parent->boundary) {
		boundary = mimeinfo->parent->boundary;
		boundary_len = strlen(boundary);
	}

	if ((fpos = ftell(fp)) < 0) {
		perror("ftell");
		return;
	}

	for (;;) {
		MimeInfo *partinfo;
		gboolean eom = FALSE;
		glong content_pos;
		gboolean is_base64;
		gint len;
		guint b64_content_len = 0;
		gint b64_pad_len = 0;

		prev_fpos = fpos;
		debug_print("prev_fpos: %ld\n", fpos);

		/* scan part header */
		if (mimeinfo->mime_type == MIME_MESSAGE_RFC822) {
			MimeInfo *sub;

			mimeinfo->sub = sub = procmime_scan_mime_header(fp);
			if (!sub) break;

			debug_print("message/rfc822 part found\n");
			sub->level = mimeinfo->level + 1;
			sub->parent = mimeinfo->parent;
			sub->main = mimeinfo;

			partinfo = sub;
		} else {
			partinfo = procmime_scan_mime_header(fp);
			if (!partinfo) break;
			procmime_mimeinfo_insert(mimeinfo, partinfo);
			debug_print("content-type: %s\n",
				    partinfo->content_type);
		}

		/* begin content */
		content_pos = ftell(fp);
		debug_print("content_pos: %ld\n", content_pos);

		if (partinfo->mime_type == MIME_MULTIPART ||
		    partinfo->mime_type == MIME_MESSAGE_RFC822) {
			if (partinfo->level < 8)
				procmime_scan_multipart_message(partinfo, fp);
		}

		/* look for next boundary */
		buf[0] = '\0';
		is_base64 = partinfo->encoding_type == ENC_BASE64;
		while ((p = fgets(buf, sizeof(buf), fp)) != NULL) {
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
		debug_print("boundary: %s\n", buf);

		fpos = ftell(fp);
		debug_print("fpos: %ld\n", fpos);

		len = strlen(buf);
		partinfo->size = fpos - prev_fpos - len;
		if (is_base64)
			partinfo->content_size =
				b64_content_len / 4 * 3 - b64_pad_len;
		else
			partinfo->content_size = fpos - content_pos - len;
		debug_print("partinfo->size: %d\n", partinfo->size);
		debug_print("partinfo->content_size: %d\n",
			    partinfo->content_size);
		if (partinfo->sub && !partinfo->sub->sub &&
		    !partinfo->sub->children) {
			partinfo->sub->size =
				fpos - partinfo->sub->fpos - strlen(buf);
			debug_print("partinfo->sub->size: %d\n",
				    partinfo->sub->size);
		}

		if (mimeinfo->mime_type == MIME_MESSAGE_RFC822) {
			if (len > 0 && fseek(fp, fpos - len, SEEK_SET) < 0)
				perror("fseek");
			break;
		}

		if (eom) break;
	}
}

void procmime_scan_encoding(MimeInfo *mimeinfo, const gchar *encoding)
{
	gchar *buf;

	Xstrdup_a(buf, encoding, return);

	g_free(mimeinfo->encoding);

	mimeinfo->encoding = g_strdup(g_strstrip(buf));
	if (!g_ascii_strcasecmp(buf, "7bit"))
		mimeinfo->encoding_type = ENC_7BIT;
	else if (!g_ascii_strcasecmp(buf, "8bit"))
		mimeinfo->encoding_type = ENC_8BIT;
	else if (!g_ascii_strcasecmp(buf, "quoted-printable"))
		mimeinfo->encoding_type = ENC_QUOTED_PRINTABLE;
	else if (!g_ascii_strcasecmp(buf, "base64"))
		mimeinfo->encoding_type = ENC_BASE64;
	else if (!g_ascii_strcasecmp(buf, "x-uuencode"))
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

void procmime_scan_content_type_str(const gchar *content_type,
				    gchar **mime_type, gchar **charset,
				    gchar **name, gchar **boundary)
{
	gchar *delim, *p;
	gchar *buf;

	Xstrdup_a(buf, content_type, return);

	if ((delim = strchr(buf, ';'))) *delim = '\0';
	if (mime_type)
		*mime_type = g_strdup(g_strstrip(buf));

	if (!delim) return;
	p = delim + 1;

	for (;;) {
		gchar *eq;
		gchar *attr, *value;

		if ((delim = strchr(p, ';'))) *delim = '\0';

		if (!(eq = strchr(p, '='))) break;

		*eq = '\0';
		attr = p;
		g_strstrip(attr);
		value = eq + 1;
		g_strstrip(value);

		if (*value == '"')
			extract_quote(value, '"');
		else {
			eliminate_parenthesis(value, '(', ')');
			g_strstrip(value);
		}

		if (*value) {
			if (charset && !g_ascii_strcasecmp(attr, "charset"))
				*charset = g_strdup(value);
			else if (name && !g_ascii_strcasecmp(attr, "name"))
				*name = conv_unmime_header(value, NULL);
			else if (boundary &&
				 !g_ascii_strcasecmp(attr, "boundary"))
				*boundary = g_strdup(value);
		}

		if (!delim) break;
		p = delim + 1;
	}
}

void procmime_scan_content_disposition(MimeInfo *mimeinfo,
				       const gchar *content_disposition)
{
	gchar *delim, *p, *dispos;
	gchar *buf;

	Xstrdup_a(buf, content_disposition, return);

	if ((delim = strchr(buf, ';'))) *delim = '\0';
	mimeinfo->content_disposition = dispos = g_strdup(g_strstrip(buf));

	if (!delim) return;
	p = delim + 1;

	for (;;) {
		gchar *eq;
		gchar *attr, *value;

		if ((delim = strchr(p, ';'))) *delim = '\0';

		if (!(eq = strchr(p, '='))) break;

		*eq = '\0';
		attr = p;
		g_strstrip(attr);
		value = eq + 1;
		g_strstrip(value);

		if (*value == '"')
			extract_quote(value, '"');
		else {
			eliminate_parenthesis(value, '(', ')');
			g_strstrip(value);
		}

		if (*value) {
			if (!g_ascii_strcasecmp(attr, "filename")) {
				g_free(mimeinfo->filename);
				mimeinfo->filename =
					conv_unmime_header(value, NULL);
				break;
			}
		}

		if (!delim) break;
		p = delim + 1;
	}
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
	    mimeinfo->name) {
		const gchar *type;
		type = procmime_get_mime_type(mimeinfo->name);
		if (type)
			mimeinfo->mime_type = procmime_scan_mime_type(type);
	}

	if (!mimeinfo->content_type)
		mimeinfo->content_type = g_strdup("text/plain");

	return mimeinfo;
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
			len = qp_decode_line(buf);
			fwrite(buf, len, 1, tmpfp);
		}

		if (normalize_lbreak) {
			rewind(tmpfp);
			while (fgets(buf, sizeof(buf), tmpfp) != NULL) {
#ifdef G_OS_WIN32
				strretchomp(buf);
				fputs(buf, outfp);
				fputs("\r\n", outfp);
#else
				strcrchomp(buf);
				fputs(buf, outfp);
#endif
			}
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
			rewind(tmpfp);
			while (fgets(buf, sizeof(buf), tmpfp) != NULL) {
#ifdef G_OS_WIN32
				strretchomp(buf);
				fputs(buf, outfp);
				fputs("\r\n", outfp);
#else
				strcrchomp(buf);
				fputs(buf, outfp);
#endif
			}
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
		while (fgets(buf, sizeof(buf), infp) != NULL &&
		       (!boundary ||
			!IS_BOUNDARY(buf, boundary, boundary_len))) {
			if (normalize_lbreak) {
#ifdef G_OS_WIN32
				strretchomp(buf);
				fputs(buf, outfp);
				fputs("\r\n", outfp);
#else
				strcrchomp(buf);
				fputs(buf, outfp);
#endif
			} else
				fputs(buf, outfp);
		}
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

	procmime_decode_content(outfp, infp, mimeinfo);

	if (fclose(outfp) == EOF) {
		FILE_OP_ERROR(outfile, "fclose");
		g_unlink(outfile);
		return -1;
	}

	return 0;
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
	gchar *filename;
	gchar f_prefix[10];

	g_return_val_if_fail(mimeinfo != NULL, NULL);

	g_snprintf(f_prefix, sizeof(f_prefix), "%08x.", id++);

	if (MIME_TEXT_HTML == mimeinfo->mime_type)
		base = g_strdup("mimetmp.html");
	else
		base = procmime_get_part_file_name(mimeinfo);

	filename = g_strconcat(get_mime_tmp_dir(), G_DIR_SEPARATOR_S,
			       f_prefix, base, NULL);

	g_free(base);

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
	gchar *ext;
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

	Xstrdup_a(ext, p + 1, return NULL);
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
			if (*p & 0x80)
				++octet_chars;
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
