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

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#if (HAVE_WCTYPE_H && HAVE_WCHAR_H)
#  include <wchar.h>
#  include <wctype.h>
#endif
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#if HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#include <dirent.h>
#include <time.h>

#ifdef G_OS_WIN32
#  include <direct.h>
#  include <io.h>
#endif

#include "utils.h"
#include "socket.h"
#include "statusbar.h"
#include "logwindow.h"

#define BUFFSIZE	8192

static gboolean debug_mode = FALSE;


#if !GLIB_CHECK_VERSION(2, 7, 0) && !defined(G_OS_UNIX)
gint g_chdir(const gchar *path)
{
#ifdef G_OS_WIN32
	if (G_WIN32_HAVE_WIDECHAR_API()) {
		wchar_t *wpath;
		gint retval;
		gint save_errno;

		wpath = g_utf8_to_utf16(path, -1, NULL, NULL, NULL);
		if (wpath == NULL) {
			errno = EINVAL;
			return -1;
		}

		retval = _wchdir(wpath);
		save_errno = errno;

		g_free(wpath);

		errno = save_errno;
		return retval;
	} else {
		gchar *cp_path;
		gint retval;
		gint save_errno;

		cp_path = g_locale_from_utf8(path, -1, NULL, NULL, NULL);
		if (cp_path == NULL) {
			errno = EINVAL;
			return -1;
		}

		retval = chdir(cp_path);
		save_errno = errno;

		g_free(cp_path);

		errno = save_errno;
		return retval;
	}
#else
	return chdir(path);
#endif
}

gint g_chmod(const gchar *path, gint mode)
{
#ifdef G_OS_WIN32
	if (G_WIN32_HAVE_WIDECHAR_API()) {
		wchar_t *wpath;
		gint retval;
		gint save_errno;

		wpath = g_utf8_to_utf16(path, -1, NULL, NULL, NULL);
		if (wpath == NULL) {
			errno = EINVAL;
			return -1;
		}

		retval = _wchmod(wpath, mode);
		save_errno = errno;

		g_free(wpath);

		errno = save_errno;
		return retval;
	} else {
		gchar *cp_path;
		gint retval;
		gint save_errno;

		cp_path = g_locale_from_utf8(path, -1, NULL, NULL, NULL);
		if (cp_path == NULL) {
			errno = EINVAL;
			return -1;
		}

		retval = chmod(cp_path, mode);
		save_errno = errno;

		g_free(cp_path);

		errno = save_errno;
		return retval;
	}
#else
	return chmod(path, mode);
#endif
}
#endif /* GLIB_CHECK_VERSION && G_OS_UNIX */

void list_free_strings(GList *list)
{
	list = g_list_first(list);

	while (list != NULL) {
		g_free(list->data);
		list = list->next;
	}
}

void slist_free_strings(GSList *list)
{
	while (list != NULL) {
		g_free(list->data);
		list = list->next;
	}
}

static void hash_free_strings_func(gpointer key, gpointer value, gpointer data)
{
	g_free(key);
}

void hash_free_strings(GHashTable *table)
{
	g_hash_table_foreach(table, hash_free_strings_func, NULL);
}

static void hash_free_value_mem_func(gpointer key, gpointer value,
				     gpointer data)
{
	g_free(value);
}

void hash_free_value_mem(GHashTable *table)
{
	g_hash_table_foreach(table, hash_free_value_mem_func, NULL);
}

gint str_case_equal(gconstpointer v, gconstpointer v2)
{
	return g_ascii_strcasecmp((const gchar *)v, (const gchar *)v2) == 0;
}

guint str_case_hash(gconstpointer key)
{
	const gchar *p = key;
	guint h = *p;

	if (h) {
		h = g_ascii_tolower(h);
		for (p += 1; *p != '\0'; p++)
			h = (h << 5) - h + g_ascii_tolower(*p);
	}

	return h;
}

void ptr_array_free_strings(GPtrArray *array)
{
	gint i;
	gchar *str;

	g_return_if_fail(array != NULL);

	for (i = 0; i < array->len; i++) {
		str = g_ptr_array_index(array, i);
		g_free(str);
	}
}

gboolean str_find(const gchar *haystack, const gchar *needle)
{
	return strstr(haystack, needle) != NULL ? TRUE : FALSE;
}

gboolean str_case_find(const gchar *haystack, const gchar *needle)
{
	return strcasestr(haystack, needle) != NULL ? TRUE : FALSE;
}

gboolean str_find_equal(const gchar *haystack, const gchar *needle)
{
	return strcmp(haystack, needle) == 0;
}

gboolean str_case_find_equal(const gchar *haystack, const gchar *needle)
{
	return g_ascii_strcasecmp(haystack, needle) == 0;
}

gint to_number(const gchar *nstr)
{
	register const gchar *p;

	if (*nstr == '\0') return -1;

	for (p = nstr; *p != '\0'; p++)
		if (!g_ascii_isdigit(*p)) return -1;

	return atoi(nstr);
}

/* convert integer into string,
   nstr must be not lower than 11 characters length */
gchar *itos_buf(gchar *nstr, gint n)
{
	g_snprintf(nstr, 11, "%d", n);
	return nstr;
}

/* convert integer into string */
gchar *itos(gint n)
{
	static gchar nstr[11];

	return itos_buf(nstr, n);
}

gchar *to_human_readable(off_t size)
{
	static gchar str[10];

	if (size < 1024)
		g_snprintf(str, sizeof(str), _("%dB"), (gint)size);
	else if (size >> 10 < 1024)
		g_snprintf(str, sizeof(str), _("%.1fKB"), (gfloat)size / (1 << 10));
	else if (size >> 20 < 1024)
		g_snprintf(str, sizeof(str), _("%.2fMB"), (gfloat)size / (1 << 20));
	else
		g_snprintf(str, sizeof(str), _("%.2fGB"), (gfloat)size / (1 << 30));

	return str;
}

/* strcmp with NULL-checking */
gint strcmp2(const gchar *s1, const gchar *s2)
{
	if (s1 == NULL || s2 == NULL)
		return -1;
	else
		return strcmp(s1, s2);
}

/* compare paths */
gint path_cmp(const gchar *s1, const gchar *s2)
{
	gint len1, len2;
#ifdef G_OS_WIN32
	gchar *s1_, *s2_;
#endif

	if (s1 == NULL || s2 == NULL) return -1;
	if (*s1 == '\0' || *s2 == '\0') return -1;

	len1 = strlen(s1);
	len2 = strlen(s2);

#ifdef G_OS_WIN32
	Xstrdup_a(s1_, s1, return -1);
	Xstrdup_a(s2_, s2, return -1);
	subst_char(s1_, '/', G_DIR_SEPARATOR);
	subst_char(s2_, '/', G_DIR_SEPARATOR);
	if (s1_[len1 - 1] == G_DIR_SEPARATOR) len1--;
	if (s2_[len2 - 1] == G_DIR_SEPARATOR) len2--;

	return strncmp(s1_, s2_, MAX(len1, len2));
#else
	if (s1[len1 - 1] == G_DIR_SEPARATOR) len1--;
	if (s2[len2 - 1] == G_DIR_SEPARATOR) len2--;

	return strncmp(s1, s2, MAX(len1, len2));
#endif
}

/* remove trailing return code */
gchar *strretchomp(gchar *str)
{
	register gchar *s;

	if (!*str) return str;

	for (s = str + strlen(str) - 1;
	     s >= str && (*s == '\n' || *s == '\r');
	     s--)
		*s = '\0';

	return str;
}

/* remove trailing character */
gchar *strtailchomp(gchar *str, gchar tail_char)
{
	register gchar *s;

	if (!*str) return str;
	if (tail_char == '\0') return str;

	for (s = str + strlen(str) - 1; s >= str && *s == tail_char; s--)
		*s = '\0';

	return str;
}

/* remove CR (carriage return) */
gchar *strcrchomp(gchar *str)
{
	register gchar *s;

	if (!*str) return str;

	s = str + strlen(str) - 1;
	if (*s == '\n' && s > str && *(s - 1) == '\r') {
		*(s - 1) = '\n';
		*s = '\0';
	}

	return str;
}

/* Similar to `strstr' but this function ignores the case of both strings.  */
gchar *strcasestr(const gchar *haystack, const gchar *needle)
{
	register size_t haystack_len, needle_len;

	haystack_len = strlen(haystack);
	needle_len   = strlen(needle);

	if (haystack_len < needle_len || needle_len == 0)
		return NULL;

	while (haystack_len >= needle_len) {
		if (!g_ascii_strncasecmp(haystack, needle, needle_len))
			return (gchar *)haystack;
		else {
			haystack++;
			haystack_len--;
		}
	}

	return NULL;
}

gpointer my_memmem(gconstpointer haystack, size_t haystacklen,
		   gconstpointer needle, size_t needlelen)
{
	const gchar *haystack_ = (const gchar *)haystack;
	const gchar *needle_ = (const gchar *)needle;
	const gchar *haystack_cur = (const gchar *)haystack;

	if (needlelen == 1)
		return memchr(haystack_, *needle_, haystacklen);

	while ((haystack_cur = memchr(haystack_cur, *needle_, haystacklen))
	       != NULL) {
		if (haystacklen - (haystack_cur - haystack_) < needlelen)
			break;
		if (memcmp(haystack_cur + 1, needle_ + 1, needlelen - 1) == 0)
			return (gpointer)haystack_cur;
		else
			haystack_cur++;
	}

	return NULL;
}

/* Copy no more than N characters of SRC to DEST, with NULL terminating.  */
gchar *strncpy2(gchar *dest, const gchar *src, size_t n)
{
	register const gchar *s = src;
	register gchar *d = dest;

	while (--n && *s)
		*d++ = *s++;
	*d = '\0';

	return dest;
}

#if !HAVE_ISWALNUM
int iswalnum(wint_t wc)
{
	return g_ascii_isalnum((int)wc);
}
#endif

#if !HAVE_ISWSPACE
int iswspace(wint_t wc)
{
	return g_ascii_isspace((int)wc);
}
#endif

#if !HAVE_TOWLOWER
wint_t towlower(wint_t wc)
{
	if (wc >= L'A' && wc <= L'Z')
		return wc + L'a' - L'A';

	return wc;
}
#endif

#if !HAVE_WCSLEN
size_t wcslen(const wchar_t *s)
{
	size_t len = 0;

	while (*s != L'\0')
		++len, ++s;

	return len;
}
#endif

#if !HAVE_WCSCPY
/* Copy SRC to DEST.  */
wchar_t *wcscpy(wchar_t *dest, const wchar_t *src)
{
	wint_t c;
	wchar_t *s = dest;

	do {
		c = *src++;
		*dest++ = c;
	} while (c != L'\0');

	return s;
}
#endif

#if !HAVE_WCSNCPY
/* Copy no more than N wide-characters of SRC to DEST.  */
wchar_t *wcsncpy (wchar_t *dest, const wchar_t *src, size_t n)
{
	wint_t c;
	wchar_t *s = dest;

	do {
		c = *src++;
		*dest++ = c;
		if (--n == 0)
			return s;
	} while (c != L'\0');

	/* zero fill */
	do
		*dest++ = L'\0';
	while (--n > 0);

	return s;
}
#endif

/* Duplicate S, returning an identical malloc'd string. */
wchar_t *wcsdup(const wchar_t *s)
{
	wchar_t *new_str;

	if (s) {
		new_str = g_new(wchar_t, wcslen(s) + 1);
		wcscpy(new_str, s);
	} else
		new_str = NULL;

	return new_str;
}

/* Duplicate no more than N wide-characters of S,
   returning an identical malloc'd string. */
wchar_t *wcsndup(const wchar_t *s, size_t n)
{
	wchar_t *new_str;

	if (s) {
		new_str = g_new(wchar_t, n + 1);
		wcsncpy(new_str, s, n);
		new_str[n] = (wchar_t)0;
	} else
		new_str = NULL;

	return new_str;
}

wchar_t *strdup_mbstowcs(const gchar *s)
{
	wchar_t *new_str;

	if (s) {
		new_str = g_new(wchar_t, strlen(s) + 1);
		if (mbstowcs(new_str, s, strlen(s) + 1) < 0) {
			g_free(new_str);
			new_str = NULL;
		} else
			new_str = g_realloc(new_str,
					    sizeof(wchar_t) * (wcslen(new_str) + 1));
	} else
		new_str = NULL;

	return new_str;
}

gchar *strdup_wcstombs(const wchar_t *s)
{
	gchar *new_str;
	size_t len;

	if (s) {
		len = wcslen(s) * MB_CUR_MAX + 1;
		new_str = g_new(gchar, len);
		if (wcstombs(new_str, s, len) < 0) {
			g_free(new_str);
			new_str = NULL;
		} else
			new_str = g_realloc(new_str, strlen(new_str) + 1);
	} else
		new_str = NULL;

	return new_str;
}

/* Compare S1 and S2, ignoring case.  */
gint wcsncasecmp(const wchar_t *s1, const wchar_t *s2, size_t n)
{
	wint_t c1;
	wint_t c2;

	while (n--) {
		c1 = towlower(*s1++);
		c2 = towlower(*s2++);
		if (c1 != c2)
			return c1 - c2;
		else if (c1 == 0 && c2 == 0)
			break;
	}

	return 0;
}

/* Find the first occurrence of NEEDLE in HAYSTACK, ignoring case.  */
wchar_t *wcscasestr(const wchar_t *haystack, const wchar_t *needle)
{
	register size_t haystack_len, needle_len;

	haystack_len = wcslen(haystack);
	needle_len   = wcslen(needle);

	if (haystack_len < needle_len || needle_len == 0)
		return NULL;

	while (haystack_len >= needle_len) {
		if (!wcsncasecmp(haystack, needle, needle_len))
			return (wchar_t *)haystack;
		else {
			haystack++;
			haystack_len--;
		}
	}

	return NULL;
}

gint get_mbs_len(const gchar *s)
{
	const gchar *p = s;
	gint mb_len;
	gint len = 0;

	if (!p)
		return -1;

	while (*p != '\0') {
		mb_len = g_utf8_skip[*(guchar *)p];
		if (mb_len == 0)
			break;
		else
			len++;

		p += mb_len;
	}

	return len;
}

/* Examine if next block is non-ASCII string */
gboolean is_next_nonascii(const gchar *s)
{
	const gchar *p;

	/* skip head space */
	for (p = s; *p != '\0' && g_ascii_isspace(*p); p++)
		;
	for (; *p != '\0' && !g_ascii_isspace(*p); p++) {
		if (*(guchar *)p > 127 || *(guchar *)p < 32)
			return TRUE;
	}

	return FALSE;
}

gint get_next_word_len(const gchar *s)
{
	gint len = 0;

	for (; *s != '\0' && !g_ascii_isspace(*s); s++, len++)
		;

	return len;
}

/* compare subjects */
gint subject_compare(const gchar *s1, const gchar *s2)
{
	gchar *str1, *str2;

	if (!s1 || !s2) return -1;
	if (!*s1 || !*s2) return -1;

	Xstrdup_a(str1, s1, return -1);
	Xstrdup_a(str2, s2, return -1);

	trim_subject_for_compare(str1);
	trim_subject_for_compare(str2);

	if (!*str1 || !*str2) return -1;

	return strcmp(str1, str2);
}

gint subject_compare_for_sort(const gchar *s1, const gchar *s2)
{
	gchar *str1, *str2;

	if (!s1 || !s2) return -1;

	Xstrdup_a(str1, s1, return -1);
	Xstrdup_a(str2, s2, return -1);

	trim_subject_for_sort(str1);
	trim_subject_for_sort(str2);

	return g_ascii_strcasecmp(str1, str2);
}

void trim_subject_for_compare(gchar *str)
{
	gchar *srcp;

	eliminate_parenthesis(str, '[', ']');
	eliminate_parenthesis(str, '(', ')');
	g_strstrip(str);

	while (!g_ascii_strncasecmp(str, "Re:", 3)) {
		srcp = str + 3;
		while (g_ascii_isspace(*srcp)) srcp++;
		memmove(str, srcp, strlen(srcp) + 1);
	}
}

void trim_subject_for_sort(gchar *str)
{
	gchar *srcp;

	g_strstrip(str);

	while (!g_ascii_strncasecmp(str, "Re:", 3)) {
		srcp = str + 3;
		while (g_ascii_isspace(*srcp)) srcp++;
		memmove(str, srcp, strlen(srcp) + 1);
	}
}

void trim_subject(gchar *str)
{
	register gchar *srcp, *destp;
	gchar op, cl;
	gint in_brace;

	destp = str;
	while (!g_ascii_strncasecmp(destp, "Re:", 3)) {
		destp += 3;
		while (g_ascii_isspace(*destp)) destp++;
	}

	if (*destp == '[') {
		op = '[';
		cl = ']';
	} else if (*destp == '(') {
		op = '(';
		cl = ')';
	} else
		return;

	srcp = destp + 1;
	in_brace = 1;
	while (*srcp) {
		if (*srcp == op)
			in_brace++;
		else if (*srcp == cl)
			in_brace--;
		srcp++;
		if (in_brace == 0)
			break;
	}
	while (g_ascii_isspace(*srcp)) srcp++;
	memmove(destp, srcp, strlen(srcp) + 1);
}

void eliminate_parenthesis(gchar *str, gchar op, gchar cl)
{
	register gchar *srcp, *destp;
	gint in_brace;

	srcp = destp = str;

	while ((destp = strchr(destp, op))) {
		in_brace = 1;
		srcp = destp + 1;
		while (*srcp) {
			if (*srcp == op)
				in_brace++;
			else if (*srcp == cl)
				in_brace--;
			srcp++;
			if (in_brace == 0)
				break;
		}
		while (g_ascii_isspace(*srcp)) srcp++;
		memmove(destp, srcp, strlen(srcp) + 1);
	}
}

void extract_parenthesis(gchar *str, gchar op, gchar cl)
{
	register gchar *srcp, *destp;
	gint in_brace;

	srcp = destp = str;

	while ((srcp = strchr(destp, op))) {
		if (destp > str)
			*destp++ = ' ';
		memmove(destp, srcp + 1, strlen(srcp));
		in_brace = 1;
		while(*destp) {
			if (*destp == op)
				in_brace++;
			else if (*destp == cl)
				in_brace--;

			if (in_brace == 0)
				break;

			destp++;
		}
	}
	*destp = '\0';
}

void extract_parenthesis_with_skip_quote(gchar *str, gchar quote_chr,
					 gchar op, gchar cl)
{
	register gchar *srcp, *destp;
	gint in_brace;
	gboolean in_quote = FALSE;

	srcp = destp = str;

	while ((srcp = strchr_with_skip_quote(destp, quote_chr, op))) {
		if (destp > str)
			*destp++ = ' ';
		memmove(destp, srcp + 1, strlen(srcp));
		in_brace = 1;
		while(*destp) {
			if (*destp == op && !in_quote)
				in_brace++;
			else if (*destp == cl && !in_quote)
				in_brace--;
			else if (*destp == quote_chr)
				in_quote ^= TRUE;

			if (in_brace == 0)
				break;

			destp++;
		}
	}
	*destp = '\0';
}

void eliminate_quote(gchar *str, gchar quote_chr)
{
	register gchar *srcp, *destp;

	srcp = destp = str;

	while ((destp = strchr(destp, quote_chr))) {
		if ((srcp = strchr(destp + 1, quote_chr))) {
			srcp++;
			while (g_ascii_isspace(*srcp)) srcp++;
			memmove(destp, srcp, strlen(srcp) + 1);
		} else {
			*destp = '\0';
			break;
		}
	}
}

void extract_quote(gchar *str, gchar quote_chr)
{
	register gchar *p;

	if ((str = strchr(str, quote_chr))) {
		if ((p = strchr(str + 1, quote_chr))) {
			*p = '\0';
			memmove(str, str + 1, p - str);
		}
	}
}

void eliminate_address_comment(gchar *str)
{
	register gchar *srcp, *destp;
	gint in_brace;

	srcp = destp = str;

	while ((destp = strchr(destp, '"'))) {
		if ((srcp = strchr(destp + 1, '"'))) {
			srcp++;
			if (*srcp == '@') {
				destp = srcp + 1;
			} else {
				while (g_ascii_isspace(*srcp)) srcp++;
				memmove(destp, srcp, strlen(srcp) + 1);
			}
		} else {
			*destp = '\0';
			break;
		}
	}

	srcp = destp = str;

	while ((destp = strchr_with_skip_quote(destp, '"', '('))) {
		in_brace = 1;
		srcp = destp + 1;
		while (*srcp) {
			if (*srcp == '(')
				in_brace++;
			else if (*srcp == ')')
				in_brace--;
			srcp++;
			if (in_brace == 0)
				break;
		}
		while (g_ascii_isspace(*srcp)) srcp++;
		memmove(destp, srcp, strlen(srcp) + 1);
	}
}

gchar *strchr_with_skip_quote(const gchar *str, gint quote_chr, gint c)
{
	gboolean in_quote = FALSE;

	while (*str) {
		if (*str == c && !in_quote)
			return (gchar *)str;
		if (*str == quote_chr)
			in_quote ^= TRUE;
		str++;
	}

	return NULL;
}

gchar *strrchr_with_skip_quote(const gchar *str, gint quote_chr, gint c)
{
	gboolean in_quote = FALSE;
	const gchar *p;

	p = str + strlen(str) - 1;
	while (p >= str) {
		if (*p == c && !in_quote)
			return (gchar *)p;
		if (*p == quote_chr)
			in_quote ^= TRUE;
		p--;
	}

	return NULL;
}

void extract_address(gchar *str)
{
	eliminate_address_comment(str);
	if (strchr_with_skip_quote(str, '"', '<'))
		extract_parenthesis_with_skip_quote(str, '"', '<', '>');
	g_strstrip(str);
}

void extract_list_id_str(gchar *str)
{
	if (strchr_with_skip_quote(str, '"', '<'))
		extract_parenthesis_with_skip_quote(str, '"', '<', '>');
	g_strstrip(str);
}

GSList *address_list_append(GSList *addr_list, const gchar *str)
{
	gchar *work;
	gchar *workp;

	if (!str) return addr_list;

	Xstrdup_a(work, str, return addr_list);

	eliminate_address_comment(work);
	workp = work;

	while (workp && *workp) {
		gchar *p, *next;

		if ((p = strchr_with_skip_quote(workp, '"', ','))) {
			*p = '\0';
			next = p + 1;
		} else
			next = NULL;

		if (strchr_with_skip_quote(workp, '"', '<'))
			extract_parenthesis_with_skip_quote
				(workp, '"', '<', '>');

		g_strstrip(workp);
		if (*workp)
			addr_list = g_slist_append(addr_list, g_strdup(workp));

		workp = next;
	}

	return addr_list;
}

GSList *references_list_prepend(GSList *msgid_list, const gchar *str)
{
	const gchar *strp;

	if (!str) return msgid_list;
	strp = str;

	while (strp && *strp) {
		const gchar *start, *end;
		gchar *msgid;

		if ((start = strchr(strp, '<')) != NULL) {
			end = strchr(start + 1, '>');
			if (!end) break;
		} else
			break;

		msgid = g_strndup(start + 1, end - start - 1);
		g_strstrip(msgid);
		if (*msgid)
			msgid_list = g_slist_prepend(msgid_list, msgid);
		else
			g_free(msgid);

		strp = end + 1;
	}

	return msgid_list;
}

GSList *references_list_append(GSList *msgid_list, const gchar *str)
{
	GSList *list;

	list = references_list_prepend(NULL, str);
	list = g_slist_reverse(list);
	msgid_list = g_slist_concat(msgid_list, list);

	return msgid_list;
}

GSList *newsgroup_list_append(GSList *group_list, const gchar *str)
{
	gchar *work;
	gchar *workp;

	if (!str) return group_list;

	Xstrdup_a(work, str, return group_list);

	workp = work;

	while (workp && *workp) {
		gchar *p, *next;

		if ((p = strchr_with_skip_quote(workp, '"', ','))) {
			*p = '\0';
			next = p + 1;
		} else
			next = NULL;

		g_strstrip(workp);
		if (*workp)
			group_list = g_slist_append(group_list,
						    g_strdup(workp));

		workp = next;
	}

	return group_list;
}

GList *add_history(GList *list, const gchar *str)
{
	GList *old;

	g_return_val_if_fail(str != NULL, list);

	old = g_list_find_custom(list, (gpointer)str, (GCompareFunc)strcmp2);
	if (old) {
		g_free(old->data);
		list = g_list_remove(list, old->data);
	} else if (g_list_length(list) >= MAX_HISTORY_SIZE) {
		GList *last;

		last = g_list_last(list);
		if (last) {
			g_free(last->data);
			g_list_remove(list, last->data);
		}
	}

	list = g_list_prepend(list, g_strdup(str));

	return list;
}

void remove_return(gchar *str)
{
	register gchar *p = str;

	while (*p) {
		if (*p == '\n' || *p == '\r')
			memmove(p, p + 1, strlen(p));
		else
			p++;
	}
}

void remove_space(gchar *str)
{
	register gchar *p = str;
	register gint spc;

	while (*p) {
		spc = 0;
		while (g_ascii_isspace(*(p + spc)))
			spc++;
		if (spc)
			memmove(p, p + spc, strlen(p + spc) + 1);
		else
			p++;
	}
}

void unfold_line(gchar *str)
{
	register gchar *p = str;
	register gint spc;

	while (*p) {
		if (*p == '\n' || *p == '\r') {
			*p++ = ' ';
			spc = 0;
			while (g_ascii_isspace(*(p + spc)))
				spc++;
			if (spc)
				memmove(p, p + spc, strlen(p + spc) + 1);
		} else
			p++;
	}
}

void subst_char(gchar *str, gchar orig, gchar subst)
{
	register gchar *p = str;

	while (*p) {
		if (*p == orig)
			*p = subst;
		p++;
	}
}

void subst_chars(gchar *str, gchar *orig, gchar subst)
{
	register gchar *p = str;

	while (*p) {
		if (strchr(orig, *p) != NULL)
			*p = subst;
		p++;
	}
}

void subst_null(gchar *str, gint len, gchar subst)
{
	register gchar *p = str;

	while (len--) {
		if (*p == '\0')
			*p = subst;
		p++;
	}
}

void subst_for_filename(gchar *str)
{
	subst_chars(str, " \t\r\n\"'/\\", '_');
}

gboolean is_header_line(const gchar *str)
{
	if (str[0] == ':') return FALSE;

	while (*str != '\0' && *str != ' ') {
		if (*str == ':')
			return TRUE;
		str++;
	}

	return FALSE;
}

gboolean is_ascii_str(const gchar *str)
{
	const guchar *p = (const guchar *)str;

	while (*p != '\0') {
		if (*p != '\t' && *p != ' ' &&
		    *p != '\r' && *p != '\n' &&
		    (*p < 32 || *p >= 127))
			return FALSE;
		p++;
	}

	return TRUE;
}

gint get_quote_level(const gchar *str)
{
	const gchar *first_pos;
	const gchar *last_pos;
	const gchar *p = str;
	gint quote_level = -1;

	/* speed up line processing by only searching to the last '>' */
	if ((first_pos = strchr(str, '>')) != NULL) {
		/* skip a line if it contains a '<' before the initial '>' */
		if (memchr(str, '<', first_pos - str) != NULL)
			return -1;
		last_pos = strrchr(first_pos, '>');
	} else
		return -1;

	while (p <= last_pos) {
		while (p < last_pos) {
			if (g_ascii_isspace(*p))
				p++;
			else
				break;
		}

		if (*p == '>')
			quote_level++;
		else if (*p != '-' && !g_ascii_isspace(*p) && p <= last_pos) {
			/* any characters are allowed except '-' and space */
			while (*p != '-' && *p != '>' && !g_ascii_isspace(*p) &&
			       p < last_pos)
				p++;
			if (*p == '>')
				quote_level++;
			else
				break;
		}

		p++;
	}

	return quote_level;
}

gint check_line_length(const gchar *str, gint max_chars, gint *line)
{
	const gchar *p = str, *q;
	gint cur_line = 0, len;

	while ((q = strchr(p, '\n')) != NULL) {
		len = q - p + 1;
		if (len > max_chars) {
			if (line)
				*line = cur_line;
			return -1;
		}
		p = q + 1;
		++cur_line;
	}

	len = strlen(p);
	if (len > max_chars) {
		if (line)
			*line = cur_line;
		return -1;
	}

	return 0;
}

gchar *strstr_with_skip_quote(const gchar *haystack, const gchar *needle)
{
	register guint haystack_len, needle_len;
	gboolean in_squote = FALSE, in_dquote = FALSE;

	haystack_len = strlen(haystack);
	needle_len   = strlen(needle);

	if (haystack_len < needle_len || needle_len == 0)
		return NULL;

	while (haystack_len >= needle_len) {
		if (!in_squote && !in_dquote &&
		    !strncmp(haystack, needle, needle_len))
			return (gchar *)haystack;

		/* 'foo"bar"' -> foo"bar"
		   "foo'bar'" -> foo'bar' */
		if (*haystack == '\'') {
			if (in_squote)
				in_squote = FALSE;
			else if (!in_dquote)
				in_squote = TRUE;
		} else if (*haystack == '\"') {
			if (in_dquote)
				in_dquote = FALSE;
			else if (!in_squote)
				in_dquote = TRUE;
		}

		haystack++;
		haystack_len--;
	}

	return NULL;
}

gchar *strchr_parenthesis_close(const gchar *str, gchar op, gchar cl)
{
	const gchar *p;
	gchar quote_chr = '"';
	gint in_brace;
	gboolean in_quote = FALSE;

	p = str;

	if ((p = strchr_with_skip_quote(p, quote_chr, op))) {
		p++;
		in_brace = 1;
		while (*p) {
			if (*p == op && !in_quote)
				in_brace++;
			else if (*p == cl && !in_quote)
				in_brace--;
			else if (*p == quote_chr)
				in_quote ^= TRUE;

			if (in_brace == 0)
				return (gchar *)p;

			p++;
		}
	}

	return NULL;
}

gchar **strsplit_parenthesis(const gchar *str, gchar op, gchar cl,
			     gint max_tokens)
{
	GSList *string_list = NULL, *slist;
	gchar **str_array;
	const gchar *s_op, *s_cl;
	guint i, n = 1;

	g_return_val_if_fail(str != NULL, NULL);

	if (max_tokens < 1)
		max_tokens = G_MAXINT;

	s_op = strchr_with_skip_quote(str, '"', op);
	if (!s_op) return NULL;
	str = s_op;
	s_cl = strchr_parenthesis_close(str, op, cl);
	if (s_cl) {
		do {
			guint len;
			gchar *new_string;

			str++;
			len = s_cl - str;
			new_string = g_new(gchar, len + 1);
			strncpy(new_string, str, len);
			new_string[len] = 0;
			string_list = g_slist_prepend(string_list, new_string);
			n++;
			str = s_cl + 1;

			while (*str && g_ascii_isspace(*str)) str++;
			if (*str != op) {
				string_list = g_slist_prepend(string_list,
							      g_strdup(""));
				n++;
				s_op = strchr_with_skip_quote(str, '"', op);
				if (!--max_tokens || !s_op) break;
				str = s_op;
			} else
				s_op = str;
			s_cl = strchr_parenthesis_close(str, op, cl);
		} while (--max_tokens && s_cl);
	}

	str_array = g_new(gchar*, n);

	i = n - 1;

	str_array[i--] = NULL;
	for (slist = string_list; slist; slist = slist->next)
		str_array[i--] = slist->data;

	g_slist_free(string_list);

	return str_array;
}

gchar **strsplit_with_quote(const gchar *str, const gchar *delim,
			    gint max_tokens)
{
	GSList *string_list = NULL, *slist;
	gchar **str_array, *s, *new_str;
	guint i, n = 1, len;

	g_return_val_if_fail(str != NULL, NULL);
	g_return_val_if_fail(delim != NULL, NULL);

	if (max_tokens < 1)
		max_tokens = G_MAXINT;

	s = strstr_with_skip_quote(str, delim);
	if (s) {
		guint delimiter_len = strlen(delim);

		do {
			len = s - str;
			new_str = g_strndup(str, len);

			if (new_str[0] == '\'' || new_str[0] == '\"') {
				if (new_str[len - 1] == new_str[0]) {
					new_str[len - 1] = '\0';
					memmove(new_str, new_str + 1, len - 1);
				}
			}
			string_list = g_slist_prepend(string_list, new_str);
			n++;
			str = s + delimiter_len;
			s = strstr_with_skip_quote(str, delim);
		} while (--max_tokens && s);
	}

	if (*str) {
		new_str = g_strdup(str);
		if (new_str[0] == '\'' || new_str[0] == '\"') {
			len = strlen(str);
			if (new_str[len - 1] == new_str[0]) {
				new_str[len - 1] = '\0';
				memmove(new_str, new_str + 1, len - 1);
			}
		}
		string_list = g_slist_prepend(string_list, new_str);
		n++;
	}

	str_array = g_new(gchar*, n);

	i = n - 1;

	str_array[i--] = NULL;
	for (slist = string_list; slist; slist = slist->next)
		str_array[i--] = slist->data;

	g_slist_free(string_list);

	return str_array;
}

gchar *get_abbrev_newsgroup_name(const gchar *group, gint len)
{
	gchar *abbrev_group;
	gchar *ap;
	const gchar *p = group;
	const gchar *last;

	last = group + strlen(group);
	abbrev_group = ap = g_malloc(strlen(group) + 1);

	while (*p) {
		while (*p == '.')
			*ap++ = *p++;
		if ((ap - abbrev_group) + (last - p) > len && strchr(p, '.')) {
			*ap++ = *p++;
			while (*p != '.') p++;
		} else {
			strcpy(ap, p);
			return abbrev_group;
		}
	}

	*ap = '\0';
	return abbrev_group;
}

gchar *trim_string(const gchar *str, gint len)
{
	const gchar *p = str;
	gint mb_len;
	gchar *new_str;
	gint new_len = 0;

	if (!str) return NULL;
	if (strlen(str) <= len)
		return g_strdup(str);
	if (g_utf8_validate(str, -1, NULL) == FALSE)
		return g_strdup(str);

	while (*p != '\0') {
		mb_len = g_utf8_skip[*(guchar *)p];
		if (mb_len == 0)
			break;
		else if (new_len + mb_len > len)
			break;

		new_len += mb_len;
		p += mb_len;
	}

	Xstrndup_a(new_str, str, new_len, return g_strdup(str));
	return g_strconcat(new_str, "...", NULL);
}

gchar *trim_string_before(const gchar *str, gint len)
{
	const gchar *p = str;
	gint mb_len;
	gint new_len;

	if (!str) return NULL;
	if ((new_len = strlen(str)) <= len)
		return g_strdup(str);
	if (g_utf8_validate(str, -1, NULL) == FALSE)
		return g_strdup(str);

	while (*p != '\0') {
		mb_len = g_utf8_skip[*(guchar *)p];
		if (mb_len == 0)
			break;

		new_len -= mb_len;
		p += mb_len;

		if (new_len <= len)
			break;
	}

	return g_strconcat("...", p, NULL);
}

GList *uri_list_extract_filenames(const gchar *uri_list)
{
	GList *result = NULL;
	const gchar *p, *q;
	gchar *file;

	p = uri_list;

	while (p) {
		if (*p != '#') {
			while (g_ascii_isspace(*p)) p++;
			if (!strncmp(p, "file:", 5)) {
				p += 5;
				while (*p == '/' && *(p + 1) == '/') p++;
				q = p;
				while (*q && *q != '\n' && *q != '\r') q++;

				if (q > p) {
					q--;
					while (q > p && g_ascii_isspace(*q))
						q--;
					file = g_malloc(q - p + 2);
					strncpy(file, p, q - p + 1);
					file[q - p + 1] = '\0';
					decode_uri(file, file);
					result = g_list_append(result,file);
				}
			}
		}
		p = strchr(p, '\n');
		if (p) p++;
	}

	return result;
}

#define HEX_TO_INT(val, hex) \
{ \
	gchar c = hex; \
 \
	if ('0' <= c && c <= '9') { \
		val = c - '0'; \
	} else if ('a' <= c && c <= 'f') { \
		val = c - 'a' + 10; \
	} else if ('A' <= c && c <= 'F') { \
		val = c - 'A' + 10; \
	} else { \
		val = 0; \
	} \
}

/* Converts two-digit hexadecimal to decimal. Used for unescaping escaped
 * characters.
 */
static gint axtoi(const gchar *hex_str)
{
	gint hi, lo;

	HEX_TO_INT(hi, hex_str[0]);
	HEX_TO_INT(lo, hex_str[1]);

	return (hi << 4) + lo;
}

gboolean is_uri_string(const gchar *str)
{
	return (g_ascii_strncasecmp(str, "http://", 7) == 0 ||
		g_ascii_strncasecmp(str, "https://", 8) == 0 ||
		g_ascii_strncasecmp(str, "ftp://", 6) == 0 ||
		g_ascii_strncasecmp(str, "www.", 4) == 0);
}

gchar *get_uri_path(const gchar *uri)
{
	if (g_ascii_strncasecmp(uri, "http://", 7) == 0)
		return (gchar *)(uri + 7);
	else if (g_ascii_strncasecmp(uri, "https://", 8) == 0)
		return (gchar *)(uri + 8);
	else if (g_ascii_strncasecmp(uri, "ftp://", 6) == 0)
		return (gchar *)(uri + 6);
	else
		return (gchar *)uri;
}

gint get_uri_len(const gchar *str)
{
	const gchar *p;

	if (is_uri_string(str)) {
		for (p = str; *p != '\0'; p++) {
			if (!g_ascii_isgraph(*p) || strchr("()<>\"", *p))
				break;
		}
		return p - str;
	}

	return 0;
}

/* Decodes URL-Encoded strings (i.e. strings in which spaces are replaced by
 * plusses, and escape characters are used)
 * Note: decoded_uri and encoded_uri can point the same location
 */
void decode_uri(gchar *decoded_uri, const gchar *encoded_uri)
{
	gchar *dec = decoded_uri;
	const gchar *enc = encoded_uri;

	while (*enc) {
		if (*enc == '%') {
			enc++;
			if (isxdigit((guchar)enc[0]) &&
			    isxdigit((guchar)enc[1])) {
				*dec = axtoi(enc);
				dec++;
				enc += 2;
			}
		} else {
			if (*enc == '+')
				*dec = ' ';
			else
				*dec = *enc;
			dec++;
			enc++;
		}
	}

	*dec = '\0';
}

gchar *encode_uri(const gchar *filename)
{
	gchar *uri;

	uri = g_filename_to_uri(filename, NULL, NULL);
	if (!uri)
		uri = g_strconcat("file://", filename, NULL);

	return uri;
}

gint scan_mailto_url(const gchar *mailto, gchar **to, gchar **cc, gchar **bcc,
		     gchar **subject, gchar **body)
{
	gchar *tmp_mailto;
	gchar *p;

	Xstrdup_a(tmp_mailto, mailto, return -1);

	if (!strncmp(tmp_mailto, "mailto:", 7))
		tmp_mailto += 7;

	p = strchr(tmp_mailto, '?');
	if (p) {
		*p = '\0';
		p++;
	}

	if (to && !*to)
		*to = g_strdup(tmp_mailto);

	while (p) {
		gchar *field, *value;

		field = p;

		p = strchr(p, '=');
		if (!p) break;
		*p = '\0';
		p++;

		value = p;

		p = strchr(p, '&');
		if (p) {
			*p = '\0';
			p++;
		}

		if (*value == '\0') continue;

		if (cc && !*cc && !g_ascii_strcasecmp(field, "cc")) {
			*cc = g_strdup(value);
		} else if (bcc && !*bcc && !g_ascii_strcasecmp(field, "bcc")) {
			*bcc = g_strdup(value);
		} else if (subject && !*subject &&
			   !g_ascii_strcasecmp(field, "subject")) {
			*subject = g_malloc(strlen(value) + 1);
			decode_uri(*subject, value);
		} else if (body && !*body &&
			   !g_ascii_strcasecmp(field, "body")) {
			*body = g_malloc(strlen(value) + 1);
			decode_uri(*body, value);
		}
	}

	return 0;
}

const gchar *get_home_dir(void)
{
#ifdef G_OS_WIN32
	static const gchar *home_dir = NULL;

	if (!home_dir) {
		home_dir = g_get_home_dir();
		if (!home_dir)
			home_dir = "C:\\Sylpheed";
	}

	return home_dir;
#else
	return g_get_home_dir();
#endif
}

const gchar *get_rc_dir(void)
{
	static gchar *rc_dir = NULL;

	if (!rc_dir)
#ifdef G_OS_WIN32
		rc_dir = g_strconcat(get_home_dir(), G_DIR_SEPARATOR_S,
				     "Application Data", G_DIR_SEPARATOR_S,
				     RC_DIR, NULL);
#else
		rc_dir = g_strconcat(get_home_dir(), G_DIR_SEPARATOR_S,
				     RC_DIR, NULL);
#endif

	return rc_dir;
}

const gchar *get_old_rc_dir(void)
{
	static gchar *old_rc_dir = NULL;

	if (!old_rc_dir)
		old_rc_dir = g_strconcat(get_home_dir(), G_DIR_SEPARATOR_S,
					 OLD_RC_DIR, NULL);

	return old_rc_dir;
}

const gchar *get_mail_base_dir(void)
{
#ifdef G_OS_WIN32
	static gchar *mail_base_dir = NULL;

	if (!mail_base_dir)
		mail_base_dir = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
					    "Mailboxes", NULL);

	return mail_base_dir;
#else
	return get_home_dir();
#endif
}

const gchar *get_news_cache_dir(void)
{
	static gchar *news_cache_dir = NULL;

	if (!news_cache_dir)
		news_cache_dir = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
					     NEWS_CACHE_DIR, NULL);

	return news_cache_dir;
}

const gchar *get_imap_cache_dir(void)
{
	static gchar *imap_cache_dir = NULL;

	if (!imap_cache_dir)
		imap_cache_dir = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
					     IMAP_CACHE_DIR, NULL);

	return imap_cache_dir;
}

const gchar *get_mime_tmp_dir(void)
{
	static gchar *mime_tmp_dir = NULL;

	if (!mime_tmp_dir)
		mime_tmp_dir = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
					   MIME_TMP_DIR, NULL);

	return mime_tmp_dir;
}

const gchar *get_template_dir(void)
{
	static gchar *template_dir = NULL;

	if (!template_dir)
		template_dir = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
					   TEMPLATE_DIR, NULL);

	return template_dir;
}

const gchar *get_tmp_dir(void)
{
	static gchar *tmp_dir = NULL;

	if (!tmp_dir)
		tmp_dir = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
				      TMP_DIR, NULL);

	return tmp_dir;
}

gchar *get_tmp_file(void)
{
	gchar *tmp_file;
	static guint32 id = 0;

	tmp_file = g_strdup_printf("%s%ctmpfile.%08x",
				   get_tmp_dir(), G_DIR_SEPARATOR, id++);

	return tmp_file;
}

const gchar *get_domain_name(void)
{
#ifdef G_OS_UNIX
	static gchar *domain_name = NULL;

	if (!domain_name) {
		gchar buf[128] = "";
		struct hostent *hp;

		if (gethostname(buf, sizeof(buf)) < 0) {
			perror("gethostname");
			domain_name = "unknown";
		} else {
			buf[sizeof(buf) - 1] = '\0';
			if ((hp = my_gethostbyname(buf)) == NULL) {
				perror("gethostbyname");
				domain_name = g_strdup(buf);
			} else {
				domain_name = g_strdup(hp->h_name);
			}
		}

		debug_print("domain name = %s\n", domain_name);
	}

	return domain_name;
#else
	return "unknown";
#endif
}

off_t get_file_size(const gchar *file)
{
	struct stat s;

	if (g_stat(file, &s) < 0) {
		FILE_OP_ERROR(file, "stat");
		return -1;
	}

	return s.st_size;
}

off_t get_file_size_as_crlf(const gchar *file)
{
	FILE *fp;
	off_t size = 0;
	gchar buf[BUFFSIZE];

	if ((fp = g_fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return -1;
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		strretchomp(buf);
		size += strlen(buf) + 2;
	}

	if (ferror(fp)) {
		FILE_OP_ERROR(file, "fgets");
		size = -1;
	}

	fclose(fp);

	return size;
}

off_t get_left_file_size(FILE *fp)
{
	glong pos;
	glong end;
	off_t size;

	if ((pos = ftell(fp)) < 0) {
		perror("ftell");
		return -1;
	}
	if (fseek(fp, 0L, SEEK_END) < 0) {
		perror("fseek");
		return -1;
	}
	if ((end = ftell(fp)) < 0) {
		perror("fseek");
		return -1;
	}
	size = end - pos;
	if (fseek(fp, pos, SEEK_SET) < 0) {
		perror("fseek");
		return -1;
	}

	return size;
}

gboolean file_exist(const gchar *file, gboolean allow_fifo)
{
	struct stat s;

	if (file == NULL)
		return FALSE;

	if (g_stat(file, &s) < 0) {
		if (ENOENT != errno) FILE_OP_ERROR(file, "stat");
		return FALSE;
	}

	if (S_ISREG(s.st_mode) || (allow_fifo && S_ISFIFO(s.st_mode)))
		return TRUE;

	return FALSE;
}

gboolean is_dir_exist(const gchar *dir)
{
	if (dir == NULL)
		return FALSE;

	return g_file_test(dir, G_FILE_TEST_IS_DIR);
}

gboolean is_file_entry_exist(const gchar *file)
{
	if (file == NULL)
		return FALSE;

	return g_file_test(file, G_FILE_TEST_EXISTS);
}

gboolean dirent_is_regular_file(struct dirent *d)
{
#ifdef HAVE_DIRENT_D_TYPE
	if (d->d_type == DT_REG)
		return TRUE;
	else if (d->d_type != DT_UNKNOWN)
		return FALSE;
#endif

	return g_file_test(d->d_name, G_FILE_TEST_IS_REGULAR);
}

gboolean dirent_is_directory(struct dirent *d)
{
#ifdef HAVE_DIRENT_D_TYPE
	if (d->d_type == DT_DIR)
		return TRUE;
	else if (d->d_type != DT_UNKNOWN)
		return FALSE;
#endif

	return g_file_test(d->d_name, G_FILE_TEST_IS_DIR);
}

gint change_dir(const gchar *dir)
{
	gchar *prevdir = NULL;

	if (debug_mode)
		prevdir = g_get_current_dir();

	if (g_chdir(dir) < 0) {
		FILE_OP_ERROR(dir, "chdir");
		if (debug_mode) g_free(prevdir);
		return -1;
	} else if (debug_mode) {
		gchar *cwd;

		cwd = g_get_current_dir();
		if (strcmp(prevdir, cwd) != 0)
			g_print("current dir: %s\n", cwd);
		g_free(cwd);
		g_free(prevdir);
	}

	return 0;
}

gint make_dir(const gchar *dir)
{
	if (g_mkdir(dir, S_IRWXU) < 0) {
		FILE_OP_ERROR(dir, "mkdir");
		return -1;
	}
	if (g_chmod(dir, S_IRWXU) < 0)
		FILE_OP_ERROR(dir, "chmod");

	return 0;
}

gint make_dir_hier(const gchar *dir)
{
	gchar *parent_dir;
	const gchar *p;

	for (p = dir; (p = strchr(p, G_DIR_SEPARATOR)) != NULL; p++) {
		parent_dir = g_strndup(dir, p - dir);
		if (*parent_dir != '\0') {
			if (!is_dir_exist(parent_dir)) {
				if (make_dir(parent_dir) < 0) {
					g_free(parent_dir);
					return -1;
				}
			}
		}
		g_free(parent_dir);
	}

	if (!is_dir_exist(dir)) {
		if (make_dir(dir) < 0)
			return -1;
	}

	return 0;
}

gint remove_all_files(const gchar *dir)
{
	GDir *dp;
	const gchar *dir_name;
	gchar *prev_dir;

	prev_dir = g_get_current_dir();

	if (g_chdir(dir) < 0) {
		FILE_OP_ERROR(dir, "chdir");
		g_free(prev_dir);
		return -1;
	}

	if ((dp = g_dir_open(".", 0, NULL)) == NULL) {
		g_warning("failed to open directory: %s\n", dir);
		g_free(prev_dir);
		return -1;
	}

	while ((dir_name = g_dir_read_name(dp)) != NULL) {
		if (g_unlink(dir_name) < 0)
			FILE_OP_ERROR(dir_name, "unlink");
	}

	g_dir_close(dp);

	if (g_chdir(prev_dir) < 0) {
		FILE_OP_ERROR(prev_dir, "chdir");
		g_free(prev_dir);
		return -1;
	}

	g_free(prev_dir);

	return 0;
}

gint remove_numbered_files(const gchar *dir, guint first, guint last)
{
	GDir *dp;
	const gchar *dir_name;
	gchar *prev_dir;
	gint file_no;

	prev_dir = g_get_current_dir();

	if (g_chdir(dir) < 0) {
		FILE_OP_ERROR(dir, "chdir");
		g_free(prev_dir);
		return -1;
	}

	if ((dp = g_dir_open(".", 0, NULL)) == NULL) {
		g_warning("failed to open directory: %s\n", dir);
		g_free(prev_dir);
		return -1;
	}

	while ((dir_name = g_dir_read_name(dp)) != NULL) {
		file_no = to_number(dir_name);
		if (file_no > 0 && first <= file_no && file_no <= last) {
			if (is_dir_exist(dir_name))
				continue;
			if (g_unlink(dir_name) < 0)
				FILE_OP_ERROR(dir_name, "unlink");
		}
	}

	g_dir_close(dp);

	if (g_chdir(prev_dir) < 0) {
		FILE_OP_ERROR(prev_dir, "chdir");
		g_free(prev_dir);
		return -1;
	}

	g_free(prev_dir);

	return 0;
}

gint remove_all_numbered_files(const gchar *dir)
{
	return remove_numbered_files(dir, 0, UINT_MAX);
}

gint remove_expired_files(const gchar *dir, guint hours)
{
	GDir *dp;
	const gchar *dir_name;
	struct stat s;
	gchar *prev_dir;
	gint file_no;
	time_t mtime, now, expire_time;

	prev_dir = g_get_current_dir();

	if (g_chdir(dir) < 0) {
		FILE_OP_ERROR(dir, "chdir");
		g_free(prev_dir);
		return -1;
	}

	if ((dp = g_dir_open(".", 0, NULL)) == NULL) {
		g_warning("failed to open directory: %s\n", dir);
		g_free(prev_dir);
		return -1;
	}

	now = time(NULL);
	expire_time = hours * 60 * 60;

	while ((dir_name = g_dir_read_name(dp)) != NULL) {
		file_no = to_number(dir_name);
		if (file_no > 0) {
			if (g_stat(dir_name, &s) < 0) {
				FILE_OP_ERROR(dir_name, "stat");
				continue;
			}
			if (S_ISDIR(s.st_mode))
				continue;
			mtime = MAX(s.st_mtime, s.st_atime);
			if (now - mtime > expire_time) {
				if (g_unlink(dir_name) < 0)
					FILE_OP_ERROR(dir_name, "unlink");
			}
		}
	}

	g_dir_close(dp);

	if (g_chdir(prev_dir) < 0) {
		FILE_OP_ERROR(prev_dir, "chdir");
		g_free(prev_dir);
		return -1;
	}

	g_free(prev_dir);

	return 0;
}

static gint remove_dir_recursive_real(const gchar *dir)
{
	struct stat s;
	GDir *dp;
	const gchar *dir_name;
	gchar *prev_dir;

	if (g_stat(dir, &s) < 0) {
		FILE_OP_ERROR(dir, "stat");
		if (ENOENT == errno) return 0;
		return -1;
	}

	if (!S_ISDIR(s.st_mode)) {
		if (g_unlink(dir) < 0) {
			FILE_OP_ERROR(dir, "unlink");
			return -1;
		}

		return 0;
	}

	prev_dir = g_get_current_dir();
	/* g_print("prev_dir = %s\n", prev_dir); */

	if (g_chdir(dir) < 0) {
		FILE_OP_ERROR(dir, "chdir");
		g_free(prev_dir);
		return -1;
	}

	if ((dp = g_dir_open(".", 0, NULL)) == NULL) {
		g_warning("failed to open directory: %s\n", dir);
		g_chdir(prev_dir);
		g_free(prev_dir);
		return -1;
	}

	/* remove all files in the directory */
	while ((dir_name = g_dir_read_name(dp)) != NULL) {
		/* g_print("removing %s\n", dir_name); */

		if (is_dir_exist(dir_name)) {
			if (remove_dir_recursive_real(dir_name) < 0) {
				g_warning("can't remove directory\n");
				return -1;
			}
		} else {
			if (g_unlink(dir_name) < 0)
				FILE_OP_ERROR(dir_name, "unlink");
		}
	}

	g_dir_close(dp);

	if (g_chdir(prev_dir) < 0) {
		FILE_OP_ERROR(prev_dir, "chdir");
		g_free(prev_dir);
		return -1;
	}

	g_free(prev_dir);

	if (g_rmdir(dir) < 0) {
		FILE_OP_ERROR(dir, "rmdir");
		return -1;
	}

	return 0;
}

gint remove_dir_recursive(const gchar *dir)
{
	gchar *cur_dir;
	gint ret;

	cur_dir = g_get_current_dir();

	if (g_chdir(dir) < 0) {
		FILE_OP_ERROR(dir, "chdir");
		ret = -1;
		goto leave;
	}
	if (g_chdir("..") < 0) {
		FILE_OP_ERROR(dir, "chdir");
		ret = -1;
		goto leave;
	}

	ret = remove_dir_recursive_real(dir);

leave:
	if (is_dir_exist(cur_dir)) {
		if (g_chdir(cur_dir) < 0) {
			FILE_OP_ERROR(cur_dir, "chdir");
		}
	}

	g_free(cur_dir);

	return ret;
}

gint rename_force(const gchar *oldpath, const gchar *newpath)
{
#ifndef G_OS_UNIX
	if (!is_file_entry_exist(oldpath)) {
		errno = ENOENT;
		return -1;
	}
	if (is_file_exist(newpath)) {
		if (g_unlink(newpath) < 0)
			FILE_OP_ERROR(newpath, "unlink");
	}
#endif
	return g_rename(oldpath, newpath);
}

gint copy_file(const gchar *src, const gchar *dest, gboolean keep_backup)
{
	FILE *src_fp, *dest_fp;
	gint n_read;
	gchar buf[BUFSIZ];
	gchar *dest_bak = NULL;
	gboolean err = FALSE;

	if ((src_fp = g_fopen(src, "rb")) == NULL) {
		FILE_OP_ERROR(src, "fopen");
		return -1;
	}
	if (is_file_exist(dest)) {
		dest_bak = g_strconcat(dest, ".bak", NULL);
		if (rename_force(dest, dest_bak) < 0) {
			FILE_OP_ERROR(dest, "rename");
			fclose(src_fp);
			g_free(dest_bak);
			return -1;
		}
	}

	if ((dest_fp = g_fopen(dest, "wb")) == NULL) {
		FILE_OP_ERROR(dest, "fopen");
		fclose(src_fp);
		if (dest_bak) {
			if (rename_force(dest_bak, dest) < 0)
				FILE_OP_ERROR(dest_bak, "rename");
			g_free(dest_bak);
		}
		return -1;
	}

	if (change_file_mode_rw(dest_fp, dest) < 0) {
		FILE_OP_ERROR(dest, "chmod");
		g_warning(_("can't change file mode\n"));
	}

	while ((n_read = fread(buf, sizeof(gchar), sizeof(buf), src_fp)) > 0) {
		if (n_read < sizeof(buf) && ferror(src_fp))
			break;
		if (fwrite(buf, n_read, 1, dest_fp) < 1) {
			g_warning(_("writing to %s failed.\n"), dest);
			fclose(dest_fp);
			fclose(src_fp);
			g_unlink(dest);
			if (dest_bak) {
				if (rename_force(dest_bak, dest) < 0)
					FILE_OP_ERROR(dest_bak, "rename");
				g_free(dest_bak);
			}
			return -1;
		}
	}

	if (ferror(src_fp)) {
		FILE_OP_ERROR(src, "fread");
		err = TRUE;
	}
	fclose(src_fp);
	if (fclose(dest_fp) == EOF) {
		FILE_OP_ERROR(dest, "fclose");
		err = TRUE;
	}

	if (err) {
		g_unlink(dest);
		if (dest_bak) {
			if (rename_force(dest_bak, dest) < 0)
				FILE_OP_ERROR(dest_bak, "rename");
			g_free(dest_bak);
		}
		return -1;
	}

	if (keep_backup == FALSE && dest_bak)
		g_unlink(dest_bak);

	g_free(dest_bak);

	return 0;
}

gint copy_dir(const gchar *src, const gchar *dest)
{
	GDir *dir;
	const gchar *dir_name;
	gchar *src_file;
	gchar *dest_file;

	if ((dir = g_dir_open(src, 0, NULL)) == NULL) {
		g_warning("failed to open directory: %s\n", src);
		return -1;
	}

	if (make_dir_hier(dest) < 0) {
		g_dir_close(dir);
		return -1;
	}

	while ((dir_name = g_dir_read_name(dir)) != NULL) {
		src_file = g_strconcat(src, G_DIR_SEPARATOR_S, dir_name, NULL);
		dest_file = g_strconcat(dest, G_DIR_SEPARATOR_S, dir_name,
					NULL);
		if (is_file_exist(src_file))
			copy_file(src_file, dest_file, FALSE);
		g_free(dest_file);
		g_free(src_file);
	}

	g_dir_close(dir);

	return 0;
}

gint move_file(const gchar *src, const gchar *dest, gboolean overwrite)
{
	if (overwrite == FALSE && is_file_exist(dest)) {
		g_warning("move_file(): file %s already exists.", dest);
		return -1;
	}

	if (rename_force(src, dest) == 0) return 0;

	if (EXDEV != errno) {
		FILE_OP_ERROR(src, "rename");
		return -1;
	}

	if (copy_file(src, dest, FALSE) < 0) return -1;

	g_unlink(src);

	return 0;
}

gint copy_file_part(FILE *fp, off_t offset, size_t length, const gchar *dest)
{
	FILE *dest_fp;
	gint n_read;
	gint bytes_left, to_read;
	gchar buf[BUFSIZ];
	gboolean err = FALSE;

	if (fseek(fp, offset, SEEK_SET) < 0) {
		perror("fseek");
		return -1;
	}

	if ((dest_fp = g_fopen(dest, "wb")) == NULL) {
		FILE_OP_ERROR(dest, "fopen");
		return -1;
	}

	if (change_file_mode_rw(dest_fp, dest) < 0) {
		FILE_OP_ERROR(dest, "chmod");
		g_warning("can't change file mode\n");
	}

	bytes_left = length;
	to_read = MIN(bytes_left, sizeof(buf));

	while ((n_read = fread(buf, sizeof(gchar), to_read, fp)) > 0) {
		if (n_read < to_read && ferror(fp))
			break;
		if (fwrite(buf, n_read, 1, dest_fp) < 1) {
			g_warning(_("writing to %s failed.\n"), dest);
			fclose(dest_fp);
			g_unlink(dest);
			return -1;
		}
		bytes_left -= n_read;
		if (bytes_left == 0)
			break;
		to_read = MIN(bytes_left, sizeof(buf));
	}

	if (ferror(fp)) {
		perror("fread");
		err = TRUE;
	}
	if (fclose(dest_fp) == EOF) {
		FILE_OP_ERROR(dest, "fclose");
		err = TRUE;
	}

	if (err) {
		g_unlink(dest);
		return -1;
	}

	return 0;
}

/* convert line endings into CRLF. If the last line doesn't end with
 * linebreak, add it.
 */
gchar *canonicalize_str(const gchar *str)
{
	const gchar *p;
	guint new_len = 0;
	gchar *out, *outp;

	for (p = str; *p != '\0'; ++p) {
		if (*p != '\r') {
			++new_len;
			if (*p == '\n')
				++new_len;
		}
	}
	if (p == str || *(p - 1) != '\n')
		new_len += 2;

	out = outp = g_malloc(new_len + 1);
	for (p = str; *p != '\0'; ++p) {
		if (*p != '\r') {
			if (*p == '\n')
				*outp++ = '\r';
			*outp++ = *p;
		}
	}
	if (p == str || *(p - 1) != '\n') {
		*outp++ = '\r';
		*outp++ = '\n';
	}
	*outp = '\0';

	return out;
}

gint canonicalize_file(const gchar *src, const gchar *dest)
{
	FILE *src_fp, *dest_fp;
	gchar buf[BUFFSIZE];
	gint len;
	gboolean err = FALSE;
	gboolean last_linebreak = FALSE;

	if ((src_fp = g_fopen(src, "rb")) == NULL) {
		FILE_OP_ERROR(src, "fopen");
		return -1;
	}

	if ((dest_fp = g_fopen(dest, "wb")) == NULL) {
		FILE_OP_ERROR(dest, "fopen");
		fclose(src_fp);
		return -1;
	}

	if (change_file_mode_rw(dest_fp, dest) < 0) {
		FILE_OP_ERROR(dest, "chmod");
		g_warning("can't change file mode\n");
	}

	while (fgets(buf, sizeof(buf), src_fp) != NULL) {
		gint r = 0;

		len = strlen(buf);
		if (len == 0) break;
		last_linebreak = FALSE;

		if (buf[len - 1] != '\n') {
			last_linebreak = TRUE;
			r = fputs(buf, dest_fp);
		} else if (len > 1 && buf[len - 1] == '\n' && buf[len - 2] == '\r') {
			r = fputs(buf, dest_fp);
		} else {
			if (len > 1) {
				r = fwrite(buf, len - 1, 1, dest_fp);
				if (r != 1)
					r = EOF;
			}
			if (r != EOF)
				r = fputs("\r\n", dest_fp);
		}

		if (r == EOF) {
			g_warning("writing to %s failed.\n", dest);
			fclose(dest_fp);
			fclose(src_fp);
			g_unlink(dest);
			return -1;
		}
	}

	if (last_linebreak == TRUE) {
		if (fputs("\r\n", dest_fp) == EOF)
			err = TRUE;
	}

	if (ferror(src_fp)) {
		FILE_OP_ERROR(src, "fgets");
		err = TRUE;
	}
	fclose(src_fp);
	if (fclose(dest_fp) == EOF) {
		FILE_OP_ERROR(dest, "fclose");
		err = TRUE;
	}

	if (err) {
		g_unlink(dest);
		return -1;
	}

	return 0;
}

gint canonicalize_file_replace(const gchar *file)
{
	gchar *tmp_file;

	tmp_file = get_tmp_file();

	if (canonicalize_file(file, tmp_file) < 0) {
		g_free(tmp_file);
		return -1;
	}

	if (move_file(tmp_file, file, TRUE) < 0) {
		g_warning("can't replace %s .\n", file);
		g_unlink(tmp_file);
		g_free(tmp_file);
		return -1;
	}

	g_free(tmp_file);
	return 0;
}

gint uncanonicalize_file(const gchar *src, const gchar *dest)
{
	FILE *src_fp, *dest_fp;
	gchar buf[BUFFSIZE];
	gboolean err = FALSE;

	if ((src_fp = g_fopen(src, "rb")) == NULL) {
		FILE_OP_ERROR(src, "fopen");
		return -1;
	}

	if ((dest_fp = g_fopen(dest, "wb")) == NULL) {
		FILE_OP_ERROR(dest, "fopen");
		fclose(src_fp);
		return -1;
	}

	if (change_file_mode_rw(dest_fp, dest) < 0) {
		FILE_OP_ERROR(dest, "chmod");
		g_warning("can't change file mode\n");
	}

	while (fgets(buf, sizeof(buf), src_fp) != NULL) {
		strcrchomp(buf);
		if (fputs(buf, dest_fp) == EOF) {
			g_warning("writing to %s failed.\n", dest);
			fclose(dest_fp);
			fclose(src_fp);
			g_unlink(dest);
			return -1;
		}
	}

	if (ferror(src_fp)) {
		FILE_OP_ERROR(src, "fgets");
		err = TRUE;
	}
	fclose(src_fp);
	if (fclose(dest_fp) == EOF) {
		FILE_OP_ERROR(dest, "fclose");
		err = TRUE;
	}

	if (err) {
		g_unlink(dest);
		return -1;
	}

	return 0;
}

gint uncanonicalize_file_replace(const gchar *file)
{
	gchar *tmp_file;

	tmp_file = get_tmp_file();

	if (uncanonicalize_file(file, tmp_file) < 0) {
		g_free(tmp_file);
		return -1;
	}

	if (move_file(tmp_file, file, TRUE) < 0) {
		g_warning("can't replace %s .\n", file);
		g_unlink(tmp_file);
		g_free(tmp_file);
		return -1;
	}

	g_free(tmp_file);
	return 0;
}

gchar *normalize_newlines(const gchar *str)
{
	const gchar *p = str;
	gchar *out, *outp;

	out = outp = g_malloc(strlen(str) + 1);
	for (p = str; *p != '\0'; ++p) {
		if (*p == '\r') {
			if (*(p + 1) != '\n')
				*outp++ = '\n';
		} else
			*outp++ = *p;
	}

	*outp = '\0';

	return out;
}

gchar *get_outgoing_rfc2822_str(FILE *fp)
{
	gchar buf[BUFFSIZE];
	GString *str;
	gchar *ret;

	str = g_string_new(NULL);

	/* output header part */
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		strretchomp(buf);
		if (!g_ascii_strncasecmp(buf, "Bcc:", 4)) {
			gint next;

			for (;;) {
				next = fgetc(fp);
				if (next == EOF)
					break;
				else if (next != ' ' && next != '\t') {
					ungetc(next, fp);
					break;
				}
				if (fgets(buf, sizeof(buf), fp) == NULL)
					break;
			}
#if 0
		} else if (!g_ascii_strncasecmp(buf, "Date:", 5)) {
			get_rfc822_date(buf, sizeof(buf));
			g_string_append_printf(str, "Date: %s\r\n", buf);
#endif
		} else {
			g_string_append(str, buf);
			g_string_append(str, "\r\n");
			if (buf[0] == '\0')
				break;
		}
	}

	/* output body part */
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		strretchomp(buf);
		if (buf[0] == '.')
			g_string_append_c(str, '.');
		g_string_append(str, buf);
		g_string_append(str, "\r\n");
	}

	ret = str->str;
	g_string_free(str, FALSE);

	return ret;
}

/*
 * Create a new boundary in a way that it is very unlikely that this
 * will occur in the following text.  It would be easy to ensure
 * uniqueness if everything is either quoted-printable or base64
 * encoded (note that conversion is allowed), but because MIME bodies
 * may be nested, it may happen that the same boundary has already
 * been used. We avoid scanning the message for conflicts and hope the
 * best.
 *
 *   boundary := 0*69<bchars> bcharsnospace
 *   bchars := bcharsnospace / " "
 *   bcharsnospace := DIGIT / ALPHA / "'" / "(" / ")" /
 *                    "+" / "_" / "," / "-" / "." /
 *                    "/" / ":" / "=" / "?"
 *
 * some special characters removed because of buggy MTAs
 */

gchar *generate_mime_boundary(const gchar *prefix)
{
	static gchar tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			     "abcdefghijklmnopqrstuvwxyz"
			     "1234567890+_./=";
	gchar buf_uniq[17];
	gchar buf_date[64];
	gint i;

	for (i = 0; i < sizeof(buf_uniq) - 1; i++)
		buf_uniq[i] = tbl[g_random_int_range(0, sizeof(tbl) - 1)];
	buf_uniq[i] = '\0';

	get_rfc822_date(buf_date, sizeof(buf_date));
	subst_char(buf_date, ' ', '_');
	subst_char(buf_date, ',', '_');
	subst_char(buf_date, ':', '_');

	return g_strdup_printf("%s=_%s_%s", prefix ? prefix : "Multipart",
			       buf_date, buf_uniq);
}

gint change_file_mode_rw(FILE *fp, const gchar *file)
{
#if HAVE_FCHMOD
	return fchmod(fileno(fp), S_IRUSR|S_IWUSR);
#else
	return g_chmod(file, S_IRUSR|S_IWUSR);
#endif
}

FILE *my_tmpfile(void)
{
#if HAVE_MKSTEMP
	const gchar suffix[] = ".XXXXXX";
	const gchar *tmpdir;
	guint tmplen;
	const gchar *progname;
	guint proglen;
	gchar *fname;
	gint fd;
	FILE *fp;

	tmpdir = get_tmp_dir();
	tmplen = strlen(tmpdir);
	progname = g_get_prgname();
	proglen = strlen(progname);
	Xalloca(fname, tmplen + 1 + proglen + sizeof(suffix),
		return tmpfile());

	memcpy(fname, tmpdir, tmplen);
	fname[tmplen] = G_DIR_SEPARATOR;
	memcpy(fname + tmplen + 1, progname, proglen);
	memcpy(fname + tmplen + 1 + proglen, suffix, sizeof(suffix));

	fd = mkstemp(fname);
	if (fd < 0)
		return tmpfile();

	g_unlink(fname);

	fp = fdopen(fd, "w+b");
	if (!fp)
		close(fd);
	else
		return fp;
#endif /* HAVE_MKSTEMP */

	return tmpfile();
}

FILE *str_open_as_stream(const gchar *str)
{
	FILE *fp;
	size_t len;

	g_return_val_if_fail(str != NULL, NULL);

	fp = my_tmpfile();
	if (!fp) {
		FILE_OP_ERROR("str_open_as_stream", "my_tmpfile");
		return NULL;
	}

	len = strlen(str);
	if (len == 0) return fp;

	if (fwrite(str, len, 1, fp) != 1) {
		FILE_OP_ERROR("str_open_as_stream", "fwrite");
		fclose(fp);
		return NULL;
	}

	rewind(fp);
	return fp;
}

gint str_write_to_file(const gchar *str, const gchar *file)
{
	FILE *fp;
	size_t len;

	g_return_val_if_fail(str != NULL, -1);
	g_return_val_if_fail(file != NULL, -1);

	if ((fp = g_fopen(file, "wb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return -1;
	}

	len = strlen(str);
	if (len == 0) {
		fclose(fp);
		return 0;
	}

	if (fwrite(str, len, 1, fp) != 1) {
		FILE_OP_ERROR(file, "fwrite");
		fclose(fp);
		g_unlink(file);
		return -1;
	}

	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(file, "fclose");
		g_unlink(file);
		return -1;
	}

	return 0;
}

gchar *file_read_to_str(const gchar *file)
{
	FILE *fp;
	gchar *str;

	g_return_val_if_fail(file != NULL, NULL);

	if ((fp = g_fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return NULL;
	}

	str = file_read_stream_to_str(fp);

	fclose(fp);

	return str;
}

gchar *file_read_stream_to_str(FILE *fp)
{
	GByteArray *array;
	guchar buf[BUFSIZ];
	gint n_read;
	gchar *str;

	g_return_val_if_fail(fp != NULL, NULL);

	array = g_byte_array_new();

	while ((n_read = fread(buf, sizeof(gchar), sizeof(buf), fp)) > 0) {
		if (n_read < sizeof(buf) && ferror(fp))
			break;
		g_byte_array_append(array, buf, n_read);
	}

	if (ferror(fp)) {
		FILE_OP_ERROR("file stream", "fread");
		g_byte_array_free(array, TRUE);
		return NULL;
	}

	buf[0] = '\0';
	g_byte_array_append(array, buf, 1);
	str = (gchar *)array->data;
	g_byte_array_free(array, FALSE);

	return str;
}

gint execute_async(gchar *const argv[])
{
	g_return_val_if_fail(argv != NULL && argv[0] != NULL, -1);

	if (g_spawn_async(NULL, (gchar **)argv, NULL, G_SPAWN_SEARCH_PATH,
			  NULL, NULL, NULL, FALSE) == FALSE) {
		g_warning("Can't execute command: %s\n", argv[0]);
		return -1;
	}

	return 0;
}

gint execute_sync(gchar *const argv[])
{
	gint status;

	g_return_val_if_fail(argv != NULL && argv[0] != NULL, -1);

	if (g_spawn_sync(NULL, (gchar **)argv, NULL, G_SPAWN_SEARCH_PATH,
			 NULL, NULL, NULL, NULL, &status, NULL) == FALSE) {
		g_warning("Can't execute command: %s\n", argv[0]);
		return -1;
	}

#ifdef G_OS_UNIX
	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	else
		return -1;
#else
	return status;
#endif
}

gint execute_command_line(const gchar *cmdline, gboolean async)
{
	gchar **argv;
	gint ret;

	debug_print("execute_command_line(): executing: %s\n", cmdline);

	argv = strsplit_with_quote(cmdline, " ", 0);

	if (async)
		ret = execute_async(argv);
	else
		ret = execute_sync(argv);

	g_strfreev(argv);

	return ret;
}

gchar *get_command_output(const gchar *cmdline)
{
	gchar *child_stdout;
	gint status;

	g_return_val_if_fail(cmdline != NULL, NULL);

	debug_print("get_command_output(): executing: %s\n", cmdline);

	if (g_spawn_command_line_sync(cmdline, &child_stdout, NULL, &status,
				      NULL) == FALSE) {
		g_warning("Can't execute command: %s\n", cmdline);
		return NULL;
	}

	return child_stdout;
}

gint open_uri(const gchar *uri, const gchar *cmdline)
{
	gchar buf[BUFFSIZE];
	gchar *p;

	g_return_val_if_fail(uri != NULL, -1);

	if (cmdline &&
	    (p = strchr(cmdline, '%')) && *(p + 1) == 's' &&
	    !strchr(p + 2, '%'))
		g_snprintf(buf, sizeof(buf), cmdline, uri);
	else {
		if (cmdline)
			g_warning("Open URI command line is invalid "
				  "(there must be only one '%%s'): %s",
				  cmdline);
		g_snprintf(buf, sizeof(buf), DEFAULT_BROWSER_CMD, uri);
	}

	execute_command_line(buf, TRUE);

	return 0;
}

time_t remote_tzoffset_sec(const gchar *zone)
{
	static gchar ustzstr[] = "PSTPDTMSTMDTCSTCDTESTEDT";
	gchar zone3[4];
	gchar *p;
	gchar c;
	gint iustz;
	gint offset;
	time_t remoteoffset;

	strncpy(zone3, zone, 3);
	zone3[3] = '\0';
	remoteoffset = 0;

	if (sscanf(zone, "%c%d", &c, &offset) == 2 &&
	    (c == '+' || c == '-')) {
		remoteoffset = ((offset / 100) * 60 + (offset % 100)) * 60;
		if (c == '-')
			remoteoffset = -remoteoffset;
	} else if (!strncmp(zone, "UT" , 2) ||
		   !strncmp(zone, "GMT", 2)) {
		remoteoffset = 0;
	} else if (strlen(zone3) == 3) {
		for (p = ustzstr; *p != '\0'; p += 3) {
			if (!g_ascii_strncasecmp(p, zone3, 3)) {
				iustz = ((gint)(p - ustzstr) / 3 + 1) / 2 - 8;
				remoteoffset = iustz * 3600;
				break;
			}
		}
		if (*p == '\0')
			return -1;
	} else if (strlen(zone3) == 1) {
		switch (zone[0]) {
		case 'Z': remoteoffset =   0; break;
		case 'A': remoteoffset =  -1; break;
		case 'B': remoteoffset =  -2; break;
		case 'C': remoteoffset =  -3; break;
		case 'D': remoteoffset =  -4; break;
		case 'E': remoteoffset =  -5; break;
		case 'F': remoteoffset =  -6; break;
		case 'G': remoteoffset =  -7; break;
		case 'H': remoteoffset =  -8; break;
		case 'I': remoteoffset =  -9; break;
		case 'K': remoteoffset = -10; break; /* J is not used */
		case 'L': remoteoffset = -11; break;
		case 'M': remoteoffset = -12; break;
		case 'N': remoteoffset =   1; break;
		case 'O': remoteoffset =   2; break;
		case 'P': remoteoffset =   3; break;
		case 'Q': remoteoffset =   4; break;
		case 'R': remoteoffset =   5; break;
		case 'S': remoteoffset =   6; break;
		case 'T': remoteoffset =   7; break;
		case 'U': remoteoffset =   8; break;
		case 'V': remoteoffset =   9; break;
		case 'W': remoteoffset =  10; break;
		case 'X': remoteoffset =  11; break;
		case 'Y': remoteoffset =  12; break;
		default:  remoteoffset =   0; break;
		}
		remoteoffset = remoteoffset * 3600;
	} else
		return -1;

	return remoteoffset;
}

time_t tzoffset_sec(time_t *now)
{
	struct tm gmt, *lt;
	gint off;

	gmt = *gmtime(now);
	lt = localtime(now);

	off = (lt->tm_hour - gmt.tm_hour) * 60 + lt->tm_min - gmt.tm_min;

	if (lt->tm_year < gmt.tm_year)
		off -= 24 * 60;
	else if (lt->tm_year > gmt.tm_year)
		off += 24 * 60;
	else if (lt->tm_yday < gmt.tm_yday)
		off -= 24 * 60;
	else if (lt->tm_yday > gmt.tm_yday)
		off += 24 * 60;

	if (off >= 24 * 60)		/* should be impossible */
		off = 23 * 60 + 59;	/* if not, insert silly value */
	if (off <= -24 * 60)
		off = -(23 * 60 + 59);

	return off * 60;
}

/* calculate timezone offset */
gchar *tzoffset(time_t *now)
{
	static gchar offset_string[6];
	struct tm gmt, *lt;
	gint off;
	gchar sign = '+';

	gmt = *gmtime(now);
	lt = localtime(now);

	off = (lt->tm_hour - gmt.tm_hour) * 60 + lt->tm_min - gmt.tm_min;

	if (lt->tm_year < gmt.tm_year)
		off -= 24 * 60;
	else if (lt->tm_year > gmt.tm_year)
		off += 24 * 60;
	else if (lt->tm_yday < gmt.tm_yday)
		off -= 24 * 60;
	else if (lt->tm_yday > gmt.tm_yday)
		off += 24 * 60;

	if (off < 0) {
		sign = '-';
		off = -off;
	}

	if (off >= 24 * 60)		/* should be impossible */
		off = 23 * 60 + 59;	/* if not, insert silly value */

	sprintf(offset_string, "%c%02d%02d", sign, off / 60, off % 60);

	return offset_string;
}

void get_rfc822_date(gchar *buf, gint len)
{
	struct tm *lt;
	time_t t;
	gchar day[4], mon[4];
	gint dd, hh, mm, ss, yyyy;

	t = time(NULL);
	lt = localtime(&t);

	sscanf(asctime(lt), "%3s %3s %d %d:%d:%d %d\n",
	       day, mon, &dd, &hh, &mm, &ss, &yyyy);
	g_snprintf(buf, len, "%s, %d %s %d %02d:%02d:%02d %s",
		   day, dd, mon, yyyy, hh, mm, ss, tzoffset(&t));
}

/* just a wrapper to suppress the warning of gcc about %c */
size_t my_strftime(gchar *s, size_t max, const gchar *format,
		   const struct tm *tm)
{
	return strftime(s, max, format, tm);
}

static FILE *log_fp = NULL;

void set_log_file(const gchar *filename)
{
	if (log_fp) return;
	log_fp = g_fopen(filename, "wb");
	if (!log_fp)
		FILE_OP_ERROR(filename, "fopen");
}

void close_log_file(void)
{
	if (log_fp) {
		fclose(log_fp);
		log_fp = NULL;
	}
}

static guint log_verbosity_count = 0;

void set_log_verbosity(gboolean verbose)
{
	if (verbose)
		log_verbosity_count++;
	else if (log_verbosity_count > 0)
		log_verbosity_count--;
}

gboolean get_debug_mode(void)
{
	return debug_mode;
}

void set_debug_mode(gboolean enable)
{
	debug_mode = enable;
}

void debug_print(const gchar *format, ...)
{
	va_list args;
	gchar buf[BUFFSIZE];

	if (!debug_mode) return;

	va_start(args, format);
	g_vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	g_print("%s", buf);
}

#define TIME_LEN	11

void log_print(const gchar *format, ...)
{
	va_list args;
	gchar buf[BUFFSIZE + TIME_LEN];
	time_t t;

	time(&t);
	strftime(buf, TIME_LEN + 1, "[%H:%M:%S] ", localtime(&t));

	va_start(args, format);
	g_vsnprintf(buf + TIME_LEN, BUFFSIZE, format, args);
	va_end(args);

	if (debug_mode) fputs(buf, stdout);
	log_window_append(buf, LOG_NORMAL);
	if (log_fp) {
		fputs(buf, log_fp);
		fflush(log_fp);
	}
	if (log_verbosity_count)
		statusbar_puts_all(buf + TIME_LEN);
}

void log_message(const gchar *format, ...)
{
	va_list args;
	gchar buf[BUFFSIZE + TIME_LEN];
	time_t t;

	time(&t);
	strftime(buf, TIME_LEN + 1, "[%H:%M:%S] ", localtime(&t));

	va_start(args, format);
	g_vsnprintf(buf + TIME_LEN, BUFFSIZE, format, args);
	va_end(args);

	if (debug_mode) g_message("%s", buf + TIME_LEN);
	log_window_append(buf + TIME_LEN, LOG_MSG);
	if (log_fp) {
		fwrite(buf, TIME_LEN, 1, log_fp);
		fputs("* message: ", log_fp);
		fputs(buf + TIME_LEN, log_fp);
		fflush(log_fp);
	}
	statusbar_puts_all(buf + TIME_LEN);
}

void log_warning(const gchar *format, ...)
{
	va_list args;
	gchar buf[BUFFSIZE + TIME_LEN];
	time_t t;

	time(&t);
	strftime(buf, TIME_LEN + 1, "[%H:%M:%S] ", localtime(&t));

	va_start(args, format);
	g_vsnprintf(buf + TIME_LEN, BUFFSIZE, format, args);
	va_end(args);

	g_warning("%s", buf);
	log_window_append(buf + TIME_LEN, LOG_WARN);
	if (log_fp) {
		fwrite(buf, TIME_LEN, 1, log_fp);
		fputs("** warning: ", log_fp);
		fputs(buf + TIME_LEN, log_fp);
		fflush(log_fp);
	}
}

void log_error(const gchar *format, ...)
{
	va_list args;
	gchar buf[BUFFSIZE + TIME_LEN];
	time_t t;

	time(&t);
	strftime(buf, TIME_LEN + 1, "[%H:%M:%S] ", localtime(&t));

	va_start(args, format);
	g_vsnprintf(buf + TIME_LEN, BUFFSIZE, format, args);
	va_end(args);

	g_warning("%s", buf);
	log_window_append(buf + TIME_LEN, LOG_ERROR);
	if (log_fp) {
		fwrite(buf, TIME_LEN, 1, log_fp);
		fputs("*** error: ", log_fp);
		fputs(buf + TIME_LEN, log_fp);
		fflush(log_fp);
	}
}
