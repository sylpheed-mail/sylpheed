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
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "xml.h"
#include "utils.h"
#include "codeconv.h"

#define SPARSE_MEMORY
/* if this is defined all attr.names and tag.names are stored
 * in a hash table */
#if defined(SPARSE_MEMORY)
#include "stringtable.h" 

static StringTable *xml_string_table;

static void xml_string_table_create(void)
{
	if (xml_string_table == NULL)
		xml_string_table = string_table_new();
}
#define XML_STRING_ADD(str) \
	string_table_insert_string(xml_string_table, (str))
#define XML_STRING_FREE(str) \
	string_table_free_string(xml_string_table, (str))

#define XML_STRING_TABLE_CREATE() \
	xml_string_table_create()

#else /* !SPARSE_MEMORY */

#define XML_STRING_ADD(str) \
	g_strdup(str)
#define XML_STRING_FREE(str) \
	g_free(str)

#define XML_STRING_TABLE_CREATE()

#endif /* SPARSE_MEMORY */

static void xml_free_tag	(XMLTag		*tag);
static gint xml_get_parenthesis	(XMLFile	*file,
				 gchar		*buf,
				 gint		 len);

XMLFile *xml_open_file(const gchar *path)
{
	XMLFile *newfile;

	g_return_val_if_fail(path != NULL, NULL);

	XML_STRING_TABLE_CREATE();

	newfile = g_new(XMLFile, 1);

	newfile->fp = g_fopen(path, "rb");
	if (!newfile->fp) {
		g_free(newfile);
		return NULL;
	}

	newfile->buf = g_string_new(NULL);
	newfile->bufp = newfile->buf->str;

	newfile->dtd = NULL;
	newfile->encoding = NULL;
	newfile->tag_stack = NULL;
	newfile->level = 0;
	newfile->is_empty_element = FALSE;

	return newfile;
}

void xml_close_file(XMLFile *file)
{
	g_return_if_fail(file != NULL);

	if (file->fp) fclose(file->fp);

	g_string_free(file->buf, TRUE);

	g_free(file->dtd);
	g_free(file->encoding);

	while (file->tag_stack != NULL)
		xml_pop_tag(file);

	g_free(file);
}

static GNode *xml_build_tree(XMLFile *file, GNode *parent, guint level)
{
	GNode *node = NULL;
	XMLNode *xmlnode;
	XMLTag *tag;

	while (xml_parse_next_tag(file) == 0) {
		if (file->level < level) break;
		if (file->level == level) {
			g_warning("xml_build_tree(): Parse error\n");
			break;
		}

		tag = xml_get_current_tag(file);
		if (!tag) break;
		xmlnode = xml_node_new(xml_copy_tag(tag), NULL);
		xmlnode->element = xml_get_element(file);
		if (!parent)
			node = g_node_new(xmlnode);
		else
			node = g_node_append_data(parent, xmlnode);

		xml_build_tree(file, node, file->level);
		if (file->level == 0) break;
	}

	return node;
}

GNode *xml_parse_file(const gchar *path)
{
	XMLFile *file;
	GNode *node;

	file = xml_open_file(path);
	g_return_val_if_fail(file != NULL, NULL);

	xml_get_dtd(file);

	node = xml_build_tree(file, NULL, file->level);

	xml_close_file(file);

#if defined(SPARSE_MEMORY)
	if (get_debug_mode())
		string_table_get_stats(xml_string_table);
#endif

	return node;
}

gint xml_get_dtd(XMLFile *file)
{
	gchar buf[XMLBUFSIZE];
	gchar *bufp = buf;

	if (xml_get_parenthesis(file, buf, sizeof(buf)) < 0) return -1;

	if ((*bufp++ == '?') &&
	    (bufp = strcasestr(bufp, "xml")) &&
	    (bufp = strcasestr(bufp + 3, "version")) &&
	    (bufp = strchr(bufp + 7, '?'))) {
		file->dtd = g_strdup(buf);
		if ((bufp = strcasestr(buf, "encoding=\""))) {
			bufp += 9;
			extract_quote(bufp, '"');
			file->encoding = g_strdup(bufp);
		} else
			file->encoding = g_strdup(CS_INTERNAL);
	} else {
		g_warning("Can't get xml dtd\n");
		return -1;
	}

	return 0;
}

gint xml_parse_next_tag(XMLFile *file)
{
	gchar buf[XMLBUFSIZE];
	gchar *bufp = buf;
	gchar *tag_str;
	XMLTag *tag;
	gint len;

	if (file->is_empty_element == TRUE) {
		file->is_empty_element = FALSE;
		xml_pop_tag(file);
		return 0;
	}

	if (xml_get_parenthesis(file, buf, sizeof(buf)) < 0) {
		g_warning("xml_parse_next_tag(): Can't parse next tag\n");
		return -1;
	}

	/* end-tag */
	if (buf[0] == '/') {
		if (strcmp(xml_get_current_tag(file)->tag, buf + 1) != 0) {
			g_warning("xml_parse_next_tag(): Tag name mismatch: %s\n", buf);
			return -1;
		}
		xml_pop_tag(file);
		return 0;
	}

	tag = xml_tag_new(NULL);
	xml_push_tag(file, tag);

	len = strlen(buf);
	if (len > 0 && buf[len - 1] == '/') {
		file->is_empty_element = TRUE;
		buf[len - 1] = '\0';
		g_strchomp(buf);
	}
	if (strlen(buf) == 0) {
		g_warning("xml_parse_next_tag(): Tag name is empty\n");
		return -1;
	}

	while (*bufp != '\0' && !g_ascii_isspace(*bufp)) bufp++;
	if (*bufp == '\0') {
		tag_str = conv_codeset_strdup(buf, file->encoding, CS_INTERNAL);
		if (tag_str) {
			tag->tag = XML_STRING_ADD(tag_str);
			g_free(tag_str);
		} else
			tag->tag = XML_STRING_ADD(buf);
		return 0;
	} else {
		*bufp++ = '\0';
		tag_str = conv_codeset_strdup(buf, file->encoding, CS_INTERNAL);
		if (tag_str) {
			tag->tag = XML_STRING_ADD(tag_str);
			g_free(tag_str);
		} else
			tag->tag = XML_STRING_ADD(buf);
	}

	/* parse attributes ( name=value ) */
	while (*bufp) {
		XMLAttr *attr;
		gchar *attr_name;
		gchar *attr_value;
		gchar *utf8_attr_name;
		gchar *utf8_attr_value;
		gchar *p;
		gchar quote;

		while (g_ascii_isspace(*bufp)) bufp++;
		attr_name = bufp;
		if ((p = strchr(attr_name, '=')) == NULL) {
			g_warning("xml_parse_next_tag(): Syntax error in tag\n");
			return -1;
		}
		bufp = p;
		*bufp++ = '\0';
		while (g_ascii_isspace(*bufp)) bufp++;

		if (*bufp != '"' && *bufp != '\'') {
			g_warning("xml_parse_next_tag(): Syntax error in tag\n");
			return -1;
		}
		quote = *bufp;
		bufp++;
		attr_value = bufp;
		if ((p = strchr(attr_value, quote)) == NULL) {
			g_warning("xml_parse_next_tag(): Syntax error in tag\n");
			return -1;
		}
		bufp = p;
		*bufp++ = '\0';

		g_strchomp(attr_name);
		xml_unescape_str(attr_value);
		utf8_attr_name = conv_codeset_strdup
			(attr_name, file->encoding, CS_INTERNAL);
		utf8_attr_value = conv_codeset_strdup
			(attr_value, file->encoding, CS_INTERNAL);
		if (!utf8_attr_name)
			utf8_attr_name = g_strdup(attr_name);
		if (!utf8_attr_value)
			utf8_attr_value = g_strdup(attr_value);

		attr = xml_attr_new(utf8_attr_name, utf8_attr_value);
		xml_tag_add_attr(tag, attr);

		g_free(utf8_attr_value);
		g_free(utf8_attr_name);
	}

	return 0;
}

void xml_push_tag(XMLFile *file, XMLTag *tag)
{
	g_return_if_fail(tag != NULL);

	file->tag_stack = g_list_prepend(file->tag_stack, tag);
	file->level++;
}

void xml_pop_tag(XMLFile *file)
{
	XMLTag *tag;

	if (!file->tag_stack) return;

	tag = (XMLTag *)file->tag_stack->data;

	xml_free_tag(tag);
	file->tag_stack = g_list_remove(file->tag_stack, tag);
	file->level--;
}

XMLTag *xml_get_current_tag(XMLFile *file)
{
	if (file->tag_stack)
		return (XMLTag *)file->tag_stack->data;
	else
		return NULL;
}

GList *xml_get_current_tag_attr(XMLFile *file)
{
	XMLTag *tag;

	tag = xml_get_current_tag(file);
	if (!tag) return NULL;

	return tag->attr;
}

gchar *xml_get_element(XMLFile *file)
{
	gchar *str;
	gchar *new_str;
	gchar *end;

	while ((end = strchr(file->bufp, '<')) == NULL)
		if (xml_read_line(file) < 0) return NULL;

	if (end == file->bufp)
		return NULL;

	str = g_strndup(file->bufp, end - file->bufp);
	/* this is not XML1.0 strict */
	g_strstrip(str);
	xml_unescape_str(str);

	file->bufp = end;
	xml_truncate_buf(file);

	if (str[0] == '\0') {
		g_free(str);
		return NULL;
	}

	new_str = conv_codeset_strdup(str, file->encoding, CS_INTERNAL);
	if (!new_str)
		new_str = g_strdup(str);
	g_free(str);

	return new_str;
}

gint xml_read_line(XMLFile *file)
{
	gchar buf[XMLBUFSIZE];
	gint index;

	if (fgets(buf, sizeof(buf), file->fp) == NULL)
		return -1;

	index = file->bufp - file->buf->str;

	g_string_append(file->buf, buf);

	file->bufp = file->buf->str + index;

	return 0;
}

void xml_truncate_buf(XMLFile *file)
{
	gint len;

	len = file->bufp - file->buf->str;
	if (len > 0) {
		g_string_erase(file->buf, 0, len);
		file->bufp = file->buf->str;
	}
}

gboolean xml_compare_tag(XMLFile *file, const gchar *name)
{
	XMLTag *tag;

	tag = xml_get_current_tag(file);

	if (tag && strcmp(tag->tag, name) == 0)
		return TRUE;
	else
		return FALSE;
}

XMLNode *xml_node_new(XMLTag *tag, const gchar *text)
{
	XMLNode *node;

	node = g_new(XMLNode, 1);
	node->tag = tag;
	node->element = g_strdup(text);

	return node;
}

XMLTag *xml_tag_new(const gchar *tag)
{
	XMLTag *new_tag;

	new_tag = g_new(XMLTag, 1);
	if (tag)
		new_tag->tag = XML_STRING_ADD(tag);
	else
		new_tag->tag = NULL;
	new_tag->attr = NULL;

	return new_tag;
}

XMLAttr *xml_attr_new(const gchar *name, const gchar *value)
{
	XMLAttr *new_attr;

	new_attr = g_new(XMLAttr, 1);
	new_attr->name = XML_STRING_ADD(name);
	new_attr->value = g_strdup(value);

	return new_attr;
}

void xml_tag_add_attr(XMLTag *tag, XMLAttr *attr)
{
	tag->attr = g_list_append(tag->attr, attr);
}

XMLTag *xml_copy_tag(XMLTag *tag)
{
	XMLTag *new_tag;
	XMLAttr *attr;
	GList *list;

	new_tag = xml_tag_new(tag->tag);
	for (list = tag->attr; list != NULL; list = list->next) {
		attr = xml_copy_attr((XMLAttr *)list->data);
		xml_tag_add_attr(new_tag, attr);
	}

	return new_tag;
}

XMLAttr *xml_copy_attr(XMLAttr *attr)
{
	return xml_attr_new(attr->name, attr->value);
}

gint xml_unescape_str(gchar *str)
{
	gchar *start;
	gchar *end;
	gchar *p = str;
	gchar ch;
	gint len;

	while ((start = strchr(p, '&')) != NULL) {
		if ((end = strchr(start + 1, ';')) == NULL) {
			g_warning("Unescaped `&' appeared\n");
			p = start + 1;
			continue;
		}
		len = end - start + 1;
		if (len < 3) {
			p = end + 1;
			continue;
		}

		if (!strncmp(start, "&lt;", 4))
			ch = '<';
		else if (!strncmp(start, "&gt;", 4))
			ch = '>';
		else if (!strncmp(start, "&amp;", 5))
			ch = '&';
		else if (!strncmp(start, "&apos;", 6))
			ch = '\'';
		else if (!strncmp(start, "&quot;", 6))
			ch = '\"';
		else {
			p = end + 1;
			continue;
		}

		*start = ch;
		memmove(start + 1, end + 1, strlen(end + 1) + 1);
		p = start + 1;
	}

	return 0;
}

gchar *xml_escape_str(const gchar *str)
{
	GString *estr;
	const gchar *p;

	if (!str)
		return NULL;

	estr = g_string_sized_new(strlen(str));
	for (p = str; *p != '\0'; p++) {
		switch (*p) {
		case '<':
			g_string_append(estr, "&lt;");
			break;
		case '>':
			g_string_append(estr, "&gt;");
			break;
		case '&':
			g_string_append(estr, "&amp;");
			break;
		case '\'':
			g_string_append(estr, "&apos;");
			break;
		case '\"':
			g_string_append(estr, "&quot;");
			break;
		default:
			g_string_append_c(estr, *p);
			break;
		}
	}

	return g_string_free(estr, FALSE);
}

gint xml_file_put_escape_str(FILE *fp, const gchar *str)
{
	const gchar *p;

	g_return_val_if_fail(fp != NULL, -1);

	if (!str) return 0;

	for (p = str; *p != '\0'; p++) {
		switch (*p) {
		case '<':
			fputs("&lt;", fp);
			break;
		case '>':
			fputs("&gt;", fp);
			break;
		case '&':
			fputs("&amp;", fp);
			break;
		case '\'':
			fputs("&apos;", fp);
			break;
		case '\"':
			fputs("&quot;", fp);
			break;
		default:
			fputc(*p, fp);
		}
	}

	return 0;
}

gint xml_file_put_xml_decl(FILE *fp)
{
	g_return_val_if_fail(fp != NULL, -1);

	fprintf(fp, "<?xml version=\"1.0\" encoding=\"%s\"?>\n", CS_INTERNAL);
	return 0;
}

gint xml_file_put_node(FILE *fp, XMLNode *node)
{
	GList *cur;

	g_return_val_if_fail(fp != NULL, -1);
	g_return_val_if_fail(node != NULL, -1);

	fprintf(fp, "<%s", node->tag->tag);

	for (cur = node->tag->attr; cur != NULL; cur = cur->next) {
		XMLAttr *attr = (XMLAttr *)cur->data;
		fprintf(fp, " %s=\"", attr->name);
		xml_file_put_escape_str(fp, attr->value);
		fputs("\"", fp);
	}

	if (node->element) {
		fputs(">", fp);
		xml_file_put_escape_str(fp, node->element);
		fprintf(fp, "</%s>\n", node->tag->tag);
	} else {
		fputs(" />\n", fp);
	}

	return 0;
}

void xml_free_node(XMLNode *node)
{
	if (!node) return;

	xml_free_tag(node->tag);
	g_free(node->element);
	g_free(node);
}

static gboolean xml_free_func(GNode *node, gpointer data)
{
	XMLNode *xmlnode = node->data;

	xml_free_node(xmlnode);
	return FALSE;
}

void xml_free_tree(GNode *node)
{
	g_return_if_fail(node != NULL);

	g_node_traverse(node, G_PRE_ORDER, G_TRAVERSE_ALL, -1, xml_free_func,
			NULL);

	g_node_destroy(node);
}

static void xml_free_tag(XMLTag *tag)
{
	if (!tag) return;

	XML_STRING_FREE(tag->tag);
	while (tag->attr != NULL) {
		XMLAttr *attr = (XMLAttr *)tag->attr->data;
		XML_STRING_FREE(attr->name);
		g_free(attr->value);
		g_free(attr);
		tag->attr = g_list_remove(tag->attr, tag->attr->data);
	}
	g_free(tag);
}

static gint xml_get_parenthesis(XMLFile *file, gchar *buf, gint len)
{
	gchar *start;
	gchar *end;

	buf[0] = '\0';

	while ((start = strchr(file->bufp, '<')) == NULL)
		if (xml_read_line(file) < 0) return -1;

	start++;
	file->bufp = start;

	while ((end = strchr(file->bufp, '>')) == NULL)
		if (xml_read_line(file) < 0) return -1;

	strncpy2(buf, file->bufp, MIN(end - file->bufp + 1, len));
	g_strstrip(buf);
	file->bufp = end + 1;
	xml_truncate_buf(file);

	return 0;
}
