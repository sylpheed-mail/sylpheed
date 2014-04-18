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

#ifndef __PROCMSG_H__
#define __PROCMSG_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <string.h>

typedef struct _MsgInfo		MsgInfo;
typedef struct _MsgFlags	MsgFlags;
typedef struct _MsgFileInfo	MsgFileInfo;
typedef struct _MsgEncryptInfo	MsgEncryptInfo;

#include "folder.h"
#include "procmime.h"
#include "utils.h"

typedef enum
{
	DATA_READ,
	DATA_WRITE,
	DATA_APPEND
} DataOpenMode;

#define MSG_NEW			(1U << 0)
#define MSG_UNREAD		(1U << 1)
#define MSG_MARKED		(1U << 2)
#define MSG_DELETED		(1U << 3)
#define MSG_REPLIED		(1U << 4)
#define MSG_FORWARDED		(1U << 5)

#define MSG_CLABEL_SBIT	(7)		/* start bit of color label */
#define MAKE_MSG_CLABEL(h, m, l)	(((h) << (MSG_CLABEL_SBIT + 2)) | \
					 ((m) << (MSG_CLABEL_SBIT + 1)) | \
					 ((l) << (MSG_CLABEL_SBIT + 0)))

#define MSG_CLABEL_NONE		MAKE_MSG_CLABEL(0U, 0U, 0U)
#define MSG_CLABEL_1		MAKE_MSG_CLABEL(0U, 0U, 1U)
#define MSG_CLABEL_2		MAKE_MSG_CLABEL(0U, 1U, 0U)
#define MSG_CLABEL_3		MAKE_MSG_CLABEL(0U, 1U, 1U)
#define MSG_CLABEL_4		MAKE_MSG_CLABEL(1U, 0U, 0U)
#define MSG_CLABEL_5		MAKE_MSG_CLABEL(1U, 0U, 1U)
#define MSG_CLABEL_6		MAKE_MSG_CLABEL(1U, 1U, 0U)
#define MSG_CLABEL_7		MAKE_MSG_CLABEL(1U, 1U, 1U)

#define MSG_CLABEL_ORANGE	MSG_CLABEL_1
#define MSG_CLABEL_RED		MSG_CLABEL_2
#define MSG_CLABEL_PINK		MSG_CLABEL_3
#define MSG_CLABEL_SKYBLUE	MSG_CLABEL_4
#define MSG_CLABEL_BLUE		MSG_CLABEL_5
#define MSG_CLABEL_GREEN	MSG_CLABEL_6
#define MSG_CLABEL_BROWN	MSG_CLABEL_7

/* RESERVED */
#define	MSG_RESERVED		(1U << 31)

#define MSG_CLABEL_FLAG_MASK	(MSG_CLABEL_7)

typedef guint32 MsgPermFlags;

#define MSG_MOVE		(1U << 0)
#define MSG_COPY		(1U << 1)
#define MSG_QUEUED		(1U << 16)
#define MSG_DRAFT		(1U << 17)
#define MSG_ENCRYPTED		(1U << 18)
#define MSG_IMAP		(1U << 19)
#define MSG_NEWS		(1U << 20)
#define MSG_SIGNED		(1U << 21)
#define MSG_MIME_HTML		(1U << 26)
#define MSG_FLAG_CHANGED	(1U << 27)
#define MSG_CACHED		(1U << 28)
#define MSG_MIME		(1U << 29)
#define MSG_INVALID		(1U << 30)
#define MSG_RECEIVED		(1U << 31)

#define MSG_CACHED_FLAG_MASK	(MSG_MIME|MSG_MIME_HTML)

typedef guint32 MsgTmpFlags;

#define MSG_SET_FLAGS(msg, flags)	{ (msg) |= (flags); }
#define MSG_UNSET_FLAGS(msg, flags)	{ (msg) &= ~(flags); }
#define MSG_SET_PERM_FLAGS(msg, flags) \
	MSG_SET_FLAGS((msg).perm_flags, flags)
#define MSG_SET_TMP_FLAGS(msg, flags) \
	MSG_SET_FLAGS((msg).tmp_flags, flags)
#define MSG_UNSET_PERM_FLAGS(msg, flags) \
	MSG_UNSET_FLAGS((msg).perm_flags, flags)
#define MSG_UNSET_TMP_FLAGS(msg, flags) \
	MSG_UNSET_FLAGS((msg).tmp_flags, flags)

#define MSG_IS_NEW(msg)			(((msg).perm_flags & MSG_NEW) != 0)
#define MSG_IS_UNREAD(msg)		(((msg).perm_flags & MSG_UNREAD) != 0)
#define MSG_IS_MARKED(msg)		(((msg).perm_flags & MSG_MARKED) != 0)
#define MSG_IS_DELETED(msg)		(((msg).perm_flags & MSG_DELETED) != 0)
#define MSG_IS_REPLIED(msg)		(((msg).perm_flags & MSG_REPLIED) != 0)
#define MSG_IS_FORWARDED(msg)		(((msg).perm_flags & MSG_FORWARDED) != 0)

#define MSG_GET_COLORLABEL(msg)		(((msg).perm_flags & MSG_CLABEL_FLAG_MASK))
#define MSG_GET_COLORLABEL_VALUE(msg)	(MSG_GET_COLORLABEL(msg) >> MSG_CLABEL_SBIT)
#define MSG_SET_COLORLABEL_VALUE(msg, val) \
	MSG_SET_PERM_FLAGS(msg, ((((guint)(val)) & 7) << MSG_CLABEL_SBIT))

#define MSG_IS_MOVE(msg)		(((msg).tmp_flags & MSG_MOVE) != 0)
#define MSG_IS_COPY(msg)		(((msg).tmp_flags & MSG_COPY) != 0)

#define MSG_IS_QUEUED(msg)		(((msg).tmp_flags & MSG_QUEUED) != 0)
#define MSG_IS_DRAFT(msg)		(((msg).tmp_flags & MSG_DRAFT) != 0)
#define MSG_IS_ENCRYPTED(msg)		(((msg).tmp_flags & MSG_ENCRYPTED) != 0)
#define MSG_IS_IMAP(msg)		(((msg).tmp_flags & MSG_IMAP) != 0)
#define MSG_IS_NEWS(msg)		(((msg).tmp_flags & MSG_NEWS) != 0)
#define MSG_IS_SIGNED(msg)		(((msg).tmp_flags & MSG_SIGNED) != 0)
#define MSG_IS_MIME_HTML(msg)		(((msg).tmp_flags & MSG_MIME_HTML) != 0)
#define MSG_IS_FLAG_CHANGED(msg)	(((msg).tmp_flags & MSG_FLAG_CHANGED) != 0)
#define MSG_IS_CACHED(msg)		(((msg).tmp_flags & MSG_CACHED) != 0)
#define MSG_IS_MIME(msg)		(((msg).tmp_flags & MSG_MIME) != 0)
#define MSG_IS_INVALID(msg)		(((msg).tmp_flags & MSG_INVALID) != 0)
#define MSG_IS_RECEIVED(msg)		(((msg).tmp_flags & MSG_RECEIVED) != 0)

#define WRITE_CACHE_DATA_INT(n, fp)		\
{						\
	guint32 idata;				\
						\
	idata = (guint32)n;			\
	fwrite(&idata, sizeof(idata), 1, fp);	\
}

#define WRITE_CACHE_DATA(data, fp)			\
{							\
	size_t len;					\
							\
	if (data == NULL) {				\
		len = 0;				\
		WRITE_CACHE_DATA_INT(len, fp);		\
	} else {					\
		len = strlen(data);			\
		WRITE_CACHE_DATA_INT(len, fp);		\
		if (len > 0)				\
			fwrite(data, len, 1, fp);	\
	}						\
}

struct _MsgFlags
{
	MsgPermFlags perm_flags;
	MsgTmpFlags  tmp_flags;
};

struct _MsgInfo
{
	guint  msgnum;
	gsize  size;
	stime_t mtime;
	stime_t date_t;

	MsgFlags flags;

	gchar *fromname;

	gchar *date;
	gchar *from;
	gchar *to;
	gchar *cc;
	gchar *newsgroups;
	gchar *subject;
	gchar *msgid;
	gchar *inreplyto;

	GSList *references;

	FolderItem *folder;
	FolderItem *to_folder;

	gchar *xface;

	/* used only for temporary messages */
	gchar *file_path;

	/* used only for encrypted (and signed) messages */
	MsgEncryptInfo *encinfo;
};

struct _MsgFileInfo
{
	gchar *file;
	MsgFlags *flags;
};

struct _MsgEncryptInfo
{
	gchar *plaintext_file;
	gchar *sigstatus;
	gchar *sigstatus_full;
	gboolean decryption_failed;
};

typedef FILE * (*DecryptMessageFunc)		(MsgInfo	*msginfo,
						 MimeInfo      **mimeinfo);

GHashTable *procmsg_msg_hash_table_create	(GSList		*mlist);
void procmsg_msg_hash_table_append		(GHashTable	*msg_table,
						 GSList		*mlist);
GHashTable *procmsg_to_folder_hash_table_create	(GSList		*mlist);

gint procmsg_read_cache_data_str	(FILE		*fp,
					 gchar	       **str);

GSList *procmsg_read_cache		(FolderItem	*item,
					 gboolean	 scan_file);
void	procmsg_set_flags		(GSList		*mlist,
					 FolderItem	*item);
void	procmsg_mark_all_read		(FolderItem	*item);
GSList *procmsg_sort_msg_list		(GSList		*mlist,
					 FolderSortKey	 sort_key,
					 FolderSortType	 sort_type);
gint	procmsg_get_last_num_in_msg_list(GSList		*mlist);
void	procmsg_msg_list_free		(GSList		*mlist);

void	procmsg_write_cache		(MsgInfo	*msginfo,
					 FILE		*fp);
void	procmsg_write_flags		(MsgInfo	*msginfo,
					 FILE		*fp);
void	procmsg_write_cache_list	(FolderItem	*item,
					 GSList		*mlist);
void	procmsg_write_flags_list	(FolderItem	*item,
					 GSList		*mlist);
void	procmsg_write_flags_for_multiple_folders
					(GSList		*mlist);

void	procmsg_flaginfo_list_free	(GSList		*flaglist);

void	procmsg_flush_mark_queue	(FolderItem	*item,
					 FILE		*fp);
void	procmsg_add_mark_queue		(FolderItem	*item,
					 gint		 num,
					 MsgFlags	 flags);
void	procmsg_flush_cache_queue	(FolderItem	*item,
					 FILE		*fp);
void	procmsg_add_cache_queue		(FolderItem	*item,
					 gint		 num,
					 MsgInfo	*msginfo);

gboolean procmsg_flush_folder		(FolderItem	*item);
void	procmsg_flush_folder_foreach	(GHashTable	*folder_table);

void	procmsg_add_flags		(FolderItem	*item,
					 gint		 num,
					 MsgFlags	 flags);

void	procmsg_get_mark_sum		(FolderItem	*item,
					 gint		*new,
					 gint		*unread,
					 gint		*total,
					 gint		*min,
					 gint		*max,
					 gint		 first);

FILE   *procmsg_open_data_file		(const gchar	*file,
					 guint		 version,
					 DataOpenMode	 mode,
					 gchar		*buf,
					 size_t		 buf_size);

FILE   *procmsg_open_cache_file		(FolderItem	*item,
					 DataOpenMode	 mode);
FILE   *procmsg_open_mark_file		(FolderItem	*item,
					 DataOpenMode	 mode);

void	procmsg_clear_cache		(FolderItem	*item);
void	procmsg_clear_mark		(FolderItem	*item);

GNode  *procmsg_get_thread_tree		(GSList		*mlist);
guint	procmsg_get_thread_date		(GNode		*node);

gint	procmsg_move_messages		(GSList		*mlist);
gint	procmsg_copy_messages		(GSList		*mlist);

gint	procmsg_add_messages_from_queue	(FolderItem	*dest,
					 GSList		*mlist,
					 gboolean	 is_move);

gchar  *procmsg_get_message_file_path	(MsgInfo	*msginfo);
gchar  *procmsg_get_message_file	(MsgInfo	*msginfo);
GSList *procmsg_get_message_file_list	(GSList		*mlist);
void	procmsg_message_file_list_free	(GSList		*file_list);
FILE   *procmsg_open_message		(MsgInfo	*msginfo);

void procmsg_set_decrypt_message_func	(DecryptMessageFunc	 func);
void procmsg_set_auto_decrypt_message	(gboolean	 enabled);
FILE   *procmsg_open_message_decrypted	(MsgInfo	*msginfo,
					 MimeInfo      **mimeinfo);

gboolean procmsg_msg_exist		(MsgInfo	*msginfo);

gboolean procmsg_trash_messages_exist	(void);
void	procmsg_empty_trash		(FolderItem	*trash);
void	procmsg_empty_all_trash		(void);

void	procmsg_remove_all_cached_messages
					(Folder		*folder);

gint	procmsg_save_to_outbox		(FolderItem	*outbox,
					 const gchar	*file);
void	procmsg_print_message		(MsgInfo	*msginfo,
					 const gchar	*cmdline,
					 gboolean	 all_headers);
void	procmsg_print_message_part	(MsgInfo	*msginfo,
					 MimeInfo	*partinfo,
					 const gchar	*cmdline,
					 gboolean	 all_headers);

gint	procmsg_save_message_as_text	(MsgInfo	*msginfo,
					 const gchar	*dest,
					 const gchar	*encoding,
					 gboolean	 all_headers);

gint	procmsg_concat_partial_messages	(GSList		*mlist,
					 const gchar	*file);

MsgInfo *procmsg_get_msginfo		(FolderItem	*item,
					 gint		 num);

MsgInfo *procmsg_msginfo_copy		(MsgInfo	*msginfo);
MsgInfo *procmsg_msginfo_get_full_info	(MsgInfo	*msginfo);
gboolean procmsg_msginfo_equal		(MsgInfo	*msginfo_a,
					 MsgInfo	*msginfo_b);
void	 procmsg_msginfo_free		(MsgInfo	*msginfo);

gint procmsg_cmp_msgnum_for_sort	(gconstpointer	 a,
					 gconstpointer	 b);

#endif /* __PROCMSG_H__ */
