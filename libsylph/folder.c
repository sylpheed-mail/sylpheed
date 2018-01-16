/*
 * LibSylph -- E-Mail client library
 * Copyright (C) 1999-2012 Hiroyuki Yamamoto
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
#include <stdlib.h>

#include "folder.h"
#include "session.h"
#include "imap.h"
#include "news.h"
#include "mh.h"
#include "virtual.h"
#include "utils.h"
#include "xml.h"
#include "codeconv.h"
#include "prefs.h"
#include "account.h"
#include "prefs_account.h"
#include "sylmain.h"

typedef struct _FolderPrivData FolderPrivData;

struct _FolderPrivData {
	Folder *folder;
	FolderItem *junk;

	FolderUIFunc2 ui_func2;
	gpointer ui_func2_data;

	gpointer data;
};

static GList *folder_list = NULL;
static GList *folder_priv_list = NULL;

static void folder_init		(Folder		*folder,
				 const gchar	*name);

static FolderPrivData *folder_get_priv	(Folder		*folder);

static gboolean folder_read_folder_func	(GNode		*node,
					 gpointer	 data);
static gchar *folder_get_list_path	(void);
static void folder_write_list_recursive	(GNode		*node,
					 gpointer	 data);


Folder *folder_new(FolderType type, const gchar *name, const gchar *path)
{
	Folder *folder = NULL;

	name = name ? name : path;
	switch (type) {
	case F_MH:
		folder = mh_get_class()->folder_new(name, path);
		break;
	case F_IMAP:
		folder = imap_get_class()->folder_new(name, path);
		break;
	case F_NEWS:
		folder = news_get_class()->folder_new(name, path);
		break;
	default:
		return NULL;
	}

	return folder;
}

static void folder_init(Folder *folder, const gchar *name)
{
	FolderItem *item;

	g_return_if_fail(folder != NULL);

	folder_set_name(folder, name);
	folder->account = NULL;
	folder->inbox = NULL;
	folder->outbox = NULL;
	folder->draft = NULL;
	folder->queue = NULL;
	folder->trash = NULL;
	folder->ui_func = NULL;
	folder->ui_func_data = NULL;
	item = folder_item_new(name, NULL);
	item->folder = folder;
	folder->node = item->node = g_node_new(item);
	folder->data = NULL;
}

void folder_local_folder_init(Folder *folder, const gchar *name,
			      const gchar *path)
{
	folder_init(folder, name);
	LOCAL_FOLDER(folder)->rootpath = g_strdup(path);
}

void folder_remote_folder_init(Folder *folder, const gchar *name,
			       const gchar *path)
{
	folder_init(folder, name);
	REMOTE_FOLDER(folder)->session = NULL;
	REMOTE_FOLDER(folder)->remove_cache_on_destroy = TRUE;
}

void folder_destroy(Folder *folder)
{
	FolderPrivData *priv;

	g_return_if_fail(folder != NULL);
	g_return_if_fail(folder->klass->destroy != NULL);

	debug_print("folder_destroy: destroying Folder (%p)\n", folder);

	folder->klass->destroy(folder);

	folder_list = g_list_remove(folder_list, folder);

	folder_tree_destroy(folder);

	priv = folder_get_priv(folder);
	folder_priv_list = g_list_remove(folder_priv_list, priv);
	g_free(priv);

	g_free(folder->name);
	g_free(folder);
}

void folder_local_folder_destroy(LocalFolder *lfolder)
{
	g_return_if_fail(lfolder != NULL);

	g_free(lfolder->rootpath);
}

void folder_remote_folder_destroy(RemoteFolder *rfolder)
{
	g_return_if_fail(rfolder != NULL);

	if (rfolder->session)
		session_destroy(rfolder->session);
}

gint folder_remote_folder_destroy_all_sessions(void)
{
	GList *list;
	Folder *folder;
	RemoteFolder *rfolder;

	for (list = folder_list; list != NULL; list = list->next) {
		folder = FOLDER(list->data);
		if (FOLDER_IS_REMOTE(folder)) {
			rfolder = REMOTE_FOLDER(folder);
			if (rfolder->session &&
			    !folder_remote_folder_is_session_active(rfolder)) {
				session_destroy(rfolder->session);
				rfolder->session = NULL;
			}
		}
	}

	return 0;
}

gboolean folder_remote_folder_is_session_active(RemoteFolder *rfolder)
{
	g_return_val_if_fail(rfolder != NULL, FALSE);

	if (FOLDER_TYPE(rfolder) == F_IMAP)
		return imap_is_session_active(IMAP_FOLDER(rfolder));

	return FALSE;
}

gboolean folder_remote_folder_active_session_exist(void)
{
	GList *list;
	Folder *folder;
	RemoteFolder *rfolder;

	for (list = folder_list; list != NULL; list = list->next) {
		folder = FOLDER(list->data);
		if (FOLDER_IS_REMOTE(folder)) {
			rfolder = REMOTE_FOLDER(folder);
			if (folder_remote_folder_is_session_active(rfolder))
				return TRUE;
		}
	}

	return FALSE;
}

gint folder_scan_tree(Folder *folder)
{
	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(folder->klass->scan_tree != NULL, -1);

	return folder->klass->scan_tree(folder);
}

gint folder_create_tree(Folder *folder)
{
	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(folder->klass->create_tree != NULL, -1);

	return folder->klass->create_tree(folder);
}

FolderItem *folder_item_new(const gchar *name, const gchar *path)
{
	FolderItem *item;

	item = g_new0(FolderItem, 1);

	item->stype = F_NORMAL;
	item->name = g_strdup(name);
	item->path = g_strdup(path);
	item->mtime = 0;
	item->new = 0;
	item->unread = 0;
	item->total = 0;
	item->unmarked_num = 0;
	item->last_num = -1;
	item->no_sub = FALSE;
	item->no_select = FALSE;
	item->collapsed = FALSE;
	item->threaded = TRUE;
	item->opened = FALSE;
	item->updated = FALSE;
	item->cache_dirty = FALSE;
	item->mark_dirty = FALSE;
	item->node = NULL;
	item->parent = NULL;
	item->folder = NULL;
	item->account = NULL;
	item->ac_apply_sub = FALSE;
	item->auto_to = NULL;
	item->use_auto_to_on_reply = FALSE;
	item->auto_cc = NULL;
	item->auto_bcc = NULL;
	item->auto_replyto = NULL;
	item->mark_queue = NULL;
	item->last_selected = 0;
	item->qsearch_cond_type = 0;
	item->data = NULL;

	return item;
}

void folder_item_append(FolderItem *parent, FolderItem *item)
{
	g_return_if_fail(parent != NULL);
	g_return_if_fail(parent->folder != NULL);
	g_return_if_fail(parent->node != NULL);
	g_return_if_fail(item != NULL);

	item->parent = parent;
	item->folder = parent->folder;
	item->node = g_node_append_data(parent->node, item);
}

FolderItem *folder_item_copy(FolderItem *item)
{
	FolderItem *new_item;

	new_item = g_new0(FolderItem, 1);

	new_item->stype = item->stype;
	new_item->name = g_strdup(item->name);
	new_item->path = g_strdup(item->path);
	new_item->mtime = item->mtime;
	new_item->new = item->new;
	new_item->unread = item->unread;
	new_item->total = item->total;
	new_item->unmarked_num = item->unmarked_num;
	new_item->last_num = item->last_num;
	new_item->no_sub = item->no_sub;
	new_item->no_select = item->no_select;
	new_item->collapsed = item->collapsed;
	new_item->threaded = item->threaded;
	new_item->opened = item->opened;
	new_item->updated = item->updated;
	new_item->cache_dirty = item->cache_dirty;
	new_item->mark_dirty = item->mark_dirty;
	new_item->node = item->node;
	new_item->parent = item->parent;
	new_item->folder = item->folder;
	new_item->account = item->account;
	new_item->ac_apply_sub = item->ac_apply_sub;
	new_item->auto_to = g_strdup(item->auto_to);
	new_item->use_auto_to_on_reply = item->use_auto_to_on_reply;
	new_item->auto_cc = g_strdup(item->auto_cc);
	new_item->auto_bcc = g_strdup(item->auto_bcc);
	new_item->auto_replyto = g_strdup(item->auto_replyto);
	new_item->mark_queue = item->mark_queue;
	new_item->last_selected = item->last_selected;
	new_item->qsearch_cond_type = item->qsearch_cond_type;
	new_item->data = item->data;

	return new_item;
}

static gboolean folder_item_remove_func(GNode *node, gpointer data)
{
	FolderItem *item = FOLDER_ITEM(node->data);

	folder_item_destroy(item);
	return FALSE;
}

void folder_item_remove(FolderItem *item)
{
	GNode *node;

	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(item->node != NULL);

	node = item->node;

	if (item->folder->node == node)
		item->folder->node = NULL;

	g_node_traverse(node, G_POST_ORDER, G_TRAVERSE_ALL, -1,
			folder_item_remove_func, NULL);
	g_node_destroy(node);
}

void folder_item_remove_children(FolderItem *item)
{
	GNode *node, *next;

	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(item->node != NULL);

	node = item->node->children;
	while (node != NULL) {
		next = node->next;
		folder_item_remove(FOLDER_ITEM(node->data));
		node = next;
	}
}

void folder_item_destroy(FolderItem *item)
{
	Folder *folder;

	g_return_if_fail(item != NULL);

	folder = item->folder;
	if (folder) {
		if (folder->inbox == item)
			folder->inbox = NULL;
		else if (folder->outbox == item)
			folder->outbox = NULL;
		else if (folder->draft == item)
			folder->draft = NULL;
		else if (folder->queue == item)
			folder->queue = NULL;
		else if (folder->trash == item)
			folder->trash = NULL;
		else if (folder_get_junk(folder) == item)
			folder_set_junk(folder, NULL);
	}

	g_free(item->name);
	g_free(item->path);
	g_free(item->auto_to);
	g_free(item->auto_cc);
	g_free(item->auto_bcc);
	g_free(item->auto_replyto);
	g_free(item);
}

gint folder_item_compare(FolderItem *item_a, FolderItem *item_b)
{
	gint ret;
	gchar *str_a, *str_b;

	if (!item_a || !item_b)
		return 0;
	if (!item_a->parent || !item_b->parent)
		return 0;
	if (!item_a->name || !item_b->name)
		return 0;

	/* if both a and b are special folders, sort them according to
	 * their types (which is in-order). Note that this assumes that
	 * there are no multiple folders of a special type. As a special
	 * case, two virtual folders are compared like normal ones. */
	if (item_a->stype != F_NORMAL && item_b->stype != F_NORMAL &&
	    (item_a->stype != F_VIRTUAL || item_b->stype != F_VIRTUAL))
		return item_a->stype - item_b->stype;

	 /* if b is normal folder, and a is not, b is smaller (ends up
	  * lower in the list) */
	if (item_a->stype != F_NORMAL && item_b->stype == F_NORMAL)
		return item_b->stype - item_a->stype;

	/* if b is special folder, and a is not, b is larger (ends up
	 * higher in the list) */
	if (item_a->stype == F_NORMAL && item_b->stype != F_NORMAL)
		return item_b->stype - item_a->stype;

	/* otherwise just compare the folder names */
	str_a = g_utf8_casefold(item_a->name, -1);
	str_b = g_utf8_casefold(item_b->name, -1);
	ret = g_utf8_collate(str_a, str_b);
	g_free(str_b);
	g_free(str_a);

	return ret;
}

void folder_set_ui_func(Folder *folder, FolderUIFunc func, gpointer data)
{
	g_return_if_fail(folder != NULL);

	folder->ui_func = func;
	folder->ui_func_data = data;
}

void folder_set_ui_func2(Folder *folder, FolderUIFunc2 func, gpointer data)
{
	FolderPrivData *priv;

	priv = folder_get_priv(folder);
	if (priv) {
		priv->ui_func2 = func;
		priv->ui_func2_data = data;
	}
}

FolderUIFunc2 folder_get_ui_func2(Folder *folder)
{
	FolderPrivData *priv;

	priv = folder_get_priv(folder);
	if (priv)
		return priv->ui_func2;

	return NULL;
}

gboolean folder_call_ui_func2(Folder *folder, FolderItem *item, guint count,
			      guint total)
{
	FolderPrivData *priv;

	priv = folder_get_priv(folder);
	if (priv && priv->ui_func2) {
		return priv->ui_func2(folder, item, count, total,
				      priv->ui_func2_data);
	}

	return TRUE;
}

void folder_set_name(Folder *folder, const gchar *name)
{
	g_return_if_fail(folder != NULL);

	g_free(folder->name);
	folder->name = name ? g_strdup(name) : NULL;
	if (folder->node && folder->node->data) {
		FolderItem *item = (FolderItem *)folder->node->data;

		g_free(item->name);
		item->name = name ? g_strdup(name) : NULL;
	}
}

void folder_tree_destroy(Folder *folder)
{
	g_return_if_fail(folder != NULL);

	if (folder->node)
		folder_item_remove(FOLDER_ITEM(folder->node->data));
}

void folder_add(Folder *folder)
{
	Folder *cur_folder;
	GList *cur;
	gint i;
	FolderPrivData *priv;

	debug_print("Adding Folder (%p) to folder list\n", folder);

	g_return_if_fail(folder != NULL);

	for (i = 0, cur = folder_list; cur != NULL; cur = cur->next, i++) {
		cur_folder = FOLDER(cur->data);
		if (FOLDER_TYPE(folder) == F_MH) {
			if (FOLDER_TYPE(cur_folder) != F_MH) break;
		} else if (FOLDER_TYPE(folder) == F_IMAP) {
			if (FOLDER_TYPE(cur_folder) != F_MH &&
			    FOLDER_TYPE(cur_folder) != F_IMAP) break;
		} else if (FOLDER_TYPE(folder) == F_NEWS) {
			if (FOLDER_TYPE(cur_folder) != F_MH &&
			    FOLDER_TYPE(cur_folder) != F_IMAP &&
			    FOLDER_TYPE(cur_folder) != F_NEWS) break;
		}
	}

	folder_list = g_list_insert(folder_list, folder, i);
	priv = g_new0(FolderPrivData, 1);
	priv->folder = folder;
	folder_priv_list = g_list_insert(folder_priv_list, priv, i);
}

GList *folder_get_list(void)
{
	return folder_list;
}

gint folder_read_list(void)
{
	GNode *node;
	XMLNode *xmlnode;
	gchar *path;

	path = folder_get_list_path();
	if (!is_file_exist(path)) return -1;
	node = xml_parse_file(path);
	if (!node) return -1;

	xmlnode = node->data;
	if (strcmp2(xmlnode->tag->tag, "folderlist") != 0) {
		g_warning("wrong folder list\n");
		xml_free_tree(node);
		return -1;
	}

	g_node_traverse(node, G_PRE_ORDER, G_TRAVERSE_ALL, 2,
			folder_read_folder_func, NULL);

	xml_free_tree(node);
	if (folder_list)
		return 0;
	else
		return -1;
}

void folder_write_list(void)
{
	GList *list;
	Folder *folder;
	gchar *path;
	PrefFile *pfile;

	path = folder_get_list_path();
	if ((pfile = prefs_file_open(path)) == NULL) return;

	fprintf(pfile->fp, "<?xml version=\"1.0\" encoding=\"%s\"?>\n",
		CS_INTERNAL);
	fputs("\n<folderlist>\n", pfile->fp);

	for (list = folder_list; list != NULL; list = list->next) {
		folder = list->data;
		folder_write_list_recursive(folder->node, pfile->fp);
	}

	fputs("</folderlist>\n", pfile->fp);

	if (prefs_file_close(pfile) < 0)
		g_warning("failed to write folder list.\n");

	if (syl_app_get())
		g_signal_emit_by_name(syl_app_get(), "folderlist-updated");
}

struct TotalMsgStatus
{
	guint new;
	guint unread;
	guint total;
	GString *str;
};

static gboolean folder_get_status_full_all_func(GNode *node, gpointer data)
{
	FolderItem *item;
	struct TotalMsgStatus *status = (struct TotalMsgStatus *)data;
	gchar *id;

	g_return_val_if_fail(node->data != NULL, FALSE);

	item = FOLDER_ITEM(node->data);

	if (!item->path) return FALSE;

	status->new += item->new;
	status->unread += item->unread;
	status->total += item->total;

	if (status->str) {
		id = folder_item_get_identifier(item);
		g_string_sprintfa(status->str, "%5d %5d %5d %s\n",
				  item->new, item->unread,
				  item->total, id);
		g_free(id);
	}

	return FALSE;
}

static void folder_get_status_full_all(GString *str, guint *new, guint *unread,
				       guint *total)
{
	GList *list;
	Folder *folder;
	struct TotalMsgStatus status;

	status.new = status.unread = status.total = 0;
	status.str = str;

	debug_print("Counting total number of messages...\n");

	for (list = folder_list; list != NULL; list = list->next) {
		folder = FOLDER(list->data);
		if (folder->node)
			g_node_traverse(folder->node, G_PRE_ORDER,
					G_TRAVERSE_ALL, -1,
					folder_get_status_full_all_func,
					&status);
	}

	*new = status.new;
	*unread = status.unread;
	*total = status.total;
}

gchar *folder_get_status(GPtrArray *folders, gboolean full)
{
	guint new, unread, total;
	GString *str;
	gint i;
	gchar *ret;

	new = unread = total = 0;

	str = g_string_new(NULL);

	if (folders) {
		for (i = 0; i < folders->len; i++) {
			FolderItem *item;

			item = g_ptr_array_index(folders, i);
			new += item->new;
			unread += item->unread;
			total += item->total;

			if (full) {
				gchar *id;

				id = folder_item_get_identifier(item);
				g_string_sprintfa(str, "%5d %5d %5d %s\n",
						  item->new, item->unread,
						  item->total, id);
				g_free(id);
			}
		}
	} else {
		folder_get_status_full_all(full ? str : NULL,
					   &new, &unread, &total);
	}

	if (full)
		g_string_sprintfa(str, "%5d %5d %5d\n", new, unread, total);
	else
		g_string_sprintfa(str, "%d %d %d\n", new, unread, total);

	ret = str->str;
	g_string_free(str, FALSE);

	return ret;
}

Folder *folder_find_from_path(const gchar *path)
{
	GList *list;
	Folder *folder;

	for (list = folder_list; list != NULL; list = list->next) {
		folder = list->data;
		if (FOLDER_TYPE(folder) == F_MH &&
		    !path_cmp(LOCAL_FOLDER(folder)->rootpath, path))
			return folder;
	}

	return NULL;
}

Folder *folder_find_from_name(const gchar *name, FolderType type)
{
	GList *list;
	Folder *folder;

	for (list = folder_list; list != NULL; list = list->next) {
		folder = list->data;
		if (FOLDER_TYPE(folder) == type &&
		    strcmp2(name, folder->name) == 0)
			return folder;
	}

	return NULL;
}

static gboolean folder_item_find_func(GNode *node, gpointer data)
{
	FolderItem *item = node->data;
	gpointer *d = data;
	const gchar *path = d[0];

	if (path_cmp(path, item->path) != 0)
		return FALSE;

	d[1] = item;

	return TRUE;
}

FolderItem *folder_find_item_from_path(const gchar *path)
{
	Folder *folder;
	gpointer d[2];

	folder = folder_get_default_folder();
	g_return_val_if_fail(folder != NULL, NULL);

	d[0] = (gpointer)path;
	d[1] = NULL;
	g_node_traverse(folder->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			folder_item_find_func, d);
	return d[1];
}

FolderItem *folder_find_child_item_by_name(FolderItem *item, const gchar *name)
{
	GNode *node;
	FolderItem *child;
	const gchar *base;

	if (!name)
		return NULL;

	for (node = item->node->children; node != NULL; node = node->next) {
		child = FOLDER_ITEM(node->data);
		base = g_basename(child->path);
#ifdef G_OS_WIN32
		if (base && g_ascii_strcasecmp(base, name) == 0)
#else
		if (strcmp2(base, name) == 0)
#endif
			return child;
	}

	return NULL;
}

static const struct {
	gchar *str;
	FolderType type;
} type_str_table[] = {
	{"#mh"     , F_MH},
	{"#mbox"   , F_MBOX},
	{"#maildir", F_MAILDIR},
	{"#imap"   , F_IMAP},
	{"#news"   , F_NEWS}
};

static gchar *folder_get_type_string(FolderType type)
{
	gint i;

	for (i = 0; i < sizeof(type_str_table) / sizeof(type_str_table[0]);
	     i++) {
		if (type_str_table[i].type == type)
			return type_str_table[i].str;
	}

	return NULL;
}

static FolderType folder_get_type_from_string(const gchar *str)
{
	gint i;

	for (i = 0; i < sizeof(type_str_table) / sizeof(type_str_table[0]);
	     i++) {
		if (g_ascii_strcasecmp(type_str_table[i].str, str) == 0)
			return type_str_table[i].type;
	}

	return F_UNKNOWN;
}

gchar *folder_get_identifier(Folder *folder)
{
	gchar *type_str;

	g_return_val_if_fail(folder != NULL, NULL);

	type_str = folder_get_type_string(FOLDER_TYPE(folder));
	return g_strconcat(type_str, "/", folder->name, NULL);
}

gchar *folder_item_get_identifier(FolderItem *item)
{
	gchar *id;
	gchar *folder_id;

	g_return_val_if_fail(item != NULL, NULL);

	if (!item->path) {
		if (!item->parent)
			return folder_get_identifier(item->folder);
		else
			return NULL;
	}

	folder_id = folder_get_identifier(item->folder);
	id = g_strconcat(folder_id, "/", item->path, NULL);
	g_free(folder_id);

	return id;
}

FolderItem *folder_find_item_from_identifier(const gchar *identifier)
{
	Folder *folder;
	gpointer d[2];
	gchar *str;
	gchar *p;
	gchar *name;
	gchar *path;
	FolderType type;

	g_return_val_if_fail(identifier != NULL, NULL);

	if (*identifier != '#')
		return folder_find_item_from_path(identifier);

	Xstrdup_a(str, identifier, return NULL);

	p = strchr(str, '/');
	if (!p)
		return folder_find_item_from_path(identifier);
	*p = '\0';
	p++;
	type = folder_get_type_from_string(str);
	if (type == F_UNKNOWN)
		return folder_find_item_from_path(identifier);

	name = p;
	p = strchr(p, '/');
	if (p) {
		*p = '\0';
		p++;
	}

	folder = folder_find_from_name(name, type);
	if (!folder)
		return folder_find_item_from_path(identifier);

	if (!p)
		return FOLDER_ITEM(folder->node->data);

	path = p;

	d[0] = (gpointer)path;
	d[1] = NULL;
	g_node_traverse(folder->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			folder_item_find_func, d);
	return d[1];
}

FolderItem *folder_find_item_and_num_from_id(const gchar *identifier, gint *num)
{
	gchar *id;
	gchar *msg;
	FolderItem *item;

	g_return_val_if_fail(identifier != NULL, NULL);

	id = g_path_get_dirname(identifier);
	msg = g_path_get_basename(identifier);
	item = folder_find_item_from_identifier(id);
	*num = to_number(msg);
	g_free(msg);
	g_free(id);

	return item;
}

Folder *folder_get_default_folder(void)
{
	return folder_list ? FOLDER(folder_list->data) : NULL;
}

FolderItem *folder_get_default_inbox(void)
{
	Folder *folder;

	if (!folder_list) return NULL;
	folder = FOLDER(folder_list->data);
	g_return_val_if_fail(folder != NULL, NULL);
	return folder->inbox;
}

FolderItem *folder_get_default_outbox(void)
{
	Folder *folder;

	if (!folder_list) return NULL;
	folder = FOLDER(folder_list->data);
	g_return_val_if_fail(folder != NULL, NULL);
	return folder->outbox;
}

FolderItem *folder_get_default_draft(void)
{
	Folder *folder;

	if (!folder_list) return NULL;
	folder = FOLDER(folder_list->data);
	g_return_val_if_fail(folder != NULL, NULL);
	return folder->draft;
}

FolderItem *folder_get_default_queue(void)
{
	Folder *folder;

	if (!folder_list) return NULL;
	folder = FOLDER(folder_list->data);
	g_return_val_if_fail(folder != NULL, NULL);
	return folder->queue;
}

FolderItem *folder_get_default_trash(void)
{
	Folder *folder;

	if (!folder_list) return NULL;
	folder = FOLDER(folder_list->data);
	g_return_val_if_fail(folder != NULL, NULL);
	return folder->trash;
}

FolderItem *folder_get_default_junk(void)
{
	FolderPrivData *priv;

	if (!folder_list) return NULL;
	if (!folder_priv_list) return NULL;
	priv = (FolderPrivData *)folder_priv_list->data;
	g_return_val_if_fail(priv != NULL, NULL);
	g_return_val_if_fail(priv->folder != NULL, NULL);
	return priv->junk;
}

static FolderPrivData *folder_get_priv(Folder *folder)
{
	FolderPrivData *priv;
	GList *cur;

	g_return_val_if_fail(folder != NULL, NULL);

	for (cur = folder_priv_list; cur != NULL; cur = cur->next) {
		priv = (FolderPrivData *)cur->data;
		if (priv->folder == folder)
			return priv;
	}

	g_warning("folder_get_priv: private data for Folder (%p) not found.",
		  folder);

	return NULL;
}

FolderItem *folder_get_junk(Folder *folder)
{
	FolderPrivData *priv;

	priv = folder_get_priv(folder);
	if (priv)
		return priv->junk;

	return NULL;
}

void folder_set_junk(Folder *folder, FolderItem *item)
{
	FolderPrivData *priv;

	priv = folder_get_priv(folder);
	if (priv)
		priv->junk = item;
}

gboolean folder_item_is_trash(FolderItem *item)
{
	PrefsAccount *ac;
	FolderItem *trash;

	g_return_val_if_fail(item != NULL, FALSE);

	if (item->stype == F_TRASH)
		return TRUE;

	ac = account_find_from_item_property(item);
	if (ac && ac->set_trash_folder && ac->trash_folder) {
		trash = folder_find_item_from_identifier(ac->trash_folder);
		if (trash == item)
			return TRUE;
	}

	return FALSE;
}

#define CREATE_FOLDER_IF_NOT_EXIST(member, dir, type)	\
{							\
	if (!folder->member) {				\
		item = folder_item_new(dir, dir);	\
		item->stype = type;			\
		folder_item_append(rootitem, item);	\
		folder->member = item;			\
	}						\
}

void folder_set_missing_folders(void)
{
	Folder *folder;
	FolderItem *rootitem;
	FolderItem *item;
	GList *list;

	for (list = folder_list; list != NULL; list = list->next) {
		folder = list->data;
		if (FOLDER_TYPE(folder) != F_MH) continue;
		rootitem = FOLDER_ITEM(folder->node->data);
		g_return_if_fail(rootitem != NULL);

		if (folder->inbox && folder->outbox && folder->draft &&
		    folder->queue && folder->trash && folder_get_junk(folder))
			continue;

		if (folder_create_tree(folder) < 0) {
			g_warning("%s: can't create the folder tree.\n",
				  LOCAL_FOLDER(folder)->rootpath);
			continue;
		}

		CREATE_FOLDER_IF_NOT_EXIST(inbox,  INBOX_DIR,  F_INBOX);
		CREATE_FOLDER_IF_NOT_EXIST(outbox, OUTBOX_DIR, F_OUTBOX);
		CREATE_FOLDER_IF_NOT_EXIST(draft,  DRAFT_DIR,  F_DRAFT);
		CREATE_FOLDER_IF_NOT_EXIST(queue,  QUEUE_DIR,  F_QUEUE);
		CREATE_FOLDER_IF_NOT_EXIST(trash,  TRASH_DIR,  F_TRASH);

		if (!folder_get_junk(folder)) {
			item = folder_item_new(JUNK_DIR, JUNK_DIR);
			item->stype = F_JUNK;
			folder_item_append(rootitem, item);
			folder_set_junk(folder, item);
		}
	}
}

static gboolean folder_unref_account_func(GNode *node, gpointer data)
{
	FolderItem *item = node->data;
	PrefsAccount *account = data;

	if (item->account == account)
		item->account = NULL;

	return FALSE;
}

void folder_unref_account_all(PrefsAccount *account)
{
	Folder *folder;
	GList *list;

	if (!account) return;

	for (list = folder_list; list != NULL; list = list->next) {
		folder = list->data;
		if (folder->account == account)
			folder->account = NULL;
		g_node_traverse(folder->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
				folder_unref_account_func, account);
	}
}

#undef CREATE_FOLDER_IF_NOT_EXIST

gchar *folder_get_path(Folder *folder)
{
	gchar *path;

	g_return_val_if_fail(folder != NULL, NULL);

	if (FOLDER_TYPE(folder) == F_MH) {
		path = g_filename_from_utf8(LOCAL_FOLDER(folder)->rootpath,
					    -1, NULL, NULL, NULL);
		if (!path) {
			g_warning("folder_get_path: failed to convert character set\n");
			path = g_strdup(LOCAL_FOLDER(folder)->rootpath);
		}
		if (!g_path_is_absolute(path)) {
			gchar *path_;

			path_ = g_strconcat(get_mail_base_dir(),
					    G_DIR_SEPARATOR_S, path, NULL);
			g_free(path);
			path = path_;
		}
	} else if (FOLDER_TYPE(folder) == F_IMAP) {
		gchar *server;
		gchar *uid;

		g_return_val_if_fail(folder->account != NULL, NULL);
		server = uriencode_for_filename(folder->account->recv_server);
		uid = uriencode_for_filename(folder->account->userid);
		path = g_strconcat(get_imap_cache_dir(),
				   G_DIR_SEPARATOR_S, server,
				   G_DIR_SEPARATOR_S, uid, NULL);
		g_free(uid);
		g_free(server);
	} else if (FOLDER_TYPE(folder) == F_NEWS) {
		gchar *server;

		g_return_val_if_fail(folder->account != NULL, NULL);
		server = uriencode_for_filename(folder->account->nntp_server);
		path = g_strconcat(get_news_cache_dir(),
				   G_DIR_SEPARATOR_S, server, NULL);
		g_free(server);
	} else
		path = NULL;

	return path;
}

gchar *folder_item_get_path(FolderItem *item)
{
	gchar *folder_path;
	gchar *item_path = NULL, *path;

	g_return_val_if_fail(item != NULL, NULL);

	folder_path = folder_get_path(item->folder);
	g_return_val_if_fail(folder_path != NULL, NULL);

	if (item->path) {
		item_path = g_filename_from_utf8(item->path, -1,
						 NULL, NULL, NULL);
		if (!item_path) {
			g_warning("folder_item_get_path: failed to convert character set\n");
			item_path = g_strdup(item->path);
		}
#ifdef G_OS_WIN32
		subst_char(item_path, '/', G_DIR_SEPARATOR);
#endif
	}

	if (item_path)
		path = g_strconcat(folder_path, G_DIR_SEPARATOR_S, item_path,
				   NULL);
	else
		path = g_strdup(folder_path);

	g_free(item_path);
	g_free(folder_path);
	return path;
}

gint folder_item_scan(FolderItem *item)
{
	Folder *folder;

	g_return_val_if_fail(item != NULL, -1);

	folder = item->folder;
	return folder->klass->scan(folder, item);
}

static void folder_item_scan_foreach_func(gpointer key, gpointer val,
					  gpointer data)
{
	folder_item_scan(FOLDER_ITEM(key));
}

void folder_item_scan_foreach(GHashTable *table)
{
	g_hash_table_foreach(table, folder_item_scan_foreach_func, NULL);
}

GSList *folder_item_get_msg_list(FolderItem *item, gboolean use_cache)
{
	Folder *folder;

	g_return_val_if_fail(item != NULL, NULL);

	folder = item->folder;

	if (item->stype == F_VIRTUAL)
		return virtual_get_class()->get_msg_list(folder, item,
							 use_cache);

	return folder->klass->get_msg_list(folder, item, use_cache);
}

GSList *folder_item_get_uncached_msg_list(FolderItem *item)
{
	Folder *folder;

	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(item->folder->klass->get_uncached_msg_list != NULL,
			     NULL);

	folder = item->folder;

	if (item->stype == F_VIRTUAL)
		return NULL;

	return folder->klass->get_uncached_msg_list(folder, item);
}

gchar *folder_item_fetch_msg(FolderItem *item, gint num)
{
	Folder *folder;

	g_return_val_if_fail(item != NULL, NULL);

	folder = item->folder;

	return folder->klass->fetch_msg(folder, item, num);
}

gint folder_item_fetch_all_msg(FolderItem *item)
{
	Folder *folder;
	GSList *mlist;
	GSList *cur;
	gint num = 0;
	gint ret = 0;

	g_return_val_if_fail(item != NULL, -1);

	debug_print("fetching all messages in %s ...\n", item->path);

	folder = item->folder;

	if (folder->ui_func)
		folder->ui_func(folder, item, folder->ui_func_data ?
				folder->ui_func_data : GINT_TO_POINTER(num));

	mlist = folder_item_get_msg_list(item, TRUE);

	for (cur = mlist; cur != NULL; cur = cur->next) {
		MsgInfo *msginfo = (MsgInfo *)cur->data;
		gchar *msg;

		num++;
		if (folder->ui_func)
			folder->ui_func(folder, item,
					folder->ui_func_data ?
					folder->ui_func_data :
					GINT_TO_POINTER(num));

		msg = folder_item_fetch_msg(item, msginfo->msgnum);
		if (!msg) {
			g_warning("Can't fetch message %d. Aborting.\n",
				  msginfo->msgnum);
			ret = -1;
			break;
		}
		g_free(msg);
	}

	procmsg_msg_list_free(mlist);

	return ret;
}

MsgInfo *folder_item_get_msginfo(FolderItem *item, gint num)
{
	Folder *folder;

	g_return_val_if_fail(item != NULL, NULL);

	folder = item->folder;

	return folder->klass->get_msginfo(folder, item, num);
}

gint folder_item_add_msg(FolderItem *dest, const gchar *file, MsgFlags *flags,
			 gboolean remove_source)
{
	Folder *folder;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(file != NULL, -1);
	g_return_val_if_fail(dest->folder->klass->add_msg != NULL, -1);

	folder = dest->folder;

	return folder->klass->add_msg(folder, dest, file, flags, remove_source);
}

gint folder_item_add_msgs(FolderItem *dest, GSList *file_list,
			  gboolean remove_source, gint *first)
{
	Folder *folder;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(file_list != NULL, -1);
	g_return_val_if_fail(dest->folder->klass->add_msgs != NULL, -1);

	folder = dest->folder;

	return folder->klass->add_msgs(folder, dest, file_list, remove_source,
				       first);
}

gint folder_item_add_msg_msginfo(FolderItem *dest, MsgInfo *msginfo,
				 gboolean remove_source)
{
	Folder *folder;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msginfo != NULL, -1);
	g_return_val_if_fail(msginfo->file_path != NULL, -1);
	g_return_val_if_fail(dest->folder->klass->add_msg_msginfo != NULL, -1);

	folder = dest->folder;

	return folder->klass->add_msg_msginfo(folder, dest, msginfo,
					      remove_source);
}

gint folder_item_add_msgs_msginfo(FolderItem *dest, GSList *msglist,
				  gboolean remove_source, gint *first)
{
	Folder *folder;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msglist != NULL, -1);
	g_return_val_if_fail(dest->folder->klass->add_msgs_msginfo != NULL, -1);

	folder = dest->folder;

	return folder->klass->add_msgs_msginfo(folder, dest, msglist,
					       remove_source, first);
}

#define IS_FROM_QUEUE(m, d) \
	(m->folder && m->folder->stype == F_QUEUE && \
	 MSG_IS_QUEUED(m->flags) && d->stype != F_QUEUE)

gint folder_item_move_msg(FolderItem *dest, MsgInfo *msginfo)
{
	Folder *folder;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msginfo != NULL, -1);
	g_return_val_if_fail(dest->folder->klass->move_msg != NULL, -1);

	folder = dest->folder;

	if (IS_FROM_QUEUE(msginfo, dest)) {
		GSList msglist;

		msglist.data = msginfo;
		msglist.next = NULL;
		return procmsg_add_messages_from_queue(dest, &msglist, TRUE);
	}

	return folder->klass->move_msg(folder, dest, msginfo);
}

gint folder_item_move_msgs(FolderItem *dest, GSList *msglist)
{
	Folder *folder;
	MsgInfo *msginfo;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msglist != NULL, -1);
	g_return_val_if_fail(dest->folder->klass->move_msgs != NULL, -1);

	folder = dest->folder;

	msginfo = (MsgInfo *)msglist->data;
	if (IS_FROM_QUEUE(msginfo, dest))
		return procmsg_add_messages_from_queue(dest, msglist, TRUE);

	return folder->klass->move_msgs(folder, dest, msglist);
}

gint folder_item_copy_msg(FolderItem *dest, MsgInfo *msginfo)
{
	Folder *folder;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msginfo != NULL, -1);
	g_return_val_if_fail(dest->folder->klass->copy_msg != NULL, -1);

	folder = dest->folder;

	if (IS_FROM_QUEUE(msginfo, dest)) {
		GSList msglist;

		msglist.data = msginfo;
		msglist.next = NULL;
		return procmsg_add_messages_from_queue(dest, &msglist, FALSE);
	}

	return folder->klass->copy_msg(folder, dest, msginfo);
}

gint folder_item_copy_msgs(FolderItem *dest, GSList *msglist)
{
	Folder *folder;
	MsgInfo *msginfo;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msglist != NULL, -1);
	g_return_val_if_fail(dest->folder->klass->copy_msgs != NULL, -1);

	folder = dest->folder;

	msginfo = (MsgInfo *)msglist->data;
	if (IS_FROM_QUEUE(msginfo, dest))
		return procmsg_add_messages_from_queue(dest, msglist, FALSE);

	return folder->klass->copy_msgs(folder, dest, msglist);
}

#undef IS_FROM_QUEUE

gint folder_item_remove_msg(FolderItem *item, MsgInfo *msginfo)
{
	Folder *folder;

	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->folder->klass->remove_msg != NULL, -1);

	folder = item->folder;

	return folder->klass->remove_msg(folder, item, msginfo);
}

gint folder_item_remove_msgs(FolderItem *item, GSList *msglist)
{
	Folder *folder;
	gint ret = 0;

	g_return_val_if_fail(item != NULL, -1);

	folder = item->folder;
	if (folder->klass->remove_msgs) {
		return folder->klass->remove_msgs(folder, item, msglist);
	}

	while (msglist != NULL) {
		MsgInfo *msginfo = (MsgInfo *)msglist->data;

		ret = folder_item_remove_msg(item, msginfo);
		if (ret != 0) break;
		msglist = msglist->next;
	}

	return ret;
}

gint folder_item_remove_all_msg(FolderItem *item)
{
	Folder *folder;

	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->folder->klass->remove_all_msg != NULL, -1);

	folder = item->folder;

	return folder->klass->remove_all_msg(folder, item);
}

gboolean folder_item_is_msg_changed(FolderItem *item, MsgInfo *msginfo)
{
	Folder *folder;

	g_return_val_if_fail(item != NULL, FALSE);
	g_return_val_if_fail(item->folder->klass->is_msg_changed != NULL,
			     FALSE);

	folder = item->folder;
	return folder->klass->is_msg_changed(folder, item, msginfo);
}

gint folder_item_close(FolderItem *item)
{
	Folder *folder;

	g_return_val_if_fail(item != NULL, -1);

	item->opened = FALSE;
	folder = item->folder;
	return folder->klass->close(folder, item);
}

gchar *folder_item_get_cache_file(FolderItem *item)
{
	gchar *path;
	gchar *file;

	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(item->path != NULL, NULL);

	path = folder_item_get_path(item);
	g_return_val_if_fail(path != NULL, NULL);
	if (!is_dir_exist(path))
		make_dir_hier(path);
	file = g_strconcat(path, G_DIR_SEPARATOR_S, CACHE_FILE, NULL);
	g_free(path);

	return file;
}

gchar *folder_item_get_mark_file(FolderItem *item)
{
	gchar *path;
	gchar *file;

	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(item->path != NULL, NULL);

	path = folder_item_get_path(item);
	g_return_val_if_fail(path != NULL, NULL);
	if (!is_dir_exist(path))
		make_dir_hier(path);
	file = g_strconcat(path, G_DIR_SEPARATOR_S, MARK_FILE, NULL);
	g_free(path);

	return file;
}

static gboolean folder_build_tree(GNode *node, gpointer data)
{
	Folder *folder = FOLDER(data);
	FolderItem *item;
	XMLNode *xmlnode;
	GList *list;
	SpecialFolderItemType stype = F_NORMAL;
	const gchar *name = NULL;
	const gchar *path = NULL;
	PrefsAccount *account = NULL;
	gboolean no_sub = FALSE, no_select = FALSE, collapsed = FALSE,
		 threaded = TRUE, ac_apply_sub = FALSE;
	FolderSortKey sort_key = SORT_BY_NONE;
	FolderSortType sort_type = SORT_ASCENDING;
	gboolean qsearch_cond_type = 0;
	gint new = 0, unread = 0, total = 0;
	time_t mtime = 0;
	gboolean use_auto_to_on_reply = FALSE;
	gchar *auto_to = NULL, *auto_cc = NULL, *auto_bcc = NULL,
	      *auto_replyto = NULL;
	gboolean trim_summary_subject = FALSE, trim_compose_subject = FALSE;

	g_return_val_if_fail(node->data != NULL, FALSE);
	if (!node->parent) return FALSE;

	xmlnode = node->data;
	if (strcmp2(xmlnode->tag->tag, "folderitem") != 0) {
		g_warning("tag name != \"folderitem\"\n");
		return FALSE;
	}

	list = xmlnode->tag->attr;
	for (; list != NULL; list = list->next) {
		XMLAttr *attr = list->data;

		if (!attr || !attr->name || !attr->value) continue;
		if (!strcmp(attr->name, "type")) {
			if (!g_ascii_strcasecmp(attr->value, "normal"))
				stype = F_NORMAL;
			else if (!g_ascii_strcasecmp(attr->value, "inbox"))
				stype = F_INBOX;
			else if (!g_ascii_strcasecmp(attr->value, "outbox"))
				stype = F_OUTBOX;
			else if (!g_ascii_strcasecmp(attr->value, "draft"))
				stype = F_DRAFT;
			else if (!g_ascii_strcasecmp(attr->value, "queue"))
				stype = F_QUEUE;
			else if (!g_ascii_strcasecmp(attr->value, "trash"))
				stype = F_TRASH;
			else if (!g_ascii_strcasecmp(attr->value, "junk"))
				stype = F_JUNK;
			else if (!g_ascii_strcasecmp(attr->value, "virtual"))
				stype = F_VIRTUAL;
		} else if (!strcmp(attr->name, "name"))
			name = attr->value;
		else if (!strcmp(attr->name, "path")) {
#ifdef G_OS_WIN32
			subst_char(attr->value, G_DIR_SEPARATOR, '/');
#endif
			path = attr->value;
		} else if (!strcmp(attr->name, "mtime"))
			mtime = strtoll(attr->value, NULL, 10);
		else if (!strcmp(attr->name, "new"))
			new = atoi(attr->value);
		else if (!strcmp(attr->name, "unread"))
			unread = atoi(attr->value);
		else if (!strcmp(attr->name, "total"))
			total = atoi(attr->value);
		else if (!strcmp(attr->name, "no_sub"))
			no_sub = *attr->value == '1' ? TRUE : FALSE;
		else if (!strcmp(attr->name, "no_select"))
			no_select = *attr->value == '1' ? TRUE : FALSE;
		else if (!strcmp(attr->name, "collapsed"))
			collapsed = *attr->value == '1' ? TRUE : FALSE;
		else if (!strcmp(attr->name, "threaded"))
			threaded =  *attr->value == '1' ? TRUE : FALSE;
		else if (!strcmp(attr->name, "sort_key")) {
			if (!strcmp(attr->value, "none"))
				sort_key = SORT_BY_NONE;
			else if (!strcmp(attr->value, "number"))
				sort_key = SORT_BY_NUMBER;
			else if (!strcmp(attr->value, "size"))
				sort_key = SORT_BY_SIZE;
			else if (!strcmp(attr->value, "date"))
				sort_key = SORT_BY_DATE;
			else if (!strcmp(attr->value, "thread-date"))
				sort_key = SORT_BY_TDATE;
			else if (!strcmp(attr->value, "from"))
				sort_key = SORT_BY_FROM;
			else if (!strcmp(attr->value, "subject"))
				sort_key = SORT_BY_SUBJECT;
			else if (!strcmp(attr->value, "score"))
				sort_key = SORT_BY_SCORE;
			else if (!strcmp(attr->value, "label"))
				sort_key = SORT_BY_LABEL;
			else if (!strcmp(attr->value, "mark"))
				sort_key = SORT_BY_MARK;
			else if (!strcmp(attr->value, "unread"))
				sort_key = SORT_BY_UNREAD;
			else if (!strcmp(attr->value, "mime"))
				sort_key = SORT_BY_MIME;
			else if (!strcmp(attr->value, "to"))
				sort_key = SORT_BY_TO;
		} else if (!strcmp(attr->name, "sort_type")) {
			if (!strcmp(attr->value, "ascending"))
				sort_type = SORT_ASCENDING;
			else
				sort_type = SORT_DESCENDING;
		} else if (!strcmp(attr->name, "qsearch_cond")) {
			if (!strcmp(attr->value, "all"))
				qsearch_cond_type = 0;
			else if (!strcmp(attr->value, "unread"))
				qsearch_cond_type = 1;
			else if (!strcmp(attr->value, "mark"))
				qsearch_cond_type = 2;
			else if (!strcmp(attr->value, "clabel"))
				qsearch_cond_type = 3;
			else if (!strcmp(attr->value, "mime"))
				qsearch_cond_type = 4;
			else if (!strcmp(attr->value, "w1day"))
				qsearch_cond_type = 5;
			else if (!strcmp(attr->value, "last5"))
				qsearch_cond_type = 6;
			else if (!strcmp(attr->value, "last7"))
				qsearch_cond_type = 7;
			else if (!strcmp(attr->value, "in-addressbook"))
				qsearch_cond_type = 8;
			else if (!strcmp(attr->value, "last30"))
				qsearch_cond_type = 9;
		} else if (!strcmp(attr->name, "account_id")) {
			account = account_find_from_id(atoi(attr->value));
			if (!account) g_warning("account_id: %s not found\n",
						attr->value);
		} else if (!strcmp(attr->name, "account_apply_sub"))
			ac_apply_sub = *attr->value == '1' ? TRUE : FALSE;
		else if (!strcmp(attr->name, "to"))
			auto_to = g_strdup(attr->value);
		else if (!strcmp(attr->name, "use_auto_to_on_reply"))
			use_auto_to_on_reply =
				*attr->value == '1' ? TRUE : FALSE;
		else if (!strcmp(attr->name, "cc"))
			auto_cc = g_strdup(attr->value);
		else if (!strcmp(attr->name, "bcc"))
			auto_bcc = g_strdup(attr->value);
		else if (!strcmp(attr->name, "replyto"))
			auto_replyto = g_strdup(attr->value);
		else if (!strcmp(attr->name, "trim_summary_subject")) {
			trim_summary_subject =
				*attr->value == '1' ? TRUE : FALSE;
		} else if (!strcmp(attr->name, "trim_compose_subject")) {
			trim_compose_subject =
				*attr->value = '1' ? TRUE : FALSE;
		}
	}

	item = folder_item_new(name, path);
	item->stype = stype;
	item->mtime = mtime;
	item->new = new;
	item->unread = unread;
	item->total = total;
	item->no_sub = no_sub;
	item->no_select = no_select;
	item->collapsed = collapsed;
	item->threaded  = threaded;
	item->sort_key  = sort_key;
	item->sort_type = sort_type;
	item->node = node;
	item->parent = FOLDER_ITEM(node->parent->data);
	item->folder = folder;
	switch (stype) {
	case F_INBOX:  folder->inbox  = item; break;
	case F_OUTBOX: folder->outbox = item; break;
	case F_DRAFT:  folder->draft  = item; break;
	case F_QUEUE:  folder->queue  = item; break;
	case F_TRASH:  folder->trash  = item; break;
	case F_JUNK:   folder_set_junk(folder, item); break;
	default:       break;
	}
	item->account = account;
	item->ac_apply_sub = ac_apply_sub;
	item->auto_to = auto_to;
	item->use_auto_to_on_reply = use_auto_to_on_reply;
	item->auto_cc = auto_cc;
	item->auto_bcc = auto_bcc;
	item->auto_replyto = auto_replyto;
	item->trim_summary_subject = trim_summary_subject;
	item->trim_compose_subject = trim_compose_subject;
	item->qsearch_cond_type = qsearch_cond_type;
	node->data = item;
	xml_free_node(xmlnode);

	return FALSE;
}

static gboolean folder_read_folder_func(GNode *node, gpointer data)
{
	Folder *folder;
	FolderItem *item;
	XMLNode *xmlnode;
	GList *list;
	FolderType type = F_UNKNOWN;
	const gchar *name = NULL;
	const gchar *path = NULL;
	PrefsAccount *account = NULL;
	gboolean collapsed = FALSE, threaded = TRUE, ac_apply_sub = FALSE;

	if (g_node_depth(node) != 2) return FALSE;
	g_return_val_if_fail(node->data != NULL, FALSE);

	xmlnode = node->data;
	if (strcmp2(xmlnode->tag->tag, "folder") != 0) {
		g_warning("tag name != \"folder\"\n");
		return TRUE;
	}
	g_node_unlink(node);
	list = xmlnode->tag->attr;
	for (; list != NULL; list = list->next) {
		XMLAttr *attr = list->data;

		if (!attr || !attr->name || !attr->value) continue;
		if (!strcmp(attr->name, "type")) {
			if (!g_ascii_strcasecmp(attr->value, "mh"))
				type = F_MH;
			else if (!g_ascii_strcasecmp(attr->value, "mbox"))
				type = F_MBOX;
			else if (!g_ascii_strcasecmp(attr->value, "maildir"))
				type = F_MAILDIR;
			else if (!g_ascii_strcasecmp(attr->value, "imap"))
				type = F_IMAP;
			else if (!g_ascii_strcasecmp(attr->value, "news"))
				type = F_NEWS;
		} else if (!strcmp(attr->name, "name"))
			name = attr->value;
		else if (!strcmp(attr->name, "path"))
			path = attr->value;
		else if (!strcmp(attr->name, "collapsed"))
			collapsed = *attr->value == '1' ? TRUE : FALSE;
		else if (!strcmp(attr->name, "threaded"))
			threaded = *attr->value == '1' ? TRUE : FALSE;
		else if (!strcmp(attr->name, "account_id")) {
			account = account_find_from_id(atoi(attr->value));
			if (!account) g_warning("account_id: %s not found\n",
						attr->value);
		} else if (!strcmp(attr->name, "account_apply_sub"))
			ac_apply_sub = *attr->value == '1' ? TRUE : FALSE;
	}

	folder = folder_new(type, name, path);
	g_return_val_if_fail(folder != NULL, FALSE);
	if (account && FOLDER_IS_REMOTE(folder)) {
		folder->account = account;
		account->folder = REMOTE_FOLDER(folder);
	}
	if (account && FOLDER_IS_LOCAL(folder))
		ac_apply_sub = TRUE;
	item = FOLDER_ITEM(folder->node->data);
	node->data = item;
	item->node = node;
	g_node_destroy(folder->node);
	folder->node = node;
	folder_add(folder);
	item->collapsed = collapsed;
	item->threaded  = threaded;
	item->account   = account;
	item->ac_apply_sub = ac_apply_sub;

	g_node_traverse(node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			folder_build_tree, folder);

	return FALSE;
}

static gchar *folder_get_list_path(void)
{
	static gchar *filename = NULL;

	if (!filename)
		filename =  g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
					FOLDER_LIST, NULL);

	return filename;
}

#define PUT_ESCAPE_STR(fp, attr, str)			\
{							\
	fputs(" " attr "=\"", fp);			\
	xml_file_put_escape_str(fp, str);		\
	fputs("\"", fp);				\
}

static void folder_write_list_recursive(GNode *node, gpointer data)
{
	FILE *fp = (FILE *)data;
	FolderItem *item;
	gint i, depth;
	static gchar *folder_type_str[] = {"mh", "mbox", "maildir", "imap",
					   "news", "unknown"};
	static gchar *folder_item_stype_str[] = {"normal", "inbox", "outbox",
						 "draft", "queue", "trash",
						 "junk", "virtual"};
	static gchar *sort_key_str[] = {"none", "number", "size", "date",
					"thread-date",
					"from", "subject", "score", "label",
					"mark", "unread", "mime", "to"};
	static gchar *qsearch_cond_str[] = {"all", "unread", "mark", "clabel",
					    "mime", "w1day", "last5", "last7",
					    "in-addressbook", "last30"};

	g_return_if_fail(node != NULL);
	g_return_if_fail(fp != NULL);

	item = FOLDER_ITEM(node->data);
	g_return_if_fail(item != NULL);

	depth = g_node_depth(node);
	for (i = 0; i < depth; i++)
		fputs("    ", fp);
	if (depth == 1) {
		Folder *folder = item->folder;

		fprintf(fp, "<folder type=\"%s\"",
			folder_type_str[FOLDER_TYPE(folder)]);
		if (folder->name)
			PUT_ESCAPE_STR(fp, "name", folder->name);
		if (FOLDER_TYPE(folder) == F_MH)
			PUT_ESCAPE_STR(fp, "path",
				       LOCAL_FOLDER(folder)->rootpath);
		if (item->collapsed && node->children)
			fputs(" collapsed=\"1\"", fp);
		if (folder->account)
			fprintf(fp, " account_id=\"%d\"",
				folder->account->account_id);
		else if (item->account)
			fprintf(fp, " account_id=\"%d\"",
				item->account->account_id);
		if (item->ac_apply_sub)
			fputs(" account_apply_sub=\"1\"", fp);
	} else {
		fprintf(fp, "<folderitem type=\"%s\"",
			folder_item_stype_str[item->stype]);
		if (item->name)
			PUT_ESCAPE_STR(fp, "name", item->name);
		if (item->path)
			PUT_ESCAPE_STR(fp, "path", item->path);

		if (item->no_sub)
			fputs(" no_sub=\"1\"", fp);
		if (item->no_select)
			fputs(" no_select=\"1\"", fp);
		if (item->collapsed && node->children)
			fputs(" collapsed=\"1\"", fp);
		if (item->threaded)
			fputs(" threaded=\"1\"", fp);
		else
			fputs(" threaded=\"0\"", fp);

		if (item->sort_key != SORT_BY_NONE) {
			fprintf(fp, " sort_key=\"%s\"",
				sort_key_str[item->sort_key]);
			if (item->sort_type == SORT_ASCENDING)
				fprintf(fp, " sort_type=\"ascending\"");
			else
				fprintf(fp, " sort_type=\"descending\"");
		}
		if (item->qsearch_cond_type > 0 &&
		    item->qsearch_cond_type < 10) {
			fprintf(fp, " qsearch_cond=\"%s\"",
				qsearch_cond_str[item->qsearch_cond_type]);
		}

		fprintf(fp,
			" mtime=\"%lld\" new=\"%d\" unread=\"%d\" total=\"%d\"",
			(gint64)item->mtime, item->new, item->unread, item->total);

		if (item->account)
			fprintf(fp, " account_id=\"%d\"",
				item->account->account_id);
		if (item->ac_apply_sub)
			fputs(" account_apply_sub=\"1\"", fp);

		if (item->auto_to)
			PUT_ESCAPE_STR(fp, "to", item->auto_to);
		if (item->use_auto_to_on_reply)
			fputs(" use_auto_to_on_reply=\"1\"", fp);
		if (item->auto_cc)
			PUT_ESCAPE_STR(fp, "cc", item->auto_cc);
		if (item->auto_bcc)
			PUT_ESCAPE_STR(fp, "bcc", item->auto_bcc);
		if (item->auto_replyto)
			PUT_ESCAPE_STR(fp, "replyto", item->auto_replyto);

		if (item->trim_summary_subject)
			fputs(" trim_summary_subject=\"1\"", fp);
		if (item->trim_compose_subject)
			fputs(" trim_compose_subject=\"1\"", fp);
	}

	if (node->children) {
		GNode *child;
		fputs(">\n", fp);

		child = node->children;
		while (child) {
			GNode *cur;

			cur = child;
			child = cur->next;
			folder_write_list_recursive(cur, data);
		}

		for (i = 0; i < depth; i++)
			fputs("    ", fp);
		fprintf(fp, "</%s>\n", depth == 1 ? "folder" : "folderitem");
	} else
		fputs(" />\n", fp);
}

#undef PUT_ESCAPE_STR
