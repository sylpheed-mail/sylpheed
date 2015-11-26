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

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "utils.h"
#include "procmsg.h"
#include "procheader.h"
#include "account.h"
#include "procmime.h"
#include "prefs_common.h"
#include "folder.h"
#include "codeconv.h"

typedef struct _MsgFlagInfo {
	guint msgnum;
	MsgFlags flags;
} MsgFlagInfo;

static GSList *procmsg_read_cache_queue		(FolderItem	*item,
						 gboolean	 scan_file);

static void mark_sum_func			(gpointer	 key,
						 gpointer	 value,
						 gpointer	 data);

static GHashTable *procmsg_read_mark_file	(FolderItem	*item);
static void procmsg_write_mark_file		(FolderItem	*item,
						 GHashTable	*mark_table);

static GMappedFile *procmsg_open_cache_file_mmap(FolderItem	*item,
						 DataOpenMode	 mode);

static gint procmsg_cmp_by_mark			(gconstpointer	 a,
						 gconstpointer	 b);
static gint procmsg_cmp_by_unread		(gconstpointer	 a,
						 gconstpointer	 b);
static gint procmsg_cmp_by_mime			(gconstpointer	 a,
						 gconstpointer	 b);
static gint procmsg_cmp_by_label		(gconstpointer	 a,
						 gconstpointer	 b);
static gint procmsg_cmp_by_number		(gconstpointer	 a,
						 gconstpointer	 b);
static gint procmsg_cmp_by_size			(gconstpointer	 a,
						 gconstpointer	 b);
static gint procmsg_cmp_by_date			(gconstpointer	 a,
						 gconstpointer	 b);
static gint procmsg_cmp_by_from			(gconstpointer	 a,
						 gconstpointer	 b);
static gint procmsg_cmp_by_to			(gconstpointer	 a,
						 gconstpointer	 b);
static gint procmsg_cmp_by_subject		(gconstpointer	 a,
						 gconstpointer	 b);


GHashTable *procmsg_msg_hash_table_create(GSList *mlist)
{
	GHashTable *msg_table;

	if (mlist == NULL) return NULL;

	msg_table = g_hash_table_new(NULL, g_direct_equal);
	procmsg_msg_hash_table_append(msg_table, mlist);

	return msg_table;
}

void procmsg_msg_hash_table_append(GHashTable *msg_table, GSList *mlist)
{
	GSList *cur;
	MsgInfo *msginfo;

	if (msg_table == NULL || mlist == NULL) return;

	for (cur = mlist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;

		g_hash_table_insert(msg_table,
				    GUINT_TO_POINTER(msginfo->msgnum),
				    msginfo);
	}
}

GHashTable *procmsg_to_folder_hash_table_create(GSList *mlist)
{
	GHashTable *msg_table;
	GSList *cur;
	MsgInfo *msginfo;

	if (mlist == NULL) return NULL;

	msg_table = g_hash_table_new(NULL, g_direct_equal);

	for (cur = mlist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;
		g_hash_table_insert(msg_table, msginfo->to_folder, msginfo);
	}

	return msg_table;
}

gint procmsg_read_cache_data_str(FILE *fp, gchar **str)
{
	gchar buf[BUFFSIZE];
	guint32 len;
	gchar *tmp = NULL;

	if (fread(&len, sizeof(len), 1, fp) != 1)
		return -1;

	if (len > G_MAXINT)
		return -1;

	while (len > 0) {
		size_t size = MIN(len, BUFFSIZE - 1);

		if (fread(buf, size, 1, fp) != 1) {
			if (tmp)
				g_free(tmp);
			*str = NULL;
			return -1;
		}

		buf[size] = '\0';
		if (tmp) {
			*str = g_strconcat(tmp, buf, NULL);
			g_free(tmp);
			tmp = *str;
		} else
			tmp = *str = g_strdup(buf);

		len -= size;
	}

	return 0;
}

static gint procmsg_read_cache_data_str_mem(const gchar **p, const gchar *endp, gchar **str)
{
	guint32 len;

	if (endp - *p < sizeof(len))
		return -1;

	memcpy(&len, *p, sizeof(len));
	*p += sizeof(len);
	if (len > G_MAXINT || len > endp - *p)
		return -1;

	if (len > 0) {
		*str = g_strndup(*p, len);
		*p += len;
	}

	return 0;
}

#define READ_CACHE_DATA(data)						\
{									\
	if (procmsg_read_cache_data_str_mem(&p, endp, &data) < 0) {	\
		g_warning("Cache data is corrupted\n");			\
		procmsg_msginfo_free(msginfo);				\
		procmsg_msg_list_free(mlist);				\
		g_mapped_file_free(mapfile);				\
		return NULL;						\
	}								\
}

#define READ_CACHE_DATA_INT(n)					\
{								\
	if (endp - p < sizeof(guint32)) {			\
		g_warning("Cache data is corrupted\n");		\
		procmsg_msginfo_free(msginfo);			\
		procmsg_msg_list_free(mlist);			\
		g_mapped_file_free(mapfile);			\
		return NULL;					\
	} else {						\
		guint32 idata;					\
		memcpy(&idata, p, sizeof(idata));		\
		n = idata;					\
		p += sizeof(guint32);				\
	}							\
}

GSList *procmsg_read_cache(FolderItem *item, gboolean scan_file)
{
	GSList *mlist = NULL;
	GSList *last = NULL;
	GMappedFile *mapfile;
	const gchar *filep;
	gsize file_len;
	const gchar *p, *endp;
	MsgInfo *msginfo;
	MsgFlags default_flags;
	guint32 num;
	guint refnum;
	FolderType type;

	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(item->folder != NULL, NULL);
	type = FOLDER_TYPE(item->folder);

	default_flags.perm_flags = MSG_NEW|MSG_UNREAD;
	default_flags.tmp_flags = 0;
	if (type == F_MH || type == F_IMAP) {
		if (item->stype == F_QUEUE) {
			MSG_SET_TMP_FLAGS(default_flags, MSG_QUEUED);
		} else if (item->stype == F_DRAFT) {
			MSG_SET_TMP_FLAGS(default_flags, MSG_DRAFT);
		}
	}
	if (type == F_IMAP) {
		MSG_SET_TMP_FLAGS(default_flags, MSG_IMAP);
	} else if (type == F_NEWS) {
		MSG_SET_TMP_FLAGS(default_flags, MSG_NEWS);
	}

	if (type == F_MH) {
		gchar *path;

		path = folder_item_get_path(item);
		if (change_dir(path) < 0) {
			g_free(path);
			return NULL;
		}
		g_free(path);
	}

	mapfile = procmsg_open_cache_file_mmap(item, DATA_READ);
	if (!mapfile) {
		item->cache_dirty = TRUE;
		return NULL;
	}

	debug_print("Reading summary cache...\n");

	filep = g_mapped_file_get_contents(mapfile);
	file_len = g_mapped_file_get_length(mapfile);
	endp = filep + file_len;
	p = filep + sizeof(guint32); /* version */

	while (endp - p >= sizeof(num)) {
		msginfo = g_new0(MsgInfo, 1);

		READ_CACHE_DATA_INT(msginfo->msgnum);

		READ_CACHE_DATA_INT(msginfo->size);
		READ_CACHE_DATA_INT(msginfo->mtime);
		READ_CACHE_DATA_INT(msginfo->date_t);
		READ_CACHE_DATA_INT(msginfo->flags.tmp_flags);

		READ_CACHE_DATA(msginfo->fromname);

		READ_CACHE_DATA(msginfo->date);
		READ_CACHE_DATA(msginfo->from);
		READ_CACHE_DATA(msginfo->to);
		READ_CACHE_DATA(msginfo->newsgroups);
		READ_CACHE_DATA(msginfo->subject);
		READ_CACHE_DATA(msginfo->msgid);
		READ_CACHE_DATA(msginfo->inreplyto);

		READ_CACHE_DATA_INT(refnum);
		for (; refnum != 0; refnum--) {
			gchar *ref;

			READ_CACHE_DATA(ref);
			msginfo->references =
				g_slist_prepend(msginfo->references, ref);
		}
		if (msginfo->references)
			msginfo->references =
				g_slist_reverse(msginfo->references);

		MSG_SET_PERM_FLAGS(msginfo->flags, default_flags.perm_flags);
		MSG_SET_TMP_FLAGS(msginfo->flags, default_flags.tmp_flags);

		/* if the message file doesn't exist or is changed,
		   don't add the data */
		if ((type == F_MH && scan_file &&
		     folder_item_is_msg_changed(item, msginfo)) ||
		     msginfo->msgnum == 0) {
			procmsg_msginfo_free(msginfo);
			item->cache_dirty = TRUE;
		} else {
			msginfo->folder = item;

			if (!mlist)
				last = mlist = g_slist_append(NULL, msginfo);
			else {
				last = g_slist_append(last, msginfo);
				last = last->next;
			}
		}
	}

	g_mapped_file_free(mapfile);

	if (item->cache_queue) {
		GSList *qlist;
		qlist = procmsg_read_cache_queue(item, scan_file);
		mlist = g_slist_concat(mlist, qlist);
	}

	debug_print("done.\n");

	return mlist;
}

#undef READ_CACHE_DATA
#undef READ_CACHE_DATA_INT

static GSList *procmsg_read_cache_queue(FolderItem *item, gboolean scan_file)
{
	FolderType type;
	MsgInfo *msginfo;
	MsgFlags default_flags;
	GSList *cur;
	GSList *qlist = NULL;

	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(item->folder != NULL, NULL);

	if (!item->cache_queue)
		return NULL;

	debug_print("Reading cache queue...\n");

	type = FOLDER_TYPE(item->folder);
	default_flags.perm_flags = MSG_NEW|MSG_UNREAD;
	default_flags.tmp_flags = 0;

	for (cur = item->cache_queue; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;

		debug_print("read cache queue: %s/%d\n",
			    item->path, msginfo->msgnum);

		MSG_SET_PERM_FLAGS(msginfo->flags, default_flags.perm_flags);
		MSG_SET_TMP_FLAGS(msginfo->flags, default_flags.tmp_flags);

		if ((type == F_MH && scan_file &&
		     folder_item_is_msg_changed(item, msginfo))) {
			procmsg_msginfo_free(msginfo);
			item->cache_dirty = TRUE;
		} else {
			msginfo->folder = item;
			qlist = g_slist_prepend(qlist, msginfo);
		}
	}

	g_slist_free(item->cache_queue);
	item->cache_queue = NULL;
	item->cache_dirty = TRUE;

	return qlist;
}

static void mark_unset_new_func(gpointer key, gpointer value, gpointer data)
{
	MSG_UNSET_PERM_FLAGS(*((MsgFlags *)value), MSG_NEW);
}

void procmsg_set_flags(GSList *mlist, FolderItem *item)
{
	GSList *cur;
	gint new = 0, unread = 0, total = 0;
	gint lastnum = 0;
	gint unflagged = 0;
	gboolean mark_queue_exist;
	MsgInfo *msginfo;
	GHashTable *mark_table;
	MsgFlags *flags;

	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	debug_print("Marking the messages...\n");

	mark_queue_exist = (item->mark_queue != NULL);
	mark_table = procmsg_read_mark_file(item);
	if (!mark_table) {
		item->new = item->unread = item->total = g_slist_length(mlist);
		item->updated = TRUE;
		item->mark_dirty = TRUE;
		return;
	}

	/* unset new flags if new (unflagged) messages exist */
	if (!mark_queue_exist) {
		for (cur = mlist; cur != NULL; cur = cur->next) {
			msginfo = (MsgInfo *)cur->data;
			flags = g_hash_table_lookup
				(mark_table, GUINT_TO_POINTER(msginfo->msgnum));
			if (!flags) {
				g_hash_table_foreach(mark_table,
						     mark_unset_new_func, NULL);
				item->mark_dirty = TRUE;
				break;
			}
		}
	}

	for (cur = mlist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;

		if (lastnum < msginfo->msgnum)
			lastnum = msginfo->msgnum;

		flags = g_hash_table_lookup
			(mark_table, GUINT_TO_POINTER(msginfo->msgnum));

		if (flags != NULL) {
			/* add the permanent flags only */
			msginfo->flags.perm_flags = flags->perm_flags;
			if (MSG_IS_NEW(*flags))
				++new;
			if (MSG_IS_UNREAD(*flags))
				++unread;
			if (FOLDER_TYPE(item->folder) == F_IMAP) {
				MSG_SET_TMP_FLAGS(msginfo->flags, MSG_IMAP);
			} else if (FOLDER_TYPE(item->folder) == F_NEWS) {
				MSG_SET_TMP_FLAGS(msginfo->flags, MSG_NEWS);
			}
		} else {
			++unflagged;
			++new;
			++unread;
		}

		++total;
	}

	item->new = new;
	item->unread = unread;
	item->total = total;
	item->unmarked_num = unflagged;
	item->last_num = lastnum;
	item->updated = TRUE;

	if (unflagged > 0)
		item->mark_dirty = TRUE;

	debug_print("new: %d unread: %d unflagged: %d total: %d\n",
		    new, unread, unflagged, total);

	hash_free_value_mem(mark_table);
	g_hash_table_destroy(mark_table);
}

static void mark_all_read_func(gpointer key, gpointer value, gpointer data)
{
	MSG_UNSET_PERM_FLAGS(*((MsgFlags *)value), MSG_NEW|MSG_UNREAD);
}

void procmsg_mark_all_read(FolderItem *item)
{
	GHashTable *mark_table;

	debug_print("Marking all messages as read\n");

	mark_table = procmsg_read_mark_file(item);
	if (mark_table) {
		g_hash_table_foreach(mark_table, mark_all_read_func, NULL);
		procmsg_write_mark_file(item, mark_table);
		hash_free_value_mem(mark_table);
		g_hash_table_destroy(mark_table);
	}

	if (item->mark_queue) {
		GSList *cur;
		MsgFlagInfo *flaginfo;

		for (cur = item->mark_queue; cur != NULL; cur = cur->next) {
			flaginfo = (MsgFlagInfo *)cur->data;
			MSG_UNSET_PERM_FLAGS
				(flaginfo->flags, MSG_NEW|MSG_UNREAD);
		}
		item->mark_dirty = TRUE;
	}

	item->new = item->unread = 0;
}

static FolderSortType cmp_func_sort_type;

GSList *procmsg_sort_msg_list(GSList *mlist, FolderSortKey sort_key,
			      FolderSortType sort_type)
{
	GCompareFunc cmp_func;

	switch (sort_key) {
	case SORT_BY_MARK:
		cmp_func = procmsg_cmp_by_mark; break;
	case SORT_BY_UNREAD:
		cmp_func = procmsg_cmp_by_unread; break;
	case SORT_BY_MIME:
		cmp_func = procmsg_cmp_by_mime; break;
	case SORT_BY_LABEL:
		cmp_func = procmsg_cmp_by_label; break;
	case SORT_BY_NUMBER:
		cmp_func = procmsg_cmp_by_number; break;
	case SORT_BY_SIZE:
		cmp_func = procmsg_cmp_by_size; break;
	case SORT_BY_DATE:
		cmp_func = procmsg_cmp_by_date; break;
	case SORT_BY_FROM:
		cmp_func = procmsg_cmp_by_from; break;
	case SORT_BY_SUBJECT:
		cmp_func = procmsg_cmp_by_subject; break;
	case SORT_BY_TO:
		cmp_func = procmsg_cmp_by_to; break;
	default:
		return mlist;
	}

	cmp_func_sort_type = sort_type;

	mlist = g_slist_sort(mlist, cmp_func);

	return mlist;
}

gint procmsg_get_last_num_in_msg_list(GSList *mlist)
{
	GSList *cur;
	MsgInfo *msginfo;
	gint last = 0;

	for (cur = mlist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;
		if (msginfo && msginfo->msgnum > last)
			last = msginfo->msgnum;
	}

	return last;
}

void procmsg_msg_list_free(GSList *mlist)
{
	GSList *cur;
	MsgInfo *msginfo;

	for (cur = mlist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;
		procmsg_msginfo_free(msginfo);
	}
	g_slist_free(mlist);
}

void procmsg_write_cache(MsgInfo *msginfo, FILE *fp)
{
	MsgTmpFlags flags = msginfo->flags.tmp_flags & MSG_CACHED_FLAG_MASK;
	GSList *cur;

	WRITE_CACHE_DATA_INT(msginfo->msgnum, fp);
	WRITE_CACHE_DATA_INT(msginfo->size, fp);
	WRITE_CACHE_DATA_INT(msginfo->mtime, fp);
	WRITE_CACHE_DATA_INT(msginfo->date_t, fp);
	WRITE_CACHE_DATA_INT(flags, fp);

	WRITE_CACHE_DATA(msginfo->fromname, fp);

	WRITE_CACHE_DATA(msginfo->date, fp);
	WRITE_CACHE_DATA(msginfo->from, fp);
	WRITE_CACHE_DATA(msginfo->to, fp);
	WRITE_CACHE_DATA(msginfo->newsgroups, fp);
	WRITE_CACHE_DATA(msginfo->subject, fp);
	WRITE_CACHE_DATA(msginfo->msgid, fp);
	WRITE_CACHE_DATA(msginfo->inreplyto, fp);

	WRITE_CACHE_DATA_INT(g_slist_length(msginfo->references), fp);
	for (cur = msginfo->references; cur != NULL; cur = cur->next) {
		WRITE_CACHE_DATA((gchar *)cur->data, fp);
	}
}

void procmsg_write_flags(MsgInfo *msginfo, FILE *fp)
{
	MsgPermFlags flags = msginfo->flags.perm_flags;

	WRITE_CACHE_DATA_INT(msginfo->msgnum, fp);
	WRITE_CACHE_DATA_INT(flags, fp);
}

void procmsg_write_cache_list(FolderItem *item, GSList *mlist)
{
	FILE *fp;
	GSList *cur;

	g_return_if_fail(item != NULL);

	debug_print("Writing summary cache (%s)\n", item->path);

	fp = procmsg_open_cache_file(item, DATA_WRITE);
	if (fp == NULL)
		return;

	for (cur = mlist; cur != NULL; cur = cur->next) {
		MsgInfo *msginfo = (MsgInfo *)cur->data;
		procmsg_write_cache(msginfo, fp);
	}

	if (item->cache_queue)
		procmsg_flush_cache_queue(item, fp);

	fclose(fp);
	item->cache_dirty = FALSE;
}

void procmsg_write_flags_list(FolderItem *item, GSList *mlist)
{
	FILE *fp;
	GSList *cur;

	g_return_if_fail(item != NULL);

	debug_print("Writing summary flags (%s)\n", item->path);

	fp = procmsg_open_mark_file(item, DATA_WRITE);
	if (fp == NULL)
		return;

	for (cur = mlist; cur != NULL; cur = cur->next) {
		MsgInfo *msginfo = (MsgInfo *)cur->data;
		procmsg_write_flags(msginfo, fp);
	}

	if (item->mark_queue)
		procmsg_flush_mark_queue(item, fp);

	fclose(fp);
	item->mark_dirty = FALSE;
}

static gint cmp_by_item(gconstpointer a, gconstpointer b)
{
	const MsgInfo *msginfo1 = a;
	const MsgInfo *msginfo2 = b;

	if (msginfo1->folder == msginfo2->folder)
		return msginfo1->msgnum - msginfo2->msgnum;

	return msginfo1->folder - msginfo2->folder;
}

void procmsg_write_flags_for_multiple_folders(GSList *mlist)
{
	GSList *tmp_list, *cur;
	FolderItem *prev_item = NULL;
	FILE *fp = NULL;

	if (!mlist)
		return;

	tmp_list = g_slist_copy(mlist);
	tmp_list = g_slist_sort(tmp_list, cmp_by_item);

	for (cur = tmp_list; cur != NULL; cur = cur->next) {
		MsgInfo *msginfo = (MsgInfo *)cur->data;
		FolderItem *item = msginfo->folder;

		if (prev_item != item) {
			if (fp)
				fclose(fp);
			fp = procmsg_open_mark_file(item, DATA_APPEND);
			if (!fp) {
				g_warning("can't open mark file\n");
				break;
			}
			item->updated = TRUE;
		}
		procmsg_write_flags(msginfo, fp);
		prev_item = item;
	}

	if (fp)
		fclose(fp);
	g_slist_free(tmp_list);
}

void procmsg_flush_mark_queue(FolderItem *item, FILE *fp)
{
	MsgFlagInfo *flaginfo;
	MsgInfo msginfo = {0};
	gboolean append = FALSE;
	GSList *qlist, *cur;

	g_return_if_fail(item != NULL);

	if (!item->mark_queue)
		return;

	debug_print("flushing mark_queue: %s ...\n", item->path);

	if (!fp) {
		append =  TRUE;
		fp = procmsg_open_mark_file(item, DATA_APPEND);
		g_return_if_fail(fp != NULL);
	}

	qlist = g_slist_reverse(item->mark_queue);
	item->mark_queue = NULL;

	for (cur = qlist; cur != NULL; cur = cur->next) {
		flaginfo = (MsgFlagInfo *)cur->data;

		msginfo.msgnum = flaginfo->msgnum;
		msginfo.flags = flaginfo->flags;
		procmsg_write_flags(&msginfo, fp);
		g_free(flaginfo);
	}

	g_slist_free(qlist);

	if (append)
		fclose(fp);
}

void procmsg_add_mark_queue(FolderItem *item, gint num, MsgFlags flags)
{
	MsgFlagInfo *flaginfo;

	flaginfo = g_new(MsgFlagInfo, 1);
	flaginfo->msgnum = num;
	flaginfo->flags = flags;
	item->mark_queue = g_slist_prepend(item->mark_queue, flaginfo);
}

void procmsg_flaginfo_list_free(GSList *flaglist)
{
	GSList *cur;
	MsgFlagInfo *flaginfo;

	for (cur = flaglist; cur != NULL; cur = cur->next) {
		flaginfo = (MsgFlagInfo *)cur->data;
		g_free(flaginfo);
	}
	g_slist_free(flaglist);
}

void procmsg_flush_cache_queue(FolderItem *item, FILE *fp)
{
	MsgInfo *msginfo;
	gboolean append = FALSE;
	GSList *qlist, *cur;

	g_return_if_fail(item != NULL);

	if (!item->cache_queue)
		return;

	debug_print("flushing cache_queue: %s ...\n", item->path);

	if (!fp) {
		append =  TRUE;
		fp = procmsg_open_cache_file(item, DATA_APPEND);
		g_return_if_fail(fp != NULL);
	}

	qlist = g_slist_reverse(item->cache_queue);
	item->cache_queue = NULL;

	for (cur = qlist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;

		debug_print("flush cache queue: %s/%d\n",
			    item->path, msginfo->msgnum);
		procmsg_write_cache(msginfo, fp);
		procmsg_msginfo_free(msginfo);
	}

	g_slist_free(qlist);

	if (append)
		fclose(fp);
}

void procmsg_add_cache_queue(FolderItem *item, gint num, MsgInfo *msginfo)
{
	MsgInfo *queue_msginfo;

	g_return_if_fail(msginfo != NULL);

	queue_msginfo = procmsg_msginfo_copy(msginfo);
	queue_msginfo->msgnum = num;
	queue_msginfo->folder = item;
	if (queue_msginfo->file_path) {
		g_free(queue_msginfo->file_path);
		queue_msginfo->file_path = NULL;
	}

	debug_print("procmsg_add_cache_queue: add msg cache: %s/%d\n",
		    item->path, num);
	item->cache_queue = g_slist_prepend(item->cache_queue, queue_msginfo);
}

gboolean procmsg_flush_folder(FolderItem *item)
{
	gboolean flushed = FALSE;
	gint n_new, n_unread, n_total, n_min, n_max;

	g_return_val_if_fail(item != NULL, FALSE);
	g_return_val_if_fail(item->folder != NULL, FALSE);

	if (FOLDER_TYPE(item->folder) != F_MH || item->last_num < 0) {
		folder_item_scan(item);
		return TRUE;
	}

	if (item->mark_queue && !item->opened)
		flushed = TRUE;
	procmsg_get_mark_sum(item, &n_new, &n_unread, &n_total, &n_min, &n_max,
			     0);
	item->unmarked_num = 0;
	item->new = n_new;
	item->unread = n_unread;
	item->total = n_total;

	if (item->cache_queue && !item->opened) {
		procmsg_flush_cache_queue(item, NULL);
		flushed = TRUE;
	}

	if (flushed)
		debug_print("procmsg_flush_folder: flushed %s\n", item->path);

	return flushed;
}

static void procmsg_flush_folder_foreach_func(gpointer key, gpointer val,
					      gpointer data)
{
	procmsg_flush_folder(FOLDER_ITEM(key));
}

void procmsg_flush_folder_foreach(GHashTable *folder_table)
{
	g_hash_table_foreach(folder_table, procmsg_flush_folder_foreach_func,
			     NULL);
}

void procmsg_add_flags(FolderItem *item, gint num, MsgFlags flags)
{
	FILE *fp;
	MsgInfo msginfo;

	g_return_if_fail(item != NULL);

	if (item->opened) {
		procmsg_add_mark_queue(item, num, flags);
		return;
	}

	if ((fp = procmsg_open_mark_file(item, DATA_APPEND)) == NULL) {
		g_warning(_("can't open mark file\n"));
		return;
	}

	msginfo.msgnum = num;
	msginfo.flags = flags;

	procmsg_write_flags(&msginfo, fp);
	fclose(fp);
}

struct MarkSum {
	gint *new;
	gint *unread;
	gint *total;
	gint *min;
	gint *max;
	gint first;
};

static void mark_sum_func(gpointer key, gpointer value, gpointer data)
{
	MsgFlags *flags = value;
	gint num = GPOINTER_TO_INT(key);
	struct MarkSum *marksum = data;

	if (marksum->first <= num) {
		if (MSG_IS_NEW(*flags)) (*marksum->new)++;
		if (MSG_IS_UNREAD(*flags)) (*marksum->unread)++;
		if (num > *marksum->max) *marksum->max = num;
		if (num < *marksum->min || *marksum->min == 0) *marksum->min = num;
		(*marksum->total)++;
	}

	g_free(flags);
}

void procmsg_get_mark_sum(FolderItem *item,
			  gint *new, gint *unread, gint *total,
			  gint *min, gint *max,
			  gint first)
{
	GHashTable *mark_table;
	struct MarkSum marksum;

	*new = *unread = *total = *min = *max = 0;
	marksum.new    = new;
	marksum.unread = unread;
	marksum.total  = total;
	marksum.min    = min;
	marksum.max    = max;
	marksum.first  = first;

	mark_table = procmsg_read_mark_file(item);

	if (mark_table) {
		g_hash_table_foreach(mark_table, mark_sum_func, &marksum);
		g_hash_table_destroy(mark_table);
	}
}

static GHashTable *procmsg_read_mark_file(FolderItem *item)
{
	FILE *fp;
	GHashTable *mark_table = NULL;
	guint32 idata;
	guint num;
	MsgFlags *flags;
	MsgPermFlags perm_flags;
	GSList *cur;

	if ((fp = procmsg_open_mark_file(item, DATA_READ)) == NULL)
		return NULL;

	mark_table = g_hash_table_new(NULL, g_direct_equal);

	while (fread(&idata, sizeof(idata), 1, fp) == 1) {
		num = idata;
		if (fread(&idata, sizeof(idata), 1, fp) != 1) break;
		perm_flags = idata;

		flags = g_hash_table_lookup(mark_table, GUINT_TO_POINTER(num));
		if (flags != NULL)
			g_free(flags);

		flags = g_new0(MsgFlags, 1);
		flags->perm_flags = perm_flags;

		g_hash_table_insert(mark_table, GUINT_TO_POINTER(num), flags);
	}

	fclose(fp);

	if (item->mark_queue) {
		g_hash_table_foreach(mark_table, mark_unset_new_func, NULL);
		item->mark_dirty = TRUE;
	}

	for (cur = item->mark_queue; cur != NULL; cur = cur->next) {
		MsgFlagInfo *flaginfo = (MsgFlagInfo *)cur->data;

		flags = g_hash_table_lookup(mark_table,
					    GUINT_TO_POINTER(flaginfo->msgnum));
		if (flags != NULL)
			g_free(flags);

		flags = g_new0(MsgFlags, 1);
		flags->perm_flags = flaginfo->flags.perm_flags;

		g_hash_table_insert(mark_table,
				    GUINT_TO_POINTER(flaginfo->msgnum), flags);
				    
	}

	if (item->mark_queue && !item->opened) {
		procmsg_write_mark_file(item, mark_table);
		procmsg_flaginfo_list_free(item->mark_queue);
		item->mark_queue = NULL;
		item->mark_dirty = FALSE;
	}

	return mark_table;
}

static void write_mark_func(gpointer key, gpointer value, gpointer data)
{
	MsgInfo msginfo;

	msginfo.msgnum = GPOINTER_TO_UINT(key);
	msginfo.flags.perm_flags = ((MsgFlags *)value)->perm_flags;
	procmsg_write_flags(&msginfo, (FILE *)data);
}

static void procmsg_write_mark_file(FolderItem *item, GHashTable *mark_table)
{
	FILE *fp;

	if ((fp = procmsg_open_mark_file(item, DATA_WRITE)) == NULL) {
		g_warning("procmsg_write_mark_file: cannot open mark file.");
		return;
	}
	g_hash_table_foreach(mark_table, write_mark_func, fp);
	fclose(fp);
}

FILE *procmsg_open_data_file(const gchar *file, guint version,
			     DataOpenMode mode, gchar *buf, size_t buf_size)
{
	FILE *fp;
	guint32 data_ver = 0;

	g_return_val_if_fail(file != NULL, NULL);

	if (mode == DATA_WRITE) {
		if ((fp = g_fopen(file, "wb")) == NULL) {
			if (errno == EACCES) {
				change_file_mode_rw(NULL, file);
				if ((fp = g_fopen(file, "wb")) == NULL) {
					FILE_OP_ERROR(file, "procmsg_open_data_file: fopen");
					return NULL;
				}
			} else {
				FILE_OP_ERROR(file, "procmsg_open_data_file: fopen");
				return NULL;
			}
		}
		if (change_file_mode_rw(fp, file) < 0)
			FILE_OP_ERROR(file, "chmod");

		WRITE_CACHE_DATA_INT(version, fp);
		return fp;
	}

	/* check version */
	if ((fp = g_fopen(file, "rb")) == NULL) {
		if (errno == EACCES) {
			change_file_mode_rw(NULL, file);
			if ((fp = g_fopen(file, "rb")) == NULL) {
				FILE_OP_ERROR(file, "procmsg_open_data_file: fopen");
			}
		} else {
			debug_print("Mark/Cache file '%s' not found\n", file);
		}
	}

	if (fp) {
		if (buf && buf_size > 0)
			setvbuf(fp, buf, _IOFBF, buf_size);
		if (fread(&data_ver, sizeof(data_ver), 1, fp) != 1) {
			g_warning("%s: cannot read mark/cache file (truncated?)\n", file);
			fclose(fp);
			fp = NULL;
		} else if (version != data_ver) {
			g_message("%s: Mark/Cache version is different (%u != %u). Discarding it.\n",
				  file, data_ver, version);
			fclose(fp);
			fp = NULL;
		}
	}

	if (mode == DATA_READ)
		return fp;

	if (fp) {
		/* reopen with append mode */
		fclose(fp);
		if ((fp = g_fopen(file, "ab")) == NULL) {
			if (errno == EACCES) {
				change_file_mode_rw(NULL, file);
				if ((fp = g_fopen(file, "ab")) == NULL) {
					FILE_OP_ERROR(file, "procmsg_open_data_file: fopen");
				}
			} else {
				FILE_OP_ERROR(file, "procmsg_open_data_file: fopen");
			}
		}
	} else {
		/* open with overwrite mode if mark file doesn't exist or
		   version is different */
		fp = procmsg_open_data_file(file, version, DATA_WRITE, buf,
					    buf_size);
	}

	return fp;
}

static GMappedFile *procmsg_open_cache_file_mmap(FolderItem *item,
						 DataOpenMode mode)
{
	gchar *cachefile;
	GMappedFile *map = NULL;
	GError *error = NULL;
	gsize size;
	guint32 data_ver = 0;
	gchar *p;

	if (mode != DATA_READ)
		return NULL;

	cachefile = folder_item_get_cache_file(item);
	if (cachefile) {
		map = g_mapped_file_new(cachefile, FALSE, &error);
		if (!map) {
			if (error && error->code == G_FILE_ERROR_NOENT)
				debug_print("%s: mark/cache file not found\n", cachefile);
			else if (error)
				g_warning("%s: cannot open mark/cache file: %s", cachefile, error->message);
			else
				g_warning("%s: cannot open mark/cache file", cachefile);
			if (error)
				g_error_free(error);
			g_free(cachefile);
			return NULL;
		}
		size = g_mapped_file_get_length(map);
		if (size < sizeof(data_ver)) {
			g_warning("%s: cannot read mark/cache file (truncated?)", cachefile);
			g_mapped_file_free(map);
			g_free(cachefile);
			return NULL;
		}
		p = g_mapped_file_get_contents(map);
		data_ver = *(guint32 *)p;
		if (CACHE_VERSION != data_ver) {
			g_message("%s: Mark/Cache version is different (%u != %u). Discarding it.\n",
				  cachefile, data_ver, CACHE_VERSION);
			g_mapped_file_free(map);
			g_free(cachefile);
			return NULL;
		}
		g_free(cachefile);
	}

	return map;
}

FILE *procmsg_open_cache_file(FolderItem *item, DataOpenMode mode)
{
	gchar *cachefile;
	FILE *fp;

	cachefile = folder_item_get_cache_file(item);
	fp = procmsg_open_data_file(cachefile, CACHE_VERSION, mode, NULL, 0);
	g_free(cachefile);

	return fp;
}

FILE *procmsg_open_mark_file(FolderItem *item, DataOpenMode mode)
{
	gchar *markfile;
	FILE *fp;

	markfile = folder_item_get_mark_file(item);
	fp = procmsg_open_data_file(markfile, MARK_VERSION, mode, NULL, 0);
	g_free(markfile);

	return fp;
}

void procmsg_clear_cache(FolderItem *item)
{
	FILE *fp;

	fp = procmsg_open_cache_file(item, DATA_WRITE);
	if (fp)
		fclose(fp);
}

void procmsg_clear_mark(FolderItem *item)
{
	FILE *fp;

	fp = procmsg_open_mark_file(item, DATA_WRITE);
	if (fp)
		fclose(fp);
}

/* return the reversed thread tree */
GNode *procmsg_get_thread_tree(GSList *mlist)
{
	GNode *root, *parent, *node, *next;
	GHashTable *table;
	MsgInfo *msginfo;
	const gchar *msgid;
	GSList *reflist;

	root = g_node_new(NULL);
	table = g_hash_table_new(g_str_hash, g_str_equal);

	for (; mlist != NULL; mlist = mlist->next) {
		msginfo = (MsgInfo *)mlist->data;
		parent = root;

		/* only look for the real parent first */
		if (msginfo->inreplyto) {
			parent = g_hash_table_lookup(table, msginfo->inreplyto);
			if (parent == NULL)
				parent = root;
		}

		node = g_node_insert_data_before
			(parent, parent == root ? parent->children : NULL,
			 msginfo);
		if ((msgid = msginfo->msgid) &&
		    g_hash_table_lookup(table, msgid) == NULL)
			g_hash_table_insert(table, (gchar *)msgid, node);
	}

	/* complete the unfinished threads */
	for (node = root->children; node != NULL; ) {
		next = node->next;
		msginfo = (MsgInfo *)node->data;
		parent = NULL;

		if (msginfo->inreplyto)
			parent = g_hash_table_lookup(table, msginfo->inreplyto);

		/* try looking for the indirect parent */
		if (!parent && msginfo->references) {
			for (reflist = msginfo->references;
			     reflist != NULL; reflist = reflist->next)
				if ((parent = g_hash_table_lookup
					(table, reflist->data)) != NULL)
					break;
		}

		/* node should not be the parent, and node should not
		   be an ancestor of parent (circular reference) */
		if (parent && parent != node &&
		    !g_node_is_ancestor(node, parent)) {
			g_node_unlink(node);
			g_node_insert_before
				(parent, parent->children, node);
		}
		node = next;
	}

	g_hash_table_destroy(table);

	return root;
}

static gboolean procmsg_thread_date_func(GNode *node, gpointer data)
{
	guint *tdate = (guint *)data;
	MsgInfo *msginfo = (MsgInfo *)node->data;

	if (*tdate < msginfo->date_t)
		*tdate = msginfo->date_t;

	return FALSE;
}

guint procmsg_get_thread_date(GNode *node)
{
	guint tdate = 0;

	g_return_val_if_fail(node != NULL && node->parent != NULL &&
			     node->parent->parent == NULL, 0);

	g_node_traverse(node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			procmsg_thread_date_func, &tdate);

	return tdate;
}

gint procmsg_move_messages(GSList *mlist)
{
	GSList *cur, *movelist = NULL;
	MsgInfo *msginfo;
	FolderItem *dest = NULL;
	GHashTable *hash;
	gint val = 0;

	if (!mlist) return 0;

	hash = procmsg_to_folder_hash_table_create(mlist);
	folder_item_scan_foreach(hash);
	g_hash_table_destroy(hash);

	for (cur = mlist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;
		if (!dest) {
			dest = msginfo->to_folder;
			movelist = g_slist_append(movelist, msginfo);
		} else if (dest == msginfo->to_folder) {
			movelist = g_slist_append(movelist, msginfo);
		} else {
			val = folder_item_move_msgs(dest, movelist);
			g_slist_free(movelist);
			movelist = NULL;
			if (val == -1)
				return val;
			dest = msginfo->to_folder;
			movelist = g_slist_append(movelist, msginfo);
		}
	}

	if (movelist) {
		val = folder_item_move_msgs(dest, movelist);
		g_slist_free(movelist);
	}

	return val == -1 ? -1 : 0;
}

gint procmsg_copy_messages(GSList *mlist)
{
	GSList *cur, *copylist = NULL;
	MsgInfo *msginfo;
	FolderItem *dest = NULL;
	GHashTable *hash;
	gint val = 0;

	if (!mlist) return 0;

	hash = procmsg_to_folder_hash_table_create(mlist);
	folder_item_scan_foreach(hash);
	g_hash_table_destroy(hash);

	for (cur = mlist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;
		if (!dest) {
			dest = msginfo->to_folder;
			copylist = g_slist_append(copylist, msginfo);
		} else if (dest == msginfo->to_folder) {
			copylist = g_slist_append(copylist, msginfo);
		} else {
			val = folder_item_copy_msgs(dest, copylist);
			g_slist_free(copylist);
			copylist = NULL;
			if (val == -1)
				return val;
			dest = msginfo->to_folder;
			copylist = g_slist_append(copylist, msginfo);
		}
	}

	if (copylist) {
		val = folder_item_copy_msgs(dest, copylist);
		g_slist_free(copylist);
	}

	return val == -1 ? -1 : 0;
}

gint procmsg_add_messages_from_queue(FolderItem *dest, GSList *mlist,
				     gboolean is_move)
{
	MsgInfo *msginfo;
	GSList *cur;
	gchar *file;
	FILE *fp;
	gchar buf[BUFFSIZE];
	gchar *dest_file;
	gboolean is_error = FALSE;
	FolderItem *src;
	MsgFlags flags;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(mlist != NULL, -1);

	msginfo = (MsgInfo *)mlist->data;
	if (!msginfo || !msginfo->folder || msginfo->folder->stype != F_QUEUE ||
	    !MSG_IS_QUEUED(msginfo->flags) || dest->stype == F_QUEUE)
		return -1;

	debug_print("procmsg_add_messages_from_queue: adding messages from queue folder\n");

	for (cur = mlist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;
		flags = msginfo->flags;
		if (!MSG_IS_QUEUED(flags))
			return -1;
		MSG_UNSET_TMP_FLAGS(flags, MSG_QUEUED);
		src = msginfo->folder;
		file = procmsg_get_message_file(msginfo);
		if (!file)
			return -1;
		if ((fp = g_fopen(file, "rb")) == NULL) {
			FILE_OP_ERROR(file, "folder_item_move_msgs: fopen");
			g_free(file);
			return -1;
		}
		while (fgets(buf, sizeof(buf), fp) != NULL) {
			if (buf[0] == '\r' || buf[0] == '\n')
				break;
		}
		if (ferror(fp)) {
			fclose(fp);
			g_free(file);
			return -1;
		}

		dest_file = get_tmp_file();
		debug_print("copy queued msg: %s -> %s\n", file, dest_file);

		if (copy_file_part(fp, ftell(fp), G_MAXINT, dest_file) < 0) {
			fclose(fp);
			is_error = TRUE;
		} else {
			fclose(fp);
			if (folder_item_add_msg(dest, dest_file, &flags, TRUE) < 0) {
				g_unlink(dest_file);
				is_error = TRUE;
			} else if (is_move &&
				   folder_item_remove_msg(src, msginfo) < 0)
				is_error = TRUE;
		}

		g_free(dest_file);
		g_free(file);
		if (is_error)
			return -1;
	}

	return 0;
}

gchar *procmsg_get_message_file_path(MsgInfo *msginfo)
{
	gchar *path, *file;

	g_return_val_if_fail(msginfo != NULL, NULL);

	if (msginfo->encinfo && msginfo->encinfo->plaintext_file)
		file = g_strdup(msginfo->encinfo->plaintext_file);
	else if (msginfo->file_path)
		return g_strdup(msginfo->file_path);
	else {
		gchar nstr[16];
		path = folder_item_get_path(msginfo->folder);
		file = g_strconcat(path, G_DIR_SEPARATOR_S,
				   utos_buf(nstr, msginfo->msgnum), NULL);
		g_free(path);
	}

	return file;
}

gchar *procmsg_get_message_file(MsgInfo *msginfo)
{
	gchar *filename = NULL;

	g_return_val_if_fail(msginfo != NULL, NULL);

	if (msginfo->file_path)
		return g_strdup(msginfo->file_path);

	filename = folder_item_fetch_msg(msginfo->folder, msginfo->msgnum);
	if (!filename)
		debug_print(_("can't fetch message %d\n"), msginfo->msgnum);

	return filename;
}

GSList *procmsg_get_message_file_list(GSList *mlist)
{
	GSList *file_list = NULL;
	MsgInfo *msginfo;
	MsgFileInfo *fileinfo;
	gchar *file;

	while (mlist != NULL) {
		msginfo = (MsgInfo *)mlist->data;
		file = procmsg_get_message_file(msginfo);
		if (!file) {
			procmsg_message_file_list_free(file_list);
			return NULL;
		}
		fileinfo = g_new(MsgFileInfo, 1);
		fileinfo->file = file;
		fileinfo->flags = g_new(MsgFlags, 1);
		*fileinfo->flags = msginfo->flags;
		file_list = g_slist_prepend(file_list, fileinfo);
		mlist = mlist->next;
	}

	file_list = g_slist_reverse(file_list);

	return file_list;
}

void procmsg_message_file_list_free(GSList *file_list)
{
	GSList *cur;
	MsgFileInfo *fileinfo;

	for (cur = file_list; cur != NULL; cur = cur->next) {
		fileinfo = (MsgFileInfo *)cur->data;
		g_free(fileinfo->file);
		g_free(fileinfo->flags);
		g_free(fileinfo);
	}

	g_slist_free(file_list);
}

FILE *procmsg_open_message(MsgInfo *msginfo)
{
	FILE *fp;
	gchar *file;

	g_return_val_if_fail(msginfo != NULL, NULL);

	file = procmsg_get_message_file_path(msginfo);
	g_return_val_if_fail(file != NULL, NULL);

	if (!is_file_exist(file)) {
		g_free(file);
		file = procmsg_get_message_file(msginfo);
		if (!file)
			return NULL;
	}

	if ((fp = g_fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "procmsg_open_message: fopen");
		g_free(file);
		return NULL;
	}

	g_free(file);

	if (MSG_IS_QUEUED(msginfo->flags)) {
		gchar buf[BUFFSIZE];

		while (fgets(buf, sizeof(buf), fp) != NULL)
			if (buf[0] == '\r' || buf[0] == '\n') break;
	}

	return fp;
}

static DecryptMessageFunc decrypt_message_func = NULL;
static gboolean auto_decrypt = TRUE;

void procmsg_set_decrypt_message_func(DecryptMessageFunc func)
{
	decrypt_message_func = func;
}

void procmsg_set_auto_decrypt_message(gboolean enabled)
{
	auto_decrypt = enabled;
}

FILE *procmsg_open_message_decrypted(MsgInfo *msginfo, MimeInfo **mimeinfo)
{
	FILE *fp;

	if (decrypt_message_func && auto_decrypt)
		return decrypt_message_func(msginfo, mimeinfo);

	*mimeinfo = NULL;
	if ((fp = procmsg_open_message(msginfo)) == NULL)
		return NULL;
	*mimeinfo = procmime_scan_mime_header(fp);

	return fp;
}

gboolean procmsg_msg_exist(MsgInfo *msginfo)
{
	gchar *path;
	gboolean ret;

	if (!msginfo) return FALSE;

	path = folder_item_get_path(msginfo->folder);
	change_dir(path);
	ret = !folder_item_is_msg_changed(msginfo->folder, msginfo);
	g_free(path);

	return ret;
}

gboolean procmsg_trash_messages_exist(void)
{
	FolderItem *trash;
	GList *cur;

	for (cur = folder_get_list(); cur != NULL; cur = cur->next) {
		trash = FOLDER(cur->data)->trash;
		if (trash && trash->total > 0)
			return TRUE;
	}

	return FALSE;
}

void procmsg_empty_trash(FolderItem *trash)
{
	if (!trash)
		return;

	g_return_if_fail(trash->stype == F_TRASH || trash->stype == F_JUNK);

	if (trash->total > 0) {
		debug_print("Emptying messages in %s ...\n", trash->path);

		folder_item_remove_all_msg(trash);
		procmsg_clear_cache(trash);
		procmsg_clear_mark(trash);
		trash->cache_dirty = FALSE;
		trash->mark_dirty = FALSE;
	}
}

void procmsg_empty_all_trash(void)
{
	FolderItem *trash;
	GList *cur;

	for (cur = folder_get_list(); cur != NULL; cur = cur->next) {
		trash = FOLDER(cur->data)->trash;
		procmsg_empty_trash(trash);
	}
}

static gboolean remove_all_cached_messages_func(GNode *node, gpointer data)
{
	FolderItem *item;
	gchar *dir;

	g_return_val_if_fail(node->data != NULL, FALSE);

	item = FOLDER_ITEM(node->data);
	if (!item->path || item->stype == F_VIRTUAL)
		return FALSE;

	dir = folder_item_get_path(item);
	if (is_dir_exist(dir)) {
		debug_print("removing all cached messages in '%s' ...\n",
			    item->path);
		remove_all_numbered_files(dir);
	}
	g_free(dir);

	return FALSE;
}

void procmsg_remove_all_cached_messages(Folder *folder)
{
	g_return_if_fail(folder != NULL);
	g_return_if_fail(FOLDER_IS_REMOTE(folder));

	debug_print("Removing all caches in the mailbox '%s' ...\n",
		    folder->name);

	g_node_traverse(folder->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			remove_all_cached_messages_func, NULL);
}

gint procmsg_save_to_outbox(FolderItem *outbox, const gchar *file)
{
	gint num;
	MsgFlags flag = {0, 0};

	debug_print("saving sent message...\n");

	if (!outbox)
		outbox = folder_get_default_outbox();
	g_return_val_if_fail(outbox != NULL, -1);

	folder_item_scan(outbox);
	if ((num = folder_item_add_msg(outbox, file, &flag, FALSE)) < 0) {
		g_warning("can't save message\n");
		return -1;
	}
	procmsg_flush_folder(outbox);

	return 0;
}

static guint print_id = 0;

static gint print_command_exec(const gchar *file, const gchar *cmdline)
{
	static const gchar *def_cmd = "lpr %s";
	gchar buf[1024];

#ifdef G_OS_WIN32
	if (canonicalize_file_replace(file) < 0)
		return -1;
#endif

	if (cmdline && str_find_format_times(cmdline, 's') == 1)
		g_snprintf(buf, sizeof(buf) - 1, cmdline, file);
	else {
		if (cmdline) {
			g_warning(_("Print command line is invalid: `%s'\n"),
				  cmdline);
			return -1;
		}

#ifdef G_OS_WIN32
		execute_print_file(file);
		return 0;
#else
		g_snprintf(buf, sizeof(buf) - 1, def_cmd, file);
#endif
	}

	g_strchomp(buf);
	if (buf[strlen(buf) - 1] != '&')
		strcat(buf, "&");

	if (system(buf) != 0)
		return -1;

	return 0;
}

static void procmsg_write_headers(MsgInfo *msginfo, MimeInfo *partinfo,
				  FILE *fp, FILE *dest_fp,
				  const gchar *encoding, gboolean all_headers)
{
	GPtrArray *headers;
	gint i;

	if (all_headers)
		headers = procheader_get_header_array_asis(fp, NULL);
	else
		headers = procheader_get_header_array_for_display(fp, NULL);

	for (i = 0; i < headers->len; i++) {
		Header *hdr;
		gchar *file_str;
		const gchar *body;

		hdr = g_ptr_array_index(headers, i);

		if (partinfo) {
			if (!g_ascii_strcasecmp(hdr->name, "Subject") ||
			    !g_ascii_strcasecmp(hdr->name, "From") ||
			    !g_ascii_strcasecmp(hdr->name, "To") ||
			    !g_ascii_strcasecmp(hdr->name, "Cc")) {
				unfold_line(hdr->body);
			}

			body = hdr->body;
			while (g_ascii_isspace(*body))
				body++;
		} else {
			if (!g_ascii_strcasecmp(hdr->name, "Subject"))
				body = msginfo->subject;
			else if (!g_ascii_strcasecmp(hdr->name, "From"))
				body = msginfo->from;
			else if (!g_ascii_strcasecmp(hdr->name, "To"))
				body = msginfo->to;
			else if (!g_ascii_strcasecmp(hdr->name, "Cc")) {
				unfold_line(hdr->body);
				body = hdr->body;
				while (g_ascii_isspace(*body))
					body++;
			} else {
				body = hdr->body;
				while (g_ascii_isspace(*body))
					body++;
			}
		}

		if (body && *body != '\0') {
			file_str = conv_codeset_strdup
				(body, CS_INTERNAL, encoding);
			fprintf(dest_fp, "%s: %s\n", hdr->name,
				file_str ? file_str : body);
			g_free(file_str);
		} else {
			fprintf(dest_fp, "%s: (none)\n", hdr->name);
		}
	}

	procheader_header_array_destroy(headers);
}

void procmsg_print_message(MsgInfo *msginfo, const gchar *cmdline,
			   gboolean all_headers)
{
	gchar *prtmp;

	g_return_if_fail(msginfo != NULL);

	prtmp = g_strdup_printf("%s%cprinttmp-%08x.txt",
				get_mime_tmp_dir(), G_DIR_SEPARATOR,
				print_id++);

	if (procmsg_save_message_as_text(msginfo, prtmp,
					 conv_get_locale_charset_str(),
					 all_headers) == 0)
		print_command_exec(prtmp, cmdline);

	g_free(prtmp);
}

void procmsg_print_message_part(MsgInfo *msginfo, MimeInfo *partinfo,
				const gchar *cmdline, gboolean all_headers)
{
	FILE *msgfp, *tmpfp, *prfp;
	gchar *prtmp;
	gchar buf[BUFFSIZE];

	if ((msgfp = procmsg_open_message(msginfo)) == NULL) {
		return;
	}

	if ((tmpfp = procmime_get_text_content
		(partinfo, msgfp, conv_get_locale_charset_str())) == NULL) {
		fclose(msgfp);
		return;
	}
	fclose(msgfp);

	prtmp = g_strdup_printf("%s%cprinttmp-%08x.txt",
				get_mime_tmp_dir(), G_DIR_SEPARATOR,
				print_id++);
	if ((prfp = g_fopen(prtmp, "w")) == NULL) {
		FILE_OP_ERROR(prtmp, "procmsg_print_message_part: fopen");
		g_free(prtmp);
		fclose(tmpfp);
		return;
	}

	while (fgets(buf, sizeof(buf), tmpfp) != NULL)
		fputs(buf, prfp);

	fclose(prfp);
	fclose(tmpfp);

	print_command_exec(prtmp, cmdline);

	g_free(prtmp);
}

gint procmsg_save_message_as_text(MsgInfo *msginfo, const gchar *dest,
				  const gchar *encoding, gboolean all_headers)
{
	MimeInfo *mimeinfo, *partinfo;
	FILE *fp;
	FILE *tmpfp;
	FILE *destfp;
	gchar buf[BUFFSIZE];
	gchar *part_str;
	gint ret = 0;

	g_return_val_if_fail(msginfo != NULL, -1);
	g_return_val_if_fail(dest != NULL, -1);

	mimeinfo = procmime_scan_message(msginfo);
	if (!mimeinfo)
		return -1;
	if ((fp = procmsg_open_message(msginfo)) == NULL) {
		procmime_mimeinfo_free_all(mimeinfo);
		return -1;
	}
	if ((destfp = g_fopen(dest, "w")) == NULL) {
		fclose(fp);
		procmime_mimeinfo_free_all(mimeinfo);
		return -1;
	}
	procmsg_write_headers(msginfo, mimeinfo, fp, destfp, encoding, all_headers);
	fputc('\n', destfp);

	partinfo = mimeinfo;

	while (partinfo != NULL) {
		if (fseek(fp, partinfo->fpos, SEEK_SET) < 0)
			break;

		if (partinfo->filename || partinfo->name)
			g_snprintf(buf, sizeof(buf), "\n[%s  %s (%s)]\n",
				   partinfo->filename ? partinfo->filename :
				   partinfo->name,
				   partinfo->content_type,
				   to_human_readable(partinfo->content_size));
		else
			g_snprintf(buf, sizeof(buf), "\n[%s (%s)]\n",
				   partinfo->content_type,
				   to_human_readable(partinfo->content_size));
		part_str = conv_codeset_strdup(buf, CS_INTERNAL, encoding);
		if (!part_str)
			part_str = g_strdup(buf);

		if (partinfo->mime_type == MIME_TEXT ||
		    partinfo->mime_type == MIME_TEXT_HTML) {
			if (!partinfo->main &&
			    partinfo->parent &&
			    partinfo->parent->children != partinfo) {
				fputs(part_str, destfp);
			}

			if ((tmpfp = procmime_get_text_content(partinfo, fp, encoding)) == NULL) {
				g_free(part_str);
				break;
			}
			while (fgets(buf, sizeof(buf), tmpfp) != NULL)
				fputs(buf, destfp);

			fclose(tmpfp);
		} else if (partinfo->mime_type == MIME_MESSAGE_RFC822) {
			fputs(part_str, destfp);
			while (fgets(buf, sizeof(buf), fp) != NULL)
				if (buf[0] == '\r' || buf[0] == '\n') break;
			procmsg_write_headers(msginfo, partinfo, fp, destfp, encoding, all_headers);
			fputc('\n', destfp);
		} else if (partinfo->mime_type != MIME_MULTIPART) {
			fputs(part_str, destfp);
		}

		g_free(part_str);

		if (partinfo->parent && partinfo->parent->content_type &&
		    !g_ascii_strcasecmp(partinfo->parent->content_type,
					"multipart/alternative"))
			partinfo = partinfo->parent->next;
		else
			partinfo = procmime_mimeinfo_next(partinfo);
	}

	if (fclose(destfp) == EOF) {
		FILE_OP_ERROR(dest, "fclose");
		g_unlink(dest);
		ret = -1;
	}

	fclose(fp);
	procmime_mimeinfo_free_all(mimeinfo);

	return ret;
}

/**
 * procmsg_concat_partial_messages:
 * @mlist: list of MsgInfo* including message/partial messages.
 * @file: output file name of concatenated message.
 *
 * Concatenate @mlist which consists of message/partial messages and
 * output to @file. If @mlist has different partial id, the first one
 * is used.
 *
 * Return value: 0 on success, or -1 if failed.
 **/
gint procmsg_concat_partial_messages(GSList *mlist, const gchar *file)
{
	static HeaderEntry hentry[] = {{"Content-Type:", NULL, FALSE},
				       {NULL, NULL, FALSE}};
	FILE *fp;
	gchar buf[BUFFSIZE];
	FILE *tmp_fp;
	gchar *part_id = NULL;
	gint total = 0;
	MsgInfo *msg_array[100] = {NULL};
	MsgInfo *msginfo;
	MimeInfo *mimeinfo;
	GSList *cur;
	gint i;

	g_return_val_if_fail(mlist != NULL, -1);
	g_return_val_if_fail(file != NULL, -1);

	debug_print("procmsg_concat_partial_messages\n");

	for (cur = mlist; cur != NULL; cur = cur->next) {
		gint n = 0;
		gint t = 0;
		gchar *cur_id = NULL;

		msginfo = (MsgInfo *)cur->data;

		fp = procmsg_open_message_decrypted(msginfo, &mimeinfo);
		if (!fp)
			continue;
		if (!mimeinfo->content_type ||
		    g_ascii_strcasecmp(mimeinfo->content_type, "message/partial") != 0)
			goto skip;

		rewind(fp);
		if (procheader_get_one_field(buf, sizeof(buf), fp, hentry) == -1)
			goto skip;

		procmime_scan_content_type_partial(buf + strlen(hentry[0].name),
						   &t, &cur_id, &n);
		if (n == 0 || n > 100 || t > 100 || (t > 0 && n > t)) {
			debug_print("bad partial number (%d/%d), skip\n", n, t);
			g_free(cur_id);
			goto skip;
		}

		debug_print("partial: %d/%d id=%s\n", n, t, cur_id);
		if (!part_id)
			part_id = g_strdup(cur_id);
		if (total == 0)
			total = t;

		if ((t > 0 && total != t) || (total > 0 && n > total) ||
		    strcmp(part_id, cur_id) != 0) {
			debug_print("skip\n");
			g_free(cur_id);
			goto skip;
		}

		msg_array[n - 1] = msginfo;

		g_free(cur_id);
skip:
		procmime_mimeinfo_free_all(mimeinfo);
		fclose(fp);
	}

	if (!part_id) {
		debug_print("piece not found\n");
		return -1;
	}

	debug_print("part_id = %s , total = %d\n", part_id, total);
	g_free(part_id);

	if (total == 0) {
		debug_print("total number not found\n");
		return -1;
	}

	/* check if all pieces exist */
	for (i = 0; i < total; i++) {
		if (msg_array[i] == NULL) {
			debug_print("message part %d not exist\n", i + 1);
			return -1;
		}
	}

	/* concatenate parts */
	if ((tmp_fp = g_fopen(file, "wb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return -1;
	}

	for (i = 0; i < total; i++) {
		msginfo = msg_array[i];
		off_t out_size;
		gint empty_line_size = 0;

		fp = procmsg_open_message_decrypted(msginfo, &mimeinfo);
		if (!fp) {
			g_warning("cannot open message part %d\n", i + 1);
			fclose(tmp_fp);
			g_unlink(file);
			return -1;
		}

		/* write out first headers */
		if (i == 0) {
			rewind(fp);
			while (procheader_get_one_field(buf, sizeof(buf), fp, NULL) != -1) {
				if (!g_ascii_strncasecmp(buf, "Content-", 8) ||
				    !g_ascii_strncasecmp(buf, "Subject", 7) ||
				    !g_ascii_strncasecmp(buf, "Message-ID", 10) ||
				    !g_ascii_strncasecmp(buf, "Encrypted", 9) ||
				    !g_ascii_strncasecmp(buf, "MIME-Version", 12))
					continue;
				fputs(buf, tmp_fp);
				fputs("\n", tmp_fp);
			}

			while (procheader_get_one_field(buf, sizeof(buf), fp, NULL) != -1) {
				if (!g_ascii_strncasecmp(buf, "Content-", 8) ||
				    !g_ascii_strncasecmp(buf, "Subject", 7) ||
				    !g_ascii_strncasecmp(buf, "Message-ID", 10) ||
				    !g_ascii_strncasecmp(buf, "Encrypted", 9) ||
				    !g_ascii_strncasecmp(buf, "MIME-Version", 12)) {
					fputs(buf, tmp_fp);
					fputs("\n", tmp_fp);
				}
			}

			/* header-body separator */
			fputs("\n", tmp_fp);
		}

		out_size = get_left_file_size(fp);
		if (out_size < 0) {
			g_warning("cannot tell left file size of part %d\n", i + 1);
			procmime_mimeinfo_free_all(mimeinfo);
			fclose(fp);
			fclose(tmp_fp);
			g_unlink(file);
			return -1;
		}
		empty_line_size = get_last_empty_line_size(fp, out_size);
		if (empty_line_size < 0) {
			g_warning("cannot get last empty line size of part %d\n", i + 1);
			procmime_mimeinfo_free_all(mimeinfo);
			fclose(fp);
			fclose(tmp_fp);
			g_unlink(file);
			return -1;
		}

		if (append_file_part(fp, ftell(fp), out_size - empty_line_size,
				     tmp_fp) < 0) {
			g_warning("write failed\n");
			procmime_mimeinfo_free_all(mimeinfo);
			fclose(fp);
			fclose(tmp_fp);
			g_unlink(file);
			return -1;
		}

		procmime_mimeinfo_free_all(mimeinfo);
		fclose(fp);
	}

	fclose(tmp_fp);

	return 0;
}

static gboolean procmsg_get_flags(FolderItem *item, gint num,
				  MsgPermFlags *flags)
{
	FILE *fp;
	guint32 idata;
	gint read_num;
	MsgPermFlags perm_flags;
	gboolean found = FALSE;
	GSList *cur;

	if ((fp = procmsg_open_mark_file(item, DATA_READ)) == NULL)
		return FALSE;

	while (fread(&idata, sizeof(idata), 1, fp) == 1) {
		read_num = idata;
		if (fread(&idata, sizeof(idata), 1, fp) != 1)
			break;
		perm_flags = idata;
		if (read_num == num) {
			*flags = perm_flags;
			found = TRUE;
			break;
		}
	}

	fclose(fp);
	if (found)
		return TRUE;

	for (cur = item->mark_queue; cur != NULL; cur = cur->next) {
		MsgFlagInfo *flaginfo = (MsgFlagInfo *)cur->data;

		if (flaginfo->msgnum == num) {
			*flags = flaginfo->flags.perm_flags;
			found = TRUE;
			break;
		}
	}

	return found;
}

MsgInfo *procmsg_get_msginfo(FolderItem *item, gint num)
{
	MsgInfo *msginfo;
	FolderType type;

	g_return_val_if_fail(item->folder != NULL, NULL);

	msginfo = folder_item_get_msginfo(item, num);
	if (!msginfo)
		return NULL;

	type = FOLDER_TYPE(item->folder);
	if (type == F_MH || type == F_IMAP) {
		if (item->stype == F_QUEUE) {
			MSG_SET_TMP_FLAGS(msginfo->flags, MSG_QUEUED);
		} else if (item->stype == F_DRAFT) {
			MSG_SET_TMP_FLAGS(msginfo->flags, MSG_DRAFT);
		}
	}
	if (type == F_IMAP) {
		MSG_SET_TMP_FLAGS(msginfo->flags, MSG_IMAP);
	} else if (type == F_NEWS) {
		MSG_SET_TMP_FLAGS(msginfo->flags, MSG_NEWS);
	}

	if (type == F_MH || type == F_NEWS) {
		MsgPermFlags flags = 0;
		if (procmsg_get_flags(item, num, &flags))
			msginfo->flags.perm_flags = flags;
	}

	return msginfo;
}

MsgInfo *procmsg_msginfo_copy(MsgInfo *msginfo)
{
	MsgInfo *newmsginfo;

	if (msginfo == NULL) return NULL;

	newmsginfo = g_new0(MsgInfo, 1);

#define MEMBCOPY(mmb)	newmsginfo->mmb = msginfo->mmb
#define MEMBDUP(mmb)	newmsginfo->mmb = msginfo->mmb ? \
			g_strdup(msginfo->mmb) : NULL

	MEMBCOPY(msgnum);
	MEMBCOPY(size);
	MEMBCOPY(mtime);
	MEMBCOPY(date_t);

	MEMBCOPY(flags);

	MEMBDUP(fromname);

	MEMBDUP(date);
	MEMBDUP(from);
	MEMBDUP(to);
	MEMBDUP(cc);
	MEMBDUP(newsgroups);
	MEMBDUP(subject);
	MEMBDUP(msgid);
	MEMBDUP(inreplyto);

	MEMBCOPY(folder);
	MEMBCOPY(to_folder);

	MEMBDUP(xface);

	MEMBDUP(file_path);

	if (msginfo->encinfo) {
		newmsginfo->encinfo = g_new0(MsgEncryptInfo, 1);
		MEMBDUP(encinfo->plaintext_file);
		MEMBDUP(encinfo->sigstatus);
		MEMBDUP(encinfo->sigstatus_full);
		MEMBCOPY(encinfo->decryption_failed);
	}

	return newmsginfo;
}

MsgInfo *procmsg_msginfo_get_full_info(MsgInfo *msginfo)
{
	MsgInfo *full_msginfo;
	gchar *file;

	if (msginfo == NULL) return NULL;

	file = procmsg_get_message_file(msginfo);
	if (!file) {
		g_warning("procmsg_msginfo_get_full_info(): can't get message file.\n");
		return NULL;
	}

	full_msginfo = procheader_parse_file(file, msginfo->flags, TRUE);
	g_free(file);
	if (!full_msginfo) return NULL;

	full_msginfo->msgnum = msginfo->msgnum;
	full_msginfo->size = msginfo->size;
	full_msginfo->mtime = msginfo->mtime;
	full_msginfo->folder = msginfo->folder;
	full_msginfo->to_folder = msginfo->to_folder;

	full_msginfo->file_path = g_strdup(msginfo->file_path);

	if (msginfo->encinfo) {
		full_msginfo->encinfo = g_new0(MsgEncryptInfo, 1);
		full_msginfo->encinfo->plaintext_file =
			g_strdup(msginfo->encinfo->plaintext_file);
		full_msginfo->encinfo->sigstatus =
			g_strdup(msginfo->encinfo->sigstatus);
		full_msginfo->encinfo->sigstatus_full =
			g_strdup(msginfo->encinfo->sigstatus_full);
		full_msginfo->encinfo->decryption_failed =
			msginfo->encinfo->decryption_failed;
	}

	return full_msginfo;
}

gboolean procmsg_msginfo_equal(MsgInfo *msginfo_a, MsgInfo *msginfo_b)
{
	if (!msginfo_a || !msginfo_b)
		return FALSE;

	if (msginfo_a == msginfo_b)
		return TRUE;

	if (msginfo_a->folder == msginfo_b->folder &&
	    msginfo_a->msgnum == msginfo_b->msgnum &&
	    msginfo_a->size   == msginfo_b->size   &&
	    msginfo_a->mtime  == msginfo_b->mtime)
		return TRUE;

	return FALSE;
}

void procmsg_msginfo_free(MsgInfo *msginfo)
{
	if (msginfo == NULL) return;

	g_free(msginfo->xface);

	g_free(msginfo->fromname);

	g_free(msginfo->date);
	g_free(msginfo->from);
	g_free(msginfo->to);
	g_free(msginfo->cc);
	g_free(msginfo->newsgroups);
	g_free(msginfo->subject);
	g_free(msginfo->msgid);
	g_free(msginfo->inreplyto);

	slist_free_strings(msginfo->references);
	g_slist_free(msginfo->references);

	g_free(msginfo->file_path);

	if (msginfo->encinfo) {
		g_free(msginfo->encinfo->plaintext_file);
		g_free(msginfo->encinfo->sigstatus);
		g_free(msginfo->encinfo->sigstatus_full);
		g_free(msginfo->encinfo);
	}

	g_free(msginfo);
}

gint procmsg_cmp_msgnum_for_sort(gconstpointer a, gconstpointer b)
{
	const MsgInfo *msginfo1 = a;
	const MsgInfo *msginfo2 = b;

	if (!msginfo1 || !msginfo2)
		return 0;

	return msginfo1->msgnum - msginfo2->msgnum;
}

#define CMP_FUNC_DEF(func_name, val)					\
static gint func_name(gconstpointer a, gconstpointer b)			\
{									\
	const MsgInfo *msginfo1 = a;					\
	const MsgInfo *msginfo2 = b;					\
	gint ret;							\
									\
	if (!msginfo1 || !msginfo2)					\
		return 0;						\
									\
	ret = (val);							\
	if (ret == 0)							\
		ret = msginfo1->date_t - msginfo2->date_t;		\
									\
	return ret * (cmp_func_sort_type == SORT_ASCENDING ? 1 : -1);	\
}

CMP_FUNC_DEF(procmsg_cmp_by_mark,
	     MSG_IS_MARKED(msginfo1->flags) - MSG_IS_MARKED(msginfo2->flags))
CMP_FUNC_DEF(procmsg_cmp_by_unread,
	     MSG_IS_UNREAD(msginfo1->flags) - MSG_IS_UNREAD(msginfo2->flags))
CMP_FUNC_DEF(procmsg_cmp_by_mime,
	     MSG_IS_MIME(msginfo1->flags) - MSG_IS_MIME(msginfo2->flags))
CMP_FUNC_DEF(procmsg_cmp_by_label,
	     MSG_GET_COLORLABEL(msginfo1->flags) -
	     MSG_GET_COLORLABEL(msginfo2->flags))
CMP_FUNC_DEF(procmsg_cmp_by_size, msginfo1->size - msginfo2->size)

#undef CMP_FUNC_DEF
#define CMP_FUNC_DEF(func_name, val)					\
static gint func_name(gconstpointer a, gconstpointer b)			\
{									\
	const MsgInfo *msginfo1 = a;					\
	const MsgInfo *msginfo2 = b;					\
									\
	if (!msginfo1 || !msginfo2)					\
		return 0;						\
									\
	return (val) * (cmp_func_sort_type == SORT_ASCENDING ? 1 : -1);	\
}

CMP_FUNC_DEF(procmsg_cmp_by_number, msginfo1->msgnum - msginfo2->msgnum)
CMP_FUNC_DEF(procmsg_cmp_by_date, msginfo1->date_t - msginfo2->date_t)

#undef CMP_FUNC_DEF
#define CMP_FUNC_DEF(func_name, var_name)				\
static gint func_name(gconstpointer a, gconstpointer b)			\
{									\
	const MsgInfo *msginfo1 = a;					\
	const MsgInfo *msginfo2 = b;					\
	gint ret;							\
									\
	if (!msginfo1->var_name)					\
		return (msginfo2->var_name != NULL) *			\
			(cmp_func_sort_type == SORT_ASCENDING ? -1 : 1);\
	if (!msginfo2->var_name)					\
		return (cmp_func_sort_type == SORT_ASCENDING ? 1 : -1);	\
									\
	ret = g_ascii_strcasecmp					\
		(msginfo1->var_name, msginfo2->var_name);		\
	if (ret == 0)							\
		ret = msginfo1->date_t - msginfo2->date_t;		\
									\
	return ret * (cmp_func_sort_type == SORT_ASCENDING ? 1 : -1);	\
}

CMP_FUNC_DEF(procmsg_cmp_by_from, fromname)
CMP_FUNC_DEF(procmsg_cmp_by_to, to)

#undef CMP_FUNC_DEF

static gint procmsg_cmp_by_subject(gconstpointer a, gconstpointer b)
{
	const MsgInfo *msginfo1 = a;
	const MsgInfo *msginfo2 = b;
	gint ret;

	if (!msginfo1->subject)
		return (msginfo2->subject != NULL) *
			(cmp_func_sort_type == SORT_ASCENDING ? -1 : 1);
	if (!msginfo2->subject)
		return (cmp_func_sort_type == SORT_ASCENDING ? 1 : -1);

	ret = subject_compare_for_sort(msginfo1->subject, msginfo2->subject);
	if (ret == 0)
		ret = msginfo1->date_t - msginfo2->date_t;

	return ret * (cmp_func_sort_type == SORT_ASCENDING ? 1 : -1);
}
