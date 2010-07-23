/*
 * LibSylph -- E-Mail client library
 * Copyright (C) 1999-2010 Hiroyuki Yamamoto
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

#undef MEASURE_TIME

#include "folder.h"
#include "virtual.h"
#include "mh.h"
#include "procmsg.h"
#include "procheader.h"
#include "filter.h"
#include "utils.h"

typedef struct _VirtualSearchInfo	VirtualSearchInfo;
typedef struct _SearchCacheInfo		SearchCacheInfo;

struct _VirtualSearchInfo {
	FilterRule *rule;
	GSList *mlist;
	GHashTable *search_cache_table;
	FILE *fp;
	gboolean requires_full_headers;
	gboolean exclude_trash;
};

struct _SearchCacheInfo {
	FolderItem *folder;
	guint msgnum;
	off_t size;
	time_t mtime;
	MsgFlags flags;
};

enum
{
	SCACHE_NOT_EXIST = 0,
	SCACHE_MATCHED = 1,
	SCACHE_NOT_MATCHED = 2
};

static void	virtual_folder_init	(Folder		*folder,
					 const gchar	*name,
					 const gchar	*path);

static GHashTable *virtual_read_search_cache
					(FolderItem	*item);
static void virtual_write_search_cache	(FILE		*fp,
					 FolderItem	*item,
					 MsgInfo	*msginfo,
					 gint		 matched);

static GSList *virtual_search_folder	(VirtualSearchInfo	*info,
					 FolderItem		*item);
static gboolean virtual_search_recursive_func
					(GNode		*node,
					 gpointer	 data);

static Folder	*virtual_folder_new	(const gchar	*name,
					 const gchar	*path);
static void     virtual_folder_destroy	(Folder		*folder);

static GSList  *virtual_get_msg_list	(Folder		*folder,
					 FolderItem	*item,
					 gboolean	 use_cache);
static gchar   *virtual_fetch_msg	(Folder		*folder,
					 FolderItem	*item,
					 gint		 num);
static MsgInfo *virtual_get_msginfo	(Folder		*folder,
					 FolderItem	*item,
					 gint		 num);
static gint    virtual_close		(Folder		*folder,
					 FolderItem	*item);

static gint    virtual_scan_folder	(Folder		*folder,
					 FolderItem	*item);

static gint    virtual_rename_folder	(Folder		*folder,
					 FolderItem	*item,
					 const gchar	*name);
static gint    virtual_move_folder	(Folder		*folder,
					 FolderItem	*item,
					 FolderItem	*new_parent);
static gint    virtual_remove_folder	(Folder		*folder,
					 FolderItem	*item);

static FolderClass virtual_class =
{
	F_VIRTUAL,

	virtual_folder_new,
	virtual_folder_destroy,

	NULL,
	NULL,

	virtual_get_msg_list,
	NULL,
	virtual_fetch_msg,
	virtual_get_msginfo,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	virtual_close,
	virtual_scan_folder,

	NULL,
	virtual_rename_folder,
	virtual_move_folder,
	virtual_remove_folder,
};


FolderClass *virtual_get_class(void)
{
	return &virtual_class;
}

static Folder *virtual_folder_new(const gchar *name, const gchar *path)
{
	Folder *folder;

	folder = (Folder *)g_new0(VirtualFolder, 1);
	virtual_folder_init(folder, name, path);

	return folder;
}

static void virtual_folder_destroy(Folder *folder)
{
	folder_local_folder_destroy(LOCAL_FOLDER(folder));
}

static void virtual_folder_init(Folder *folder, const gchar *name,
				const gchar *path)
{
	folder->klass = virtual_get_class();
	folder_local_folder_init(folder, name, path);
}

guint sinfo_hash(gconstpointer key)
{
	const SearchCacheInfo *sinfo = key;
	guint h;

	h = (guint)sinfo->folder;
	h ^= sinfo->msgnum;
	h ^= (guint)sinfo->size;
	h ^= (guint)sinfo->mtime;
	/* h ^= (guint)sinfo->flags.tmp_flags; */
	h ^= (guint)sinfo->flags.perm_flags;

	/* g_print("path: %s, n = %u, hash = %u\n",
		   sinfo->folder->path, sinfo->msgnum, h); */

	return h;
}

gint sinfo_equal(gconstpointer v, gconstpointer v2)
{
	const SearchCacheInfo *s1 = v;
	const SearchCacheInfo *s2 = v2;

	return (s1->folder == s2->folder && s1->msgnum == s2->msgnum &&
		s1->size == s2->size && s1->mtime == s2->mtime &&
		/* s1->flags.tmp_flags == s2->flags.tmp_flags && */
		s1->flags.perm_flags == s2->flags.perm_flags);
}

#define READ_CACHE_DATA_INT(n, fp)			\
{							\
	guint32 idata;					\
							\
	if (fread(&idata, sizeof(idata), 1, fp) != 1) {	\
		g_warning("Cache data is corrupted\n");	\
		fclose(fp);				\
		return table;				\
	} else						\
		n = idata;				\
}

static GHashTable *virtual_read_search_cache(FolderItem *item)
{
	GHashTable *table;
	gchar *path, *file;
	FILE *fp;
	gchar *id;
	gint count = 0;

	g_return_val_if_fail(item != NULL, NULL);

	path = folder_item_get_path(item);
	file = g_strconcat(path, G_DIR_SEPARATOR_S, SEARCH_CACHE, NULL);
	debug_print("reading search cache: %s\n", file);
	fp = procmsg_open_data_file(file, SEARCH_CACHE_VERSION, DATA_READ,
				    NULL, 0);
	g_free(file);
	g_free(path);
	if (!fp)
		return NULL;

	table = g_hash_table_new(sinfo_hash, sinfo_equal);

	while (procmsg_read_cache_data_str(fp, &id) == 0) {
		FolderItem *folder;
		guint32 msgnum;
		off_t size;
		time_t mtime;
		MsgFlags flags;
		gint matched;
		SearchCacheInfo *sinfo;

		folder = folder_find_item_from_identifier(id);
		g_free(id);

		while (fread(&msgnum, sizeof(msgnum), 1, fp) == 1) {
			if (msgnum == 0)
				break;

			READ_CACHE_DATA_INT(size, fp);
			READ_CACHE_DATA_INT(mtime, fp);
			READ_CACHE_DATA_INT(flags.tmp_flags, fp);
			READ_CACHE_DATA_INT(flags.perm_flags, fp);
			READ_CACHE_DATA_INT(matched, fp);

			if (folder) {
				sinfo = g_new(SearchCacheInfo, 1);
				sinfo->folder = folder;
				sinfo->msgnum = msgnum;
				sinfo->size = size;
				sinfo->mtime = mtime;
				sinfo->flags = flags;
				g_hash_table_insert(table, sinfo,
						    GINT_TO_POINTER(matched));
				++count;
			}
		}
	}

	debug_print("%d cache items read.\n", count);

	fclose(fp);
	return table;
}

static void virtual_write_search_cache(FILE *fp, FolderItem *item,
				       MsgInfo *msginfo, gint matched)
{
	if (!item && !msginfo) {
		WRITE_CACHE_DATA_INT(0, fp);
		return;
	}

	if (item) {
		gchar *id;

		id = folder_item_get_identifier(item);
		if (id) {
			WRITE_CACHE_DATA(id, fp);
			g_free(id);
		}
	}

	if (msginfo) {
		WRITE_CACHE_DATA_INT(msginfo->msgnum, fp);
		WRITE_CACHE_DATA_INT(msginfo->size, fp);
		WRITE_CACHE_DATA_INT(msginfo->mtime, fp);
		WRITE_CACHE_DATA_INT
			((msginfo->flags.tmp_flags & MSG_CACHED_FLAG_MASK), fp);
		WRITE_CACHE_DATA_INT(msginfo->flags.perm_flags, fp);
		WRITE_CACHE_DATA_INT(matched, fp);
	}
}

static void search_cache_free_func(gpointer key, gpointer value, gpointer data)
{
	g_free(key);
}

static void virtual_search_cache_free(GHashTable *table)
{
	if (table) {
		g_hash_table_foreach(table, search_cache_free_func, NULL);
		g_hash_table_destroy(table);
	}
}

static GSList *virtual_search_folder(VirtualSearchInfo *info, FolderItem *item)
{
	GSList *match_list = NULL;
	GSList *mlist;
	GSList *cur;
	FilterInfo fltinfo;
	gint count = 1, total, ncachehit = 0;
	GTimeVal tv_prev, tv_cur;

	g_return_val_if_fail(info != NULL, NULL);
	g_return_val_if_fail(info->rule != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(item->path != NULL, NULL);

	/* prevent circular reference */
	if (item->stype == F_VIRTUAL)
		return NULL;

	g_get_current_time(&tv_prev);
	status_print(_("Searching %s ..."), item->path);

	mlist = folder_item_get_msg_list(item, TRUE);
	total = g_slist_length(mlist);

	memset(&fltinfo, 0, sizeof(FilterInfo));

	debug_print("start query search: %s\n", item->path);

	virtual_write_search_cache(info->fp, item, NULL, 0);

	for (cur = mlist; cur != NULL; cur = cur->next) {
		MsgInfo *msginfo = (MsgInfo *)cur->data;
		GSList *hlist;

		g_get_current_time(&tv_cur);
		if (tv_cur.tv_sec > tv_prev.tv_sec ||
		    tv_cur.tv_usec - tv_prev.tv_usec >
		    PROGRESS_UPDATE_INTERVAL * 1000) {
			status_print(_("Searching %s (%d / %d)..."),
				     item->path, count, total);
			tv_prev = tv_cur;
		}
		++count;

		if (info->search_cache_table) {
			gint matched;
			SearchCacheInfo sinfo;

			sinfo.folder = item;
			sinfo.msgnum = msginfo->msgnum;
			sinfo.size = msginfo->size;
			sinfo.mtime = msginfo->mtime;
			sinfo.flags = msginfo->flags;

			matched = (gint)g_hash_table_lookup
				(info->search_cache_table, &sinfo);
			if (matched == SCACHE_MATCHED) {
				match_list = g_slist_prepend
					(match_list, msginfo);
				cur->data = NULL;
				virtual_write_search_cache(info->fp, NULL,
							   msginfo, matched);
				++ncachehit;
				continue;
			} else if (matched == SCACHE_NOT_MATCHED) {
				virtual_write_search_cache(info->fp, NULL,
							   msginfo, matched);
				++ncachehit;
				continue;
			}
		}

		fltinfo.flags = msginfo->flags;
		if (info->requires_full_headers) {
			gchar *file;

			file = procmsg_get_message_file(msginfo);
			hlist = procheader_get_header_list_from_file(file);
			g_free(file);
		} else
			hlist = procheader_get_header_list_from_msginfo
				(msginfo);
		if (!hlist)
			continue;

		if (filter_match_rule(info->rule, msginfo, hlist, &fltinfo)) {
			match_list = g_slist_prepend(match_list, msginfo);
			cur->data = NULL;
			virtual_write_search_cache(info->fp, NULL, msginfo,
						   SCACHE_MATCHED);
		} else {
			virtual_write_search_cache(info->fp, NULL, msginfo,
						   SCACHE_NOT_MATCHED);
		}

		procheader_header_list_destroy(hlist);
	}

	debug_print("%d cache hits (%d total)\n", ncachehit, total);

	virtual_write_search_cache(info->fp, NULL, NULL, 0);
	procmsg_msg_list_free(mlist);

	return g_slist_reverse(match_list);
}

static gboolean virtual_search_recursive_func(GNode *node, gpointer data)
{
	VirtualSearchInfo *info = (VirtualSearchInfo *)data;
	FolderItem *item;
	GSList *mlist;

	g_return_val_if_fail(node->data != NULL, FALSE);

	item = FOLDER_ITEM(node->data);

	if (!item->path)
		return FALSE;
	if (info->exclude_trash && item->stype == F_TRASH)
		return FALSE;

	mlist = virtual_search_folder(info, item);
	info->mlist = g_slist_concat(info->mlist, mlist);

	return FALSE;
}

static GSList *virtual_get_msg_list(Folder *folder, FolderItem *item,
				    gboolean use_cache)
{
	GSList *mlist = NULL;
	GSList *flist;
	GSList *cur;
	FilterRule *rule;
	gchar *path;
	gchar *rule_file;
	gchar *cache_file;
	FolderItem *target;
	gint new = 0, unread = 0, total = 0;
	VirtualSearchInfo info;

	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(item->stype == F_VIRTUAL, NULL);

	path = folder_item_get_path(item);
	rule_file = g_strconcat(path, G_DIR_SEPARATOR_S, "filter.xml", NULL);
	flist = filter_read_file(rule_file);
	g_free(rule_file);

	g_free(path);

	if (!flist) {
		g_warning("filter rule not found\n");
		return NULL;
	}

	rule = (FilterRule *)flist->data;
	target = folder_find_item_from_identifier(rule->target_folder);

	if (!target || target == item) {
		g_warning("invalid target folder\n");
		goto finish;
	}

	info.rule = rule;
	info.mlist = NULL;
	if (use_cache)
		info.search_cache_table = virtual_read_search_cache(item);
	else
		info.search_cache_table = NULL;

	path = folder_item_get_path(item);
	cache_file = g_strconcat(path, G_DIR_SEPARATOR_S, SEARCH_CACHE, NULL);
	info.fp = procmsg_open_data_file(cache_file, SEARCH_CACHE_VERSION,
					 DATA_WRITE, NULL, 0);
	g_free(cache_file);
	g_free(path);
	if (!info.fp)
		goto finish;

	info.requires_full_headers =
		filter_rule_requires_full_headers(rule);

	if (rule->recursive) {
		if (target->stype == F_TRASH)
			info.exclude_trash = FALSE;
		else
			info.exclude_trash = TRUE;
	} else
		info.exclude_trash = FALSE;

	if (rule->recursive) {
		g_node_traverse(target->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
				virtual_search_recursive_func, &info);
		mlist = info.mlist;
	} else
		mlist = virtual_search_folder(&info, target);

	fclose(info.fp);
	virtual_search_cache_free(info.search_cache_table);

	for (cur = mlist; cur != NULL; cur = cur->next) {
		MsgInfo *msginfo = (MsgInfo *)cur->data;

		if (MSG_IS_NEW(msginfo->flags))
			++new;
		if (MSG_IS_UNREAD(msginfo->flags))
			++unread;
		++total;
	}

	item->new = new;
	item->unread = unread;
	item->total = total;
	item->updated = TRUE;

finish:
	filter_rule_list_free(flist);
	return mlist;
}

static gchar *virtual_fetch_msg(Folder *folder, FolderItem *item, gint num)
{
	return NULL;
}

static MsgInfo *virtual_get_msginfo(Folder *folder, FolderItem *item, gint num)
{
	return NULL;
}

static gint virtual_close(Folder *folder, FolderItem *item)
{
	return 0;
}

static gint virtual_scan_folder(Folder *folder, FolderItem *item)
{
	return 0;
}

static gint virtual_rename_folder(Folder *folder, FolderItem *item,
				  const gchar *name)
{
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->stype == F_VIRTUAL, -1);

	return mh_get_class()->rename_folder(folder, item, name);
}

static gint virtual_move_folder(Folder *folder, FolderItem *item,
				FolderItem *new_parent)
{
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->stype == F_VIRTUAL, -1);

	return mh_get_class()->move_folder(folder, item, new_parent);
}

static gint virtual_remove_folder(Folder *folder, FolderItem *item)
{
	gchar *path;

	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->stype == F_VIRTUAL, -1);

	path = folder_item_get_path(item);
	if (remove_dir_recursive(path) < 0) {
		g_warning("can't remove directory '%s'\n", path);
		g_free(path);
		return -1;
	}

	g_free(path);
	folder_item_remove(item);
	return 0;
}
