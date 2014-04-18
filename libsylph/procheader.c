/*
 * LibSylph -- E-Mail client library
 * Copyright (C) 1999-2013 Hiroyuki Yamamoto
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
#include <glib/gi18n.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>

#include "procheader.h"
#include "procmsg.h"
#include "codeconv.h"
#include "displayheader.h"
#include "prefs_common.h"
#include "utils.h"

#define BUFFSIZE	8192

gint procheader_get_one_field(gchar *buf, size_t len, FILE *fp,
			      HeaderEntry hentry[])
{
	gint nexthead;
	gint hnum = 0;
	HeaderEntry *hp = NULL;

	if (hentry != NULL) {
		/* skip non-required headers */
		do {
			do {
				if (fgets(buf, len, fp) == NULL)
					return -1;
				if (buf[0] == '\r' || buf[0] == '\n')
					return -1;
			} while (buf[0] == ' ' || buf[0] == '\t');

			for (hp = hentry, hnum = 0; hp->name != NULL;
			     hp++, hnum++) {
				if (!g_ascii_strncasecmp(hp->name, buf,
							 strlen(hp->name)))
					break;
			}
		} while (hp->name == NULL);
	} else {
		if (fgets(buf, len, fp) == NULL) return -1;
		if (buf[0] == '\r' || buf[0] == '\n') return -1;
	}

	/* unfold the specified folded line */
	if (hp && hp->unfold) {
		gboolean folded = FALSE;
		gchar *bufp = buf + strlen(buf);

		for (; bufp > buf &&
		     (*(bufp - 1) == '\n' || *(bufp - 1) == '\r');
		     bufp--)
			*(bufp - 1) = '\0';

		while (1) {
			nexthead = fgetc(fp);

			/* folded */
			if (nexthead == ' ' || nexthead == '\t')
				folded = TRUE;
			else if (nexthead == EOF)
				break;
			else if (folded == TRUE) {
				if ((len - (bufp - buf)) <= 2) break;

				if (nexthead == '\n') {
					folded = FALSE;
					continue;
				}

				/* replace return code on the tail end
				   with space */
				*bufp++ = ' ';
				*bufp++ = nexthead;
				*bufp = '\0';

				/* concatenate next line */
				if (fgets(bufp, len - (bufp - buf), fp)
				    == NULL) break;
				bufp += strlen(bufp);

				for (; bufp > buf &&
				     (*(bufp - 1) == '\n' || *(bufp - 1) == '\r');
				     bufp--)
					*(bufp - 1) = '\0';

				folded = FALSE;
			} else {
				ungetc(nexthead, fp);
				break;
			}
		}

		return hnum;
	}

	while (1) {
		nexthead = fgetc(fp);
		if (nexthead == ' ' || nexthead == '\t') {
			size_t buflen = strlen(buf);

			/* concatenate next line */
			if ((len - buflen) > 2) {
				gchar *p = buf + buflen;

				*p++ = nexthead;
				*p = '\0';
				buflen++;
				if (fgets(p, len - buflen, fp) == NULL)
					break;
			} else
				break;
		} else {
			if (nexthead != EOF)
				ungetc(nexthead, fp);
			break;
		}
	}

	/* remove trailing return code */
	strretchomp(buf);

	return hnum;
}

gchar *procheader_get_unfolded_line(gchar *buf, size_t len, FILE *fp)
{
	gboolean folded = FALSE;
	gint nexthead;
	gchar *bufp;

	if (fgets(buf, len, fp) == NULL) return NULL;
	if (buf[0] == '\r' || buf[0] == '\n') return NULL;
	bufp = buf + strlen(buf);

	for (; bufp > buf &&
	     (*(bufp - 1) == '\n' || *(bufp - 1) == '\r');
	     bufp--)
		*(bufp - 1) = '\0';

	while (1) {
		nexthead = fgetc(fp);

		/* folded */
		if (nexthead == ' ' || nexthead == '\t')
			folded = TRUE;
		else if (nexthead == EOF)
			break;
		else if (folded == TRUE) {
			if ((len - (bufp - buf)) <= 2) break;

			if (nexthead == '\n') {
				folded = FALSE;
				continue;
			}

			/* replace return code on the tail end
			   with space */
			*bufp++ = ' ';
			*bufp++ = nexthead;
			*bufp = '\0';

			/* concatenate next line */
			if (fgets(bufp, len - (bufp - buf), fp)
			    == NULL) break;
			bufp += strlen(bufp);

			for (; bufp > buf &&
			     (*(bufp - 1) == '\n' || *(bufp - 1) == '\r');
			     bufp--)
				*(bufp - 1) = '\0';

			folded = FALSE;
		} else {
			ungetc(nexthead, fp);
			break;
		}
	}

	/* remove trailing return code */
	strretchomp(buf);

	return buf;
}

GSList *procheader_get_header_list_from_file(const gchar *file)
{
	FILE *fp;
	GSList *hlist;

	if ((fp = g_fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "procheader_get_header_list_from_file: fopen");
		return NULL;
	}

	hlist = procheader_get_header_list(fp);

	fclose(fp);
	return hlist;
}

GSList *procheader_get_header_list(FILE *fp)
{
	gchar buf[BUFFSIZE];
	gchar *p;
	GSList *hlist = NULL;
	Header *header;

	g_return_val_if_fail(fp != NULL, NULL);

	while (procheader_get_unfolded_line(buf, sizeof(buf), fp) != NULL) {
		if (*buf == ':') continue;
		for (p = buf; *p && *p != ' '; p++) {
			if (*p == ':') {
				header = g_new(Header, 1);
				header->name = g_strndup(buf, p - buf);
				p++;
				while (*p == ' ' || *p == '\t') p++;
				header->body = conv_unmime_header(p, NULL);

				hlist = g_slist_append(hlist, header);
				break;
			}
		}
	}

	return hlist;
}

GSList *procheader_get_header_list_from_msginfo(MsgInfo *msginfo)
{
	GSList *hlist = NULL;

	g_return_val_if_fail(msginfo != NULL, NULL);

	if (msginfo->subject)
		hlist = procheader_add_header_list(hlist, "Subject",
						   msginfo->subject);
	if (msginfo->from)
		hlist = procheader_add_header_list(hlist, "From",
						   msginfo->from);
	if (msginfo->to)
		hlist = procheader_add_header_list(hlist, "To", msginfo->to);
	if (msginfo->cc)
		hlist = procheader_add_header_list(hlist, "Cc", msginfo->cc);
	if (msginfo->newsgroups)
		hlist = procheader_add_header_list(hlist, "Newsgroups",
						   msginfo->newsgroups);
	if (msginfo->date)
		hlist = procheader_add_header_list(hlist, "Date",
						   msginfo->date);

	return hlist;
}

GSList *procheader_add_header_list(GSList *hlist, const gchar *header_name,
				  const gchar *body)
{
	Header *header;

	g_return_val_if_fail(header_name != NULL, hlist);

	header = g_new(Header, 1);
	header->name = g_strdup(header_name);
	header->body = g_strdup(body);

	return g_slist_append(hlist, header);
}

GSList *procheader_copy_header_list(GSList *hlist)
{
	GSList *newlist = NULL, *cur;

	for (cur = hlist; cur != NULL; cur = cur->next) {
		Header *header = (Header *)cur->data;
		newlist = procheader_add_header_list(newlist, header->name,
						     header->body);
	}

	return newlist;
}

GSList *procheader_merge_header_list(GSList *hlist1, GSList *hlist2)
{
	GSList *cur;

	for (cur = hlist2; cur != NULL; cur = cur->next) {
		Header *header = (Header *)cur->data;
		if (procheader_find_header_list(hlist1, header->name) < 0)
			hlist1 = g_slist_append(hlist1, header);
	}

	return hlist1;
}

GSList *procheader_merge_header_list_dup(GSList *hlist1, GSList *hlist2)
{
	GSList *list, *cur;

	list = procheader_copy_header_list(hlist1);

	for (cur = hlist2; cur != NULL; cur = cur->next) {
		Header *header = (Header *)cur->data;
		if (procheader_find_header_list(list, header->name) < 0)
			list = procheader_add_header_list(list, header->name,
							  header->body);
	}

	return list;
}

gint procheader_find_header_list(GSList *hlist, const gchar *header_name)
{
	GSList *cur;
	gint index = 0;
	Header *header;

	g_return_val_if_fail(header_name != NULL, -1);

	for (cur = hlist; cur != NULL; cur = cur->next, index++) {
		header = (Header *)cur->data;
		if (g_ascii_strcasecmp(header->name, header_name) == 0)
			return index;
	}

	return -1;
}

GPtrArray *procheader_get_header_array(FILE *fp, const gchar *encoding)
{
	gchar buf[BUFFSIZE];
	gchar *p;
	GPtrArray *headers;
	Header *header;

	g_return_val_if_fail(fp != NULL, NULL);

	headers = g_ptr_array_new();

	while (procheader_get_unfolded_line(buf, sizeof(buf), fp) != NULL) {
		if (*buf == ':') continue;
		for (p = buf; *p && *p != ' '; p++) {
			if (*p == ':') {
				header = g_new(Header, 1);
				header->name = g_strndup(buf, p - buf);
				p++;
				while (*p == ' ' || *p == '\t') p++;
				header->body = conv_unmime_header(p, encoding);

				g_ptr_array_add(headers, header);
				break;
			}
		}
	}

	return headers;
}

GPtrArray *procheader_get_header_array_asis(FILE *fp, const gchar *encoding)
{
	gchar buf[BUFFSIZE];
	gchar *p;
	GPtrArray *headers;
	Header *header;

	g_return_val_if_fail(fp != NULL, NULL);

	headers = g_ptr_array_new();

	while (procheader_get_one_field(buf, sizeof(buf), fp, NULL) != -1) {
		if (*buf == ':') continue;
		for (p = buf; *p && *p != ' '; p++) {
			if (*p == ':') {
				header = g_new(Header, 1);
				header->name = g_strndup(buf, p - buf);
				p++;
				header->body = conv_unmime_header(p, encoding);

				g_ptr_array_add(headers, header);
				break;
			}
		}
	}

	return headers;
}

GPtrArray *procheader_get_header_array_for_display(FILE *fp,
						   const gchar *encoding)
{
	GPtrArray *headers, *sorted_headers;
	GSList *disphdr_list;
	Header *header;
	gint i;

	g_return_val_if_fail(fp != NULL, NULL);

	headers = procheader_get_header_array_asis(fp, encoding);

	sorted_headers = g_ptr_array_new();

	for (disphdr_list = prefs_common.disphdr_list; disphdr_list != NULL;
	     disphdr_list = disphdr_list->next) {
		DisplayHeaderProp *dp =
			(DisplayHeaderProp *)disphdr_list->data;

		for (i = 0; i < headers->len; i++) {
			header = g_ptr_array_index(headers, i);

			if (!g_ascii_strcasecmp(header->name, dp->name)) {
				if (dp->hidden)
					procheader_header_free(header);
				else
					g_ptr_array_add(sorted_headers, header);

				g_ptr_array_remove_index(headers, i);
				i--;
			}
		}
	}

	if (prefs_common.show_other_header) {
		for (i = 0; i < headers->len; i++) {
			header = g_ptr_array_index(headers, i);
			g_ptr_array_add(sorted_headers, header);
		}
		g_ptr_array_free(headers, TRUE);
	} else
		procheader_header_array_destroy(headers);

	return sorted_headers;
}

void procheader_header_list_destroy(GSList *hlist)
{
	Header *header;

	while (hlist != NULL) {
		header = hlist->data;
		procheader_header_free(header);
		hlist = g_slist_remove(hlist, header);
	}
}

void procheader_header_array_destroy(GPtrArray *harray)
{
	gint i;
	Header *header;

	for (i = 0; i < harray->len; i++) {
		header = g_ptr_array_index(harray, i);
		procheader_header_free(header);
	}

	g_ptr_array_free(harray, TRUE);
}

void procheader_header_free(Header *header)
{
	if (!header) return;

	g_free(header->name);
	g_free(header->body);
	g_free(header);
}

void procheader_get_header_fields(FILE *fp, HeaderEntry hentry[])
{
	gchar buf[BUFFSIZE];
	HeaderEntry *hp;
	gint hnum;
	gchar *p;

	if (hentry == NULL) return;

	while ((hnum = procheader_get_one_field(buf, sizeof(buf), fp, hentry))
	       != -1) {
		hp = hentry + hnum;

		p = buf + strlen(hp->name);
		while (*p == ' ' || *p == '\t') p++;

		if (hp->body == NULL)
			hp->body = g_strdup(p);
		else if (!g_ascii_strcasecmp(hp->name, "To:") ||
			 !g_ascii_strcasecmp(hp->name, "Cc:")) {
			gchar *tp = hp->body;
			hp->body = g_strconcat(tp, ", ", p, NULL);
			g_free(tp);
		}
	}
}

MsgInfo *procheader_parse_file(const gchar *file, MsgFlags flags,
			       gboolean full)
{
	GStatBuf s;
	FILE *fp;
	MsgInfo *msginfo;

	if (g_stat(file, &s) < 0) {
		FILE_OP_ERROR(file, "stat");
		return NULL;
	}
	if (!S_ISREG(s.st_mode))
		return NULL;

	if ((fp = g_fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "procheader_parse_file: fopen");
		return NULL;
	}

	msginfo = procheader_parse_stream(fp, flags, full);
	fclose(fp);

	if (msginfo) {
		msginfo->size = s.st_size;
		msginfo->mtime = s.st_mtime;
	}

	return msginfo;
}

MsgInfo *procheader_parse_str(const gchar *str, MsgFlags flags, gboolean full)
{
	FILE *fp;
	MsgInfo *msginfo;

	if ((fp = str_open_as_stream(str)) == NULL)
		return NULL;

	msginfo = procheader_parse_stream(fp, flags, full);
	fclose(fp);
	return msginfo;
}

enum
{
	H_DATE		= 0,
	H_FROM		= 1,
	H_TO		= 2,
	H_NEWSGROUPS	= 3,
	H_SUBJECT	= 4,
	H_MSG_ID	= 5,
	H_REFERENCES	= 6,
	H_IN_REPLY_TO	= 7,
	H_CONTENT_TYPE	= 8,
	H_SEEN		= 9,
	H_CC		= 10,
	H_X_FACE	= 11
};

MsgInfo *procheader_parse_stream(FILE *fp, MsgFlags flags, gboolean full)
{
	static HeaderEntry hentry_full[] = {{"Date:",		NULL, FALSE},
					   {"From:",		NULL, TRUE},
					   {"To:",		NULL, TRUE},
					   {"Newsgroups:",	NULL, TRUE},
					   {"Subject:",		NULL, TRUE},
					   {"Message-Id:",	NULL, FALSE},
					   {"References:",	NULL, FALSE},
					   {"In-Reply-To:",	NULL, FALSE},
					   {"Content-Type:",	NULL, FALSE},
					   {"Seen:",		NULL, FALSE},
					   {"Cc:",		NULL, TRUE},
					   {"X-Face:",		NULL, FALSE},
					   {NULL,		NULL, FALSE}};

	static HeaderEntry hentry_short[] = {{"Date:",		NULL, FALSE},
					    {"From:",		NULL, TRUE},
					    {"To:",		NULL, TRUE},
					    {"Newsgroups:",	NULL, TRUE},
					    {"Subject:",	NULL, TRUE},
					    {"Message-Id:",	NULL, FALSE},
					    {"References:",	NULL, FALSE},
					    {"In-Reply-To:",	NULL, FALSE},
					    {"Content-Type:",	NULL, FALSE},
					    {"Seen:",		NULL, FALSE},
					    {NULL,		NULL, FALSE}};

	MsgInfo *msginfo;
	gchar buf[BUFFSIZE];
	gchar *p;
	gchar *hp;
	HeaderEntry *hentry;
	gint hnum;
	gchar *from = NULL, *to = NULL, *subject = NULL, *cc = NULL;
	gchar *charset = NULL;

	hentry = full ? hentry_full : hentry_short;

	if (MSG_IS_QUEUED(flags)) {
		while (fgets(buf, sizeof(buf), fp) != NULL)
			if (buf[0] == '\r' || buf[0] == '\n') break;
	}

	msginfo = g_new0(MsgInfo, 1);
	msginfo->flags = flags;
	msginfo->references = NULL;
	msginfo->inreplyto = NULL;

	while ((hnum = procheader_get_one_field(buf, sizeof(buf), fp, hentry))
	       != -1) {
		hp = buf + strlen(hentry[hnum].name);
		while (*hp == ' ' || *hp == '\t') hp++;

		switch (hnum) {
		case H_DATE:
			if (msginfo->date) break;
			msginfo->date_t =
				procheader_date_parse(NULL, hp, 0);
			msginfo->date = g_strdup(hp);
			break;
		case H_FROM:
			if (from) break;
			from = g_strdup(hp);
			break;
		case H_TO:
			if (to) {
				p = to;
				to = g_strconcat(p, ", ", hp, NULL);
				g_free(p);
			} else
				to = g_strdup(hp);
			break;
		case H_NEWSGROUPS:
			if (msginfo->newsgroups) {
				p = msginfo->newsgroups;
				msginfo->newsgroups =
					g_strconcat(p, ",", hp, NULL);
				g_free(p);
			} else
				msginfo->newsgroups = g_strdup(buf + 12);
			break;
		case H_SUBJECT:
			if (msginfo->subject) break;
			subject = g_strdup(hp);
			break;
		case H_MSG_ID:
			if (msginfo->msgid) break;

			extract_parenthesis(hp, '<', '>');
			remove_space(hp);
			msginfo->msgid = g_strdup(hp);
			break;
		case H_REFERENCES:
			msginfo->references =
				references_list_prepend(msginfo->references,
							hp);
			break;
		case H_IN_REPLY_TO:
			if (msginfo->inreplyto) break;

			eliminate_parenthesis(hp, '(', ')');
			if ((p = strrchr(hp, '<')) != NULL &&
			    strchr(p + 1, '>') != NULL) {
				extract_parenthesis(p, '<', '>');
				remove_space(p);
				if (*p != '\0')
					msginfo->inreplyto = g_strdup(p);
			}
			break;
		case H_CONTENT_TYPE:
			if (!g_ascii_strncasecmp(hp, "multipart", 9)) {
				MSG_SET_TMP_FLAGS(msginfo->flags, MSG_MIME);
			} else {
				if (!g_ascii_strncasecmp(hp, "text/html", 9)) {
					MSG_SET_TMP_FLAGS(msginfo->flags, MSG_MIME_HTML);
				}
				if (!charset) {
					procmime_scan_content_type_str
						(hp, NULL, &charset, NULL, NULL);
				}
			}
			break;
		case H_SEEN:
			/* mnews Seen header */
			MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_NEW|MSG_UNREAD);
			break;
		case H_CC:
			if (cc) {
				p = cc;
				cc = g_strconcat(p, ", ", hp, NULL);
				g_free(p);
			} else
				cc = g_strdup(hp);
			break;
		case H_X_FACE:
			if (msginfo->xface) break;
			msginfo->xface = g_strdup(hp);
			break;
		default:
			break;
		}
	}

	if (from) {
		msginfo->from = conv_unmime_header(from, charset);
		subst_control(msginfo->from, ' ');
		msginfo->fromname = procheader_get_fromname(msginfo->from);
		g_free(from);
	}
	if (to) {
		msginfo->to = conv_unmime_header(to, charset);
		subst_control(msginfo->to, ' ');
		g_free(to);
	}
	if (subject) {
		msginfo->subject = conv_unmime_header(subject, charset);
		subst_control(msginfo->subject, ' ');
		g_free(subject);
	}
	if (cc) {
		msginfo->cc = conv_unmime_header(cc, charset);
		subst_control(msginfo->cc, ' ');
		g_free(cc);
	}

	if (!msginfo->inreplyto && msginfo->references)
		msginfo->inreplyto =
			g_strdup((gchar *)msginfo->references->data);

	if (MSG_IS_MIME(msginfo->flags)) {
		MimeInfo *mimeinfo, *part;
		gboolean has_html = FALSE;

		part = mimeinfo = procmime_scan_message_stream(fp);
		while (part) {
			if (part->mime_type != MIME_TEXT &&
			    part->mime_type != MIME_TEXT_HTML &&
			    part->mime_type != MIME_MULTIPART)
				break;
			if (part->mime_type == MIME_TEXT_HTML)
				has_html = TRUE;
			part = procmime_mimeinfo_next(part);
		}

		if (has_html && !part) {
			MSG_SET_TMP_FLAGS(msginfo->flags, MSG_MIME_HTML);
		}

		procmime_mimeinfo_free_all(mimeinfo);
	}

	g_free(charset);

	return msginfo;
}

gchar *procheader_get_fromname(const gchar *str)
{
	gchar *tmp, *name;

	tmp = g_strdup(str);

	if (*tmp == '\"') {
		extract_quote_with_escape(tmp, '\"');
		g_strstrip(tmp);
	} else if (strchr(tmp, '<')) {
		eliminate_parenthesis(tmp, '<', '>');
		g_strstrip(tmp);
		if (*tmp == '\0') {
			strcpy(tmp, str);
			extract_parenthesis(tmp, '<', '>');
			g_strstrip(tmp);
		}
	} else if (strchr(tmp, '(')) {
		extract_parenthesis_with_escape(tmp, '(', ')');
		g_strstrip(tmp);
	}

	if (*tmp == '\0') {
		g_free(tmp);
		name = g_strdup(str);
	} else
		name = tmp;

	return name;
}

gchar *procheader_get_toname(const gchar *str)
{
	GSList *addr_list, *cur;
	GString *toname;
	gchar *name;

	if (strchr(str, ',') == NULL)
		return procheader_get_fromname(str);

	addr_list = address_list_append_orig(NULL, str);
	toname = g_string_new(NULL);

	for (cur = addr_list; cur != NULL; cur = cur->next) {
		name = procheader_get_fromname((gchar *)cur->data);
		g_string_append(toname, name);
		g_free(name);
		if (cur->next)
			g_string_append(toname, ", ");
	}

	slist_free_strings(addr_list);

	return g_string_free(toname, FALSE);
}

static gint procheader_scan_date_string(const gchar *str,
					gchar *weekday, gint *day,
					gchar *month, gint *year,
					gint *hh, gint *mm, gint *ss,
					gchar *zone)
{
	gint result;

	*zone = '\0';
	result = sscanf(str, "%10s %d %9s %d %2d:%2d:%2d %5s",
			weekday, day, month, year, hh, mm, ss, zone);
	if (result >= 7) return 0;

	result = sscanf(str, "%3s,%d %9s %d %2d:%2d:%2d %5s",
			weekday, day, month, year, hh, mm, ss, zone);
	if (result >= 7) return 0;

	result = sscanf(str, "%3s,%d %9s %d %2d.%2d.%2d %5s",
			weekday, day, month, year, hh, mm, ss, zone);
	if (result >= 7) return 0;

	result = sscanf(str, "%3s %d, %9s %d %2d:%2d:%2d %5s",
			weekday, day, month, year, hh, mm, ss, zone);
	if (result >= 7) return 0;

	result = sscanf(str, "%d %9s %d %2d:%2d:%2d %5s",
			day, month, year, hh, mm, ss, zone);
	if (result >= 6) return 0;

	result = sscanf(str, "%d-%2s-%2d %2d:%2d:%2d",
			year, month, day, hh, mm, ss);
	if (result == 6) return 0;

	*ss = 0;
	result = sscanf(str, "%10s %d %9s %d %2d:%2d %5s",
			weekday, day, month, year, hh, mm, zone);
	if (result >= 6) return 0;

	result = sscanf(str, "%d %9s %d %2d:%2d %5s",
			day, month, year, hh, mm, zone);
	if (result >= 5) return 0;

	return -1;
}

stime_t procheader_date_parse(gchar *dest, const gchar *src, gint len)
{
	static gchar monthstr[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
	gchar weekday[11];
	gint day;
	gchar month[10];
	gint year;
	gint hh, mm, ss;
	gchar zone[6];
	GDateMonth dmonth = G_DATE_BAD_MONTH;
	struct tm t;
	gchar *p;
	time_t timer_;
	time_t tz_offset;
	stime_t timer;

	if (procheader_scan_date_string(src, weekday, &day, month, &year,
					&hh, &mm, &ss, zone) < 0) {
		g_warning("procheader_scan_date_string: date parse failed: %s", src);
		if (dest && len > 0)
			strncpy2(dest, src, len);
		return 0;
	}

	/* Y2K compliant :) */
	if (year < 1000) {
		if (year < 50)
			year += 2000;
		else
			year += 1900;
	}

	month[3] = '\0';
	if (g_ascii_isdigit(month[0])) {
		dmonth = atoi(month);
	} else {
		for (p = monthstr; *p != '\0'; p += 3) {
			if (!g_ascii_strncasecmp(p, month, 3)) {
				dmonth = (gint)(p - monthstr) / 3 + 1;
				break;
			}
		}
	}

	t.tm_sec = ss;
	t.tm_min = mm;
	t.tm_hour = hh;
	t.tm_mday = day;
	t.tm_mon = dmonth - 1;
	t.tm_year = year - 1900;
	t.tm_wday = 0;
	t.tm_yday = 0;
	t.tm_isdst = -1;

	timer_ = mktime(&t);
	if (timer_ == -1) {
		if (year >= 2038) {
			g_warning("mktime: date overflow: %s", src);
			timer_ = G_MAXINT - 12 * 3600;
		} else {
			g_warning("mktime: can't convert date: %s", src);
			if (dest)
				dest[0] = '\0';
			return 0;
		}
	}

	timer = timer_;
	if (timer < G_MAXINT - 12 * 3600) {
		tz_offset = remote_tzoffset_sec(zone);
		if (tz_offset != -1)
			timer += tzoffset_sec(&timer) - tz_offset;
	}

	if (dest)
		procheader_date_get_localtime(dest, len, timer);

	return timer;
}

void procheader_date_get_localtime(gchar *dest, gint len, const stime_t timer)
{
	time_t timer_ = timer;
	struct tm *lt;
	gchar *default_format = "%y/%m/%d(%a) %H:%M";
	gchar *buf;
	gchar tmp[BUFFSIZE];

	lt = localtime(&timer_);
	if (!lt) {
		g_warning("can't get localtime of %ld\n", timer);
		dest[0] = '\0';
		return;
	}

	if (prefs_common.date_format)
		strftime(tmp, sizeof(tmp), prefs_common.date_format, lt);
	else
		strftime(tmp, sizeof(tmp), default_format, lt);

	buf = conv_localetodisp(tmp, NULL);
	strncpy2(dest, buf, len);
	g_free(buf);
}
