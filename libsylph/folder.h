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

#ifndef __FOLDER_H__
#define __FOLDER_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <time.h>

typedef struct _Folder		Folder;
typedef struct _FolderClass	FolderClass;

typedef struct _LocalFolder	LocalFolder;
typedef struct _RemoteFolder	RemoteFolder;
#if 0
typedef struct _MboxFolder	MboxFolder;
typedef struct _MaildirFolder	MaildirFolder;
#endif

typedef struct _FolderItem	FolderItem;

#define FOLDER(obj)		((Folder *)obj)
#define FOLDER_CLASS(obj)	(FOLDER(obj)->klass)
#define FOLDER_TYPE(obj)	(FOLDER(obj)->klass->type)

#define LOCAL_FOLDER(obj)	((LocalFolder *)obj)
#define REMOTE_FOLDER(obj)	((RemoteFolder *)obj)

#define FOLDER_IS_LOCAL(obj)	(FOLDER_TYPE(obj) == F_MH      || \
				 FOLDER_TYPE(obj) == F_MBOX    || \
				 FOLDER_TYPE(obj) == F_MAILDIR)
#define FOLDER_IS_REMOTE(obj)	(FOLDER_TYPE(obj) == F_IMAP || \
				 FOLDER_TYPE(obj) == F_NEWS)

#if 0
#define MBOX_FOLDER(obj)	((MboxFolder *)obj)
#define MAILDIR_FOLDER(obj)	((MaildirFolder *)obj)
#endif

#define FOLDER_ITEM(obj)	((FolderItem *)obj)

#define FOLDER_ITEM_CAN_ADD(obj)					\
		((obj) && FOLDER_ITEM(obj)->folder &&			\
		 FOLDER_ITEM(obj)->path &&				\
		 (FOLDER_IS_LOCAL(FOLDER_ITEM(obj)->folder) ||		\
		  FOLDER_TYPE(FOLDER_ITEM(obj)->folder) == F_IMAP) &&	\
		 !FOLDER_ITEM(obj)->no_select)

#define FOLDER_ITEM_IS_SENT_FOLDER(obj)	((obj) &&			\
					 ((obj)->stype == F_OUTBOX ||	\
					  (obj)->stype == F_DRAFT ||	\
					  (obj)->stype == F_QUEUE))

typedef enum
{
	F_MH,
	F_MBOX,
	F_MAILDIR,
	F_IMAP,
	F_NEWS,
	F_UNKNOWN
} FolderType;

typedef enum
{
	F_NORMAL,
	F_INBOX,
	F_OUTBOX,
	F_DRAFT,
	F_QUEUE,
	F_TRASH,
	F_JUNK,
	F_VIRTUAL
} SpecialFolderItemType;

typedef enum
{
	SORT_BY_NONE,
	SORT_BY_NUMBER,
	SORT_BY_SIZE,
	SORT_BY_DATE,
	SORT_BY_TDATE,
	SORT_BY_FROM,
	SORT_BY_SUBJECT,
	SORT_BY_SCORE,
	SORT_BY_LABEL,
	SORT_BY_MARK,
	SORT_BY_UNREAD,
	SORT_BY_MIME,
	SORT_BY_TO
} FolderSortKey;

typedef enum
{
	SORT_ASCENDING,
	SORT_DESCENDING
} FolderSortType;

typedef void (*FolderUIFunc)		(Folder		*folder,
					 FolderItem	*item,
					 gpointer	 data);
typedef gboolean (*FolderUIFunc2)	(Folder		*folder,
					 FolderItem	*item,
					 guint		 count,
					 guint		 total,
					 gpointer	 data);
typedef void (*FolderDestroyNotify)	(Folder		*folder,
					 FolderItem	*item,
					 gpointer	 data);

#include "prefs_account.h"
#include "session.h"
#include "procmsg.h"
#include "utils.h"

struct _Folder
{
	FolderClass *klass;

	gchar *name;
	PrefsAccount *account;

	FolderItem *inbox;
	FolderItem *outbox;
	FolderItem *draft;
	FolderItem *queue;
	FolderItem *trash;

	FolderUIFunc ui_func;
	gpointer     ui_func_data;

	GNode *node;

	gpointer data;
};

struct _FolderClass
{
	FolderType type;

	/* virtual functions */
	Folder * (*folder_new)		(const gchar	*name,
					 const gchar	*path);
	void     (*destroy)		(Folder		*folder);

	gint     (*scan_tree)		(Folder		*folder);
	gint     (*create_tree)		(Folder		*folder);

	GSList * (*get_msg_list)	(Folder		*folder,
					 FolderItem	*item,
					 gboolean	 use_cache);
	GSList * (*get_uncached_msg_list)
					(Folder		*folder,
					 FolderItem	*item);
	/* return value is filename encoding */
	gchar *  (*fetch_msg)		(Folder		*folder,
					 FolderItem	*item,
					 gint		 num);
	MsgInfo * (*get_msginfo)	(Folder		*folder,
					 FolderItem	*item,
					 gint		 num);
	gint     (*add_msg)		(Folder		*folder,
					 FolderItem	*dest,
					 const gchar	*file,
					 MsgFlags	*flags,
					 gboolean	 remove_source);
	gint     (*add_msgs)		(Folder		*folder,
					 FolderItem	*dest,
					 GSList		*file_list,
					 gboolean	 remove_source,
					 gint		*first);
	gint     (*add_msg_msginfo)	(Folder		*folder,
					 FolderItem	*dest,
					 MsgInfo	*msginfo,
					 gboolean	 remove_source);
	gint     (*add_msgs_msginfo)	(Folder		*folder,
					 FolderItem	*dest,
					 GSList		*msginfo_list,
					 gboolean	 remove_source,
					 gint		*first);
	gint     (*move_msg)		(Folder		*folder,
					 FolderItem	*dest,
					 MsgInfo	*msginfo);
	gint     (*move_msgs)		(Folder		*folder,
					 FolderItem	*dest,
					 GSList		*msglist);
	gint     (*copy_msg)		(Folder		*folder,
					 FolderItem	*dest,
					 MsgInfo	*msginfo);
	gint     (*copy_msgs)		(Folder		*folder,
					 FolderItem	*dest,
					 GSList		*msglist);
	gint     (*remove_msg)		(Folder		*folder,
					 FolderItem	*item,
					 MsgInfo	*msginfo);
	gint     (*remove_msgs)		(Folder		*folder,
					 FolderItem	*item,
					 GSList		*msglist);
	gint     (*remove_all_msg)	(Folder		*folder,
					 FolderItem	*item);
	gboolean (*is_msg_changed)	(Folder		*folder,
					 FolderItem	*item,
					 MsgInfo	*msginfo);
	gint     (*close)		(Folder		*folder,
					 FolderItem	*item);
	gint     (*scan)		(Folder		*folder,
					 FolderItem	*item);

	FolderItem * (*create_folder)	(Folder		*folder,
					 FolderItem	*parent,
					 const gchar	*name);
	gint     (*rename_folder)	(Folder		*folder,
					 FolderItem	*item,
					 const gchar	*name);
	gint     (*move_folder)		(Folder		*folder,
					 FolderItem	*item,
					 FolderItem	*new_parent);
	gint     (*remove_folder)	(Folder		*folder,
					 FolderItem	*item);
};

struct _LocalFolder
{
	Folder folder;

	gchar *rootpath;
};

struct _RemoteFolder
{
	Folder folder;

	Session *session;

	guint remove_cache_on_destroy : 1;
	guint dummy1 : 1;
	guint dummy2 : 1;
	guint dummy3 : 1;
};

#if 0
struct _MboxFolder
{
	LocalFolder lfolder;
};

struct _MaildirFolder
{
	LocalFolder lfolder;
};
#endif

struct _FolderItem
{
	SpecialFolderItemType stype;

	gchar *name; /* UTF-8 */
	gchar *path; /* UTF-8 */

	stime_t mtime;

	gint new;
	gint unread;
	gint total;
	gint unmarked_num;

	gint last_num;

	/* special flags */
	guint no_sub    : 1; /* no child allowed?    */
	guint no_select : 1; /* not selectable?      */
	guint collapsed : 1; /* collapsed item       */
	guint threaded  : 1; /* threaded folder view */

	guint opened    : 1; /* opened by summary view */
	guint updated   : 1; /* folderview should be updated */

	guint cache_dirty : 1; /* cache file needs to be updated */
	guint mark_dirty  : 1; /* mark file needs to be updated */

	FolderSortKey sort_key;
	FolderSortType sort_type;

	GNode *node;

	FolderItem *parent;

	Folder *folder;

	PrefsAccount *account;

	gboolean ac_apply_sub;

	gchar *auto_to;
	gboolean use_auto_to_on_reply;
	gchar *auto_cc;
	gchar *auto_bcc;
	gchar *auto_replyto;

	gboolean trim_summary_subject;
	gboolean trim_compose_subject;

	GSList *cache_queue;
	GSList *mark_queue;

	guint last_selected;
	gint qsearch_cond_type;

	gpointer data;
};

Folder     *folder_new			(FolderType	 type,
					 const gchar	*name,
					 const gchar	*path);
void        folder_local_folder_init	(Folder		*folder,
					 const gchar	*name,
					 const gchar	*path);
void        folder_remote_folder_init	(Folder		*folder,
					 const gchar	*name,
					 const gchar	*path);

void        folder_destroy		(Folder		*folder);
void        folder_local_folder_destroy	(LocalFolder	*lfolder);
void        folder_remote_folder_destroy(RemoteFolder	*rfolder);

gint        folder_remote_folder_destroy_all_sessions	(void);

gboolean    folder_remote_folder_is_session_active
					(RemoteFolder	*rfolder);
gboolean folder_remote_folder_active_session_exist	(void);

gint        folder_scan_tree		(Folder		*folder);
gint        folder_create_tree		(Folder		*folder);

FolderItem *folder_item_new		(const gchar	*name,
					 const gchar	*path);
void        folder_item_append		(FolderItem	*parent,
					 FolderItem	*item);
FolderItem *folder_item_copy		(FolderItem	*item);
void        folder_item_remove		(FolderItem	*item);
void        folder_item_remove_children	(FolderItem	*item);
void        folder_item_destroy		(FolderItem	*item);

gint        folder_item_compare		(FolderItem	*item_a,
					 FolderItem	*item_b);

void        folder_set_ui_func		(Folder		*folder,
					 FolderUIFunc	 func,
					 gpointer	 data);
void        folder_set_ui_func2		(Folder		*folder,
					 FolderUIFunc2	 func,
					 gpointer	 data);
FolderUIFunc2 folder_get_ui_func2	(Folder		*folder);
gboolean    folder_call_ui_func2	(Folder		*folder,
					 FolderItem	*item,
					 guint		 count,
					 guint		 total);

void        folder_set_name	(Folder		*folder,
				 const gchar	*name);
void        folder_tree_destroy	(Folder		*folder);

void   folder_add		(Folder		*folder);

GList *folder_get_list		(void);
gint   folder_read_list		(void);
void   folder_write_list	(void);

gchar *folder_get_status	(GPtrArray	*folders,
				 gboolean	 full);

Folder     *folder_find_from_path		(const gchar	*path);
Folder     *folder_find_from_name		(const gchar	*name,
						 FolderType	 type);
FolderItem *folder_find_item_from_path		(const gchar	*path);
FolderItem *folder_find_child_item_by_name	(FolderItem	*item,
						 const gchar	*name);
gchar      *folder_get_identifier		(Folder		*folder);
gchar      *folder_item_get_identifier		(FolderItem	*item);
FolderItem *folder_find_item_from_identifier	(const gchar	*identifier);
FolderItem *folder_find_item_and_num_from_id	(const gchar	*identifier,
						 gint		*num);

Folder     *folder_get_default_folder	(void);
FolderItem *folder_get_default_inbox	(void);
FolderItem *folder_get_default_outbox	(void);
FolderItem *folder_get_default_draft	(void);
FolderItem *folder_get_default_queue	(void);
FolderItem *folder_get_default_trash	(void);
FolderItem *folder_get_default_junk	(void);

gboolean folder_item_is_trash		(FolderItem	*item);

FolderItem *folder_get_junk		(Folder		*folder);
void folder_set_junk			(Folder		*folder,
					 FolderItem	*item);

void folder_set_missing_folders		(void);
void folder_unref_account_all		(PrefsAccount	*account);

/* return value is filename encoding */
gchar *folder_get_path			(Folder		*folder);
gchar *folder_item_get_path		(FolderItem	*item);

gint   folder_item_scan			(FolderItem	*item);
void   folder_item_scan_foreach		(GHashTable	*table);
GSList *folder_item_get_msg_list	(FolderItem	*item,
					 gboolean	 use_cache);
GSList *folder_item_get_uncached_msg_list
					(FolderItem	*item);
/* return value is filename encoding */
gchar *folder_item_fetch_msg		(FolderItem	*item,
					 gint		 num);
gint   folder_item_fetch_all_msg	(FolderItem	*item);
MsgInfo *folder_item_get_msginfo	(FolderItem	*item,
					 gint		 num);
gint   folder_item_add_msg		(FolderItem	*dest,
					 const gchar	*file,
					 MsgFlags	*flags,
					 gboolean	 remove_source);
gint   folder_item_add_msgs		(FolderItem	*dest,
					 GSList		*file_list,
					 gboolean	 remove_source,
					 gint		*first);
gint   folder_item_add_msg_msginfo	(FolderItem	*dest,
					 MsgInfo	*msginfo,
					 gboolean	 remove_source);
gint   folder_item_add_msgs_msginfo	(FolderItem	*dest,
					 GSList		*msginfo_list,
					 gboolean	 remove_source,
					 gint		*first);
gint   folder_item_move_msg		(FolderItem	*dest,
					 MsgInfo	*msginfo);
gint   folder_item_move_msgs		(FolderItem	*dest,
					 GSList		*msglist);
gint   folder_item_copy_msg		(FolderItem	*dest,
					 MsgInfo	*msginfo);
gint   folder_item_copy_msgs		(FolderItem	*dest,
					 GSList		*msglist);
gint   folder_item_remove_msg		(FolderItem	*item,
					 MsgInfo	*msginfo);
gint   folder_item_remove_msgs		(FolderItem	*item,
					 GSList		*msglist);
gint   folder_item_remove_all_msg	(FolderItem	*item);

gboolean folder_item_is_msg_changed	(FolderItem	*item,
					 MsgInfo	*msginfo);

/* return value is filename encoding */
gchar *folder_item_get_cache_file	(FolderItem	*item);
gchar *folder_item_get_mark_file	(FolderItem	*item);

gint   folder_item_close		(FolderItem	*item);

#endif /* __FOLDER_H__ */
