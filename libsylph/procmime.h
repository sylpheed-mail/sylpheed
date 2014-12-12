/*
 * LibSylph -- E-Mail client library
 * Copyright (C) 1999-2010 Hiroyuki Yamamoto
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

#ifndef __PROCMIME_H__
#define __PROCMIME_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <stdio.h>

typedef struct _MimeType	MimeType;
typedef struct _MailCap		MailCap;
typedef struct _MimeInfo	MimeInfo;

#include "procmsg.h"
#include "utils.h"

typedef enum
{
	ENC_7BIT,
	ENC_8BIT,
	ENC_QUOTED_PRINTABLE,
	ENC_BASE64,
	ENC_X_UUENCODE,
	ENC_UNKNOWN
} EncodingType;

typedef enum
{
	MIME_TEXT,
	MIME_TEXT_HTML,
	MIME_MESSAGE_RFC822,
	MIME_APPLICATION,
	MIME_APPLICATION_OCTET_STREAM,
	MIME_MULTIPART,
	MIME_IMAGE,
	MIME_AUDIO,
	MIME_VIDEO,
	MIME_UNKNOWN
} ContentType;

struct _MimeType
{
	gchar *type;
	gchar *sub_type;

	gchar *extension;
};

struct _MailCap
{
	gchar *mime_type;
	gchar *cmdline_fmt;
	gboolean needs_terminal;
};

/*
 * An example of MimeInfo structure:
 *
 * multipart/mixed            root  <-+ parent
 *                                    |
 *   multipart/alternative      children <-+ parent
 *                                         |
 *     text/plain                 children  --+
 *                                            |
 *     text/html                  next      <-+
 *
 *   message/rfc822             next  <-+ main
 *                                      |
 *                                sub (capsulated message)
 *
 *   image/jpeg                 next
 */

struct _MimeInfo
{
	gchar *encoding;

	EncodingType encoding_type;
	ContentType  mime_type;

	gchar *content_type;
	gchar *charset;
	gchar *name;
	gchar *boundary;

	gchar *content_disposition;
	gchar *filename;

	glong fpos;
	guint size;
	guint content_size;

	MimeInfo *main;
	MimeInfo *sub;

	MimeInfo *next;
	MimeInfo *parent;
	MimeInfo *children;

	MimeInfo *plaintext;
	gchar *sigstatus;
	gchar *sigstatus_full;

	gint level;
};

#define IS_BOUNDARY(s, bnd, len) \
	(bnd && s[0] == '-' && s[1] == '-' && !strncmp(s + 2, bnd, len))

/* MimeInfo handling */

MimeInfo *procmime_mimeinfo_new		(void);
void procmime_mimeinfo_free_all		(MimeInfo	*mimeinfo);

MimeInfo *procmime_mimeinfo_insert	(MimeInfo	*parent,
					 MimeInfo	*mimeinfo);
#if 0
void procmime_mimeinfo_replace		(MimeInfo	*old,
					 MimeInfo	*new);
#endif

MimeInfo *procmime_mimeinfo_next	(MimeInfo	*mimeinfo);

MimeInfo *procmime_scan_message		(MsgInfo	*msginfo);
MimeInfo *procmime_scan_message_stream	(FILE		*fp);
void procmime_scan_multipart_message	(MimeInfo	*mimeinfo,
					 FILE		*fp);

/* scan headers */

void procmime_scan_encoding		(MimeInfo	*mimeinfo,
					 const gchar	*encoding);
void procmime_scan_content_type		(MimeInfo	*mimeinfo,
					 const gchar	*content_type);
void procmime_scan_content_type_str	(const gchar	*content_type,
					 gchar	       **mime_type,
					 gchar	       **charset,
					 gchar	       **name,
					 gchar	       **boundary);
void procmime_scan_content_type_partial	(const gchar	*content_type,
					 gint		*total,
					 gchar	       **part_id,
					 gint		*number);
void procmime_scan_content_disposition	(MimeInfo	*mimeinfo,
					 const gchar	*content_disposition);
MimeInfo *procmime_scan_mime_header	(FILE		*fp);

FILE *procmime_decode_content		(FILE		*outfp,
					 FILE		*infp,
					 MimeInfo	*mimeinfo);
gint procmime_get_part			(const gchar	*outfile,
					 const gchar	*infile,
					 MimeInfo	*mimeinfo);
gint procmime_get_part_fp		(const gchar	*outfile,
					 FILE		*infp,
					 MimeInfo	*mimeinfo);
FILE *procmime_get_part_fp_fp		(FILE		*outfp,
					 FILE		*infp,
					 MimeInfo	*mimeinfo);
gint procmime_get_all_parts		(const gchar	*dir,
					 const gchar	*infile,
					 MimeInfo	*mimeinfo);
FILE *procmime_get_text_content		(MimeInfo	*mimeinfo,
					 FILE		*infp,
					 const gchar	*encoding);
FILE *procmime_get_first_text_content	(MsgInfo	*msginfo,
					 const gchar	*encoding);

gboolean procmime_find_string_part	(MimeInfo	*mimeinfo,
					 const gchar	*filename,
					 const gchar	*str,
					 StrFindFunc	 find_func);
gboolean procmime_find_string		(MsgInfo	*msginfo,
					 const gchar	*str,
					 StrFindFunc	 find_func);

gchar *procmime_get_part_file_name	(MimeInfo	*mimeinfo);
gchar *procmime_get_tmp_file_name	(MimeInfo	*mimeinfo);
gchar *procmime_get_tmp_file_name_for_user
					(MimeInfo	*mimeinfo);

ContentType procmime_scan_mime_type	(const gchar	*mime_type);
gchar *procmime_get_mime_type		(const gchar	*filename);

gint procmime_execute_open_file		(const gchar	*file,
					 const gchar	*mime_type);

EncodingType procmime_get_encoding_for_charset	(const gchar	*charset);
EncodingType procmime_get_encoding_for_text_file(const gchar	*file);
EncodingType procmime_get_encoding_for_str	(const gchar	*str);
const gchar *procmime_get_encoding_str		(EncodingType	 encoding);

#endif /* __PROCMIME_H__ */
