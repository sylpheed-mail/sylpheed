/*
 * LibSylph -- E-Mail client library
 * Copyright (C) 1999-2011 Hiroyuki Yamamoto
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
	{"&quot;"  , "\""}
};

/* &#160; - &#255; */
static HTMLSymbol latin_symbol_list[] = {
	{"&nbsp;"  , " "},
	/* {"&nbsp;"  , "\302\240"}, */
	{"&iexcl;" , "\302\241"},
	{"&cent;"  , "\302\242"},
	{"&pound;" , "\302\243"},
	{"&curren;", "\302\244"},
	{"&yen;"   , "\302\245"},
	{"&brvbar;", "\302\246"},
	{"&sect;"  , "\302\247"},
	{"&uml;"   , "\302\250"},
	{"&copy;"  , "\302\251"},
	{"&ordf;"  , "\302\252"},
	{"&laquo;" , "\302\253"},
	{"&not;"   , "\302\254"},
	{"&shy;"   , "\302\255"},
	{"&reg;"   , "\302\256"},
	{"&macr;"  , "\302\257"},
	{"&deg;"   , "\302\260"},
	{"&plusm;" , "\302\261"},
	{"&sup2;"  , "\302\262"},
	{"&sup3;"  , "\302\263"},
	{"&acute;" , "\302\264"},
	{"&micro;" , "\302\265"},
	{"&para;"  , "\302\266"},
	{"&middot;", "\302\267"},
	{"&cedil;" , "\302\270"},
	{"&sup1;"  , "\302\271"},
	{"&ordm;"  , "\302\272"},
	{"&raquo;" , "\302\273"},
	{"&frac14;", "\302\274"},
	{"&frac12;", "\302\275"},
	{"&frac34;", "\302\276"},
	{"&iquest;", "\302\277"},

	{"&Agrave;", "\303\200"},
	{"&Aacute;", "\303\201"},
	{"&Acirc;" , "\303\202"},
	{"&Atilde;", "\303\203"},
	{"&Auml;"  , "\303\204"},
	{"&Aring;" , "\303\205"},
	{"&AElig;" , "\303\206"},
	{"&Ccedil;", "\303\207"},
	{"&Egrave;", "\303\210"},
	{"&Eacute;", "\303\211"},
	{"&Ecirc;" , "\303\212"},
	{"&Euml;"  , "\303\213"},
	{"&Igrave;", "\303\214"},
	{"&Iacute;", "\303\215"},
	{"&Icirc;" , "\303\216"},
	{"&Iuml;"  , "\303\217"},
	{"&ETH;"   , "\303\220"},
	{"&Ntilde;", "\303\221"},
	{"&Ograve;", "\303\222"},
	{"&Oacute;", "\303\223"},
	{"&Ocirc;" , "\303\224"},
	{"&Otilde;", "\303\225"},
	{"&Ouml;"  , "\303\226"},
	{"&times;" , "\303\227"},
	{"&Oslash;", "\303\230"},
	{"&Ugrave;", "\303\231"},
	{"&Uacute;", "\303\232"},
	{"&Ucirc;" , "\303\233"},
	{"&Uuml;"  , "\303\234"},
	{"&Yacute;", "\303\235"},
	{"&THORN;" , "\303\236"},
	{"&szlig;" , "\303\237"},
	{"&agrave;", "\303\240"},
	{"&aacute;", "\303\241"},
	{"&acirc;" , "\303\242"},
	{"&atilde;", "\303\243"},
	{"&auml;"  , "\303\244"},
	{"&aring;" , "\303\245"},
	{"&aelig;" , "\303\246"},
	{"&ccedil;", "\303\247"},
	{"&egrave;", "\303\250"},
	{"&eacute;", "\303\251"},
	{"&ecirc;" , "\303\252"},
	{"&euml;"  , "\303\253"},
	{"&igrave;", "\303\254"},
	{"&iacute;", "\303\255"},
	{"&icirc;" , "\303\256"},
	{"&iuml;"  , "\303\257"},
	{"&eth;"   , "\303\260"},
	{"&ntilde;", "\303\261"},
	{"&ograve;", "\303\262"},
	{"&oacute;", "\303\263"},
	{"&ocirc;" , "\303\264"},
	{"&otilde;", "\303\265"},
	{"&ouml;"  , "\303\266"},
	{"&divide;", "\303\267"},
	{"&oslash;", "\303\270"},
	{"&ugrave;", "\303\271"},
	{"&uacute;", "\303\272"},
	{"&ucirc;" , "\303\273"},
	{"&uuml;"  , "\303\274"},
	{"&yacute;", "\303\275"},
	{"&thorn;" , "\303\276"},
	{"&yuml;"  , "\303\277"}
};

static HTMLSymbol other_symbol_list[] = {
	/* Non-standard? */
	{"&#133;"  , "..."},
	{"&#146;"  , "'"},
	{"&#150;"  , "-"},
	{"&#153;"  , "\xe2\x84\xa2"},
	{"&#156;"  , "\xc5\x93"},

	/* Symbolic characters */
	{"&trade;" , "\xe2\x84\xa2"},

	/* Latin extended */
	{"&OElig;" , "\xc5\x92"},
	{"&oelig;" , "\xc5\x93"},
	{"&Scaron;", "\xc5\xa0"},
	{"&scaron;", "\xc5\xa1"},
	{"&Yuml;"  , "\xc5\xb8"},
	{"&circ;"  , "\xcb\x86"},
	{"&tilde;" , "\xcb\x9c"},
	{"&fnof;"  , "\xc6\x92"},
};

static GHashTable *default_symbol_table;

static HTMLState html_read_line		(HTMLParser	*parser);

static void html_append_char		(HTMLParser	*parser,
					 gchar		 ch);
static void html_append_str		(HTMLParser	*parser,
					 const gchar	*str,
					 gint		 len);

static gchar *html_find_char		(HTMLParser	*parser,
					 gchar		 ch);
static gchar *html_find_str		(HTMLParser	*parser,
					 const gchar	*str);
static gchar *html_find_str_case	(HTMLParser	*parser,
					 const gchar	*str);

static HTMLState html_parse_tag		(HTMLParser	*parser);
static void html_parse_special		(HTMLParser	*parser);
static void html_get_parenthesis	(HTMLParser	*parser,
					 gchar		*buf,
					 gint		 len);

static gchar *html_unescape_str		(HTMLParser	*parser,
					 const gchar	*str);


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
	parser->blockquote = 0;

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
		SYMBOL_TABLE_ADD(default_symbol_table, latin_symbol_list);
		SYMBOL_TABLE_ADD(default_symbol_table, other_symbol_list);
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
	const gchar *bq_prefix = NULL;

	if (!parser->pre && parser->space) {
		g_string_append_c(str, ' ');
		parser->space = FALSE;
	}

	if (parser->newline && parser->blockquote > 0)
		bq_prefix = "  ";

	parser->empty_line = FALSE;
	if (ch == '\n') {
		parser->newline = TRUE;
		if (str->len > 0 && str->str[str->len - 1] == '\n')
			parser->empty_line = TRUE;
	} else
		parser->newline = FALSE;

	if (bq_prefix) {
		gint i;
		for (i = 0; i < parser->blockquote; i++)
			g_string_append(str, bq_prefix);
	}
	g_string_append_c(str, ch);
}

static void html_append_str(HTMLParser *parser, const gchar *str, gint len)
{
	GString *string = parser->str;
	const gchar *bq_prefix = NULL;

	if (!parser->pre && parser->space) {
		g_string_append_c(string, ' ');
		parser->space = FALSE;
	}

	if (len == 0) return;

	if (parser->newline && parser->blockquote > 0)
		bq_prefix = "  ";

	if (bq_prefix) {
		gint i;
		for (i = 0; i < parser->blockquote; i++)
			g_string_append(string, bq_prefix);
	}

	if (len < 0)
		g_string_append(string, str);
	else
		g_string_append_len(string, str, len);

	parser->empty_line = FALSE;
	if (string->len > 0 && string->str[string->len - 1] == '\n') {
		parser->newline = TRUE;
		if (string->len > 1 && string->str[string->len - 2] == '\n')
			parser->empty_line = TRUE;
	} else
		parser->newline = FALSE;
}

static gchar *html_find_char(HTMLParser *parser, gchar ch)
{
	gchar *p;

	while ((p = strchr(parser->bufp, ch)) == NULL) {
		if (html_read_line(parser) == HTML_EOF)
			return NULL;
	}

	return p;
}

static gchar *html_find_str(HTMLParser *parser, const gchar *str)
{
	gchar *p;

	while ((p = strstr(parser->bufp, str)) == NULL) {
		if (html_read_line(parser) == HTML_EOF)
			return NULL;
	}

	return p;
}

static gchar *html_find_str_case(HTMLParser *parser, const gchar *str)
{
	gchar *p;

	while ((p = strcasestr(parser->bufp, str)) == NULL) {
		if (html_read_line(parser) == HTML_EOF)
			return NULL;
	}

	return p;
}

static HTMLTag *html_get_tag(const gchar *str)
{
	HTMLTag *tag;
	gchar *tmp;
	gchar *tmpp;

	g_return_val_if_fail(str != NULL, NULL);

	if (*str == '\0' || *str == '!') return NULL;

	tmp = g_strdup(str);

	tag = g_new0(HTMLTag, 1);

	for (tmpp = tmp; *tmpp != '\0' && !g_ascii_isspace(*tmpp); tmpp++) {
		if (tmpp > tmp && *tmpp == '/') {
			*tmpp = '\0';
			break;
		}
	}

	if (*tmpp == '\0') {
		g_strdown(tmp);
		tag->name = tmp;
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
		if (tmpp > tmp && *tmpp == '/')
			break;
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
					break;
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

	g_free(tmp);

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
		GList *cur;

		for (cur = tag->attr; cur != NULL; cur = cur->next) {
			HTMLAttr *attr = (HTMLAttr *)cur->data;

			if (attr && !strcmp(attr->name, "href")) {
				g_free(parser->href);
				parser->href = html_unescape_str(parser, attr->value);
				parser->state = HTML_HREF;
				break;
			}
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
	} else if (!strcmp(tag->name, "blockquote")) {
		parser->blockquote++;
		parser->state = HTML_BLOCKQUOTE;
	} else if (!strcmp(tag->name, "/blockquote")) {
		parser->blockquote--;
		if (parser->blockquote < 0)
			parser->blockquote = 0;
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

		ch = atoi(symbol_name + 2);
		if (ch < 128 && g_ascii_isprint(ch)) {
			html_append_char(parser, ch);
			parser->state = HTML_NORMAL;
			return;
		} else {
			/* ISO 10646 to UTF-8 */
			gchar buf[6];
			gint len;

			len = g_unichar_to_utf8((gunichar)ch, buf);
			if (len > 0) {
				html_append_str(parser, buf, len);
				parser->state = HTML_NORMAL;
				return;
			}
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
		if ((p = html_find_str(parser, "-->")) != NULL)
			parser->bufp = p + 3;
		return;
	}
	if (!g_ascii_strncasecmp(parser->bufp, "<style", 6)) {
		parser->bufp += 6;
		if ((p = html_find_str_case(parser, "</style")) != NULL) {
			parser->bufp = p + 7;
			if ((p = html_find_char(parser, '>')) != NULL)
				parser->bufp = p + 1;
		}
		return;
	}
	if (!g_ascii_strncasecmp(parser->bufp, "<script", 7)) {
		parser->bufp += 7;
		if ((p = html_find_str_case(parser, "</script")) != NULL) {
			parser->bufp = p + 8;
			if ((p = html_find_char(parser, '>')) != NULL)
				parser->bufp = p + 1;
		}
		return;
	}

	parser->bufp++;
	if ((p = html_find_char(parser, '>')) == NULL)
		return;

	strncpy2(buf, parser->bufp, MIN(p - parser->bufp + 1, len));
	g_strstrip(buf);
	parser->bufp = p + 1;
}

static gchar *html_unescape_str(HTMLParser *parser, const gchar *str)
{
	const gchar *p = str;
	gchar symbol_name[9];
	gint n;
	const gchar *val;
	gchar *unescape_str;
	gchar *up;

	if (!str)
		return NULL;

	up = unescape_str = g_malloc(strlen(str) + 1);

	while (*p != '\0') {
		switch (*p) {
		case '&':
			for (n = 0; p[n] != '\0' && p[n] != ';'; n++)
				;
			if (n > 7 || p[n] != ';') {
				*up++ = *p++;
				break;
			}
			strncpy2(symbol_name, p, n + 2);
			p += n + 1;

			if ((val = g_hash_table_lookup(parser->symbol_table, symbol_name)) != NULL) {
				gint len = strlen(val);
				if (len <= n + 1) {
					strcpy(up, val);
					up += len;
				} else {
					strcpy(up, symbol_name);
					up += n + 1;
				}
			} else if (symbol_name[1] == '#' && g_ascii_isdigit(symbol_name[2])) {
				gint ch;

				ch = atoi(symbol_name + 2);
				if (ch < 128 && g_ascii_isprint(ch)) {
					*up++ = ch;
				} else {
					/* ISO 10646 to UTF-8 */
					gchar buf[6];
					gint len;

					len = g_unichar_to_utf8((gunichar)ch, buf);
					if (len > 0 && len <= 6 && len <= n + 1) {
						memcpy(up, buf, len);
						up += len;
					} else {
						strcpy(up, symbol_name);
						up += n + 1;
					}
				}
			}

			break;
		default:
			*up++ = *p++;
		}
	}

	*up = '\0';
	return unescape_str;
}
