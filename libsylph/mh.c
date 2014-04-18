/*
 * LibSylph -- E-Mail client library
 * Copyright (C) 1999-2009 Hiroyuki Yamamoto
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
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#ifdef G_OS_WIN32
#  include <windows.h>
#endif

#undef MEASURE_TIME

#include "sylmain.h"
#include "folder.h"
#include "mh.h"
#include "procmsg.h"
#include "procheader.h"
#include "utils.h"
#include "prefs_common.h"

#if USE_THREADS
G_LOCK_DEFINE_STATIC(mh);
#define S_LOCK(name)	G_LOCK(name)
#define S_UNLOCK(name)	G_UNLOCK(name)
#else
#define S_LOCK(name)
#define S_UNLOCK(name)
#endif

static void	mh_folder_init		(Folder		*folder,
					 const gchar	*name,
					 const gchar	*path);

static Folder	*mh_folder_new		(const gchar	*name,
					 const gchar	*path);
static void     mh_folder_destroy	(Folder		*folder);

static GSList  *mh_get_msg_list		(Folder		*folder,
					 FolderItem	*item,
					 gboolean	 use_cache);
static GSList  *mh_get_uncached_msg_list(Folder		*folder,
					 FolderItem	*item);
static gchar   *mh_fetch_msg		(Folder		*folder,
					 FolderItem	*item,
					 gint		 num);
static MsgInfo *mh_get_msginfo		(Folder		*folder,
					 FolderItem	*item,
					 gint		 num);
static gint     mh_add_msg		(Folder		*folder,
					 FolderItem	*dest,
					 const gchar	*file,
					 MsgFlags	*flags,
					 gboolean	 remove_source);
static gint     mh_add_msgs		(Folder		*folder,
					 FolderItem	*dest,
					 GSList		*file_list,
					 gboolean	 remove_source,
					 gint		*first);
static gint     mh_add_msg_msginfo	(Folder		*folder,
					 FolderItem	*dest,
					 MsgInfo	*msginfo,
					 gboolean	 remove_source);
static gint     mh_add_msgs_msginfo	(Folder		*folder,
					 FolderItem	*dest,
					 GSList		*msglist,
					 gboolean	 remove_source,
					 gint		*first);
static gint     mh_move_msg		(Folder		*folder,
					 FolderItem	*dest,
					 MsgInfo	*msginfo);
static gint     mh_move_msgs		(Folder		*folder,
					 FolderItem	*dest,
					 GSList		*msglist);
static gint     mh_copy_msg		(Folder		*folder,
					 FolderItem	*dest,
					 MsgInfo	*msginfo);
static gint     mh_copy_msgs		(Folder		*folder,
					 FolderItem	*dest,
					 GSList		*msglist);
static gint     mh_remove_msg		(Folder		*folder,
					 FolderItem	*item,
					 MsgInfo	*msginfo);
static gint     mh_remove_all_msg	(Folder		*folder,
					 FolderItem	*item);
static gboolean mh_is_msg_changed	(Folder		*folder,
					 FolderItem	*item,
					 MsgInfo	*msginfo);
static gint    mh_close			(Folder		*folder,
					 FolderItem	*item);

static gint    mh_scan_folder_full	(Folder		*folder,
					 FolderItem	*item,
					 gboolean	 count_sum);
static gint    mh_scan_folder		(Folder		*folder,
					 FolderItem	*item);
static gint    mh_scan_tree		(Folder		*folder);

static gint    mh_create_tree		(Folder		*folder);
static FolderItem *mh_create_folder	(Folder		*folder,
					 FolderItem	*parent,
					 const gchar	*name);
static gint    mh_rename_folder		(Folder		*folder,
					 FolderItem	*item,
					 const gchar	*name);
static gint    mh_move_folder		(Folder		*folder,
					 FolderItem	*item,
					 FolderItem	*new_parent);
static gint    mh_remove_folder		(Folder		*folder,
					 FolderItem	*item);

static gchar   *mh_get_new_msg_filename		(FolderItem	*dest);

static gint	mh_do_move_msgs			(Folder		*folder,
						 FolderItem	*dest,
						 GSList		*msglist);

static time_t  mh_get_mtime			(FolderItem	*item);
static GSList  *mh_get_uncached_msgs		(GHashTable	*msg_table,
						 FolderItem	*item);
static MsgInfo *mh_parse_msg			(const gchar	*file,
						 FolderItem	*item);
static void	mh_remove_missing_folder_items	(Folder		*folder);
static void	mh_scan_tree_recursive		(FolderItem	*item);

static gboolean mh_rename_folder_func		(GNode		*node,
						 gpointer	 data);

static FolderClass mh_class =
{
	F_MH,

	mh_folder_new,
	mh_folder_destroy,

	mh_scan_tree,
	mh_create_tree,

	mh_get_msg_list,
	mh_get_uncached_msg_list,
	mh_fetch_msg,
	mh_get_msginfo,
	mh_add_msg,
	mh_add_msgs,
	mh_add_msg_msginfo,
	mh_add_msgs_msginfo,
	mh_move_msg,
	mh_move_msgs,
	mh_copy_msg,
	mh_copy_msgs,
	mh_remove_msg,
	NULL,
	mh_remove_all_msg,
	mh_is_msg_changed,
	mh_close,
	mh_scan_folder,

	mh_create_folder,
	mh_rename_folder,
	mh_move_folder,
	mh_remove_folder,
};


FolderClass *mh_get_class(void)
{
	return &mh_class;
}

static Folder *mh_folder_new(const gchar *name, const gchar *path)
{
	Folder *folder;

	folder = (Folder *)g_new0(MHFolder, 1);
	mh_folder_init(folder, name, path);

	return folder;
}

static void mh_folder_destroy(Folder *folder)
{
	folder_local_folder_destroy(LOCAL_FOLDER(folder));
}

static void mh_folder_init(Folder *folder, const gchar *name, const gchar *path)
{
	folder->klass = mh_get_class();
	folder_local_folder_init(folder, name, path);
}

static GSList *mh_get_msg_list_full(Folder *folder, FolderItem *item,
				    gboolean use_cache, gboolean uncached_only)
{
	GSList *mlist;
	GHashTable *msg_table;
	time_t cur_mtime;
	GSList *newlist = NULL;
#ifdef MEASURE_TIME
	GTimer *timer;
#endif

	g_return_val_if_fail(item != NULL, NULL);

	S_LOCK(mh);

#ifdef MEASURE_TIME
	timer = g_timer_new();
#endif

	cur_mtime = mh_get_mtime(item);

	if (use_cache && item->mtime == cur_mtime) {
		debug_print("Folder is not modified.\n");
		mlist = procmsg_read_cache(item, FALSE);
		if (!mlist) {
			mlist = mh_get_uncached_msgs(NULL, item);
			if (mlist)
				item->cache_dirty = TRUE;
		}
	} else if (use_cache) {
		GSList *cur, *next;
		gboolean strict_cache_check = prefs_common.strict_cache_check;

		if (item->stype == F_QUEUE || item->stype == F_DRAFT)
			strict_cache_check = TRUE;

		mlist = procmsg_read_cache(item, strict_cache_check);
		msg_table = procmsg_msg_hash_table_create(mlist);
		newlist = mh_get_uncached_msgs(msg_table, item);
		if (newlist)
			item->cache_dirty = TRUE;
		if (msg_table)
			g_hash_table_destroy(msg_table);

		if (!strict_cache_check) {
			/* remove nonexistent messages */
			for (cur = mlist; cur != NULL; cur = next) {
				MsgInfo *msginfo = (MsgInfo *)cur->data;
				next = cur->next;
				if (!MSG_IS_CACHED(msginfo->flags)) {
					debug_print("removing nonexistent message %d from cache\n", msginfo->msgnum);
					mlist = g_slist_remove(mlist, msginfo);
					procmsg_msginfo_free(msginfo);
					item->cache_dirty = TRUE;
					item->mark_dirty = TRUE;
				}
			}
		}

		mlist = g_slist_concat(mlist, newlist);
	} else {
		mlist = mh_get_uncached_msgs(NULL, item);
		item->cache_dirty = TRUE;
		newlist = mlist;
	}

	procmsg_set_flags(mlist, item);

	if (!uncached_only)
		mlist = procmsg_sort_msg_list(mlist, item->sort_key,
					      item->sort_type);

	if (item->mark_queue)
		item->mark_dirty = TRUE;

#ifdef MEASURE_TIME
	g_timer_stop(timer);
	g_print("%s: %s: elapsed time: %f sec\n",
		G_STRFUNC, item->path, g_timer_elapsed(timer, NULL));
	g_timer_destroy(timer);
#endif
	debug_print("cache_dirty: %d, mark_dirty: %d\n",
		    item->cache_dirty, item->mark_dirty);

	if (!item->opened) {
		item->mtime = cur_mtime;
		if (item->cache_dirty)
			procmsg_write_cache_list(item, mlist);
		if (item->mark_dirty)
			procmsg_write_flags_list(item, mlist);
	}

	if (uncached_only) {
		GSList *cur;

		if (newlist == NULL) {
			procmsg_msg_list_free(mlist);
			S_UNLOCK(mh);
			return NULL;
		}
		if (mlist == newlist) {
			S_UNLOCK(mh);
			return newlist;
		}
		for (cur = mlist; cur != NULL; cur = cur->next) {
			if (cur->next == newlist) {
				cur->next = NULL;
				procmsg_msg_list_free(mlist);
				S_UNLOCK(mh);
				return newlist;
			}
		}
		procmsg_msg_list_free(mlist);
		S_UNLOCK(mh);
		return NULL;
	}

	S_UNLOCK(mh);
	return mlist;
}

static GSList *mh_get_msg_list(Folder *folder, FolderItem *item,
			       gboolean use_cache)
{
	return mh_get_msg_list_full(folder, item, use_cache, FALSE);
}

static GSList *mh_get_uncached_msg_list(Folder *folder, FolderItem *item)
{
	return mh_get_msg_list_full(folder, item, TRUE, TRUE);
}

static gchar *mh_fetch_msg(Folder *folder, FolderItem *item, gint num)
{
	gchar *path;
	gchar *file;
	gchar buf[16];

	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(num > 0, NULL);

	if (item->last_num < 0 || num > item->last_num) {
		mh_scan_folder(folder, item);
		if (item->last_num < 0) return NULL;
	}

	if (num > item->last_num)
		return NULL;

	path = folder_item_get_path(item);
	file = g_strconcat(path, G_DIR_SEPARATOR_S, utos_buf(buf, num), NULL);
	g_free(path);
	if (!is_file_exist(file)) {
		g_free(file);
		return NULL;
	}

	return file;
}

static MsgInfo *mh_get_msginfo(Folder *folder, FolderItem *item, gint num)
{
	MsgInfo *msginfo;
	gchar *file;

	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(num > 0, NULL);

	file = mh_fetch_msg(folder, item, num);
	if (!file) return NULL;

	msginfo = mh_parse_msg(file, item);
	if (msginfo)
		msginfo->msgnum = num;

	g_free(file);

	return msginfo;
}

static gchar *mh_get_new_msg_filename(FolderItem *dest)
{
	gchar *destfile;
	gchar *destpath;

	destpath = folder_item_get_path(dest);
	g_return_val_if_fail(destpath != NULL, NULL);

	if (!is_dir_exist(destpath))
		make_dir_hier(destpath);

	for (;;) {
		destfile = g_strdup_printf("%s%c%d", destpath, G_DIR_SEPARATOR,
					   dest->last_num + 1);
		if (is_file_entry_exist(destfile)) {
			dest->last_num++;
			g_free(destfile);
		} else
			break;
	}

	g_free(destpath);

	return destfile;
}

#define SET_DEST_MSG_FLAGS(fp, dest, n, fl)				\
{									\
	MsgInfo newmsginfo;						\
									\
	newmsginfo.msgnum = n;						\
	newmsginfo.flags = fl;						\
	if (dest->stype == F_OUTBOX ||					\
	    dest->stype == F_QUEUE  ||					\
	    dest->stype == F_DRAFT) {					\
		MSG_UNSET_PERM_FLAGS(newmsginfo.flags,			\
				     MSG_NEW|MSG_UNREAD|MSG_DELETED);	\
	} else if (dest->stype == F_TRASH) {				\
		MSG_UNSET_PERM_FLAGS(newmsginfo.flags, MSG_DELETED);	\
	}								\
									\
	if (fp)								\
		procmsg_write_flags(&newmsginfo, fp);			\
	else 								\
		procmsg_add_mark_queue(dest, n, newmsginfo.flags);	\
}

static gint mh_add_msg(Folder *folder, FolderItem *dest, const gchar *file,
		       MsgFlags *flags, gboolean remove_source)
{
	GSList file_list;
	MsgFileInfo fileinfo;

	g_return_val_if_fail(file != NULL, -1);

	fileinfo.file = (gchar *)file;
	fileinfo.flags = flags;
	file_list.data = &fileinfo;
	file_list.next = NULL;

	return mh_add_msgs(folder, dest, &file_list, remove_source, NULL);
}

static gint mh_add_msgs(Folder *folder, FolderItem *dest, GSList *file_list,
			gboolean remove_source, gint *first)
{
	gchar *destfile;
	GSList *cur;
	MsgFileInfo *fileinfo;
	MsgInfo *msginfo;
	gint first_ = 0;
	FILE *fp = NULL;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(file_list != NULL, -1);

	if (dest->last_num < 0) {
		mh_scan_folder(folder, dest);
		if (dest->last_num < 0) return -1;
	}

	S_LOCK(mh);

	if (!dest->opened) {
		if ((fp = procmsg_open_mark_file(dest, DATA_APPEND)) == NULL)
			g_warning("mh_add_msgs: can't open mark file.");
	}

	for (cur = file_list; cur != NULL; cur = cur->next) {
		MsgFlags flags = {MSG_NEW|MSG_UNREAD, 0};

		fileinfo = (MsgFileInfo *)cur->data;
		if (fileinfo->flags)
			flags = *fileinfo->flags;
		msginfo = procheader_parse_file(fileinfo->file, flags, 0);
		if (!msginfo) {
			if (fp) fclose(fp);
			S_UNLOCK(mh);
			return -1;
		}

		destfile = mh_get_new_msg_filename(dest);
		if (destfile == NULL) {
			S_UNLOCK(mh);
			return -1;
		}
		if (first_ == 0 || first_ > dest->last_num + 1)
			first_ = dest->last_num + 1;

		if (syl_link(fileinfo->file, destfile) < 0) {
			if (copy_file(fileinfo->file, destfile, TRUE) < 0) {
				g_warning(_("can't copy message %s to %s\n"),
					  fileinfo->file, destfile);
				g_free(destfile);
				if (fp) fclose(fp);
				S_UNLOCK(mh);
				return -1;
			}
		}

		if (syl_app_get())
			g_signal_emit_by_name(syl_app_get(), "add-msg", dest, destfile, dest->last_num + 1);

		g_free(destfile);
		dest->last_num++;
		dest->total++;
		dest->updated = TRUE;
		dest->mtime = 0;

		if (MSG_IS_RECEIVED(flags)) {
			/* resets new flags of existing messages on
			   received mode */
			if (dest->unmarked_num == 0)
				dest->new = 0;
			dest->unmarked_num++;
			procmsg_add_mark_queue(dest, dest->last_num, flags);
		} else {
			SET_DEST_MSG_FLAGS(fp, dest, dest->last_num, flags);
		}
		procmsg_add_cache_queue(dest, dest->last_num, msginfo);
		if (MSG_IS_NEW(flags))
			dest->new++;
		if (MSG_IS_UNREAD(flags))
			dest->unread++;
	}

	if (fp)
		fclose(fp);

	if (first)
		*first = first_;

	if (remove_source) {
		for (cur = file_list; cur != NULL; cur = cur->next) {
			fileinfo = (MsgFileInfo *)cur->data;
			if (g_unlink(fileinfo->file) < 0)
				FILE_OP_ERROR(fileinfo->file, "unlink");
		}
	}

	S_UNLOCK(mh);
	return dest->last_num;
}


static gint mh_add_msg_msginfo(Folder *folder, FolderItem *dest,
			       MsgInfo *msginfo, gboolean remove_source)
{
	GSList msglist;

	g_return_val_if_fail(msginfo != NULL, -1);

	msglist.data = msginfo;
	msglist.next = NULL;

	return mh_add_msgs_msginfo(folder, dest, &msglist, remove_source, NULL);
}

static gint mh_add_msgs_msginfo(Folder *folder, FolderItem *dest,
				GSList *msglist, gboolean remove_source,
				gint *first)
{
	GSList *cur;
	MsgInfo *msginfo;
	gchar *srcfile;
	gchar *destfile;
	gint first_ = 0;
	FILE *fp = NULL;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msglist != NULL, -1);

	if (dest->last_num < 0) {
		mh_scan_folder(folder, dest);
		if (dest->last_num < 0) return -1;
	}

	S_LOCK(mh);

	if (!dest->opened) {
		if ((fp = procmsg_open_mark_file(dest, DATA_APPEND)) == NULL)
			g_warning("mh_add_msgs_msginfo: can't open mark file.");
	}

	for (cur = msglist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;

		destfile = mh_get_new_msg_filename(dest);
		if (!destfile) {
			if (fp) fclose(fp);
			S_UNLOCK(mh);
			return -1;
		}
		if (first_ == 0 || first_ > dest->last_num + 1)
			first_ = dest->last_num + 1;

		srcfile = procmsg_get_message_file(msginfo);
		if (!srcfile) {
			if (fp) fclose(fp);
			g_free(destfile);
			S_UNLOCK(mh);
			return -1;
		}
		if (syl_link(srcfile, destfile) < 0) {
			if (copy_file(srcfile, destfile, TRUE) < 0) {
				g_warning("mh_add_msgs_msginfo: can't copy message %s to %s", srcfile, destfile);
				g_free(srcfile);
				g_free(destfile);
				if (fp) fclose(fp);
				S_UNLOCK(mh);
				return -1;
			}
		}

		if (syl_app_get())
			g_signal_emit_by_name(syl_app_get(), "add-msg", dest, destfile, dest->last_num + 1);

		g_free(srcfile);
		g_free(destfile);
		dest->last_num++;
		dest->total++;
		dest->updated = TRUE;
		dest->mtime = 0;

		if (MSG_IS_RECEIVED(msginfo->flags)) {
			/* resets new flags of existing messages on
			   received mode */
			if (dest->unmarked_num == 0)
				dest->new = 0;
			dest->unmarked_num++;
			procmsg_add_mark_queue(dest, dest->last_num,
					       msginfo->flags);
		} else {
			SET_DEST_MSG_FLAGS(fp, dest, dest->last_num,
					   msginfo->flags);
		}
		procmsg_add_cache_queue(dest, dest->last_num, msginfo);
		if (MSG_IS_NEW(msginfo->flags))
			dest->new++;
		if (MSG_IS_UNREAD(msginfo->flags))
			dest->unread++;
	}

	if (fp)
		fclose(fp);

	if (first)
		*first = first_;

	if (remove_source) {
		for (cur = msglist; cur != NULL; cur = cur->next) {
			msginfo = (MsgInfo *)cur->data;
			srcfile = procmsg_get_message_file(msginfo);
			if (g_unlink(srcfile) < 0)
				FILE_OP_ERROR(srcfile, "unlink");
			g_free(srcfile);
		}
	}

	S_UNLOCK(mh);
	return dest->last_num;
}


static gint mh_do_move_msgs(Folder *folder, FolderItem *dest, GSList *msglist)
{
	FolderItem *src;
	gchar *srcfile;
	gchar *destfile;
	GSList *cur;
	MsgInfo *msginfo;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msglist != NULL, -1);

	if (dest->last_num < 0) {
		mh_scan_folder(folder, dest);
		if (dest->last_num < 0) return -1;
	}

	S_LOCK(mh);

	for (cur = msglist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;
		src = msginfo->folder;

		if (src == dest) {
			g_warning(_("the src folder is identical to the dest.\n"));
			continue;
		}
		debug_print("Moving message %s/%d to %s ...\n",
			    src->path, msginfo->msgnum, dest->path);

		destfile = mh_get_new_msg_filename(dest);
		if (!destfile) break;
		srcfile = procmsg_get_message_file(msginfo);

		/* g_signal_emit_by_name(syl_app_get(), "remove-msg", src, srcfile, msginfo->msgnum); */

		if (move_file(srcfile, destfile, FALSE) < 0) {
			g_free(srcfile);
			g_free(destfile);
			break;
		}

		if (syl_app_get()) {
			g_signal_emit_by_name(syl_app_get(), "add-msg", dest, destfile, dest->last_num + 1);
			g_signal_emit_by_name(syl_app_get(), "remove-msg", src, srcfile, msginfo->msgnum);
		}

		g_free(srcfile);
		g_free(destfile);
		src->total--;
		src->updated = TRUE;
		src->mtime = 0;
		dest->last_num++;
		dest->total++;
		dest->updated = TRUE;
		dest->mtime = 0;

		SET_DEST_MSG_FLAGS(NULL, dest, dest->last_num, msginfo->flags);
		procmsg_add_cache_queue(dest, dest->last_num, msginfo);

		if (MSG_IS_NEW(msginfo->flags)) {
			src->new--;
			dest->new++;
		}
		if (MSG_IS_UNREAD(msginfo->flags)) {
			src->unread--;
			dest->unread++;
		}

		MSG_SET_TMP_FLAGS(msginfo->flags, MSG_INVALID);
	}

	if (!dest->opened) {
		procmsg_flush_mark_queue(dest, NULL);
		procmsg_flush_cache_queue(dest, NULL);
	}

	S_UNLOCK(mh);
	return dest->last_num;
}

static gint mh_move_msg(Folder *folder, FolderItem *dest, MsgInfo *msginfo)
{
	GSList msglist;

	g_return_val_if_fail(msginfo != NULL, -1);

	msglist.data = msginfo;
	msglist.next = NULL;

	return mh_move_msgs(folder, dest, &msglist);
}

static gint mh_move_msgs(Folder *folder, FolderItem *dest, GSList *msglist)
{
	MsgInfo *msginfo;
	gint ret = 0;
	gint first;

	msginfo = (MsgInfo *)msglist->data;
	if (folder == msginfo->folder->folder)
		return mh_do_move_msgs(folder, dest, msglist);

	ret = mh_add_msgs_msginfo(folder, dest, msglist, FALSE, &first);

	if (ret != -1)
		ret = folder_item_remove_msgs(msginfo->folder, msglist);

	return ret;
}

static gint mh_copy_msg(Folder *folder, FolderItem *dest, MsgInfo *msginfo)
{
	GSList msglist;

	g_return_val_if_fail(msginfo != NULL, -1);

	msglist.data = msginfo;
	msglist.next = NULL;

	return mh_copy_msgs(folder, dest, &msglist);
}

static gint mh_copy_msgs(Folder *folder, FolderItem *dest, GSList *msglist)
{
	gchar *srcfile;
	gchar *destfile;
	GSList *cur;
	MsgInfo *msginfo;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msglist != NULL, -1);

	if (dest->last_num < 0) {
		mh_scan_folder(folder, dest);
		if (dest->last_num < 0) return -1;
	}

	S_LOCK(mh);

	for (cur = msglist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;

		if (msginfo->folder == dest) {
			g_warning(_("the src folder is identical to the dest.\n"));
			continue;
		}
		debug_print(_("Copying message %s/%d to %s ...\n"),
			    msginfo->folder->path, msginfo->msgnum, dest->path);

		destfile = mh_get_new_msg_filename(dest);
		if (!destfile) break;
		srcfile = procmsg_get_message_file(msginfo);

		if (copy_file(srcfile, destfile, TRUE) < 0) {
			FILE_OP_ERROR(srcfile, "copy");
			g_free(srcfile);
			g_free(destfile);
			break;
		}

		if (syl_app_get())
			g_signal_emit_by_name(syl_app_get(), "add-msg", dest, destfile, dest->last_num + 1);

		g_free(srcfile);
		g_free(destfile);
		dest->last_num++;
		dest->total++;
		dest->updated = TRUE;
		dest->mtime = 0;

		SET_DEST_MSG_FLAGS(NULL, dest, dest->last_num, msginfo->flags);
		procmsg_add_cache_queue(dest, dest->last_num, msginfo);

		if (MSG_IS_NEW(msginfo->flags))
			dest->new++;
		if (MSG_IS_UNREAD(msginfo->flags))
			dest->unread++;
	}

	if (!dest->opened) {
		procmsg_flush_mark_queue(dest, NULL);
		procmsg_flush_cache_queue(dest, NULL);
	}

	S_UNLOCK(mh);
	return dest->last_num;
}

static gint mh_remove_msg(Folder *folder, FolderItem *item, MsgInfo *msginfo)
{
	gchar *file;

	g_return_val_if_fail(item != NULL, -1);

	file = mh_fetch_msg(folder, item, msginfo->msgnum);
	g_return_val_if_fail(file != NULL, -1);

	if (syl_app_get())
		g_signal_emit_by_name(syl_app_get(), "remove-msg", item, file, msginfo->msgnum);

	S_LOCK(mh);

	if (g_unlink(file) < 0) {
		FILE_OP_ERROR(file, "unlink");
		g_free(file);
		S_UNLOCK(mh);
		return -1;
	}
	g_free(file);

	item->total--;
	item->updated = TRUE;
	item->mtime = 0;
	if (MSG_IS_NEW(msginfo->flags))
		item->new--;
	if (MSG_IS_UNREAD(msginfo->flags))
		item->unread--;
	MSG_SET_TMP_FLAGS(msginfo->flags, MSG_INVALID);

	S_UNLOCK(mh);

	if (msginfo->msgnum == item->last_num)
		mh_scan_folder_full(folder, item, FALSE);

	return 0;
}

static gint mh_remove_all_msg(Folder *folder, FolderItem *item)
{
	gchar *path;
	gint val;

	g_return_val_if_fail(item != NULL, -1);

	path = folder_item_get_path(item);
	g_return_val_if_fail(path != NULL, -1);
	if (syl_app_get())
		g_signal_emit_by_name(syl_app_get(), "remove-all-msg", item);

	S_LOCK(mh);

	val = remove_all_numbered_files(path);
	g_free(path);
	if (val == 0) {
		item->new = item->unread = item->total = 0;
		item->last_num = 0;
		item->updated = TRUE;
		item->mtime = 0;
	}

	S_UNLOCK(mh);

	return val;
}

static gboolean mh_is_msg_changed(Folder *folder, FolderItem *item,
				  MsgInfo *msginfo)
{
	GStatBuf s;
	gchar buf[16];

	if (g_stat(utos_buf(buf, msginfo->msgnum), &s) < 0 ||
	    msginfo->size  != s.st_size ||
	    msginfo->mtime != s.st_mtime)
		return TRUE;

	return FALSE;
}

static gint mh_close(Folder *folder, FolderItem *item)
{
	return 0;
}

#ifdef G_OS_WIN32
struct wfddata {
	WIN32_FIND_DATAA wfda;
	WIN32_FIND_DATAW wfdw;
	DWORD file_attr;
	gchar *file_name;
};

static HANDLE find_first_file(const gchar *path, struct wfddata *wfd)
{
	HANDLE hfind;

	if (G_WIN32_HAVE_WIDECHAR_API()) {
		if (path) {
			gchar *wildcard_path;
			wchar_t *wpath;

			wildcard_path = g_strconcat(path, "\\*", NULL);
			wpath = g_utf8_to_utf16(wildcard_path, -1,
						NULL, NULL, NULL);
			if (wpath) {
				hfind = FindFirstFileW(wpath, &wfd->wfdw);
				g_free(wpath);
			} else
				hfind = INVALID_HANDLE_VALUE;
			g_free(wildcard_path);
		} else
			hfind = FindFirstFileW(L"*", &wfd->wfdw);

		if (hfind != INVALID_HANDLE_VALUE) {
			wfd->file_attr = wfd->wfdw.dwFileAttributes;
			wfd->file_name = g_utf16_to_utf8(wfd->wfdw.cFileName, -1,
							 NULL, NULL, NULL);
		}
	} else {
		if (path) {
			gchar *wildcard_path;
			gchar *cp_path;

			wildcard_path = g_strconcat(path, "\\*", NULL);
			cp_path = g_locale_from_utf8(wildcard_path, -1,
						     NULL, NULL, NULL);
			if (cp_path) {
				hfind = FindFirstFileA(cp_path, &wfd->wfda);
				g_free(cp_path);
			} else
				hfind = INVALID_HANDLE_VALUE;
			g_free(wildcard_path);
		} else
			hfind = FindFirstFileA("*", &wfd->wfda);

		if (hfind != INVALID_HANDLE_VALUE) {
			wfd->file_attr = wfd->wfda.dwFileAttributes;
			wfd->file_name = g_locale_to_utf8(wfd->wfda.cFileName,
							  -1, NULL, NULL, NULL);
		}
	}

	return hfind;
}

static BOOL find_next_file(HANDLE hfind, struct wfddata *wfd)
{
	BOOL retval;

	if (G_WIN32_HAVE_WIDECHAR_API()) {
		retval = FindNextFileW(hfind, &wfd->wfdw);
		if (retval) {
			wfd->file_attr = wfd->wfdw.dwFileAttributes;
			wfd->file_name = g_utf16_to_utf8(wfd->wfdw.cFileName, -1,
							 NULL, NULL, NULL);
		}
	} else {
		retval = FindNextFileA(hfind, &wfd->wfda);
		if (retval) {
			wfd->file_attr = wfd->wfda.dwFileAttributes;
			wfd->file_name = g_locale_to_utf8(wfd->wfda.cFileName,
							  -1, NULL, NULL, NULL);
		}
	}

	return retval;
}
#endif

static gint mh_scan_folder_full(Folder *folder, FolderItem *item,
				gboolean count_sum)
{
	gchar *path;
#ifdef G_OS_WIN32
	struct wfddata wfd;
	HANDLE hfind;
#else
	DIR *dp;
	struct dirent *d;
#endif
	gint max = 0;
	gint num;
	gint n_msg = 0;

	g_return_val_if_fail(item != NULL, -1);

	debug_print("mh_scan_folder(): Scanning %s ...\n", item->path);

	S_LOCK(mh);

	path = folder_item_get_path(item);
	if (!path) {
		S_UNLOCK(mh);
		return -1;
	}
	if (change_dir(path) < 0) {
		g_free(path);
		S_UNLOCK(mh);
		return -1;
	}
	g_free(path);

#ifdef G_OS_WIN32
	if ((hfind = find_first_file(NULL, &wfd)) == INVALID_HANDLE_VALUE) {
		g_warning("failed to open directory\n");
#else
	if ((dp = opendir(".")) == NULL) {
		FILE_OP_ERROR(item->path, "opendir");
#endif
		S_UNLOCK(mh);
		return -1;
	}

	if (folder->ui_func)
		folder->ui_func(folder, item, folder->ui_func_data);

#ifdef G_OS_WIN32
	do {
		if ((wfd.file_attr &
		     (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_DEVICE)) == 0) {
			if (wfd.file_name) {
				if ((num = to_number(wfd.file_name)) > 0) {
					n_msg++;
					if (max < num)
						max = num;
				}
			}
		}

		if (wfd.file_name) {
			g_free(wfd.file_name);
			wfd.file_name = NULL;
		}
	} while (find_next_file(hfind, &wfd));

	FindClose(hfind);
#else
	while ((d = readdir(dp)) != NULL) {
		if ((num = to_number(d->d_name)) > 0 &&
		    dirent_is_regular_file(d)) {
			n_msg++;
			if (max < num)
				max = num;
		}
	}

	closedir(dp);
#endif

	if (n_msg == 0)
		item->new = item->unread = item->total = 0;
	else if (count_sum) {
		gint new, unread, total, min, max_;

		procmsg_get_mark_sum
			(item, &new, &unread, &total, &min, &max_, 0);

		if (n_msg > total) {
			item->unmarked_num = new = n_msg - total;
			unread += n_msg - total;
		} else
			item->unmarked_num = 0;

		item->new = new;
		item->unread = unread;
		item->total = n_msg;

		if (item->cache_queue && !item->opened) {
			procmsg_flush_cache_queue(item, NULL);
		}
	}

	item->updated = TRUE;
	item->mtime = 0;

	debug_print("Last number in dir %s = %d\n", item->path, max);
	item->last_num = max;

	S_UNLOCK(mh);
	return 0;
}

static gint mh_scan_folder(Folder *folder, FolderItem *item)
{
	return mh_scan_folder_full(folder, item, TRUE);
}

static gint mh_scan_tree(Folder *folder)
{
	FolderItem *item;
	gchar *rootpath;

	g_return_val_if_fail(folder != NULL, -1);

	S_LOCK(mh);

	if (!folder->node) {
		item = folder_item_new(folder->name, NULL);
		item->folder = folder;
		folder->node = item->node = g_node_new(item);
	} else
		item = FOLDER_ITEM(folder->node->data);

	rootpath = folder_item_get_path(item);
	if (change_dir(rootpath) < 0) {
		g_free(rootpath);
		S_UNLOCK(mh);
		return -1;
	}
	g_free(rootpath);

	mh_create_tree(folder);
	mh_remove_missing_folder_items(folder);
	mh_scan_tree_recursive(item);

	S_UNLOCK(mh);
	return 0;
}

#define MAKE_DIR_IF_NOT_EXIST(dir) \
{ \
	if (!is_dir_exist(dir)) { \
		if (is_file_exist(dir)) { \
			g_warning(_("File `%s' already exists.\n" \
				    "Can't create folder."), dir); \
			return -1; \
		} \
		if (make_dir(dir) < 0) \
			return -1; \
	} \
}

#define MAKE_DIR_HIER_IF_NOT_EXIST(dir) \
{ \
	if (!is_dir_exist(dir)) { \
		if (is_file_exist(dir)) { \
			g_warning(_("File `%s' already exists.\n" \
				    "Can't create folder."), dir); \
			return -1; \
		} \
		if (make_dir_hier(dir) < 0) \
			return -1; \
	} \
}

static gint mh_create_tree(Folder *folder)
{
	gchar *rootpath;

	g_return_val_if_fail(folder != NULL, -1);

	CHDIR_RETURN_VAL_IF_FAIL(get_mail_base_dir(), -1);
	rootpath = LOCAL_FOLDER(folder)->rootpath;
	MAKE_DIR_HIER_IF_NOT_EXIST(rootpath);
	CHDIR_RETURN_VAL_IF_FAIL(rootpath, -1);
	MAKE_DIR_IF_NOT_EXIST(INBOX_DIR);
	MAKE_DIR_IF_NOT_EXIST(OUTBOX_DIR);
	MAKE_DIR_IF_NOT_EXIST(QUEUE_DIR);
	MAKE_DIR_IF_NOT_EXIST(DRAFT_DIR);
	MAKE_DIR_IF_NOT_EXIST(TRASH_DIR);
	MAKE_DIR_IF_NOT_EXIST(JUNK_DIR);

	return 0;
}

#undef MAKE_DIR_IF_NOT_EXIST
#undef MAKE_DIR_HIER_IF_NOT_EXIST

static FolderItem *mh_create_folder(Folder *folder, FolderItem *parent,
				    const gchar *name)
{
	gchar *path;
	gchar *fs_name;
	gchar *fullpath;
	FolderItem *new_item;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(parent != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	S_LOCK(mh);

	path = folder_item_get_path(parent);
	fs_name = g_filename_from_utf8(name, -1, NULL, NULL, NULL);
	fullpath = g_strconcat(path, G_DIR_SEPARATOR_S,
			       fs_name ? fs_name : name, NULL);
	g_free(fs_name);
	g_free(path);

	if (make_dir_hier(fullpath) < 0) {
		g_free(fullpath);
		S_UNLOCK(mh);
		return NULL;
	}

	g_free(fullpath);

	/* path is a logical folder path */
	if (parent->path)
		path = g_strconcat(parent->path, "/", name, NULL);
	else
		path = g_strdup(name);
	new_item = folder_item_new(name, path);
	folder_item_append(parent, new_item);
	g_free(path);

	S_UNLOCK(mh);
	return new_item;
}

static gint mh_move_folder_real(Folder *folder, FolderItem *item,
				FolderItem *new_parent, const gchar *name)
{
	gchar *rootpath;
	gchar *oldpath;
	gchar *newpath;
	gchar *dirname;
	gchar *new_dir;
	gchar *name_;
	gchar *utf8_name;
	gchar *paths[2];
	gchar *old_id, *new_id;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(folder == item->folder, -1);
	g_return_val_if_fail(item->path != NULL, -1);
	g_return_val_if_fail(new_parent != NULL || name != NULL, -1);
	if (new_parent) {
		g_return_val_if_fail(item != new_parent, -1);
		g_return_val_if_fail(item->parent != new_parent, -1);
		g_return_val_if_fail(item->folder == new_parent->folder, -1);
		if (g_node_is_ancestor(item->node, new_parent->node)) {
			g_warning("folder to be moved is ancestor of new parent\n");
			return -1;
		}
	}

	S_LOCK(mh);

	oldpath = folder_item_get_path(item);
	if (new_parent) {
		if (name) {
			name_ = g_filename_from_utf8(name, -1, NULL, NULL,
						     NULL);
			if (!name_)
				name_ = g_strdup(name);
			utf8_name = g_strdup(name);
		} else {
			name_ = g_path_get_basename(oldpath);
			utf8_name = g_filename_to_utf8(name_, -1, NULL, NULL,
						       NULL);
			if (!utf8_name)
				utf8_name = g_strdup(name_);
		}
		new_dir = folder_item_get_path(new_parent);
		newpath = g_strconcat(new_dir, G_DIR_SEPARATOR_S, name_, NULL);
		g_free(new_dir);
	} else {
		name_ = g_filename_from_utf8(name, -1, NULL, NULL, NULL);
		utf8_name = g_strdup(name);
		dirname = g_dirname(oldpath);
		newpath = g_strconcat(dirname, G_DIR_SEPARATOR_S,
				      name_ ? name_ : name, NULL);
		g_free(dirname);
	}
	g_free(name_);

	if (is_file_entry_exist(newpath)) {
		g_warning("%s already exists\n", newpath);
		g_free(oldpath);
		g_free(newpath);
		g_free(utf8_name);
		S_UNLOCK(mh);
		return -1;
	}

	rootpath = folder_get_path(folder);
	if (change_dir(rootpath) < 0) {
		g_free(rootpath);
		g_free(oldpath);
		g_free(newpath);
		g_free(utf8_name);
		S_UNLOCK(mh);
		return -1;
	}
	g_free(rootpath);

	debug_print("mh_move_folder: rename(%s, %s)\n", oldpath, newpath);

	if (g_rename(oldpath, newpath) < 0) {
		FILE_OP_ERROR(oldpath, "rename");
		g_free(oldpath);
		g_free(newpath);
		g_free(utf8_name);
		S_UNLOCK(mh);
		return -1;
	}

	g_free(oldpath);
	g_free(newpath);

	old_id = folder_item_get_identifier(item);

	if (new_parent) {
		g_node_unlink(item->node);
		g_node_append(new_parent->node, item->node);
		item->parent = new_parent;
		if (new_parent->path != NULL) {
			newpath = g_strconcat(new_parent->path, "/", utf8_name,
					      NULL);
			g_free(utf8_name);
		} else
			newpath = utf8_name;
	} else {
		if (strchr(item->path, '/') != NULL) {
			dirname = g_dirname(item->path);
			newpath = g_strconcat(dirname, "/", utf8_name, NULL);
			g_free(dirname);
			g_free(utf8_name);
		} else
			newpath = utf8_name;
	}

	if (name) {
		g_free(item->name);
		item->name = g_strdup(name);
	}

	paths[0] = g_strdup(item->path);
	paths[1] = newpath;
	g_node_traverse(item->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			mh_rename_folder_func, paths);

	g_free(paths[0]);
	g_free(paths[1]);

	new_id = folder_item_get_identifier(item);
	if (syl_app_get())
		g_signal_emit_by_name(syl_app_get(), "move-folder", item,
				      old_id, new_id);
	g_free(new_id);
	g_free(old_id);

	S_UNLOCK(mh);
	return 0;
}

static gint mh_move_folder(Folder *folder, FolderItem *item,
			   FolderItem *new_parent)
{
	return mh_move_folder_real(folder, item, new_parent, NULL);
}

static gint mh_rename_folder(Folder *folder, FolderItem *item,
			     const gchar *name)
{
	return mh_move_folder_real(folder, item, NULL, name);
}

static gint mh_remove_folder(Folder *folder, FolderItem *item)
{
	gchar *path;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->path != NULL, -1);

	S_LOCK(mh);

	path = folder_item_get_path(item);
	if (remove_dir_recursive(path) < 0) {
		g_warning("can't remove directory `%s'\n", path);
		g_free(path);
		S_UNLOCK(mh);
		return -1;
	}

	g_free(path);
	if (syl_app_get())
		g_signal_emit_by_name(syl_app_get(), "remove-folder", item);
	folder_item_remove(item);

	S_UNLOCK(mh);
	return 0;
}


static time_t mh_get_mtime(FolderItem *item)
{
	gchar *path;
	GStatBuf s;

	path = folder_item_get_path(item);
	if (g_stat(path, &s) < 0) {
		FILE_OP_ERROR(path, "stat");
		g_free(path);
		return -1;
	} else {
		g_free(path);
		return MAX(s.st_mtime, s.st_ctime);
	}
}

static GSList *mh_get_uncached_msgs(GHashTable *msg_table, FolderItem *item)
{
	gchar *path;
	GDir *dp;
	const gchar *dir_name;
	GSList *newlist = NULL;
	GSList *last = NULL;
	MsgInfo *msginfo;
	gint n_newmsg = 0;
	gint num;
	Folder *folder;

	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(item->folder != NULL, NULL);

	folder = item->folder;

	path = folder_item_get_path(item);
	g_return_val_if_fail(path != NULL, NULL);
	if (change_dir(path) < 0) {
		g_free(path);
		return NULL;
	}
	g_free(path);

	if ((dp = g_dir_open(".", 0, NULL)) == NULL) {
		FILE_OP_ERROR(item->path, "opendir");
		return NULL;
	}

	debug_print("Searching uncached messages...\n");

	if (msg_table) {
		gint count = 0;

		while ((dir_name = g_dir_read_name(dp)) != NULL) {
			if ((num = to_number(dir_name)) <= 0) continue;

			msginfo = g_hash_table_lookup
				(msg_table, GUINT_TO_POINTER(num));

			if (msginfo) {
				MSG_SET_TMP_FLAGS(msginfo->flags, MSG_CACHED);
			} else {
				/* not found in the cache (uncached message) */
				msginfo = mh_parse_msg(dir_name, item);
				if (!msginfo) continue;

				if (!newlist)
					last = newlist =
						g_slist_append(NULL, msginfo);
				else {
					last = g_slist_append(last, msginfo);
					last = last->next;
				}
				n_newmsg++;
			}

			count++;
			if (folder->ui_func)
				folder->ui_func(folder, item, folder->ui_func_data ? folder->ui_func_data : GINT_TO_POINTER(count));
		}
	} else {
		/* discard all previous cache */
		while ((dir_name = g_dir_read_name(dp)) != NULL) {
			if (to_number(dir_name) <= 0) continue;

			msginfo = mh_parse_msg(dir_name, item);
			if (!msginfo) continue;

			if (!newlist)
				last = newlist = g_slist_append(NULL, msginfo);
			else {
				last = g_slist_append(last, msginfo);
				last = last->next;
			}
			n_newmsg++;
			if (folder->ui_func)
				folder->ui_func(folder, item, folder->ui_func_data ? folder->ui_func_data : GINT_TO_POINTER(n_newmsg));
		}
	}

	g_dir_close(dp);

	if (n_newmsg)
		debug_print("%d uncached message(s) found.\n", n_newmsg);
	else
		debug_print("done.\n");

	/* sort new messages in numerical order */
	if (newlist && item->sort_key == SORT_BY_NONE) {
		debug_print("Sorting uncached messages in numerical order...\n");
		newlist = g_slist_sort
			(newlist, (GCompareFunc)procmsg_cmp_msgnum_for_sort);
		debug_print("done.\n");
	}

	return newlist;
}

static MsgInfo *mh_parse_msg(const gchar *file, FolderItem *item)
{
	MsgInfo *msginfo;
	MsgFlags flags;

	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(file != NULL, NULL);

	flags.perm_flags = MSG_NEW|MSG_UNREAD;
	flags.tmp_flags = 0;

	if (item->stype == F_QUEUE) {
		MSG_SET_TMP_FLAGS(flags, MSG_QUEUED);
	} else if (item->stype == F_DRAFT) {
		MSG_SET_TMP_FLAGS(flags, MSG_DRAFT);
	}

	msginfo = procheader_parse_file(file, flags, FALSE);
	if (!msginfo) return NULL;

	msginfo->msgnum = atoi(file);
	msginfo->folder = item;

	return msginfo;
}

#if 0
static gboolean mh_is_maildir_one(const gchar *path, const gchar *dir)
{
	gchar *entry;
	gboolean result;

	entry = g_strconcat(path, G_DIR_SEPARATOR_S, dir, NULL);
	result = is_dir_exist(entry);
	g_free(entry);

	return result;
}

/*
 * check whether PATH is a Maildir style mailbox.
 * This is the case if the 3 subdir: new, cur, tmp are existing.
 * This functon assumes that entry is an directory
 */
static gboolean mh_is_maildir(const gchar *path)
{
	return mh_is_maildir_one(path, "new") &&
	       mh_is_maildir_one(path, "cur") &&
	       mh_is_maildir_one(path, "tmp");
}
#endif

static gboolean mh_remove_missing_folder_items_func(GNode *node, gpointer data)
{
	FolderItem *item;
	gchar *path;

	g_return_val_if_fail(node->data != NULL, FALSE);

	if (G_NODE_IS_ROOT(node))
		return FALSE;

	item = FOLDER_ITEM(node->data);

#if 0
	if (item->path && strchr(item->path, '/')) {
		debug_print("folder '%s' includes Unix path separator. removing...\n", item->path);
		folder_item_remove(item);
		return FALSE;
	}
#endif

	path = folder_item_get_path(item);
	if (!is_dir_exist(path)) {
		debug_print("folder '%s' not found. removing...\n", path);
		folder_item_remove(item);
	}
	g_free(path);

	return FALSE;
}

static void mh_remove_missing_folder_items(Folder *folder)
{
	g_return_if_fail(folder != NULL);

	debug_print("searching missing folders...\n");

	g_node_traverse(folder->node, G_POST_ORDER, G_TRAVERSE_ALL, -1,
			mh_remove_missing_folder_items_func, folder);
}

#define MAX_RECURSION_LEVEL	64

static void mh_scan_tree_recursive(FolderItem *item)
{
	Folder *folder;
#ifdef G_OS_WIN32
	struct wfddata wfd;
	HANDLE hfind;
#else
	DIR *dp;
	struct dirent *d;
	GStatBuf s;
#endif
	const gchar *dir_name;
	gchar *fs_path;
	gchar *entry;
	gchar *utf8entry;
	gchar *utf8name;
	gint n_msg = 0;

	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	if (item->stype == F_VIRTUAL)
		return;

	folder = item->folder;

	if (g_node_depth(item->node) >= MAX_RECURSION_LEVEL) {
		g_warning("mh_scan_tree_recursive(): max recursion level (%u) reached.", MAX_RECURSION_LEVEL);
		return;
	}

	debug_print("scanning %s ...\n",
		    item->path ? item->path : LOCAL_FOLDER(folder)->rootpath);
	if (folder->ui_func)
		folder->ui_func(folder, item, folder->ui_func_data);

	fs_path = item->path ?
		g_filename_from_utf8(item->path, -1, NULL, NULL, NULL)
		: g_strdup(".");
	if (!fs_path)
		fs_path = g_strdup(item->path);
#ifdef G_OS_WIN32
	hfind = find_first_file(fs_path, &wfd);
	if (hfind == INVALID_HANDLE_VALUE) {
		g_warning("failed to open directory: %s\n", fs_path);
		g_free(fs_path);
		return;
	}
#else
	dp = opendir(fs_path);
	if (!dp) {
		FILE_OP_ERROR(fs_path, "opendir");
		g_free(fs_path);
		return;
	}
#endif
	g_free(fs_path);

#ifdef G_OS_WIN32
	do {
		if (!wfd.file_name) continue;
		if (wfd.file_name[0] == '.') {
			g_free(wfd.file_name);
			wfd.file_name = NULL;
			continue;
		}
		dir_name = utf8name = wfd.file_name;
		wfd.file_name = NULL;
#else
	while ((d = readdir(dp)) != NULL) {
		dir_name = d->d_name;
		if (dir_name[0] == '.') continue;

		utf8name = g_filename_to_utf8(dir_name, -1, NULL, NULL, NULL);
		if (!utf8name)
			utf8name = g_strdup(dir_name);
#endif

		if (item->path) {
			utf8entry = g_strconcat(item->path, "/", utf8name,
						NULL);
		} else
			utf8entry = g_strdup(utf8name);
		entry = g_filename_from_utf8(utf8entry, -1, NULL, NULL, NULL);
		if (!entry)
			entry = g_strdup(utf8entry);
#ifdef G_OS_WIN32
		subst_char(entry, '/', G_DIR_SEPARATOR);
#endif

		if (
#ifdef G_OS_WIN32
			(wfd.file_attr & FILE_ATTRIBUTE_DIRECTORY) != 0
#else
#if HAVE_DIRENT_D_TYPE
			d->d_type == DT_DIR ||
			((d->d_type == DT_UNKNOWN || d->d_type == DT_LNK) &&
#endif
			g_stat(entry, &s) == 0 && S_ISDIR(s.st_mode)
#if HAVE_DIRENT_D_TYPE
			)
#endif
#endif /* G_OS_WIN32 */
		   ) {
			FolderItem *new_item = NULL;
			GNode *node;

#ifndef G_OS_WIN32
			if (g_utf8_validate(utf8name, -1, NULL) == FALSE) {
				g_warning(_("Directory name\n"
					    "'%s' is not a valid UTF-8 string.\n"
					    "Maybe the locale encoding is used for filename.\n"
					    "If that is the case, you must set the following environmental variable\n"
					    "(see README for detail):\n"
					    "\n"
					    "\tG_FILENAME_ENCODING=@locale\n"),
					  utf8name);
				g_free(entry);
				g_free(utf8entry);
				g_free(utf8name);
				continue;
			}
#endif /* G_OS_WIN32 */

			node = item->node;
			for (node = node->children; node != NULL; node = node->next) {
				FolderItem *cur_item = FOLDER_ITEM(node->data);
				if (!strcmp2(cur_item->path, utf8entry)) {
					new_item = cur_item;
					break;
				}
			}
			if (!new_item) {
				debug_print("new folder '%s' found.\n",
					    utf8entry);
				new_item = folder_item_new(utf8name, utf8entry);
				folder_item_append(item, new_item);
			}

			if (!item->path) {
				if (!folder->inbox &&
				    !strcmp(dir_name, INBOX_DIR)) {
					new_item->stype = F_INBOX;
					folder->inbox = new_item;
				} else if (!folder->outbox &&
					   !strcmp(dir_name, OUTBOX_DIR)) {
					new_item->stype = F_OUTBOX;
					folder->outbox = new_item;
				} else if (!folder->draft &&
					   !strcmp(dir_name, DRAFT_DIR)) {
					new_item->stype = F_DRAFT;
					folder->draft = new_item;
				} else if (!folder->queue &&
					   !strcmp(dir_name, QUEUE_DIR)) {
					new_item->stype = F_QUEUE;
					folder->queue = new_item;
				} else if (!folder->trash &&
					   !strcmp(dir_name, TRASH_DIR)) {
					new_item->stype = F_TRASH;
					folder->trash = new_item;
				} else if (!folder_get_junk(folder) &&
					   !strcmp(dir_name, JUNK_DIR)) {
					new_item->stype = F_JUNK;
					folder_set_junk(folder, new_item);
				}
			}

			mh_scan_tree_recursive(new_item);
		} else if (to_number(dir_name) > 0) n_msg++;

		g_free(entry);
		g_free(utf8entry);
		g_free(utf8name);
#ifdef G_OS_WIN32
	} while (find_next_file(hfind, &wfd));
#else
	}
#endif

#ifdef G_OS_WIN32
	FindClose(hfind);
#else
	closedir(dp);
#endif

	if (item->path) {
		gint new, unread, total, min, max;

		procmsg_get_mark_sum
			(item, &new, &unread, &total, &min, &max, 0);
		if (n_msg > total) {
			new += n_msg - total;
			unread += n_msg - total;
		}
		item->new = new;
		item->unread = unread;
		item->total = n_msg;
		item->updated = TRUE;
		item->mtime = 0;
	}
}

static gboolean mh_rename_folder_func(GNode *node, gpointer data)
{
	FolderItem *item = node->data;
	gchar **paths = data;
	const gchar *oldpath = paths[0];
	const gchar *newpath = paths[1];
	gchar *base;
	gchar *new_itempath;
	gint oldpathlen;

	oldpathlen = strlen(oldpath);
	if (strncmp(oldpath, item->path, oldpathlen) != 0) {
		g_warning("path doesn't match: %s, %s\n", oldpath, item->path);
		return TRUE;
	}

	base = item->path + oldpathlen;
	while (*base == '/') base++;
	if (*base == '\0')
		new_itempath = g_strdup(newpath);
	else
		new_itempath = g_strconcat(newpath, "/", base, NULL);
	g_free(item->path);
	item->path = new_itempath;

	return FALSE;
}
