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

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "main.h"
#include "utils.h"
#include "procmsg.h"
#include "procheader.h"
#include "account.h"
#include "send_message.h"
#include "procmime.h"
#include "statusbar.h"
#include "prefs_common.h"
#include "prefs_filter.h"
#include "filter.h"
#include "folder.h"
#include "codeconv.h"
#if USE_GPGME
#  include "rfc2015.h"
#endif

static void mark_sum_func			(gpointer	 key,
						 gpointer	 value,
						 gpointer	 data);

static GHashTable *procmsg_read_mark_file	(FolderItem	*item);
static void procmsg_write_mark_file		(FolderItem	*item,
						 GHashTable	*mark_table);

static FILE *procmsg_open_data_file		(const gchar	*file,
						 guint		 version,
						 DataOpenMode	 mode,
						 gchar		*buf,
						 size_t		 buf_size);
static FILE *procmsg_open_cache_file_with_buffer(FolderItem	*item,
						 DataOpenMode	 mode,
						 gchar		*buf,
						 size_t		 buf_size);

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

static gint procmsg_read_cache_data_str(FILE *fp, gchar **str)
{
	gchar buf[BUFFSIZE];
	gint ret = 0;
	guint32 len;

	if (fread(&len, sizeof(len), 1, fp) == 1) {
		if (len > G_MAXINT)
			ret = -1;
		else {
			gchar *tmp = NULL;

			while (len > 0) {
				size_t size = MIN(len, BUFFSIZE - 1);

				if (fread(buf, size, 1, fp) != 1) {
					ret = -1;
					if (tmp) g_free(tmp);
					*str = NULL;
					break;
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
		}
	} else
		ret = -1;

	if (ret < 0)
		g_warning("Cache data is corrupted\n");

	return ret;
}

#define READ_CACHE_DATA(data, fp)				\
{								\
	if (procmsg_read_cache_data_str(fp, &data) < 0) {	\
		procmsg_msginfo_free(msginfo);			\
		procmsg_msg_list_free(mlist);			\
		mlist = NULL;					\
		break;						\
	}							\
}

#define READ_CACHE_DATA_INT(n, fp)				\
{								\
	guint32 idata;						\
								\
	if (fread(&idata, sizeof(idata), 1, fp) != 1) {		\
		g_warning("Cache data is corrupted\n");		\
		procmsg_msginfo_free(msginfo);			\
		procmsg_msg_list_free(mlist);			\
		mlist = NULL;					\
		break;						\
	} else							\
		n = idata;					\
}

GSList *procmsg_read_cache(FolderItem *item, gboolean scan_file)
{
	GSList *mlist = NULL;
	GSList *last = NULL;
	FILE *fp;
	MsgInfo *msginfo;
	MsgFlags default_flags;
	gchar file_buf[BUFFSIZE];
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

	if ((fp = procmsg_open_cache_file_with_buffer
		(item, DATA_READ, file_buf, sizeof(file_buf))) == NULL) {
		item->cache_dirty = TRUE;
		return NULL;
	}

	debug_print("Reading summary cache...");

	while (fread(&num, sizeof(num), 1, fp) == 1) {
		msginfo = g_new0(MsgInfo, 1);
		msginfo->msgnum = num;
		READ_CACHE_DATA_INT(msginfo->size, fp);
		READ_CACHE_DATA_INT(msginfo->mtime, fp);
		READ_CACHE_DATA_INT(msginfo->date_t, fp);
		READ_CACHE_DATA_INT(msginfo->flags.tmp_flags, fp);

		READ_CACHE_DATA(msginfo->fromname, fp);

		READ_CACHE_DATA(msginfo->date, fp);
		READ_CACHE_DATA(msginfo->from, fp);
		READ_CACHE_DATA(msginfo->to, fp);
		READ_CACHE_DATA(msginfo->newsgroups, fp);
		READ_CACHE_DATA(msginfo->subject, fp);
		READ_CACHE_DATA(msginfo->msgid, fp);
		READ_CACHE_DATA(msginfo->inreplyto, fp);

		READ_CACHE_DATA_INT(refnum, fp);
		for (; refnum != 0; refnum--) {
			gchar *ref;

			READ_CACHE_DATA(ref, fp);
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
		     folder_item_is_msg_changed(item, msginfo)) || num == 0) {
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

	fclose(fp);

	debug_print("done.\n");

	return mlist;
}

#undef READ_CACHE_DATA
#undef READ_CACHE_DATA_INT

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

void procmsg_flush_mark_queue(FolderItem *item, FILE *fp)
{
	MsgInfo *flaginfo;

	g_return_if_fail(item != NULL);
	g_return_if_fail(fp != NULL);

	if (item->mark_queue)
		debug_print("flushing mark_queue...\n");

	while (item->mark_queue != NULL) {
		flaginfo = (MsgInfo *)item->mark_queue->data;
		procmsg_write_flags(flaginfo, fp);
		procmsg_msginfo_free(flaginfo);
		item->mark_queue = g_slist_remove(item->mark_queue, flaginfo);
	}
}

void procmsg_add_mark_queue(FolderItem *item, gint num, MsgFlags flags)
{
	MsgInfo *queue_msginfo;

	queue_msginfo = g_new0(MsgInfo, 1);
	queue_msginfo->msgnum = num;
	queue_msginfo->flags = flags;
	item->mark_queue = g_slist_append
		(item->mark_queue, queue_msginfo);
	return;
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
		MsgInfo *msginfo = (MsgInfo *)cur->data;

		flags = g_hash_table_lookup(mark_table,
					    GUINT_TO_POINTER(msginfo->msgnum));
		if (flags != NULL)
			g_free(flags);

		flags = g_new0(MsgFlags, 1);
		flags->perm_flags = msginfo->flags.perm_flags;

		g_hash_table_insert(mark_table,
				    GUINT_TO_POINTER(msginfo->msgnum), flags);
				    
	}

	if (item->mark_queue && !item->opened) {
		procmsg_write_mark_file(item, mark_table);
		procmsg_msg_list_free(item->mark_queue);
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

	fp = procmsg_open_mark_file(item, DATA_WRITE);
	g_hash_table_foreach(mark_table, write_mark_func, fp);
	fclose(fp);
}

static FILE *procmsg_open_data_file(const gchar *file, guint version,
				    DataOpenMode mode,
				    gchar *buf, size_t buf_size)
{
	FILE *fp;
	guint32 data_ver;

	g_return_val_if_fail(file != NULL, NULL);

	if (mode == DATA_WRITE) {
		if ((fp = fopen(file, "wb")) == NULL) {
			FILE_OP_ERROR(file, "fopen");
			return NULL;
		}
		if (change_file_mode_rw(fp, file) < 0)
			FILE_OP_ERROR(file, "chmod");

		WRITE_CACHE_DATA_INT(version, fp);
		return fp;
	}

	/* check version */
	if ((fp = fopen(file, "rb")) == NULL)
		debug_print("Mark/Cache file '%s' not found\n", file);
	else {
		if (buf && buf_size > 0)
			setvbuf(fp, buf, _IOFBF, buf_size);
		if (fread(&data_ver, sizeof(data_ver), 1, fp) != 1 ||
		    version != data_ver) {
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
		if ((fp = fopen(file, "ab")) == NULL)
			FILE_OP_ERROR(file, "fopen");
	} else {
		/* open with overwrite mode if mark file doesn't exist or
		   version is different */
		fp = procmsg_open_data_file(file, version, DATA_WRITE, buf,
					    buf_size);
	}

	return fp;
}

static FILE *procmsg_open_cache_file_with_buffer(FolderItem *item,
						 DataOpenMode mode,
						 gchar *buf, size_t buf_size)
{
	gchar *cachefile;
	FILE *fp;

	cachefile = folder_item_get_cache_file(item);
	fp = procmsg_open_data_file(cachefile, CACHE_VERSION, mode, buf,
				    buf_size);
	g_free(cachefile);

	return fp;
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

gchar *procmsg_get_message_file_path(MsgInfo *msginfo)
{
	gchar *path, *file;

	g_return_val_if_fail(msginfo != NULL, NULL);

	if (msginfo->plaintext_file)
		file = g_strdup(msginfo->plaintext_file);
	else if (msginfo->file_path)
		return g_strdup(msginfo->file_path);
	else {
		path = folder_item_get_path(msginfo->folder);
		file = g_strconcat(path, G_DIR_SEPARATOR_S,
				   itos(msginfo->msgnum), NULL);
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

	if ((fp = fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
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

#if USE_GPGME
FILE *procmsg_open_message_decrypted(MsgInfo *msginfo, MimeInfo **mimeinfo)
{
	FILE *fp;
	MimeInfo *mimeinfo_;
	glong fpos;

	g_return_val_if_fail(msginfo != NULL, NULL);

	if (mimeinfo) *mimeinfo = NULL;

	if ((fp = procmsg_open_message(msginfo)) == NULL) return NULL;

	mimeinfo_ = procmime_scan_mime_header(fp);
	if (!mimeinfo_) {
		fclose(fp);
		return NULL;
	}

	if (!MSG_IS_ENCRYPTED(msginfo->flags) &&
	    rfc2015_is_encrypted(mimeinfo_)) {
		MSG_SET_TMP_FLAGS(msginfo->flags, MSG_ENCRYPTED);
	}

	if (MSG_IS_ENCRYPTED(msginfo->flags) &&
	    !msginfo->plaintext_file &&
	    !msginfo->decryption_failed) {
		fpos = ftell(fp);
		rfc2015_decrypt_message(msginfo, mimeinfo_, fp);
		if (msginfo->plaintext_file &&
		    !msginfo->decryption_failed) {
			fclose(fp);
			procmime_mimeinfo_free_all(mimeinfo_);
			if ((fp = procmsg_open_message(msginfo)) == NULL)
				return NULL;
			mimeinfo_ = procmime_scan_mime_header(fp);
			if (!mimeinfo_) {
				fclose(fp);
				return NULL;
			}
		} else {
			if (fseek(fp, fpos, SEEK_SET) < 0)
				perror("fseek");
		}
	}

	if (mimeinfo) *mimeinfo = mimeinfo_;
	return fp;
}
#endif

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

void procmsg_get_filter_keyword(MsgInfo *msginfo, gchar **header, gchar **key,
				PrefsFilterType type)
{
	static HeaderEntry hentry[] = {{"List-Id:",	   NULL, TRUE},
				       {"X-ML-Name:",	   NULL, TRUE},
				       {"X-List:",	   NULL, TRUE},
				       {"X-Mailing-list:", NULL, TRUE},
				       {"X-Sequence:",	   NULL, TRUE},
				       {NULL,		   NULL, FALSE}};
	enum
	{
		H_LIST_ID	 = 0,
		H_X_ML_NAME	 = 1,
		H_X_LIST	 = 2,
		H_X_MAILING_LIST = 3,
		H_X_SEQUENCE	 = 4
	};

	FILE *fp;

	g_return_if_fail(msginfo != NULL);
	g_return_if_fail(header != NULL);
	g_return_if_fail(key != NULL);

	*header = NULL;
	*key = NULL;

	switch (type) {
	case FILTER_BY_NONE:
		return;
	case FILTER_BY_AUTO:
		if ((fp = procmsg_open_message(msginfo)) == NULL)
			return;
		procheader_get_header_fields(fp, hentry);
		fclose(fp);

#define SET_FILTER_KEY(hstr, idx)	\
{					\
	*header = g_strdup(hstr);	\
	*key = hentry[idx].body;	\
	hentry[idx].body = NULL;	\
}

		if (hentry[H_LIST_ID].body != NULL) {
			SET_FILTER_KEY("List-Id", H_LIST_ID);
			extract_list_id_str(*key);
		} else if (hentry[H_X_ML_NAME].body != NULL) {
			SET_FILTER_KEY("X-ML-Name", H_X_ML_NAME);
		} else if (hentry[H_X_LIST].body != NULL) {
			SET_FILTER_KEY("X-List", H_X_LIST);
		} else if (hentry[H_X_MAILING_LIST].body != NULL) {
			SET_FILTER_KEY("X-Mailing-list", H_X_MAILING_LIST);
		} else if (hentry[H_X_SEQUENCE].body != NULL) {
			guchar *p;

			SET_FILTER_KEY("X-Sequence", H_X_SEQUENCE);
			p = *key;
			while (*p != '\0') {
				while (*p != '\0' && !isspace(*p)) p++;
				while (isspace(*p)) p++;
				if (isdigit(*p)) {
					*p = '\0';
					break;
				}
			}
			g_strstrip(*key);
		} else if (msginfo->subject) {
			*header = g_strdup("Subject");
			*key = g_strdup(msginfo->subject);
		}

#undef SET_FILTER_KEY

		g_free(hentry[H_LIST_ID].body);
		hentry[H_LIST_ID].body = NULL;
		g_free(hentry[H_X_ML_NAME].body);
		hentry[H_X_ML_NAME].body = NULL;
		g_free(hentry[H_X_LIST].body);
		hentry[H_X_LIST].body = NULL;
		g_free(hentry[H_X_MAILING_LIST].body);
		hentry[H_X_MAILING_LIST].body = NULL;

		break;
	case FILTER_BY_FROM:
		*header = g_strdup("From");
		*key = g_strdup(msginfo->from);
		break;
	case FILTER_BY_TO:
		*header = g_strdup("To");
		*key = g_strdup(msginfo->to);
		break;
	case FILTER_BY_SUBJECT:
		*header = g_strdup("Subject");
		*key = g_strdup(msginfo->subject);
		break;
	default:
		break;
	}
}

void procmsg_empty_trash(FolderItem *trash)
{
	if (trash && trash->total > 0) {
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

gint procmsg_send_queue(FolderItem *queue, gboolean save_msgs,
			gboolean filter_msgs)
{
	gint ret = 0;
	GSList *mlist = NULL;
	GSList *cur;

	if (!queue)
		queue = folder_get_default_queue();
	g_return_val_if_fail(queue != NULL, -1);

	mlist = folder_item_get_msg_list(queue, FALSE);
	mlist = procmsg_sort_msg_list(mlist, SORT_BY_NUMBER, SORT_ASCENDING);

	for (cur = mlist; cur != NULL; cur = cur->next) {
		gchar *file;
		MsgInfo *msginfo = (MsgInfo *)cur->data;
		QueueInfo *qinfo;
		gchar tmp[MAXPATHLEN + 1];

		file = procmsg_get_message_file(msginfo);
		if (!file)
			continue;

		qinfo = send_get_queue_info(file);
		if (!qinfo || send_message_queue(qinfo) < 0) {
			g_warning("Sending queued message %d failed.\n",
				  msginfo->msgnum);
			send_queue_info_free(qinfo);
			g_free(file);
			continue;
		}

		g_snprintf(tmp, sizeof(tmp), "%s%ctmpmsg.out.%08x",
			   get_rc_dir(), G_DIR_SEPARATOR,
			   (guint)random());

		if (send_get_queue_contents(qinfo, tmp) == 0) {
			if (save_msgs) {
				FolderItem *outbox;
				outbox = account_get_special_folder
					(qinfo->ac, F_OUTBOX);
				procmsg_save_to_outbox(outbox, tmp);
			}
			if (filter_msgs) {
				FilterInfo *fltinfo;

				fltinfo = filter_info_new();
				fltinfo->account = qinfo->ac;
				fltinfo->flags.perm_flags = 0;
				fltinfo->flags.tmp_flags = MSG_RECEIVED;

				filter_apply(prefs_common.fltlist, tmp,
					     fltinfo);

				filter_info_free(fltinfo);
			}
			unlink(tmp);
		}

		folder_item_remove_msg(queue, msginfo);
		ret++;

		send_queue_info_free(qinfo);
		g_free(file);
	}

	procmsg_msg_list_free(mlist);

	procmsg_clear_cache(queue);
	queue->cache_dirty = FALSE;
	queue->mtime = 0;

	return ret;
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

	return 0;
}

void procmsg_print_message(MsgInfo *msginfo, const gchar *cmdline)
{
	static const gchar *def_cmd = "lpr %s";
	static guint id = 0;
	gchar *prtmp;
	FILE *tmpfp, *prfp;
	gchar buf[1024];
	gchar *p;

	g_return_if_fail(msginfo);

	if ((tmpfp = procmime_get_first_text_content
		(msginfo, conv_get_locale_charset_str())) == NULL) {
		g_warning(_("Can't get text part\n"));
		return;
	}

	prtmp = g_strdup_printf("%s%cprinttmp.%08x",
				get_mime_tmp_dir(), G_DIR_SEPARATOR, id++);

	if ((prfp = fopen(prtmp, "wb")) == NULL) {
		FILE_OP_ERROR(prtmp, "fopen");
		g_free(prtmp);
		fclose(tmpfp);
		return;
	}

#define OUTPUT_HEADER(s, fmt)						 \
	if (s) {							 \
		gchar *locale_str;					 \
		locale_str = conv_codeset_strdup			 \
			(s, CS_INTERNAL, conv_get_locale_charset_str()); \
		fprintf(prfp, fmt, locale_str ? locale_str : s);	 \
		g_free(locale_str);					 \
	}

	OUTPUT_HEADER(msginfo->date, "Date: %s\n");
	OUTPUT_HEADER(msginfo->from, "From: %s\n");
	OUTPUT_HEADER(msginfo->to, "To: %s\n");
	OUTPUT_HEADER(msginfo->newsgroups, "Newsgroups: %s\n");
	OUTPUT_HEADER(msginfo->subject, "Subject: %s\n");
	fputc('\n', prfp);

#undef OUTPUT_HEADER

	while (fgets(buf, sizeof(buf), tmpfp) != NULL)
		fputs(buf, prfp);

	fclose(prfp);
	fclose(tmpfp);

	if (cmdline && (p = strchr(cmdline, '%')) && *(p + 1) == 's' &&
	    !strchr(p + 2, '%'))
		g_snprintf(buf, sizeof(buf) - 1, cmdline, prtmp);
	else {
		if (cmdline)
			g_warning(_("Print command line is invalid: `%s'\n"),
				  cmdline);
		g_snprintf(buf, sizeof(buf) - 1, def_cmd, prtmp);
	}

	g_free(prtmp);

	g_strchomp(buf);
	if (buf[strlen(buf) - 1] != '&') strcat(buf, "&");
	system(buf);
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

	MEMBDUP(plaintext_file);
	MEMBCOPY(decryption_failed);

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

#if USE_GPGME
	full_msginfo->plaintext_file = g_strdup(msginfo->plaintext_file);
	full_msginfo->decryption_failed = msginfo->decryption_failed;
#endif

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

	g_free(msginfo->plaintext_file);

	g_free(msginfo);
}

gint procmsg_cmp_msgnum_for_sort(gconstpointer a, gconstpointer b)
{
	const MsgInfo *msginfo1 = a;
	const MsgInfo *msginfo2 = b;

	if (!msginfo1)
		return -1;
	if (!msginfo2)
		return -1;

	return msginfo1->msgnum - msginfo2->msgnum;
}

#define CMP_FUNC_DEF(func_name, val)					\
static gint func_name(gconstpointer a, gconstpointer b)			\
{									\
	const MsgInfo *msginfo1 = a;					\
	const MsgInfo *msginfo2 = b;					\
									\
	if (!msginfo1 || !msginfo2)					\
		return -1;						\
									\
	return (val) * (cmp_func_sort_type == SORT_ASCENDING ? 1 : -1);	\
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

CMP_FUNC_DEF(procmsg_cmp_by_number, msginfo1->msgnum - msginfo2->msgnum)
CMP_FUNC_DEF(procmsg_cmp_by_size, msginfo1->size - msginfo2->size)
CMP_FUNC_DEF(procmsg_cmp_by_date, msginfo1->date_t - msginfo2->date_t)

#undef CMP_FUNC_DEF
#define CMP_FUNC_DEF(func_name, var_name)				\
static gint func_name(gconstpointer a, gconstpointer b)			\
{									\
	const MsgInfo *msginfo1 = a;					\
	const MsgInfo *msginfo2 = b;					\
									\
	if (!msginfo1->var_name)					\
		return (msginfo2->var_name != NULL);			\
	if (!msginfo2->var_name)					\
		return -1;						\
									\
	return g_ascii_strcasecmp					\
		(msginfo1->var_name, msginfo2->var_name) *		\
			(cmp_func_sort_type == SORT_ASCENDING ? 1 : -1);\
}

CMP_FUNC_DEF(procmsg_cmp_by_from, fromname)
CMP_FUNC_DEF(procmsg_cmp_by_to, to)

#undef CMP_FUNC_DEF

static gint procmsg_cmp_by_subject(gconstpointer a, gconstpointer b)	\
{									\
	const MsgInfo *msginfo1 = a;					\
	const MsgInfo *msginfo2 = b;					\
									\
	if (!msginfo1->subject)						\
		return (msginfo2->subject != NULL);			\
	if (!msginfo2->subject)						\
		return -1;						\
									\
	return subject_compare_for_sort					\
		(msginfo1->subject, msginfo2->subject) *		\
		(cmp_func_sort_type == SORT_ASCENDING ? 1 : -1);	\
}
