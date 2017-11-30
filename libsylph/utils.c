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
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#if HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#include <dirent.h>
#include <time.h>

#ifdef G_OS_WIN32
#ifndef WINVER
#  define WINVER 0x0500
#endif
#  include <windows.h>
#  include <wchar.h>
#  include <direct.h>
#  include <io.h>
#  include <shlobj.h>
#endif

#ifdef __APPLE__
#  include <AppKit/AppKit.h>
#endif

#include "utils.h"
#include "socket.h"

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

#ifndef G_OS_UNIX
gint syl_link(const gchar *src, const gchar *dest)
{
#ifdef G_OS_WIN32
	wchar_t *wsrc;
	wchar_t *wdest;
	gint retval;
	gint save_errno;

	wsrc = g_utf8_to_utf16(src, -1, NULL, NULL, NULL);
	if (wsrc == NULL) {
		errno = EINVAL;
		return -1;
	}
	wdest = g_utf8_to_utf16(dest, -1, NULL, NULL, NULL);
	if (wdest == NULL) {
		g_free(wsrc);
		errno = EINVAL;
		return -1;
	}

	errno = 0;
	if (CreateHardLinkW(wdest, wsrc, NULL)) {
		retval = 0;
		/* debug_print("hard link created: %s -> %s\n", src, dest); */
	} else {
		retval = -1;
		switch (GetLastError()) {
		case ERROR_FILE_NOT_FOUND:
		case ERROR_PATH_NOT_FOUND:
			errno = ENOENT; break;
		case ERROR_ACCESS_DENIED:
		case ERROR_LOCK_VIOLATION:
		case ERROR_SHARING_VIOLATION:
			errno = EACCES; break;
		case ERROR_NOT_SAME_DEVICE:
			errno = EXDEV; break;
		case ERROR_FILE_EXISTS:
		case ERROR_ALREADY_EXISTS:
			errno = EEXIST; break;
		case ERROR_TOO_MANY_LINKS:
			errno = EMLINK; break;
		default:
			errno = EIO; break;
		}
	}
	save_errno = errno;

	g_free(wdest);
	g_free(wsrc);

	errno = save_errno;
	return retval;
#else
	return link(src, dest);
#endif
}
#endif /* !G_OS_UNIX */

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

guint to_unumber(const gchar *nstr)
{
	register const gchar *p;
	gulong val;

	if (*nstr == '\0') return 0;

	for (p = nstr; *p != '\0'; p++)
		if (!g_ascii_isdigit(*p)) return 0;

	errno = 0;
	val = strtoul(nstr, NULL, 10);
	if (val == ULONG_MAX && errno != 0)
		val = 0;

	return (guint)val;
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

gchar *utos_buf(gchar *nstr, guint n)
{
	g_snprintf(nstr, 11, "%u", n);
	return nstr;
}

gchar *to_human_readable_buf(gchar *buf, size_t bufsize, gint64 size)
{
	if (size < 1024)
		g_snprintf(buf, bufsize, "%dB", (gint)size);
	else if ((size >> 10) < 1024)
		g_snprintf(buf, bufsize, "%.1fKB", (gfloat)size / (1 << 10));
	else if ((size >> 20) < 1024)
		g_snprintf(buf, bufsize, "%.2fMB", (gfloat)size / (1 << 20));
	else
		g_snprintf(buf, bufsize, "%.2fGB", (gfloat)size / (1 << 30));

	return buf;
}

gchar *to_human_readable(gint64 size)
{
	static gchar str[16];

	return to_human_readable_buf(str, sizeof(str), size);
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

/* return TRUE if parent is equal to or ancestor of child */
gboolean is_path_parent(const gchar *parent, const gchar *child)
{
	gint plen;
	const gchar *base;

	g_return_val_if_fail(parent != NULL, FALSE);
	g_return_val_if_fail(child != NULL, FALSE);

	plen = strlen(parent);
	while (plen > 0 && G_IS_DIR_SEPARATOR(parent[plen - 1]))
		plen--;

#ifdef G_OS_WIN32
	if (!g_ascii_strncasecmp(parent, child, plen)) {
#else
	if (!strncmp(parent, child, plen)) {
#endif
		base = child + plen;
		if (!G_IS_DIR_SEPARATOR(*base) && *base != '\0')
			return FALSE;
		return TRUE;
	}

	return FALSE;
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
	size_t haystack_left = haystacklen;

	if (needlelen == 1)
		return memchr(haystack_, *needle_, haystacklen);

	while ((haystack_cur = memchr(haystack_cur, *needle_, haystack_left))
	       != NULL) {
		if (haystacklen - (haystack_cur - haystack_) < needlelen)
			break;
		if (memcmp(haystack_cur + 1, needle_ + 1, needlelen - 1) == 0)
			return (gpointer)haystack_cur;
		else {
			haystack_cur++;
			haystack_left = haystacklen - (haystack_cur - haystack_);
		}
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

/* Similar to g_str_has_suffix() but case-insensitive */
gboolean str_has_suffix_case(const gchar *str, const gchar *suffix)
{
	size_t len, s_len;

	if (!str || !suffix)
		return FALSE;

	len = strlen(str);
	s_len = strlen(suffix);

	if (s_len > len)
		return FALSE;

	return (g_ascii_strcasecmp(str + (len - s_len), suffix) == 0);
}

gint str_find_format_times(const gchar *haystack, gchar ch)
{
	gint n = 0;
	const gchar *p = haystack;

	while ((p = strchr(p, '%')) != NULL) {
		++p;
		if (*p == '%') {
			++p;
		} else if (*p == ch) {
			++p;
			++n;
		} else
			return -1;
	}

	return n;
}

/* Examine if next block is non-ASCII string */
gboolean is_next_nonascii(const gchar *s)
{
	const gchar *p;
	gboolean in_quote = FALSE;

	/* skip head space */
	for (p = s; *p != '\0' && g_ascii_isspace(*p); p++)
		;
	while (*p != '\0') {
		if (!in_quote && g_ascii_isspace(*p))
			break;
		if (*p == '"')
			in_quote ^= TRUE;
		else if (*(guchar *)p > 127 || *(guchar *)p < 32)
			return TRUE;
		++p;
	}

	return FALSE;
}

gint get_next_word_len(const gchar *s)
{
	const gchar *p = s;
	gboolean in_quote = FALSE;

	while (*p != '\0') {
		if (!in_quote && g_ascii_isspace(*p))
			break;
		if (*p == '"')
			in_quote ^= TRUE;
		++p;
	}

	return p - s;
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

void extract_parenthesis_with_escape(gchar *str, gchar op, gchar cl)
{
	register gchar *srcp, *destp;
	gint in_brace;

	srcp = destp = str;

	while ((srcp = strchr(srcp, op))) {
		if (destp > str)
			*destp++ = ' ';
		++srcp;
		in_brace = 1;
		while (*srcp) {
			if (*srcp == op)
				in_brace++;
			else if (*srcp == cl)
				in_brace--;

			if (in_brace == 0)
				break;

			if (*srcp == '\\' && *(srcp + 1) != '\0')
				++srcp;

			*destp++ = *srcp++;
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

void extract_quote_with_escape(gchar *str, gchar quote_chr)
{
	register gchar *sp, *dp;

	if ((sp = strchr(str, quote_chr))) {
		dp = sp;
		++sp;
		while (*sp) {
			if (*sp == quote_chr)
				break;
			else if (*sp == '\\' && *(sp + 1) != '\0')
				++sp;

			*dp++ = *sp++;
		}
		*dp = '\0';
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

gchar *extract_addresses(const gchar *str)
{
	GString *new_str;
	GSList *addr_list, *cur;

	if (!str)
		return NULL;

	addr_list = address_list_append(NULL, str);

	new_str = g_string_new(NULL);

	for (cur = addr_list; cur != NULL; cur = cur->next) {
		g_string_append(new_str, (gchar *)cur->data);
		if (cur->next)
			g_string_append(new_str, ", ");
	}

	slist_free_strings(addr_list);
	g_slist_free(addr_list);

	return g_string_free(new_str, FALSE);
}

gchar *normalize_address_field(const gchar *str)
{
	GString *new_str;
	GSList *addr_list, *cur;
	gchar *addr, *p, *q, *r;
	gchar *ret_str;

	addr_list = address_list_append_orig(NULL, str);

	new_str = g_string_new(NULL);

	for (cur = addr_list; cur != NULL; cur = cur->next) {
		p = addr = (gchar *)cur->data;
		q = strchr_with_skip_quote(p, '"', '<');
		if (q && q > p) {
			r = q - 1;
			while (r > p && g_ascii_isspace(*r))
				--r;
			g_string_append_len(new_str, p, r - p + 1);
			g_string_append_c(new_str, ' ');
			p = q;
		}
		if (*p == '<') {
			q = strchr(p, '>');
			if (q) {
				r = q + 1;
				if (*r) {
					while (g_ascii_isspace(*r))
						++r;
					g_string_append(new_str, r);
					if (new_str->len > 0 &&
					    !g_ascii_isspace
						(new_str->str[new_str->len - 1]))
						g_string_append_c(new_str, ' ');
				}
				g_string_append_len(new_str, p, q - p + 1);
			} else {
				g_string_append(new_str, p);
				g_string_append_c(new_str, '>');
			}
		} else
			g_string_append(new_str, p);

		if (cur->next)
			g_string_append(new_str, ", ");
	}

	slist_free_strings(addr_list);
	ret_str = new_str->str;
	g_string_free(new_str, FALSE);

	return ret_str;
}

gboolean address_equal(const gchar *addr1, const gchar *addr2)
{
	gchar *addr1_, *addr2_;

	if (!addr1 || !addr2)
		return FALSE;

	Xstrdup_a(addr1_, addr1, return FALSE);
	Xstrdup_a(addr2_, addr2, return FALSE);

	extract_address(addr1_);
	extract_address(addr2_);

	return strcmp(addr1_, addr2_) == 0;
}

GSList *address_list_append_orig(GSList *addr_list, const gchar *str)
{
	const gchar *p = str, *q;
	gchar *addr;

	if (!str) return addr_list;

	while (*p) {
		if (*p == ',' || g_ascii_isspace(*p)) {
			++p;
		} else if ((q = strchr_with_skip_quote(p, '"', ','))) {
			addr = g_strndup(p, q - p);
			g_strstrip(addr);
			addr_list = g_slist_append(addr_list, addr);
			p = q + 1;
		} else {
			addr = g_strdup(p);
			g_strstrip(addr);
			addr_list = g_slist_append(addr_list, addr);
			break;
		}
	}

	return addr_list;
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
			list = g_list_remove(list, last->data);
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
		++p;
	}
}

void subst_null(gchar *str, gint len, gchar subst)
{
	register gchar *p = str;

	while (len--) {
		if (*p == '\0')
			*p = subst;
		++p;
	}
}

void subst_control(gchar *str, gchar subst)
{
	register gchar *p = str;

	while (*p) {
		if (g_ascii_iscntrl(*p))
			*p = subst;
		++p;
	}
}

void subst_for_filename(gchar *str)
{
	subst_chars(str, " \t\r\n\"'\\/:;*?<>|", '_');
}

gchar *get_alt_filename(const gchar *filename, gint count)
{
	const gchar *ext;
	gchar *alt_filename;

	ext = strrchr(filename, '.');

	if (ext) {
		gchar *base;

		base = g_strndup(filename, ext - filename);
		alt_filename = g_strdup_printf("%s-%d%s", base, count, ext);
		g_free(base);
	} else
		alt_filename = g_strdup_printf("%s-%d", filename, count);

	return alt_filename;
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

gchar *strcasestr_with_skip_quote(const gchar *haystack, const gchar *needle)
{
	register guint haystack_len, needle_len;
	gboolean in_squote = FALSE, in_dquote = FALSE;

	haystack_len = strlen(haystack);
	needle_len   = strlen(needle);

	if (haystack_len < needle_len || needle_len == 0)
		return NULL;

	while (haystack_len >= needle_len) {
		if (!in_squote && !in_dquote &&
		    !g_ascii_strncasecmp(haystack, needle, needle_len))
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

gchar **strsplit_csv(const gchar *str, gchar delim, gint max_tokens)
{
	GSList *string_list = NULL, *slist;
	gchar **str_array, *s, *new_str;
	gchar *tmp, *tmpp, *p;
	guint i, n = 1, len;

	g_return_val_if_fail(str != NULL, NULL);

	if (max_tokens < 1)
		max_tokens = G_MAXINT;

	s = strchr_with_skip_quote(str, '"', delim);
	if (s) {
		do {
			len = s - str;
			tmpp = tmp = g_strndup(str, len);

			if (tmp[0] == '"' && tmp[len - 1] == tmp[0]) {
				tmp[len - 1] = '\0';
				++tmpp;
				p = new_str = g_malloc(len - 1);
				while (*tmpp) {
					if (*tmpp == '"' && *(tmpp + 1) == '"')
						++tmpp;
					*p++ = *tmpp++;
				}
				*p = '\0';
				g_free(tmp);
			} else
				new_str = tmp;

			string_list = g_slist_prepend(string_list, new_str);
			n++;
			str = s + 1;
			s = strchr_with_skip_quote(str, '"', delim);
		} while (--max_tokens && s);
	}

	if (*str) {
		len = strlen(str);
		tmpp = tmp = g_strdup(str);

		if (tmp[0] == '"' && tmp[len - 1] == tmp[0]) {
			tmp[len - 1] = '\0';
			++tmpp;
			p = new_str = g_malloc(len - 1);
			while (*tmpp) {
				if (*tmpp == '"' && *(tmpp + 1) == '"')
					++tmpp;
				*p++ = *tmpp++;
			}
			*p = '\0';
			g_free(tmp);
		} else
			new_str = tmp;

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

/* Concatenate multiple strings into a CSV (TSV) record.
 * If delimiter characters or double-quotes appear in the string,
   it is enclosed by double quotes.
 * Double quote characters are escaped by another double quote.
 * Strings must not include line breaks in this implementation.
 */
gchar *strconcat_csv(gchar delim, const gchar *field1, ...)
{
	va_list args;
	gchar *s;
	GString *csv;
	gint count = 0;
	const gchar *p;

	g_return_val_if_fail(field1 != NULL, NULL);

	csv = g_string_new("");

	va_start(args, field1);
	s = (gchar *)field1;
	while (s) {
		gboolean add_quote = FALSE;

		if (count > 0)
			g_string_append_c(csv, delim);
		if (strchr(s, delim) != NULL || strchr(s, '"') != NULL)
			add_quote = TRUE;
		if (add_quote)
			g_string_append_c(csv, '"');
		for (p = s; *p != '\0'; p++) {
			if (*p == '"')
				g_string_append_c(csv, '"');
			g_string_append_c(csv, *p);
		}
		if (add_quote)
			g_string_append_c(csv, '"');
		count++;
		s = va_arg(args, gchar*);
	}
	va_end(args);

	return g_string_free(csv, FALSE);
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
	gchar *file;

#if GLIB_CHECK_VERSION(2, 6, 0)
	gchar **uris;
	gint i;

	uris = g_uri_list_extract_uris(uri_list);
	g_return_val_if_fail(uris != NULL, NULL);

	for (i = 0; uris[i] != NULL; i++) {
		file = g_filename_from_uri(uris[i], NULL, NULL);
		if (file)
			result = g_list_append(result, file);
	}

	g_strfreev(uris);

	return result;
#else
	const gchar *p, *q;

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
					result = g_list_append(result, file);
				}
			}
		}
		p = strchr(p, '\n');
		if (p) p++;
	}

	return result;
#endif
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

#define INT_TO_HEX(hex, val)		\
{					\
	if ((val) < 10)			\
		hex = '0' + (val);	\
	else				\
		hex = 'a' + (val) - 10;	\
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

static void get_hex_str(gchar *out, guchar ch)
{
	gchar hex;

	INT_TO_HEX(hex, ch >> 4);
	*out++ = hex;
	INT_TO_HEX(hex, ch & 0x0f);
	*out++ = hex;
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
			if (g_ascii_isxdigit((guchar)enc[0]) &&
			    g_ascii_isxdigit((guchar)enc[1])) {
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

void decode_xdigit_encoded_str(gchar *decoded, const gchar *encoded)
{
	gchar *dec = decoded;
	const gchar *enc = encoded;

	while (*enc) {
		if (*enc == '%') {
			enc++;
			if (g_ascii_isxdigit((guchar)enc[0]) &&
			    g_ascii_isxdigit((guchar)enc[1])) {
				*dec++ = axtoi(enc);
				enc += 2;
			}
		} else
			*dec++ = *enc++;
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

gchar *uriencode_for_filename(const gchar *filename)
{
	const gchar *p = filename;
	gchar *enc, *outp;

	outp = enc = g_malloc(strlen(filename) * 3 + 1);

	for (p = filename; *p != '\0'; p++) {
		if (strchr("\t\r\n\"'\\/:;*?<>|", *p)) {
			*outp++ = '%';
			get_hex_str(outp, *p);
			outp += 2;
		} else
			*outp++ = *p;
	}

	*outp = '\0';
	return enc;
}

gchar *uriencode_for_mailto(const gchar *mailto)
{
	const gchar *p = mailto;
	gchar *enc, *outp;

	outp = enc = g_malloc(strlen(mailto) * 3 + 1);

	for (p = mailto; *p != '\0'; p++) {
		if (*p == '+') {
			*outp++ = '%';
			get_hex_str(outp, *p);
			outp += 2;
		} else
			*outp++ = *p;
	}

	*outp = '\0';
	return enc;
}

gint scan_mailto_url(const gchar *mailto, gchar **to, gchar **cc, gchar **bcc,
		     gchar **subject, gchar **inreplyto, gchar **body)
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

	if (to && !*to) {
		*to = g_malloc(strlen(tmp_mailto) + 1);
		decode_uri(*to, tmp_mailto);
	}

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
			*cc = g_malloc(strlen(value) + 1);
			decode_uri(*cc, value);
		} else if (bcc && !*bcc && !g_ascii_strcasecmp(field, "bcc")) {
			*bcc = g_malloc(strlen(value) + 1);
			decode_uri(*bcc, value);
		} else if (subject && !*subject &&
			   !g_ascii_strcasecmp(field, "subject")) {
			*subject = g_malloc(strlen(value) + 1);
			decode_uri(*subject, value);
		} else if (inreplyto && !*inreplyto &&
			   !g_ascii_strcasecmp(field, "in-reply-to")) {
			*inreplyto = g_malloc(strlen(value) + 1);
			decode_uri(*inreplyto, value);
		} else if (body && !*body &&
			   !g_ascii_strcasecmp(field, "body")) {
			*body = g_malloc(strlen(value) + 1);
			decode_uri(*body, value);
		}
	}

	return 0;
}

static gchar *startup_dir = NULL;
static gchar *rc_dir = NULL;

void set_startup_dir(void)
{
#ifdef G_OS_WIN32
	if (!startup_dir) {
		startup_dir = g_win32_get_package_installation_directory
			(NULL, NULL);
		if (startup_dir) {
			if (g_chdir(startup_dir) < 0) {
				FILE_OP_ERROR(startup_dir, "chdir");
				g_free(startup_dir);
				startup_dir = g_get_current_dir();
			}
		} else
			startup_dir = g_get_current_dir();
	}
#elif defined(__APPLE__)
	if (!startup_dir) {
		const gchar *path;
		NSAutoreleasePool *pool;

		pool = [[NSAutoreleasePool alloc] init];

		path = [[[NSBundle mainBundle] bundlePath] UTF8String];
		startup_dir = g_strdup(path);

		[pool release];

		if (!startup_dir)
			startup_dir = g_get_current_dir();
	}
#else
	if (!startup_dir)
		startup_dir = g_get_current_dir();
#endif
}

void set_rc_dir(const gchar *dir)
{
	if (rc_dir)
		g_free(rc_dir);

	if (dir) {
		if (g_path_is_absolute(dir))
			rc_dir = g_strdup(dir);
		else
			rc_dir = g_strconcat(get_startup_dir(),
					     G_DIR_SEPARATOR_S, dir, NULL);
	} else
		rc_dir = NULL;
}

const gchar *get_startup_dir(void)
{
	if (!startup_dir)
		set_startup_dir();

	return startup_dir;
}

#ifdef G_OS_WIN32
static gchar *get_win32_special_folder_path(gint nfolder)
{
	gchar *folder = NULL;
	HRESULT hr;

	if (G_WIN32_HAVE_WIDECHAR_API()) {
		wchar_t path[MAX_PATH + 1];
		hr = SHGetFolderPathW(NULL, nfolder, NULL, 0, path);
		if (hr == S_OK)
			folder = g_utf16_to_utf8(path, -1, NULL, NULL, NULL);
	} else {
		gchar path[MAX_PATH + 1];
		hr = SHGetFolderPathA(NULL, nfolder, NULL, 0, path);
		if (hr == S_OK)
			folder = g_locale_to_utf8(path, -1, NULL, NULL, NULL);
	}

	return folder;
}
#endif

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

const gchar *get_document_dir(void)
{
#ifdef G_OS_WIN32
	static const gchar *document_dir = NULL;
	HRESULT hr;

	if (!document_dir) {
		document_dir = get_win32_special_folder_path(CSIDL_PERSONAL);
		if (!document_dir)
			document_dir = get_home_dir();
	}

	return document_dir;
#elif defined(__APPLE__)
	static const gchar *document_dir = NULL;

	if (!document_dir) {
		document_dir = g_strconcat(get_home_dir(), G_DIR_SEPARATOR_S,
					   "Documents", NULL);
	}

	return document_dir;
#else
	return get_home_dir();
#endif
}

const gchar *get_rc_dir(void)
{
	if (!rc_dir) {
#ifdef G_OS_WIN32
		gchar *appdata;

		appdata = get_win32_special_folder_path(CSIDL_APPDATA);
		if (appdata)
			rc_dir = g_strconcat(appdata, G_DIR_SEPARATOR_S,
					     RC_DIR, NULL);
		else
			rc_dir = g_strconcat(get_home_dir(), G_DIR_SEPARATOR_S,
					     RC_DIR, NULL);
		g_free(appdata);
#elif defined(__APPLE__)
		rc_dir = g_strconcat(get_home_dir(), G_DIR_SEPARATOR_S,
				     "Library", G_DIR_SEPARATOR_S,
				     "Application Support", G_DIR_SEPARATOR_S,
				     RC_DIR, NULL);
#else
		rc_dir = g_strconcat(get_home_dir(), G_DIR_SEPARATOR_S,
				     RC_DIR, NULL);
#endif
	}

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
#if defined(G_OS_WIN32) || defined(__APPLE__)
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
		if (is_next_nonascii(domain_name)) {
			g_warning("invalid domain name: %s\n", domain_name);
			g_free(domain_name);
			domain_name = "unknown";
		}
	}

	return domain_name;
}

off_t get_file_size(const gchar *file)
{
	GStatBuf s;

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

gint get_last_empty_line_size(FILE *fp, off_t size)
{
	glong pos;
	gint lsize = 0;
	gchar buf[4];
	size_t nread;

	if (size < 4)
		return -1;

	if ((pos = ftell(fp)) < 0) {
		perror("ftell");
		return -1;
	}
	if (fseek(fp, size - 4, SEEK_CUR) < 0) {
		perror("fseek");
		return -1;
	}

	/* read last 4 bytes */
	nread = fread(buf, sizeof(buf), 1, fp);
	if (nread != 1) {
		perror("fread");
		return -1;
	}
	/* g_print("last 4 bytes: %02x %02x %02x %02x\n", buf[0], buf[1], buf[2], buf[3]); */
	if (buf[3] == '\n') {
		if (buf[2] == '\n')
			lsize = 1;
		else if (buf[2] == '\r') {
			if (buf[1] == '\n')
				lsize = 2;
		}
	}

	if (fseek(fp, pos, SEEK_SET) < 0) {
		perror("fseek");
		return -1;
	}

	return lsize;
}

gboolean file_exist(const gchar *file, gboolean allow_fifo)
{
	if (file == NULL)
		return FALSE;

	if (allow_fifo) {
		GStatBuf s;

		if (g_stat(file, &s) < 0) {
			if (ENOENT != errno) FILE_OP_ERROR(file, "stat");
			return FALSE;
		}
		if (S_ISREG(s.st_mode) || S_ISFIFO(s.st_mode))
			return TRUE;
	} else {
		return g_file_test(file, G_FILE_TEST_IS_REGULAR);
	}

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
	guint file_no;

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
		file_no = to_unumber(dir_name);
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
	GStatBuf s;
	gchar *prev_dir;
	guint file_no;
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
		file_no = to_unumber(dir_name);
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
	GStatBuf s;
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
		if (ENOTDIR == errno) {
			if (g_unlink(dir) < 0) {
				FILE_OP_ERROR(dir, "unlink");
				return -1;
			}
		} else {
			FILE_OP_ERROR(dir, "rmdir");
			return -1;
		}
	}

	return 0;
}

gint remove_dir_recursive(const gchar *dir)
{
	gchar *cur_dir;
	gint ret;

	debug_print("remove_dir_recursive: %s\n", dir);

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
#if !defined(G_OS_UNIX) && !GLIB_CHECK_VERSION(2, 9, 1)
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
#ifdef G_OS_WIN32
	wchar_t *wsrc;
	wchar_t *wdest;
	gchar *dest_bak = NULL;
	gboolean err = FALSE;

	wsrc = g_utf8_to_utf16(src, -1, NULL, NULL, NULL);
	if (wsrc == NULL) {
		return -1;
	}
	wdest = g_utf8_to_utf16(dest, -1, NULL, NULL, NULL);
	if (wdest == NULL) {
		g_free(wsrc);
		return -1;
	}

	if (keep_backup == FALSE) {
		if (CopyFileW(wsrc, wdest, FALSE) == 0)
			err = TRUE;
		g_free(wdest);
		g_free(wsrc);
		return err ? -1 : 0;
	}

	if (is_file_exist(dest)) {
		dest_bak = g_strconcat(dest, ".bak", NULL);
		if (rename_force(dest, dest_bak) < 0) {
			FILE_OP_ERROR(dest, "rename");
			g_free(dest_bak);
			g_free(wdest);
			g_free(wsrc);
			return -1;
		}
	}

	if (CopyFileW(wsrc, wdest, FALSE) == 0)
		err = TRUE;

	g_free(wdest);
	g_free(wsrc);
#else
	gint srcfd, destfd;
	gint n_read;
	gchar buf[BUFFSIZE];
	gchar *dest_bak = NULL;
	gboolean err = FALSE;

	if ((srcfd = g_open(src, O_RDONLY, 0600)) < 0) {
		FILE_OP_ERROR(src, "open");
		return -1;
	}
	if (is_file_exist(dest)) {
		dest_bak = g_strconcat(dest, ".bak", NULL);
		if (rename_force(dest, dest_bak) < 0) {
			FILE_OP_ERROR(dest, "rename");
			close(srcfd);
			g_free(dest_bak);
			return -1;
		}
	}

	if ((destfd = g_open(dest, O_WRONLY | O_CREAT, 0600)) < 0) {
		FILE_OP_ERROR(dest, "open");
		close(srcfd);
		if (dest_bak) {
			if (rename_force(dest_bak, dest) < 0)
				FILE_OP_ERROR(dest_bak, "rename");
			g_free(dest_bak);
		}
		return -1;
	}

	while ((n_read = read(srcfd, buf, sizeof(buf))) > 0) {
		gchar *p = buf;
		const gchar *endp = buf + n_read;
		gint n_write;

		while (p < endp) {
			if ((n_write = write(destfd, p, endp - p)) < 0) {
				g_warning(_("writing to %s failed.\n"), dest);
				close(destfd);
				close(srcfd);
				g_unlink(dest);
				if (dest_bak) {
					if (rename_force(dest_bak, dest) < 0)
						FILE_OP_ERROR(dest_bak, "rename");
					g_free(dest_bak);
				}
				return -1;
			}
			p += n_write;
		}
	}

	if (close(destfd) < 0) {
		FILE_OP_ERROR(dest, "close");
		err = TRUE;
	}
	close(srcfd);
#endif

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
	if (overwrite == FALSE && is_file_entry_exist(dest)) {
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

gint append_file_part(FILE *fp, off_t offset, size_t length, FILE *dest_fp)
{
	gint n_read;
	gint bytes_left, to_read;
	gchar buf[BUFSIZ];

	g_return_val_if_fail(fp != NULL, -1);
	g_return_val_if_fail(dest_fp != NULL, -1);

	if (fseek(fp, offset, SEEK_SET) < 0) {
		perror("fseek");
		return -1;
	}

	bytes_left = length;
	to_read = MIN(bytes_left, sizeof(buf));

	while ((n_read = fread(buf, sizeof(gchar), to_read, fp)) > 0) {
		if (n_read < to_read && ferror(fp))
			break;
		if (fwrite(buf, n_read, 1, dest_fp) < 1) {
			g_warning("append_file_part: writing to file failed.\n");
			return -1;
		}
		bytes_left -= n_read;
		if (bytes_left == 0)
			break;
		to_read = MIN(bytes_left, sizeof(buf));
	}

	if (ferror(fp)) {
		perror("fread");
		return -1;
	}
	if (fflush(dest_fp) == EOF) {
		FILE_OP_ERROR("append_file_part", "fflush");
		return -1;
	}

	return 0;
}

gint copy_file_part(FILE *fp, off_t offset, size_t length, const gchar *dest)
{
	FILE *dest_fp;

	if ((dest_fp = g_fopen(dest, "wb")) == NULL) {
		FILE_OP_ERROR(dest, "fopen");
		return -1;
	}

	if (change_file_mode_rw(dest_fp, dest) < 0) {
		FILE_OP_ERROR(dest, "chmod");
		g_warning("can't change file mode\n");
	}

	if (append_file_part(fp, offset, length, dest_fp) < 0) {
		g_warning("writing to %s failed.\n", dest);
		fclose(dest_fp);
		g_unlink(dest);
		return -1;
	}

	if (fclose(dest_fp) == EOF) {
		FILE_OP_ERROR(dest, "fclose");
		g_unlink(dest);
		return -1;
	}

	return 0;
}

gint copy_file_stream(FILE *fp, FILE *dest_fp)
{
	gint n_read;
	gchar buf[BUFFSIZE];

	g_return_val_if_fail(fp != NULL, -1);
	g_return_val_if_fail(dest_fp != NULL, -1);

	while ((n_read = fread(buf, sizeof(gchar), sizeof(buf), fp)) > 0) {
		if (n_read < sizeof(buf) && ferror(fp))
			break;
		if (fwrite(buf, n_read, 1, dest_fp) < 1) {
			g_warning("copy_file_stream: writing to file failed.\n");
			return -1;
		}
	}

	if (ferror(fp)) {
		perror("fread");
		return -1;
	}
	if (fflush(dest_fp) == EOF) {
		FILE_OP_ERROR("copy_file_stream", "fflush");
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

FILE *canonicalize_file_stream(FILE *src_fp, gint *length)
{
	FILE *dest_fp;
	gchar buf[BUFFSIZE];
	gint len;
	gint length_ = 0;
	gboolean err = FALSE;
	gboolean last_linebreak = FALSE;

	if ((dest_fp = my_tmpfile()) == NULL)
		return NULL;

	while (fgets(buf, sizeof(buf), src_fp) != NULL) {
		gint r = 0;

		len = strlen(buf);
		if (len == 0) break;
		last_linebreak = FALSE;

		if (buf[len - 1] != '\n') {
			last_linebreak = TRUE;
			r = fputs(buf, dest_fp);
			length_ += len;
		} else if (len > 1 && buf[len - 1] == '\n' && buf[len - 2] == '\r') {
			r = fputs(buf, dest_fp);
			length_ += len;
		} else {
			if (len > 1) {
				r = fwrite(buf, len - 1, 1, dest_fp);
				if (r != 1)
					r = EOF;
				else
					length_ += len - 1;
			}
			if (r != EOF) {
				r = fputs("\r\n", dest_fp);
				length_ += 2;
			}
		}

		if (r == EOF) {
			g_warning("writing to temporary file failed.\n");
			fclose(dest_fp);
			return NULL;
		}
	}

	if (last_linebreak == TRUE) {
		if (fputs("\r\n", dest_fp) == EOF)
			err = TRUE;
		else
			length_ += 2;
	}

	if (ferror(src_fp)) {
		FILE_OP_ERROR("canonicalize_file_stream", "fgets");
		err = TRUE;
	}
	if (fflush(dest_fp) == EOF) {
		FILE_OP_ERROR("canonicalize_file_stream", "fflush");
		err = TRUE;
	}

	if (err) {
		fclose(dest_fp);
		return NULL;
	}

	if (length)
		*length = length_;

	rewind(dest_fp);
	return dest_fp;
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

gchar *strchomp_all(const gchar *str)
{
	const gchar *p = str;
	const gchar *newline, *last;
	gchar *out, *outp;

	out = outp = g_malloc(strlen(str) + 1);
	while (*p != '\0') {
		newline = strchr(p, '\n');
		if (newline) {
			for (last = newline;
			     p < last && g_ascii_isspace(*(last - 1)); --last)
				;
			strncpy(outp, p, last - p);
			outp += last - p;

			if (p < newline && *(newline - 1) == '\r') {
				strncpy(outp, newline - 1, 2);
				outp += 2;
			} else {
				*outp++ = *newline;
			}

			p = newline + 1;
		} else {
			for (last = p + strlen(p);
			     p < last && g_ascii_isspace(*(last - 1)); --last)
				;
			strncpy(outp, p, last - p);
			outp += last - p;
			break;
		}
	}

	*outp = '\0';

	return out;
}

FILE *get_outgoing_rfc2822_file(FILE *fp)
{
	gchar buf[BUFFSIZE];
	FILE *outfp;

	outfp = my_tmpfile();
	if (!outfp) {
		FILE_OP_ERROR("get_outgoing_rfc2822_file", "my_tmpfile");
		return NULL;
	}

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
		} else {
			if (fputs(buf, outfp) == EOF)
				goto file_error;
			if (fputs("\r\n", outfp) == EOF)
				goto file_error;
			if (buf[0] == '\0')
				break;
		}
	}

	/* output body part */
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		strretchomp(buf);
		if (buf[0] == '.') {
			if (fputc('.', outfp) == EOF)
				goto file_error;
		}
		if (fputs(buf, outfp) == EOF)
			goto file_error;
		if (fputs("\r\n", outfp) == EOF)
			goto file_error;
	}

	if (fflush(outfp) == EOF) {
		FILE_OP_ERROR("get_outgoing_rfc2822_file", "fflush");
		goto file_error;
	}

	rewind(outfp);
	return outfp;

file_error:
	g_warning("get_outgoing_rfc2822_file(): writing to temporary file failed.\n");
	fclose(outfp);
	return NULL;
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
	subst_chars(buf_date, " ,:", '_');

	return g_strdup_printf("%s=_%s_%s", prefix ? prefix : "Multipart",
			       buf_date, buf_uniq);
}

gint change_file_mode_rw(FILE *fp, const gchar *file)
{
#ifdef G_OS_WIN32
	DWORD attr;
	BOOL retval;

	if (G_WIN32_HAVE_WIDECHAR_API()) {
		wchar_t *wpath;

		wpath = g_utf8_to_utf16(file, -1, NULL, NULL, NULL);
		if (wpath == NULL)
			return -1;

		attr = GetFileAttributesW(wpath);
		retval = SetFileAttributesW
			(wpath, attr & ~(FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_HIDDEN));

		g_free(wpath);
	} else {
		gchar *cp_path;

		cp_path = g_locale_from_utf8(file, -1, NULL, NULL, NULL);
		if (cp_path == NULL)
			return -1;

		attr = GetFileAttributesA(cp_path);
		retval = SetFileAttributesA
			(cp_path, attr & ~(FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_HIDDEN));

		g_free(cp_path);
	}

	if (retval)
		return 0;
	else
		return -1;
#else
#if HAVE_FCHMOD
	if (fp)
		return fchmod(fileno(fp), S_IRUSR|S_IWUSR);
	else
#endif
		return g_chmod(file, S_IRUSR|S_IWUSR);
#endif
}

#ifdef G_OS_WIN32
gchar *_s_tempnam(const gchar *dir, const gchar *prefix)
{
	if (G_WIN32_HAVE_WIDECHAR_API()) {
		wchar_t *wpath;
		wchar_t *wprefix;
		wchar_t *wname;
		gint save_errno;
		gchar *name;

		wpath = g_utf8_to_utf16(dir, -1, NULL, NULL, NULL);
		if (wpath == NULL) {
			errno = EINVAL;
			return NULL;
		}
		wprefix = g_utf8_to_utf16(prefix, -1, NULL, NULL, NULL);
		if (wprefix == NULL) {
			errno = EINVAL;
			g_free(wpath);
			return NULL;
		}

		wname = _wtempnam(wpath, wprefix);
		save_errno = errno;

		name = g_utf16_to_utf8(wname, -1, NULL, NULL, NULL);
		if (name == NULL) {
			save_errno = EINVAL;
		}

		free(wname);     /* must be freed by same MSVCRT used for
				    _wtempnam() */
		g_free(wprefix);
		g_free(wpath);

		errno = save_errno;
		return name;
	} else {
		gchar *cp_path;
		gchar *cp_prefix;
		gchar *cp_name;
		gint save_errno;
		gchar *name;

		cp_path = g_locale_from_utf8(dir, -1, NULL, NULL, NULL);
		if (cp_path == NULL) {
			errno = EINVAL;
			return NULL;
		}

		cp_prefix = g_locale_from_utf8(prefix, -1, NULL, NULL, NULL);
		if (cp_prefix == NULL) {
			errno = EINVAL;
			g_free(cp_path);
			return NULL;
		}

		cp_name = _tempnam(cp_path, cp_prefix);
		save_errno = errno;

		name = g_locale_to_utf8(cp_name, -1, NULL, NULL, NULL);
		if (name == NULL) {
			save_errno = EINVAL;
		}

		free(cp_name);
		g_free(cp_prefix);
		g_free(cp_path);

		errno = save_errno;
		return name;
	}
}
#endif

FILE *my_tmpfile(void)
{
#ifdef G_OS_WIN32
	const gchar *tmpdir;
	gchar *fname;
	gint fd;
	FILE *fp;

	tmpdir = get_tmp_dir();
	fname = _s_tempnam(tmpdir, "sylph");
	if (!fname)
		return NULL;

	fd = g_open(fname, O_RDWR | O_CREAT | O_EXCL |
		    _O_TEMPORARY | _O_SHORT_LIVED | _O_BINARY, 0600);
	if (fd < 0) {
		g_free(fname);
		return NULL;
	}

	fp = fdopen(fd, "w+b");
	if (!fp) {
		perror("fdopen");
		close(fd);
	}

	g_free(fname);

	return fp;
#else
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
	if (!progname)
		progname = "sylph";
	proglen = strlen(progname);
	fname = g_malloc(tmplen + 1 + proglen + sizeof(suffix));

	memcpy(fname, tmpdir, tmplen);
	fname[tmplen] = G_DIR_SEPARATOR;
	memcpy(fname + tmplen + 1, progname, proglen);
	memcpy(fname + tmplen + 1 + proglen, suffix, sizeof(suffix));

	fd = g_mkstemp(fname);
	if (fd < 0) {
		g_free(fname);
		return tmpfile();
	}

	g_unlink(fname);

	fp = fdopen(fd, "w+b");
	if (!fp) {
		perror("fdopen");
		close(fd);
	}

	g_free(fname);

	return fp;
#endif
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
	if (fflush(fp) == EOF) {
		FILE_OP_ERROR("str_open_as_stream", "fflush");
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

#if defined(G_OS_WIN32) && !GLIB_CHECK_VERSION(2, 8, 2)
static gchar **argv_utf8_to_locale(gchar **argv)
{
	gint argc = 0, i;
	gchar **cp_argv;

	while (argv[argc] != NULL)
		argc++;

	cp_argv = g_new(gchar *, argc + 1);

	for (i = 0; i < argc; i++) {
		cp_argv[i] = g_locale_from_utf8(argv[i], -1, NULL, NULL, NULL);
		if (cp_argv[i] == NULL) {
			g_warning("Failed to convert from UTF-8 to locale encoding: %s\n", argv[i]);
			g_strfreev(cp_argv);
			return NULL;
		}
	}
	cp_argv[i] = NULL;

	return cp_argv;
}
#endif

gint execute_async(gchar *const argv[])
{
#if defined(G_OS_WIN32) && !GLIB_CHECK_VERSION(2, 8, 2)
	gchar **cp_argv;

	g_return_val_if_fail(argv != NULL && argv[0] != NULL, -1);

	cp_argv = argv_utf8_to_locale((gchar **)argv);
	if (!cp_argv)
		return -1;
	if (g_spawn_async(NULL, cp_argv, NULL, G_SPAWN_SEARCH_PATH,
			  NULL, NULL, NULL, NULL) == FALSE) {
		g_warning("Can't execute command: %s\n", argv[0]);
		g_strfreev(cp_argv);
		return -1;
	}
	g_strfreev(cp_argv);
#else
	g_return_val_if_fail(argv != NULL && argv[0] != NULL, -1);

	if (g_spawn_async(NULL, (gchar **)argv, NULL, G_SPAWN_SEARCH_PATH,
			  NULL, NULL, NULL, NULL) == FALSE) {
		g_warning("Can't execute command: %s\n", argv[0]);
		return -1;
	}
#endif

	return 0;
}

gint execute_sync(gchar *const argv[])
{
	gint status;
#if defined(G_OS_WIN32) && !GLIB_CHECK_VERSION(2, 8, 2)
	gchar **cp_argv;
#endif

	g_return_val_if_fail(argv != NULL && argv[0] != NULL, -1);

#ifdef G_OS_WIN32
#if !GLIB_CHECK_VERSION(2, 8, 2)
	cp_argv = argv_utf8_to_locale((gchar **)argv);
	if (!cp_argv)
		return -1;
	if (g_spawn_sync(NULL, cp_argv, NULL,
			 G_SPAWN_SEARCH_PATH | G_SPAWN_CHILD_INHERITS_STDIN |
			 G_SPAWN_LEAVE_DESCRIPTORS_OPEN,
			 NULL, NULL, NULL, NULL, &status, NULL) == FALSE) {
		g_warning("Can't execute command: %s\n", argv[0]);
		g_strfreev(cp_argv);
		return -1;
	}
	g_strfreev(cp_argv);
#else /* !GLIB_CHECK_VERSION */
	if (g_spawn_sync(NULL, (gchar **)argv, NULL,
			 G_SPAWN_SEARCH_PATH | G_SPAWN_CHILD_INHERITS_STDIN |
			 G_SPAWN_LEAVE_DESCRIPTORS_OPEN,
			 NULL, NULL, NULL, NULL, &status, NULL) == FALSE) {
		g_warning("Can't execute command: %s\n", argv[0]);
		return -1;
	}
#endif /* !GLIB_CHECK_VERSION */

	return status;
#else /* G_OS_WIN32 */
	if (g_spawn_sync(NULL, (gchar **)argv, NULL, G_SPAWN_SEARCH_PATH,
			 NULL, NULL, NULL, NULL, &status, NULL) == FALSE) {
		g_warning("Can't execute command: %s\n", argv[0]);
		return -1;
	}

	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	else
		return -1;
#endif /* G_OS_WIN32 */
}

gint execute_command_line(const gchar *cmdline, gboolean async)
{
	gchar **argv;
	gint ret;

	if (debug_mode) {
		gchar *utf8_cmdline;

		utf8_cmdline = g_filename_to_utf8
			(cmdline, -1, NULL, NULL, NULL);
		debug_print("execute_command_line(): executing: %s\n",
			    utf8_cmdline ? utf8_cmdline : cmdline);
		g_free(utf8_cmdline);
	}

	argv = strsplit_with_quote(cmdline, " ", 0);

	if (async)
		ret = execute_async(argv);
	else
		ret = execute_sync(argv);

	g_strfreev(argv);

	return ret;
}

#if USE_THREADS
typedef struct _CmdData
{
	const gchar *cmdline;
	volatile gint flag;
	gint status;
} CmdData;

static gpointer execute_command_line_async_func(gpointer data)
{
	CmdData *cmd_data = (CmdData *)data;
	gchar **argv;

	argv = strsplit_with_quote(cmd_data->cmdline, " ", 0);
	cmd_data->status = execute_sync(argv);
	g_strfreev(argv);

	debug_print("execute_command_line_async_func: exec done: %s\n",
		    cmd_data->cmdline);
	g_atomic_int_set(&cmd_data->flag, 1);
	g_main_context_wakeup(NULL);

	return GINT_TO_POINTER(0);
}

gint execute_command_line_async_wait(const gchar *cmdline)
{
	CmdData data = {NULL, 0, 0};
	GThread *thread;

	if (debug_mode) {
		gchar *utf8_cmdline;

		utf8_cmdline = g_filename_to_utf8
			(cmdline, -1, NULL, NULL, NULL);
		debug_print("execute_command_line(): executing: %s\n",
			    utf8_cmdline ? utf8_cmdline : cmdline);
		g_free(utf8_cmdline);
	}

	data.cmdline = cmdline;
	thread = g_thread_create(execute_command_line_async_func, &data, TRUE,
				 NULL);
	if (!thread)
		return -1;

	debug_print("execute_command_line_async_wait: waiting thread\n");
	while (g_atomic_int_get(&data.flag) == 0)
		event_loop_iterate();

	g_thread_join(thread);
	debug_print("execute_command_line_async_wait: thread exited\n");

	return data.status;
}
#else /* USE_THREADS */
gint execute_command_line_async_wait(const gchar *cmdline)
{
	return execute_command_line(cmdline, FALSE);
}
#endif /* USE_THREADS */

gint execute_open_file(const gchar *file, const gchar *content_type)
{
#ifdef G_OS_WIN32
	g_return_val_if_fail(file != NULL, -1);

	log_print("opening %s - %s\n", file, content_type ? content_type : "");

	if (G_WIN32_HAVE_WIDECHAR_API()) {
		wchar_t *wpath;

		wpath = g_utf8_to_utf16(file, -1, NULL, NULL, NULL);
		if (wpath == NULL)
			return -1;

		ShellExecuteW(NULL, L"open", wpath, NULL, NULL, SW_SHOWNORMAL);

		g_free(wpath);

		return 0;
	} else {
		gchar *cp_path;

		cp_path = g_locale_from_utf8(file, -1, NULL, NULL, NULL);
		if (cp_path == NULL)
			return -1;

		ShellExecuteA(NULL, "open", cp_path, NULL, NULL, SW_SHOWNORMAL);

		g_free(cp_path);

		return 0;
	}
#elif defined(__APPLE__)
	const gchar *argv[3] = {"open", NULL, NULL};

	g_return_val_if_fail(file != NULL, -1);

	log_print("opening %s - %s\n", file, content_type ? content_type : "");

	argv[1] = file;
	execute_async(argv);
#else
	const gchar *argv[3] = {"xdg-open", NULL, NULL};

	g_return_val_if_fail(file != NULL, -1);

	log_print("opening %s - %s\n", file, content_type ? content_type : "");

	argv[1] = file;
	execute_async(argv);
#endif

	return 0;
}

gint execute_print_file(const gchar *file)
{
	g_return_val_if_fail(file != NULL, -1);

#ifdef G_OS_WIN32
	log_print("printing %s\n", file);

	if (G_WIN32_HAVE_WIDECHAR_API()) {
		wchar_t *wpath;

		wpath = g_utf8_to_utf16(file, -1, NULL, NULL, NULL);
		if (wpath == NULL)
			return -1;

		ShellExecuteW(NULL, L"print", wpath, NULL, NULL, SW_SHOWNORMAL);

		g_free(wpath);

		return 0;
	} else {
		gchar *cp_path;

		cp_path = g_locale_from_utf8(file, -1, NULL, NULL, NULL);
		if (cp_path == NULL)
			return -1;

		ShellExecuteA(NULL, "print", cp_path, NULL, NULL,
			      SW_SHOWNORMAL);

		g_free(cp_path);

		return 0;
	}
#endif
	return 0;
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

	g_return_val_if_fail(uri != NULL, -1);

#if defined(G_OS_WIN32) || defined(__APPLE__)
	if (!cmdline || cmdline[0] == '\0')
		return execute_open_file(uri, NULL);
#endif

	if (cmdline && str_find_format_times(cmdline, 's') == 1)
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

gint play_sound(const gchar *file, gboolean async)
{
#ifdef G_OS_WIN32
	wchar_t *wfile;
	DWORD flag = SND_FILENAME;

	wfile = g_utf8_to_utf16(file, -1, NULL, NULL, NULL);
	if (wfile == NULL)
		return -1;
	if (async)
		flag |= SND_ASYNC;
	else
		flag |= SND_SYNC;
	PlaySoundW(wfile, NULL, flag);
	g_free(wfile);
#endif
	return 0;
}

stime_t remote_tzoffset_sec(const gchar *zone)
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

stime_t tzoffset_sec(stime_t *now)
{
	time_t now_ = *now;
	struct tm gmt, *tmp, *lt;
	gint off;

	tmp = gmtime(&now_);
	g_return_val_if_fail(tmp != NULL, -1);
	gmt = *tmp;
	lt = localtime(&now_);
	g_return_val_if_fail(lt != NULL, -1);

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

/* calculate timezone offset (buf must not be less than 6 bytes) */
gchar *tzoffset_buf(gchar *buf, stime_t *now)
{
	time_t now_ = *now;
	struct tm gmt, *tmp, *lt;
	gint off;
	gchar sign = '+';

	tmp = gmtime(&now_);
	g_return_val_if_fail(tmp != NULL, NULL);
	gmt = *tmp;
	lt = localtime(&now_);
	g_return_val_if_fail(lt != NULL, NULL);

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

	g_snprintf(buf, 6, "%c%02d%02d", sign, off / 60, off % 60);

	return buf;
}

gchar *tzoffset(stime_t *now)
{
	static gchar offset_string[6];

	return tzoffset_buf(offset_string, now);
}

void get_rfc822_date(gchar *buf, gint len)
{
	struct tm *lt;
	time_t t;
	stime_t t_;
	gchar day[4], mon[4];
	gint dd, hh, mm, ss, yyyy;
	gchar off[6];

	t_ = t = time(NULL);
	lt = localtime(&t);

	sscanf(asctime(lt), "%3s %3s %d %d:%d:%d %d\n",
	       day, mon, &dd, &hh, &mm, &ss, &yyyy);
	g_snprintf(buf, len, "%s, %d %s %d %02d:%02d:%02d %s",
		   day, dd, mon, yyyy, hh, mm, ss, tzoffset_buf(off, &t_));
}

/* just a wrapper to suppress the warning of gcc about %c */
size_t my_strftime(gchar *s, size_t max, const gchar *format,
		   const struct tm *tm)
{
	return strftime(s, max, format, tm);
}

/* UI hints */

static UIUpdateFunc ui_update_func = NULL;

void set_ui_update_func(UIUpdateFunc func)
{
	ui_update_func = func;
}

void ui_update(void)
{
	if (ui_update_func)
		ui_update_func();
}

static EventLoopFunc event_loop_func = NULL;

void set_event_loop_func(EventLoopFunc func)
{
	event_loop_func = func;
}

void event_loop_iterate(void)
{
	if (event_loop_func)
		event_loop_func();
	else
		g_main_context_iteration(NULL, TRUE);
}

static ProgressFunc progress_func = NULL;

void set_progress_func(ProgressFunc func)
{
	progress_func = func;
}

void progress_show(gint cur, gint total)
{
	if (progress_func)
		progress_func(cur, total);
}

/* user input */

static QueryPasswordFunc query_password_func = NULL;

void set_input_query_password_func(QueryPasswordFunc func)
{
	query_password_func = func;
}

gchar *input_query_password(const gchar *server, const gchar *user)
{
	if (query_password_func)
		return query_password_func(server, user);
	else
		return NULL;
}

/* logging */

static FILE *log_fp = NULL;
#if USE_THREADS
G_LOCK_DEFINE_STATIC(log_fp);
#define S_LOCK(name)	G_LOCK(name)
#define S_UNLOCK(name)	G_UNLOCK(name)
#else
#define S_LOCK(name)
#define S_UNLOCK(name)
#endif

void set_log_file(const gchar *filename)
{
	S_LOCK(log_fp);
	if (!log_fp) {
		log_fp = g_fopen(filename, "w");
		if (!log_fp)
			FILE_OP_ERROR(filename, "fopen");
	}
	S_UNLOCK(log_fp);
}

void close_log_file(void)
{
	S_LOCK(log_fp);
	if (log_fp) {
		fclose(log_fp);
		log_fp = NULL;
	}
	S_UNLOCK(log_fp);
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

static void log_dummy_func(const gchar *str)
{
}

static void log_dummy_flush_func(void)
{
}

static LogFunc log_print_ui_func = log_dummy_func;
static LogFunc log_message_ui_func = log_dummy_func;
static LogFunc log_warning_ui_func = log_dummy_func;
static LogFunc log_error_ui_func = log_dummy_func;
static LogFlushFunc log_flush_ui_func = log_dummy_flush_func;

static LogFunc log_show_status_func = log_dummy_func;

void set_log_ui_func(LogFunc print_func, LogFunc message_func,
		     LogFunc warning_func, LogFunc error_func)
{
	log_print_ui_func = print_func;
	log_message_ui_func = message_func;
	log_warning_ui_func = warning_func;
	log_error_ui_func = error_func;
}

void set_log_ui_func_full(LogFunc print_func, LogFunc message_func,
			  LogFunc warning_func, LogFunc error_func,
			  LogFlushFunc flush_func)
{
	set_log_ui_func(print_func, message_func, warning_func, error_func);
	log_flush_ui_func = flush_func;
}

void set_log_show_status_func(LogFunc status_func)
{
	log_show_status_func = status_func;
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

void status_print(const gchar *format, ...)
{
	va_list args;
	gchar buf[BUFFSIZE];

	va_start(args, format);
	g_vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	log_show_status_func(buf);
}

#define TIME_LEN	11

void log_write(const gchar *str, const gchar *prefix)
{
	S_LOCK(log_fp);

	if (log_fp) {
		gchar buf[TIME_LEN + 1];
		time_t t;

		time(&t);
		strftime(buf, TIME_LEN + 1, "[%H:%M:%S] ", localtime(&t));

		fputs(buf, log_fp);
		if (prefix)
			fputs(prefix, log_fp);
		fputs(str, log_fp);
		fflush(log_fp);
	}

	S_UNLOCK(log_fp);
}

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

	if (debug_mode) g_print("%s", buf);
	log_print_ui_func(buf);
	S_LOCK(log_fp);
	if (log_fp) {
		fputs(buf, log_fp);
		fflush(log_fp);
	}
	S_UNLOCK(log_fp);
	if (log_verbosity_count)
		log_show_status_func(buf + TIME_LEN);
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
	log_message_ui_func(buf + TIME_LEN);
	S_LOCK(log_fp);
	if (log_fp) {
		fwrite(buf, TIME_LEN, 1, log_fp);
		fputs("* message: ", log_fp);
		fputs(buf + TIME_LEN, log_fp);
		fflush(log_fp);
	}
	S_UNLOCK(log_fp);
	log_show_status_func(buf + TIME_LEN);
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
	log_warning_ui_func(buf + TIME_LEN);
	S_LOCK(log_fp);
	if (log_fp) {
		fwrite(buf, TIME_LEN, 1, log_fp);
		fputs("** warning: ", log_fp);
		fputs(buf + TIME_LEN, log_fp);
		fflush(log_fp);
	}
	S_UNLOCK(log_fp);
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
	log_error_ui_func(buf + TIME_LEN);
	S_LOCK(log_fp);
	if (log_fp) {
		fwrite(buf, TIME_LEN, 1, log_fp);
		fputs("*** error: ", log_fp);
		fputs(buf + TIME_LEN, log_fp);
		fflush(log_fp);
	}
	S_UNLOCK(log_fp);
}

void log_flush(void)
{
	S_LOCK(log_fp);
	if (log_fp)
		fflush(log_fp);
	S_UNLOCK(log_fp);
	log_flush_ui_func();
}
