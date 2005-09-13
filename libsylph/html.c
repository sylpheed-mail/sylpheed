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

#include "html.h"
#include "codeconv.h"
#include "utils.h"

#define HTMLBUFSIZE	8192
#define HR_STR		"------------------------------------------------"

typedef struct _HTMLSymbol	HTMLSymbol;

struct _HTMLSymbol
{
	gchar *const key;
	gchar *const val;
};

static HTMLSymbol symbol_list[] = {
	{"&lt;"    , "<"},
	{"&gt;"    , ">"},
	{"&amp;"   , "&"},
	{"&quot;"  , "\""},
	{"&nbsp;"  , " "},
	{"&trade;" , "(TM)"},

	{"&#153;", "(TM)"},
};

static HTMLSymbol ascii_symbol_list[] = {
	{"&iexcl;" , "^!"},
	{"&brvbar;", "|"},
	{"&copy;"  , "(C)"},
	{"&laquo;" , "<<"},
	{"&reg;"   , "(R)"},

	{"&sup2;"  , "^2"},
	{"&sup3;"  , "^3"},
	{"&acute;" , "'"},
	{"&cedil;" , ","},
	{"&sup1;"  , "^1"},
	{"&raquo;" , ">>"},
	{"&frac14;", "1/4"},
	{"&frac12;", "1/2"},
	{"&frac34;", "3/4"},
	{"&iquest;", "^?"},

	{"&Agrave;", "A`"},
	{"&Aacute;", "A'"},
	{"&Acirc;" , "A^"},
	{"&Atilde;", "A~"},
	{"&AElig;" , "AE"},
	{"&Egrave;", "E`"},
	{"&Eacute;", "E'"},
	{"&Ecirc;" , "E^"},
	{"&Igrave;", "I`"},
	{"&Iacute;", "I'"},
	{"&Icirc;" , "I^"},

	{"&Ntilde;", "N~"},
	{"&Ograve;", "O`"},
	{"&Oacute;", "O'"},
	{"&Ocirc;" , "O^"},
	{"&Otilde;", "O~"},
	{"&Ugrave;", "U`"},
	{"&Uacute;", "U'"},
	{"&Ucirc;" , "U^"},
	{"&Yacute;", "Y'"},

	{"&agrave;", "a`"},
	{"&aacute;", "a'"},
	{"&acirc;" , "a^"},
	{"&atilde;", "a~"},
	{"&aelig;" , "ae"},
	{"&egrave;", "e`"},
	{"&eacute;", "e'"},
	{"&ecirc;" , "e^"},
	{"&igrave;", "i`"},
	{"&iacute;", "i'"},
	{"&icirc;" , "i^"},

	{"&ntilde;", "n~"},
	{"&ograve;", "o`"},
	{"&oacute;", "o'"},
	{"&ocirc;" , "o^"},
	{"&otilde;", "o~"},
	{"&ugrave;", "u`"},
	{"&uacute;", "u'"},
	{"&ucirc;" , "u^"},
	{"&yacute;", "y'"},
};

static GHashTable *default_symbol_table;

static HTMLState html_read_line		(HTMLParser	*parser);
static void html_append_char		(HTMLParser	*parser,
					 gchar		 ch);
static void html_append_str		(HTMLParser	*parser,
					 const gchar	*str,
					 gint		 len);
static HTMLState html_parse_tag		(HTMLParser	*parser);
static void html_parse_special		(HTMLParser	*parser);
static void html_get_parenthesis	(HTMLParser	*parser,
					 gchar		*buf,
					 gint		 len);


HTMLParser *html_parser_new(FILE *fp, CodeConverter *conv)
{
	HTMLParser *parser;

	g_return_val_if_fail(fp != NULL, NULL);
	g_return_val_if_fail(conv != NULL, NULL);

	parser = g_new0(HTMLParser, 1);
	parser->fp = fp;
	parser->conv = conv;
	parser->str = g_string_new(NULL);
	parser->buf = g_string_new(NULL);
	parser->bufp = parser->buf->str;
	parser->state = HTML_NORMAL;
	parser->href = NULL;
	parser->newline = TRUE;
	parser->empty_line = TRUE;
	parser->space = FALSE;
	parser->pre = FALSE;

#define SYMBOL_TABLE_ADD(table, list) \
{ \
	gint i; \
 \
	for (i = 0; i < sizeof(list) / sizeof(list[0]); i++) \
		g_hash_table_insert(table, list[i].key, list[i].val); \
}

	if (!default_symbol_table) {
		default_symbol_table =
			g_hash_table_new(g_str_hash, g_str_equal);
		SYMBOL_TABLE_ADD(default_symbol_table, symbol_list);
		SYMBOL_TABLE_ADD(default_symbol_table, ascii_symbol_list);
	}

#undef SYMBOL_TABLE_ADD

	parser->symbol_table = default_symbol_table;

	return parser;
}

void html_parser_destroy(HTMLParser *parser)
{
	g_string_free(parser->str, TRUE);
	g_string_free(parser->buf, TRUE);
	g_free(parser->href);
	g_free(parser);
}

const gchar *html_parse(HTMLParser *parser)
{
	parser->state = HTML_NORMAL;
	g_string_truncate(parser->str, 0);

	if (*parser->bufp == '\0') {
		g_string_truncate(parser->buf, 0);
		parser->bufp = parser->buf->str;
		if (html_read_line(parser) == HTML_EOF)
			return NULL;
	}

	while (*parser->bufp != '\0') {
		switch (*parser->bufp) {
		case '<':
			if (parser->str->len == 0)
				html_parse_tag(parser);
			else
				return parser->str->str;
			break;
		case '&':
			html_parse_special(parser);
			break;
		case ' ':
		case '\t':
		case '\r':
		case '\n':
			if (parser->bufp[0] == '\r' && parser->bufp[1] == '\n')
				parser->bufp++;

			if (!parser->pre) {
				if (!parser->newline)
					parser->space = TRUE;

				parser->bufp++;
				break;
			}
			/* fallthrough */
		default:
			html_append_char(parser, *parser->bufp++);
		}
	}

	return parser->str->str;
}

static HTMLState html_read_line(HTMLParser *parser)
{
	gchar buf[HTMLBUFSIZE];
	gchar *conv_str;
	gint index;

	if (fgets(buf, sizeof(buf), parser->fp) == NULL) {
		parser->state = HTML_EOF;
		return HTML_EOF;
	}

	conv_str = conv_convert(parser->conv, buf);
	if (!conv_str) {
		index = parser->bufp - parser->buf->str;

		conv_str = conv_utf8todisp(buf, NULL);
		g_string_append(parser->buf, conv_str);
		g_free(conv_str);

		parser->bufp = parser->buf->str + index;

		return HTML_CONV_FAILED;
	}

	index = parser->bufp - parser->buf->str;

	g_string_append(parser->buf, conv_str);
	g_free(conv_str);

	parser->bufp = parser->buf->str + index;

	return HTML_NORMAL;
}

static void html_append_char(HTMLParser *parser, gchar ch)
{
	GString *str = parser->str;

	if (!parser->pre && parser->space) {
		g_string_append_c(str, ' ');
		parser->space = FALSE;
	}

	g_string_append_c(str, ch);

	parser->empty_line = FALSE;
	if (ch == '\n') {
		parser->newline = TRUE;
		if (str->len > 1 && str->str[str->len - 2] == '\n')
			parser->empty_line = TRUE;
	} else
		parser->newline = FALSE;
}

static void html_append_str(HTMLParser *parser, const gchar *str, gint len)
{
	GString *string = parser->str;

	if (!parser->pre && parser->space) {
		g_string_append_c(string, ' ');
		parser->space = FALSE;
	}

	if (len == 0) return;
	if (len < 0)
		g_string_append(string, str);
	else {
		gchar *s;
		Xstrndup_a(s, str, len, return);
		g_string_append(string, s);
	}

	parser->empty_line = FALSE;
	if (string->len > 0 && string->str[string->len - 1] == '\n') {
		parser->newline = TRUE;
		if (string->len > 1 && string->str[string->len - 2] == '\n')
			parser->empty_line = TRUE;
	} else
		parser->newline = FALSE;
}

static HTMLTag *html_get_tag(const gchar *str)
{
	HTMLTag *tag;
	gchar *tmp;
	gchar *tmpp;

	g_return_val_if_fail(str != NULL, NULL);

	if (*str == '\0' || *str == '!') return NULL;

	Xstrdup_a(tmp, str, return NULL);

	tag = g_new0(HTMLTag, 1);

	for (tmpp = tmp; *tmpp != '\0' && !g_ascii_isspace(*tmpp); tmpp++)
		;

	if (*tmpp == '\0') {
		g_strdown(tmp);
		tag->name = g_strdup(tmp);
		return tag;
	} else {
		*tmpp++ = '\0';
		g_strdown(tmp);
		tag->name = g_strdup(tmp);
	}

	while (*tmpp != '\0') {
		HTMLAttr *attr;
		gchar *attr_name;
		gchar *attr_value;
		gchar *p;
		gchar quote;

		while (g_ascii_isspace(*tmpp)) tmpp++;
		attr_name = tmpp;

		while (*tmpp != '\0' && !g_ascii_isspace(*tmpp) &&
		       *tmpp != '=')
			tmpp++;
		if (*tmpp != '\0' && *tmpp != '=') {
			*tmpp++ = '\0';
			while (g_ascii_isspace(*tmpp)) tmpp++;
		}

		if (*tmpp == '=') {
			*tmpp++ = '\0';
			while (g_ascii_isspace(*tmpp)) tmpp++;

			if (*tmpp == '"' || *tmpp == '\'') {
				/* name="value" */
				quote = *tmpp;
				tmpp++;
				attr_value = tmpp;
				if ((p = strchr(attr_value, quote)) == NULL) {
					g_warning("html_get_tag(): syntax error in tag: '%s'\n", str);
					return tag;
				}
				tmpp = p;
				*tmpp++ = '\0';
				while (g_ascii_isspace(*tmpp)) tmpp++;
			} else {
				/* name=value */
				attr_value = tmpp;
				while (*tmpp != '\0' && !g_ascii_isspace(*tmpp)) tmpp++;
				if (*tmpp != '\0')
					*tmpp++ = '\0';
			}
		} else
			attr_value = "";

		g_strchomp(attr_name);
		g_strdown(attr_name);
		attr = g_new(HTMLAttr, 1);
		attr->name = g_strdup(attr_name);
		attr->value = g_strdup(attr_value);
		tag->attr = g_list_append(tag->attr, attr);
	}

	return tag;
}

static void html_free_tag(HTMLTag *tag)
{
	if (!tag) return;

	g_free(tag->name);
	while (tag->attr != NULL) {
		HTMLAttr *attr = (HTMLAttr *)tag->attr->data;
		g_free(attr->name);
		g_free(attr->value);
		g_free(attr);
		tag->attr = g_list_remove(tag->attr, tag->attr->data);
	}
	g_free(tag);
}

static HTMLState html_parse_tag(HTMLParser *parser)
{
	gchar buf[HTMLBUFSIZE];
	HTMLTag *tag;

	html_get_parenthesis(parser, buf, sizeof(buf));

	tag = html_get_tag(buf);

	parser->state = HTML_UNKNOWN;
	if (!tag) return HTML_UNKNOWN;

	if (!strcmp(tag->name, "br")) {
		parser->space = FALSE;
		html_append_char(parser, '\n');
		parser->state = HTML_BR;
	} else if (!strcmp(tag->name, "a")) {
		if (tag->attr && tag->attr->data &&
		    !strcmp(((HTMLAttr *)tag->attr->data)->name, "href")) {
			g_free(parser->href);
			parser->href =
				g_strdup(((HTMLAttr *)tag->attr->data)->value);
			parser->state = HTML_HREF;
		}
	} else if (!strcmp(tag->name, "/a")) {
		g_free(parser->href);
		parser->href = NULL;
		parser->state = HTML_NORMAL;
	} else if (!strcmp(tag->name, "p")) {
		parser->space = FALSE;
		if (!parser->empty_line) {
			parser->space = FALSE;
			if (!parser->newline) html_append_char(parser, '\n');
			html_append_char(parser, '\n');
		}
		parser->state = HTML_PAR;
	} else if (!strcmp(tag->name, "pre")) {
		parser->pre = TRUE;
		parser->state = HTML_PRE;
	} else if (!strcmp(tag->name, "/pre")) {
		parser->pre = FALSE;
		parser->state = HTML_NORMAL;
	} else if (!strcmp(tag->name, "hr")) {
		if (!parser->newline) {
			parser->space = FALSE;
			html_append_char(parser, '\n');
		}
		html_append_str(parser, HR_STR "\n", -1);
		parser->state = HTML_HR;
	} else if (!strcmp(tag->name, "div")    ||
		   !strcmp(tag->name, "ul")     ||
		   !strcmp(tag->name, "li")     ||
		   !strcmp(tag->name, "table")  ||
		   !strcmp(tag->name, "tr")     ||
		   (tag->name[0] == 'h' && g_ascii_isdigit(tag->name[1]))) {
		if (!parser->newline) {
			parser->space = FALSE;
			html_append_char(parser, '\n');
		}
		parser->state = HTML_NORMAL;
	} else if (!strcmp(tag->name, "/table") ||
		   (tag->name[0] == '/' &&
		    tag->name[1] == 'h' &&
		    g_ascii_isdigit(tag->name[1]))) {
		if (!parser->empty_line) {
			parser->space = FALSE;
			if (!parser->newline) html_append_char(parser, '\n');
			html_append_char(parser, '\n');
		}
		parser->state = HTML_NORMAL;
	} else if (!strcmp(tag->name, "/div")   ||
		   !strcmp(tag->name, "/ul")    ||
		   !strcmp(tag->name, "/li")) {
		if (!parser->newline) {
			parser->space = FALSE;
			html_append_char(parser, '\n');
		}
		parser->state = HTML_NORMAL;
			}

	html_free_tag(tag);

	return parser->state;
}

static void html_parse_special(HTMLParser *parser)
{
	gchar symbol_name[9];
	gint n;
	const gchar *val;

	parser->state = HTML_UNKNOWN;
	g_return_if_fail(*parser->bufp == '&');

	/* &foo; */
	for (n = 0; parser->bufp[n] != '\0' && parser->bufp[n] != ';'; n++)
		;
	if (n > 7 || parser->bufp[n] != ';') {
		/* output literal `&' */
		html_append_char(parser, *parser->bufp++);
		parser->state = HTML_NORMAL;
		return;
	}
	strncpy2(symbol_name, parser->bufp, n + 2);
	parser->bufp += n + 1;

	if ((val = g_hash_table_lookup(parser->symbol_table, symbol_name))
	    != NULL) {
		html_append_str(parser, val, -1);
		parser->state = HTML_NORMAL;
		return;
	} else if (symbol_name[1] == '#' && g_ascii_isdigit(symbol_name[2])) {
		gint ch;

		/* TODO: support other entity references */
		ch = atoi(symbol_name + 2);
		if (g_ascii_isprint(ch)) {
			html_append_char(parser, ch);
			parser->state = HTML_NORMAL;
			return;
		}
	}

	html_append_str(parser, symbol_name, -1);
}

static void html_get_parenthesis(HTMLParser *parser, gchar *buf, gint len)
{
	gchar *p;

	buf[0] = '\0';
	g_return_if_fail(*parser->bufp == '<');

	/* ignore comment / CSS / script stuff */
	if (!strncmp(parser->bufp, "<!--", 4)) {
		parser->bufp += 4;
		while ((p = strstr(parser->bufp, "-->")) == NULL)
			if (html_read_line(parser) == HTML_EOF) return;
		parser->bufp = p + 3;
		return;
	}
	if (!g_ascii_strncasecmp(parser->bufp, "<style", 6)) {
		parser->bufp += 6;
		while ((p = strcasestr(parser->bufp, "</style>")) == NULL)
			if (html_read_line(parser) == HTML_EOF) return;
		parser->bufp = p + 8;
		return;
	}
	if (!g_ascii_strncasecmp(parser->bufp, "<script", 7)) {
		parser->bufp += 7;
		while ((p = strcasestr(parser->bufp, "</script>")) == NULL)
			if (html_read_line(parser) == HTML_EOF) return;
		parser->bufp = p + 9;
		return;
	}

	parser->bufp++;
	while ((p = strchr(parser->bufp, '>')) == NULL)
		if (html_read_line(parser) == HTML_EOF) return;

	strncpy2(buf, parser->bufp, MIN(p - parser->bufp + 1, len));
	g_strstrip(buf);
	parser->bufp = p + 1;
}
