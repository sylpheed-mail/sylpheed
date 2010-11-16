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

#ifndef __XML_H__
#define __XML_H__

#include <glib.h>
#include <stdio.h>

#define XMLBUFSIZE	8192

typedef struct _XMLAttr		XMLAttr;
typedef struct _XMLTag		XMLTag;
typedef struct _XMLNode		XMLNode;
typedef struct _XMLFile		XMLFile;

struct _XMLAttr
{
	gchar *name;
	gchar *value;
};

struct _XMLTag
{
	gchar *tag;
	GList *attr;
};

struct _XMLNode
{
	XMLTag *tag;
	gchar *element;
};

struct _XMLFile
{
	FILE *fp;

	GString *buf;
	gchar *bufp;

	gchar *dtd;
	gchar *encoding;

	GList *tag_stack;
	guint level;

	gboolean is_empty_element;
};

XMLFile *xml_open_file		(const gchar	*path);
void     xml_close_file		(XMLFile	*file);
GNode   *xml_parse_file		(const gchar	*path);

gint xml_get_dtd		(XMLFile	*file);
gint xml_parse_next_tag		(XMLFile	*file);
void xml_push_tag		(XMLFile	*file,
				 XMLTag		*tag);
void xml_pop_tag		(XMLFile	*file);

XMLTag *xml_get_current_tag	(XMLFile	*file);
GList  *xml_get_current_tag_attr(XMLFile	*file);
gchar  *xml_get_element		(XMLFile	*file);

gint xml_read_line		(XMLFile	*file);
void xml_truncate_buf		(XMLFile	*file);
gboolean  xml_compare_tag	(XMLFile	*file,
				 const gchar	*name);

XMLNode *xml_node_new		(XMLTag		*tag,
				 const gchar	*text);
XMLTag *xml_tag_new		(const gchar	*tag);
XMLAttr *xml_attr_new		(const gchar	*name,
				 const gchar	*value);
void xml_tag_add_attr		(XMLTag		*tag,
				 XMLAttr	*attr);

XMLTag  *xml_copy_tag		(XMLTag		*tag);
XMLAttr *xml_copy_attr		(XMLAttr	*attr);

gint xml_unescape_str		(gchar		*str);
gchar *xml_escape_str		(const gchar	*str);
gint xml_file_put_escape_str	(FILE		*fp,
				 const gchar	*str);

gint xml_file_put_xml_decl	(FILE		*fp);
gint xml_file_put_node		(FILE		*fp,
				 XMLNode	*node);

void xml_free_node		(XMLNode	*node);
void xml_free_tree		(GNode		*node);

#endif /* __XML_H__ */
