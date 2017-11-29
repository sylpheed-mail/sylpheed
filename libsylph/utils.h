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

#ifndef __UTILS_H__
#define __UTILS_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#if HAVE_ALLOCA_H
#  include <alloca.h>
#endif

/* Wrappers for C library function that take pathname arguments. */
#if GLIB_CHECK_VERSION(2, 6, 0)
#  include <glib/gstdio.h>
#else

#define g_open		open
#define g_rename	rename
#define g_mkdir		mkdir
#define g_stat		stat
#define g_lstat		lstat
#define g_unlink	unlink
#define g_remove	remove
#define g_rmdir		rmdir
#define g_fopen		fopen
#define g_freopen	freopen

#endif /* GLIB_CHECK_VERSION */

#if !GLIB_CHECK_VERSION(2, 7, 0)

#ifdef G_OS_UNIX
#define g_chdir		chdir
#define g_chmod		chmod
#else
gint g_chdir	(const gchar	*path);
gint g_chmod	(const gchar	*path,
		 gint		 mode);
#endif /* G_OS_UNIX */

#endif /* !GLIB_CHECK_VERSION */

#ifdef G_OS_UNIX
#define syl_link	link
#else
gint syl_link	(const gchar	*src,
		 const gchar	*dest);
#endif

/* The AC_CHECK_SIZEOF() in configure fails for some machines.
 * we provide some fallback values here */
#if !SIZEOF_UNSIGNED_SHORT
  #undef SIZEOF_UNSIGNED_SHORT
  #define SIZEOF_UNSIGNED_SHORT 2
#endif
#if !SIZEOF_UNSIGNED_INT
  #undef SIZEOF_UNSIGNED_INT
  #define SIZEOF_UNSIGNED_INT 4
#endif
#if !SIZEOF_UNSIGNED_LONG
  #undef SIZEOF_UNSIGNED_LONG
  #define SIZEOF_UNSIGNED_LONG 4
#endif

#ifndef HAVE_U32_TYPEDEF
  #undef u32	    /* maybe there is a macro with this name */
  typedef guint32 u32;
  #define HAVE_U32_TYPEDEF
#endif

#if defined(G_OS_WIN32) && !defined(_WIN64) && defined(HAVE_64BIT_TIME_T)
  /* for backward binary compatibility. Use only in struct definition and
     pointer arguments. */
  typedef gint32 stime_t;
#else
  typedef time_t stime_t;
#endif

#if !GLIB_CHECK_VERSION(2, 26, 0)
#if (defined (_MSC_VER) || defined (__MINGW32__)) && !defined(_WIN64)
  typedef struct _stat32 GStatBuf;
#else
  typedef struct stat GStatBuf;
#endif
#endif

#ifndef BIG_ENDIAN_HOST
  #if (G_BYTE_ORDER == G_BIG_ENDIAN)
    #define BIG_ENDIAN_HOST 1
  #endif
#endif

#define CHDIR_RETURN_IF_FAIL(dir) \
{ \
	if (change_dir(dir) < 0) return; \
}

#define CHDIR_RETURN_VAL_IF_FAIL(dir, val) \
{ \
	if (change_dir(dir) < 0) return val; \
}

#define Xalloca(ptr, size, iffail) \
{ \
	if ((ptr = alloca(size)) == NULL) { \
		g_warning("can't allocate memory\n"); \
		iffail; \
	} \
}

#define Xstrdup_a(ptr, str, iffail) \
{ \
	gchar *__tmp; \
 \
	if ((__tmp = alloca(strlen(str) + 1)) == NULL) { \
		g_warning("can't allocate memory\n"); \
		iffail; \
	} else \
		strcpy(__tmp, str); \
 \
	ptr = __tmp; \
}

#define Xstrndup_a(ptr, str, len, iffail) \
{ \
	gchar *__tmp; \
 \
	if ((__tmp = alloca(len + 1)) == NULL) { \
		g_warning("can't allocate memory\n"); \
		iffail; \
	} else { \
		strncpy(__tmp, str, len); \
		__tmp[len] = '\0'; \
	} \
 \
	ptr = __tmp; \
}

#define Xstrcat_a(ptr, str1, str2, iffail) \
{ \
	gchar *__tmp; \
	gint len1, len2; \
 \
	len1 = strlen(str1); \
	len2 = strlen(str2); \
	if ((__tmp = alloca(len1 + len2 + 1)) == NULL) { \
		g_warning("can't allocate memory\n"); \
		iffail; \
	} else { \
		memcpy(__tmp, str1, len1); \
		memcpy(__tmp + len1, str2, len2 + 1); \
	} \
 \
	ptr = __tmp; \
}

#define AUTORELEASE_STR(str, iffail) \
{ \
	gchar *__str; \
	Xstrdup_a(__str, str, iffail); \
	g_free(str); \
	str = __str; \
}

#define FILE_OP_ERROR(file, func) \
{ \
	fprintf(stderr, "%s: ", file); \
	fflush(stderr); \
	perror(func); \
}

typedef void (*UIUpdateFunc)		(void);
typedef void (*EventLoopFunc)		(void);
typedef void (*ProgressFunc)		(gint		 cur,
					 gint		 total);
typedef gchar * (*QueryPasswordFunc)	(const gchar	*server,
					 const gchar	*user);
typedef void (*LogFunc)			(const gchar	*str);
typedef void (*LogFlushFunc)		(void);

/* for macro expansion */
#define Str(x)	#x
#define Xstr(x)	Str(x)

void list_free_strings		(GList		*list);
void slist_free_strings		(GSList		*list);

void hash_free_strings		(GHashTable	*table);
void hash_free_value_mem	(GHashTable	*table);

gint str_case_equal		(gconstpointer	 v,
				 gconstpointer	 v2);
guint str_case_hash		(gconstpointer	 key);

void ptr_array_free_strings	(GPtrArray	*array);

typedef gboolean (*StrFindFunc) (const gchar	*haystack,
				 const gchar	*needle);

gboolean str_find		(const gchar	*haystack,
				 const gchar	*needle);
gboolean str_case_find		(const gchar	*haystack,
				 const gchar	*needle);
gboolean str_find_equal		(const gchar	*haystack,
				 const gchar	*needle);
gboolean str_case_find_equal	(const gchar	*haystack,
				 const gchar	*needle);

/* number-string conversion */
gint to_number			(const gchar *nstr);
guint to_unumber		(const gchar *nstr);
gchar *itos_buf			(gchar	     *nstr,
				 gint	      n);
gchar *itos			(gint	      n);
gchar *utos_buf			(gchar	     *nstr,
				 guint	      n);
gchar *to_human_readable_buf	(gchar	     *buf,
				 size_t	      bufsize,
				 gint64	      size);
gchar *to_human_readable	(gint64	      size);

/* alternative string functions */
gint strcmp2		(const gchar	*s1,
			 const gchar	*s2);
gint path_cmp		(const gchar	*s1,
			 const gchar	*s2);
gboolean is_path_parent	(const gchar	*parent,
			 const gchar	*child);

gchar *strretchomp	(gchar		*str);
gchar *strtailchomp	(gchar		*str,
			 gchar		 tail_char);
gchar *strcrchomp	(gchar		*str);

gchar *strcasestr	(const gchar	*haystack,
			 const gchar	*needle);
gpointer my_memmem	(gconstpointer	 haystack,
			 size_t		 haystacklen,
			 gconstpointer	 needle,
			 size_t		 needlelen);

gchar *strncpy2		(gchar		*dest,
			 const gchar	*src,
			 size_t		 n);

gboolean str_has_suffix_case	(const gchar	*str,
				 const gchar	*suffix);

gint str_find_format_times	(const gchar	*haystack,
				 gchar		 ch);

gboolean is_next_nonascii	(const gchar	*s);
gint get_next_word_len		(const gchar	*s);

/* functions for string parsing */
gint subject_compare			(const gchar	*s1,
					 const gchar	*s2);
gint subject_compare_for_sort		(const gchar	*s1,
					 const gchar	*s2);
void trim_subject_for_compare		(gchar		*str);
void trim_subject_for_sort		(gchar		*str);
void trim_subject			(gchar		*str);
void eliminate_parenthesis		(gchar		*str,
					 gchar		 op,
					 gchar		 cl);
void extract_parenthesis		(gchar		*str,
					 gchar		 op,
					 gchar		 cl);
void extract_parenthesis_with_escape	(gchar		*str,
					 gchar		 op,
					 gchar		 cl);

void extract_parenthesis_with_skip_quote	(gchar		*str,
						 gchar		 quote_chr,
						 gchar		 op,
						 gchar		 cl);

void eliminate_quote			(gchar		*str,
					 gchar		 quote_chr);
void extract_quote			(gchar		*str,
					 gchar		 quote_chr);
void extract_quote_with_escape		(gchar		*str,
					 gchar		 quote_chr);
void eliminate_address_comment		(gchar		*str);
gchar *strchr_with_skip_quote		(const gchar	*str,
					 gint		 quote_chr,
					 gint		 c);
gchar *strrchr_with_skip_quote		(const gchar	*str,
					 gint		 quote_chr,
					 gint		 c);
void extract_address			(gchar		*str);
void extract_list_id_str		(gchar		*str);

gchar *extract_addresses		(const gchar	*str);

gchar *normalize_address_field		(const gchar	*str);

gboolean address_equal			(const gchar	*addr1,
					 const gchar	*addr2);

GSList *address_list_append_orig	(GSList		*addr_list,
					 const gchar	*str);
GSList *address_list_append		(GSList		*addr_list,
					 const gchar	*str);
GSList *references_list_prepend		(GSList		*msgid_list,
					 const gchar	*str);
GSList *references_list_append		(GSList		*msgid_list,
					 const gchar	*str);
GSList *newsgroup_list_append		(GSList		*group_list,
					 const gchar	*str);

GList *add_history			(GList		*list,
					 const gchar	*str);

/* modify string */
void remove_return			(gchar		*str);
void remove_space			(gchar		*str);
void unfold_line			(gchar		*str);
void subst_char				(gchar		*str,
					 gchar		 orig,
					 gchar		 subst);
void subst_chars			(gchar		*str,
					 gchar		*orig,
					 gchar		 subst);
void subst_null				(gchar		*str,
					 gint		 len,
					 gchar		 subst);
void subst_control			(gchar		*str,
					 gchar		 subst);
void subst_for_filename			(gchar		*str);

gchar *get_alt_filename			(const gchar	*filename,
					 gint		 count);

gboolean is_header_line			(const gchar	*str);
gboolean is_ascii_str			(const gchar	*str);

gint get_quote_level			(const gchar	*str);
gint check_line_length			(const gchar	*str,
					 gint		 max_chars,
					 gint		*line);

gchar *strstr_with_skip_quote		(const gchar	*haystack,
					 const gchar	*needle);
gchar *strcasestr_with_skip_quote	(const gchar	*haystack,
					 const gchar	*needle);
gchar *strchr_parenthesis_close		(const gchar	*str,
					 gchar		 op,
					 gchar		 cl);

gchar **strsplit_parenthesis		(const gchar	*str,
					 gchar		 op,
					 gchar		 cl,
					 gint		 max_tokens);
gchar **strsplit_with_quote		(const gchar	*str,
					 const gchar	*delim,
					 gint		 max_tokens);
gchar **strsplit_csv			(const gchar	*str,
					 gchar		 delim,
					 gint		 max_tokens);

gchar *strconcat_csv			(gchar		 delim,
					 const gchar	*field1,
					 ...) G_GNUC_MALLOC G_GNUC_NULL_TERMINATED;

gchar *get_abbrev_newsgroup_name	(const gchar	*group,
					 gint		 len);
gchar *trim_string			(const gchar	*str,
					 gint		 len);
gchar *trim_string_before		(const gchar	*str,
					 gint		 len);

GList *uri_list_extract_filenames	(const gchar	*uri_list);
gboolean is_uri_string			(const gchar	*str);
gchar *get_uri_path			(const gchar	*uri);
gint get_uri_len			(const gchar	*str);
void decode_uri				(gchar		*decoded_uri,
					 const gchar	*encoded_uri);
void decode_xdigit_encoded_str		(gchar		*decoded,
					 const gchar	*encoded);
gchar *encode_uri			(const gchar	*filename);
gchar *uriencode_for_filename		(const gchar	*filename);
gchar *uriencode_for_mailto		(const gchar	*mailto);
gint scan_mailto_url			(const gchar	*mailto,
					 gchar	       **to,
					 gchar	       **cc,
					 gchar	       **bcc,
					 gchar	       **subject,
					 gchar	       **inreplyto,
					 gchar	       **body);

void set_startup_dir			(void);
void set_rc_dir				(const gchar	*dir);

/* return static strings */
const gchar *get_startup_dir		(void);
const gchar *get_home_dir		(void);
const gchar *get_document_dir		(void);
const gchar *get_rc_dir			(void);
const gchar *get_old_rc_dir		(void);
const gchar *get_mail_base_dir		(void);
const gchar *get_news_cache_dir		(void);
const gchar *get_imap_cache_dir		(void);
const gchar *get_mime_tmp_dir		(void);
const gchar *get_template_dir		(void);
const gchar *get_tmp_dir		(void);
gchar *get_tmp_file			(void);
const gchar *get_domain_name		(void);

/* file / directory handling */
off_t get_file_size		(const gchar	*file);
off_t get_file_size_as_crlf	(const gchar	*file);
off_t get_left_file_size	(FILE		*fp);

gint get_last_empty_line_size	(FILE		*fp,
				 off_t		 size);

gboolean file_exist		(const gchar	*file,
				 gboolean	 allow_fifo);
gboolean is_dir_exist		(const gchar	*dir);
gboolean is_file_entry_exist	(const gchar	*file);
gboolean dirent_is_regular_file	(struct dirent	*d);
gboolean dirent_is_directory	(struct dirent	*d);

#define is_file_exist(file)		file_exist(file, FALSE)
#define is_file_or_fifo_exist(file)	file_exist(file, TRUE)

gint change_dir			(const gchar	*dir);
gint make_dir			(const gchar	*dir);
gint make_dir_hier		(const gchar	*dir);
gint remove_all_files		(const gchar	*dir);
gint remove_numbered_files	(const gchar	*dir,
				 guint		 first,
				 guint		 last);
gint remove_all_numbered_files	(const gchar	*dir);
gint remove_expired_files	(const gchar	*dir,
				 guint		 hours);
gint remove_dir_recursive	(const gchar	*dir);
gint rename_force		(const gchar	*oldpath,
				 const gchar	*newpath);
gint copy_file			(const gchar	*src,
				 const gchar	*dest,
				 gboolean	 keep_backup);
gint copy_dir			(const gchar	*src,
				 const gchar	*dest);
gint move_file			(const gchar	*src,
				 const gchar	*dest,
				 gboolean	 overwrite);

gint append_file_part		(FILE		*fp,
				 off_t		 offset,
				 size_t		 length,
				 FILE		*dest_fp);
gint copy_file_part		(FILE		*fp,
				 off_t		 offset,
				 size_t		 length,
				 const gchar	*dest);

gint copy_file_stream		(FILE		*fp,
				 FILE		*dest_fp);

gchar *canonicalize_str		(const gchar	*str);
gint canonicalize_file		(const gchar	*src,
				 const gchar	*dest);
gint canonicalize_file_replace	(const gchar	*file);
FILE *canonicalize_file_stream	(FILE		*fp,
				 gint		*length);
gint uncanonicalize_file	(const gchar	*src,
				 const gchar	*dest);
gint uncanonicalize_file_replace(const gchar	*file);

gchar *normalize_newlines	(const gchar	*str);
gchar *strchomp_all		(const gchar	*str);

FILE *get_outgoing_rfc2822_file	(FILE		*fp);
gchar *get_outgoing_rfc2822_str	(FILE		*fp);
gchar *generate_mime_boundary	(const gchar	*prefix);

gint change_file_mode_rw	(FILE		*fp,
				 const gchar	*file);
FILE *my_tmpfile		(void);
FILE *str_open_as_stream	(const gchar	*str);
gint str_write_to_file		(const gchar	*str,
				 const gchar	*file);
gchar *file_read_to_str		(const gchar	*file);
gchar *file_read_stream_to_str	(FILE		*fp);

/* process execution */
gint execute_async		(gchar *const	 argv[]);
gint execute_sync		(gchar *const	 argv[]);
gint execute_command_line	(const gchar	*cmdline,
				 gboolean	 async);
gint execute_command_line_async_wait
				(const gchar	*cmdline);

gint execute_open_file		(const gchar	*file,
				 const gchar	*content_type);
gint execute_print_file		(const gchar	*file);
gchar *get_command_output	(const gchar	*cmdline);

/* open URI with external browser */
gint open_uri			(const gchar	*uri,
				 const gchar	*cmdline);

/* play sound */
gint play_sound			(const gchar	*file,
				 gboolean	 async);

/* time functions */
stime_t remote_tzoffset_sec	(const gchar	*zone);
stime_t tzoffset_sec		(stime_t	*now);
gchar *tzoffset_buf		(gchar		*buf,
				 stime_t	*now);
gchar *tzoffset			(stime_t	*now);
void get_rfc822_date		(gchar		*buf,
				 gint		 len);

size_t my_strftime		(gchar			*s,
				 size_t			 max,
				 const gchar		*format,
				 const struct tm	*tm);

/* UI hints */
void set_ui_update_func	(UIUpdateFunc	 func);
void ui_update		(void);

void set_event_loop_func	(EventLoopFunc	 func);
void event_loop_iterate		(void);

void set_progress_func	(ProgressFunc	 func);
void progress_show	(gint		 cur,
			 gint		 total);

/* user input */
void set_input_query_password_func	(QueryPasswordFunc	func);

gchar *input_query_password	(const gchar	*server,
				 const gchar	*user);

/* logging */
void set_log_file	(const gchar	*filename);
void close_log_file	(void);
void set_log_verbosity	(gboolean	 verbose);
gboolean get_debug_mode	(void);
void set_debug_mode	(gboolean	 enable);

void set_log_ui_func		(LogFunc	 print_func,
				 LogFunc	 message_func,
				 LogFunc	 warning_func,
				 LogFunc	 error_func);
void set_log_ui_func_full	(LogFunc	 print_func,
				 LogFunc	 message_func,
				 LogFunc	 warning_func,
				 LogFunc	 error_func,
				 LogFlushFunc	 flush_func);

void set_log_show_status_func	(LogFunc	 status_func);

void debug_print	(const gchar *format, ...) G_GNUC_PRINTF(1, 2);
void status_print	(const gchar *format, ...) G_GNUC_PRINTF(1, 2);

void log_write		(const gchar	*str,
			 const gchar	*prefix);

void log_print		(const gchar *format, ...) G_GNUC_PRINTF(1, 2);
void log_message	(const gchar *format, ...) G_GNUC_PRINTF(1, 2);
void log_warning	(const gchar *format, ...) G_GNUC_PRINTF(1, 2);
void log_error		(const gchar *format, ...) G_GNUC_PRINTF(1, 2);

void log_flush		(void);

#endif /* __UTILS_H__ */
