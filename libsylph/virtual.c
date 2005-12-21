/*
 * LibSylph -- E-Mail client library
 * Copyright (C) 1999-2005 Hiroyuki Yamamoto
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
#include "procmsg.h"
#include "procheader.h"
#include "filter.h"
#include "utils.h"

typedef struct _VirtualSearchInfo	VirtualSearchInfo;

struct _VirtualSearchInfo {
	FilterRule *rule;
	GSList *mlist;
};

static void	virtual_folder_init	(Folder		*folder,
					 const gchar	*name,
					 const gchar	*path);

static GSList *virtual_search_folder	(FilterRule	*rule,
					 FolderItem	*item);
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

static FolderClass virtual_class =
{
	F_VIRTUAL,

	virtual_folder_new,
	virtual_folder_destroy,

	NULL,
	NULL,

	virtual_get_msg_list,
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
	virtual_close,
	virtual_scan_folder,

	NULL,
	NULL,
	NULL,
	NULL,
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

static GSList *virtual_search_folder(FilterRule *rule, FolderItem *item)
{
	GSList *match_list = NULL;
	GSList *mlist;
	GSList *cur;
	FilterInfo fltinfo;
	gboolean full_headers;
	gint count = 1, total;
	GTimeVal tv_prev, tv_cur;

	g_return_val_if_fail(rule != NULL, NULL);
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

	full_headers = filter_rule_requires_full_headers(rule);

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

		fltinfo.flags = msginfo->flags;
		if (full_headers) {
			gchar *file;

			file = procmsg_get_message_file(msginfo);
			hlist = procheader_get_header_list_from_file(file);
			g_free(file);
		} else
			hlist = procheader_get_header_list_from_msginfo
				(msginfo);
		if (!hlist)
			continue;

		if (filter_match_rule(rule, msginfo, hlist, &fltinfo)) {
			match_list = g_slist_prepend(match_list, msginfo);
			cur->data = NULL;
		}

		procheader_header_list_destroy(hlist);
	}

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

	mlist = virtual_search_folder(info->rule, item);
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
	gchar *rule_file;
	gchar *path;
	FolderItem *target;
	gint new = 0, unread = 0, total = 0;

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

	if (rule->recursive) {
		VirtualSearchInfo info;

		info.rule = rule;
		info.mlist = NULL;
		g_node_traverse(target->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
				virtual_search_recursive_func, &info);
		mlist = info.mlist;
	} else
		mlist = virtual_search_folder(rule, target);

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
	/* do search */
	return 0;
}
