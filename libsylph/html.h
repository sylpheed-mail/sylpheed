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

#ifndef __HTML_H__
#define __HTML_H__

#include <glib.h>
#include <stdio.h>

#include "codeconv.h"

typedef enum
{
	HTML_NORMAL,
	HTML_PAR,
	HTML_BR,
	HTML_HR,
	HTML_HREF,
	HTML_IMG,
	HTML_FONT,
	HTML_PRE,
	HTML_UNKNOWN,
	HTML_CONV_FAILED,
	HTML_ERR,
	HTML_EOF,
	HTML_BLOCKQUOTE
} HTMLState;

typedef struct _HTMLParser	HTMLParser;
typedef struct _HTMLAttr	HTMLAttr;
typedef struct _HTMLTag		HTMLTag;

struct _HTMLParser
{
	FILE *fp;
	CodeConverter *conv;

	GHashTable *symbol_table;

	GString *str;
	GString *buf;

	gchar *bufp;

	HTMLState state;

	gchar *href;

	gboolean newline;
	gboolean empty_line;
	gboolean space;
	gboolean pre;
	gint blockquote;
};

struct _HTMLAttr
{
	gchar *name;
	gchar *value;
};

struct _HTMLTag
{
	gchar *name;
	GList *attr;
};

HTMLParser *html_parser_new	(FILE		*fp,
				 CodeConverter	*conv);
void html_parser_destroy	(HTMLParser	*parser);
const gchar *html_parse		(HTMLParser	*parser);

#endif /* __HTML_H__ */
