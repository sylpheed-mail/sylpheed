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

#ifndef __FILTER_H__
#define __FILTER_H__

#include <glib.h>

#include "folder.h"
#include "procmsg.h"
#include "utils.h"

typedef struct _FilterCond	FilterCond;
typedef struct _FilterAction	FilterAction;
typedef struct _FilterRule	FilterRule;
typedef struct _FilterInfo	FilterInfo;

typedef enum
{
	FLT_TIMING_ANY,
	FLT_TIMING_ON_RECEIVE,
	FLT_TIMING_MANUAL
} FilterTiming;

typedef enum
{
	FLT_COND_HEADER,
	FLT_COND_ANY_HEADER,
	FLT_COND_TO_OR_CC,
	FLT_COND_BODY,
	FLT_COND_CMD_TEST,
	FLT_COND_SIZE_GREATER,
	FLT_COND_AGE_GREATER,
	FLT_COND_UNREAD,
	FLT_COND_MARK,
	FLT_COND_COLOR_LABEL,
	FLT_COND_MIME,
	FLT_COND_ACCOUNT
} FilterCondType;

typedef enum
{
	FLT_CONTAIN,
	FLT_EQUAL,
	FLT_REGEX,
	FLT_IN_ADDRESSBOOK
} FilterMatchType;

typedef enum
{
	FLT_NOT_MATCH	= 1 << 0,
	FLT_CASE_SENS	= 1 << 1
} FilterMatchFlag;

typedef enum
{
	FLT_OR,
	FLT_AND
} FilterBoolOp;

typedef enum
{
	FLT_ACTION_MOVE,
	FLT_ACTION_COPY,
	FLT_ACTION_NOT_RECEIVE,
	FLT_ACTION_DELETE,
	FLT_ACTION_EXEC,
	FLT_ACTION_EXEC_ASYNC,
	FLT_ACTION_MARK,
	FLT_ACTION_COLOR_LABEL,
	FLT_ACTION_MARK_READ,
	FLT_ACTION_FORWARD,
	FLT_ACTION_FORWARD_AS_ATTACHMENT,
	FLT_ACTION_REDIRECT,
	FLT_ACTION_STOP_EVAL,
	FLT_ACTION_NONE
} FilterActionType;

typedef enum
{
	FLT_BY_NONE,
	FLT_BY_AUTO,
	FLT_BY_FROM,
	FLT_BY_TO,
	FLT_BY_SUBJECT
} FilterCreateType;

typedef enum
{
	FLT_ERROR_OK,
	FLT_ERROR_ERROR,
	FLT_ERROR_EXEC_FAILED
} FilterErrorValue;

#define FLT_IS_NOT_MATCH(flag)	((flag & FLT_NOT_MATCH) != 0)
#define FLT_IS_CASE_SENS(flag)	((flag & FLT_CASE_SENS) != 0)

typedef gboolean (*FilterInAddressBookFunc)	(const gchar	*address);

struct _FilterCond
{
	FilterCondType type;

	gchar *header_name;

	gchar *str_value;
	gint int_value;

	FilterMatchType match_type;
	FilterMatchFlag match_flag;

	StrFindFunc match_func;
};

struct _FilterAction
{
	FilterActionType type;

	gchar *str_value;
	gint int_value;
};

struct _FilterRule
{
	gchar *name;

	FilterBoolOp bool_op;

	GSList *cond_list;
	GSList *action_list;

	FilterTiming timing;
	gboolean enabled;

	gchar *target_folder;
	gboolean recursive;
};

struct _FilterInfo
{
	PrefsAccount *account;
	MsgFlags flags;

	gboolean actions[FLT_ACTION_NONE];
	GSList *dest_list;
	FolderItem *move_dest;
	gboolean drop_done;

	FilterErrorValue error;
	gint last_exec_exit_status;
};

gint filter_apply			(GSList			*fltlist,
					 const gchar		*file,
					 FilterInfo		*fltinfo);
gint filter_apply_msginfo		(GSList			*fltlist,
					 MsgInfo		*msginfo,
					 FilterInfo		*fltinfo);

gint filter_action_exec			(FilterRule		*rule,
					 MsgInfo		*msginfo,
					 const gchar		*file,
					 FilterInfo		*fltinfo);

gboolean filter_match_rule		(FilterRule		*rule,
					 MsgInfo		*msginfo,
					 GSList			*hlist,
					 FilterInfo		*fltinfo);

gboolean filter_rule_requires_full_headers	(FilterRule	*rule);

/* read / write config */
GSList *filter_xml_node_to_filter_list	(GNode			*node);
GSList *filter_read_file		(const gchar		*file);
void filter_read_config			(void);
void filter_write_file			(GSList			*list,
					 const gchar		*file);
void filter_write_config		(void);

/* for old filterrc */
gchar *filter_get_str			(FilterRule		*rule);
FilterRule *filter_read_str		(const gchar		*str);

void filter_set_addressbook_func	(FilterInAddressBookFunc func);
FilterInAddressBookFunc filter_get_addressbook_func
					(void);

FilterRule *filter_rule_new		(const gchar		*name,
					 FilterBoolOp		 bool_op,
					 GSList			*cond_list,
					 GSList			*action_list);
FilterCond *filter_cond_new		(FilterCondType		 type,
					 FilterMatchType	 match_type,
					 FilterMatchFlag	 match_flag,
					 const gchar		*header,
					 const gchar		*value);
FilterAction *filter_action_new		(FilterActionType	 type,
					 const gchar		*str);
FilterInfo *filter_info_new		(void);

FilterRule *filter_junk_rule_create	(PrefsAccount		*account,
					 FolderItem		*default_junk,
					 gboolean		 is_manual);

void filter_rule_rename_dest_path	(FilterRule		*rule,
					 const gchar		*old_path,
					 const gchar		*new_path);
void filter_rule_delete_action_by_dest_path
					(FilterRule		*rule,
					 const gchar		*path);

void filter_list_rename_path		(const gchar		*old_path,
					 const gchar		*new_path);
void filter_list_delete_path		(const gchar		*path);

void filter_rule_match_type_str_to_enum	(const gchar		*type_str,
					 FilterMatchType	*type,
					 FilterMatchFlag	*flag);

void filter_get_keyword_from_msg	(MsgInfo		*msginfo,
					 gchar		       **header,
					 gchar		       **key,
					 FilterCreateType	 type);

void filter_rule_list_free		(GSList			*fltlist);
void filter_rule_free			(FilterRule		*rule);
void filter_cond_list_free		(GSList			*cond_list);
void filter_action_list_free		(GSList			*action_list);
void filter_info_free			(FilterInfo		*info);

#endif /* __FILTER_H__ */
