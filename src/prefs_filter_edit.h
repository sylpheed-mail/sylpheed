/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2012 Hiroyuki Yamamoto
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

#ifndef __PREFS_FILTER_EDIT_H__
#define __PREFS_FILTER_EDIT_H__

#include <gtk/gtkwidget.h>

#include "filter.h"

typedef struct _FilterCondEdit	FilterCondEdit;
typedef struct _CondHBox	CondHBox;
typedef struct _ActionHBox	ActionHBox;

typedef enum
{
	PF_COND_HEADER,
	PF_COND_TO_OR_CC,
	PF_COND_ANY_HEADER,
	PF_COND_BODY,
	PF_COND_CMD_TEST,
	PF_COND_SIZE,
	PF_COND_AGE,
	PF_COND_UNREAD,
	PF_COND_MARK,
	PF_COND_COLOR_LABEL,
	PF_COND_MIME,
	PF_COND_ACCOUNT,
	PF_COND_EDIT_HEADER,
	PF_COND_SEPARATOR,
	PF_COND_NONE
} CondMenuType;

typedef enum
{
	PF_MATCH_CONTAIN,
	PF_MATCH_NOT_CONTAIN,
	PF_MATCH_EQUAL,
	PF_MATCH_NOT_EQUAL,
	PF_MATCH_REGEX,
	PF_MATCH_NOT_REGEX,
	PF_MATCH_IN_ADDRESSBOOK,
	PF_MATCH_NOT_IN_ADDRESSBOOK,
	PF_MATCH_NONE
} MatchMenuType;

typedef enum
{
	PF_SIZE_LARGER,
	PF_SIZE_SMALLER
} SizeMatchType;

typedef enum
{
	PF_AGE_SHORTER,
	PF_AGE_LONGER
} AgeMatchType;

typedef enum
{
	PF_STATUS_MATCH,
	PF_STATUS_NOT_MATCH
} StatusMatchType;

typedef enum
{
	PF_ACTION_MOVE,
	PF_ACTION_COPY,
	PF_ACTION_NOT_RECEIVE,
	PF_ACTION_DELETE,
	PF_ACTION_EXEC,
	PF_ACTION_EXEC_ASYNC,
	PF_ACTION_MARK,
	PF_ACTION_COLOR_LABEL,
	PF_ACTION_MARK_READ,
	PF_ACTION_FORWARD,
	PF_ACTION_FORWARD_AS_ATTACHMENT,
	PF_ACTION_REDIRECT,
	PF_ACTION_STOP_EVAL,
	PF_ACTION_SEPARATOR,
	PF_ACTION_NONE
} ActionMenuType;

struct _FilterCondEdit {
	GtkWidget *cond_vbox;
	GSList *cond_hbox_list;

	GSList *hdr_list;
	GSList *rule_hdr_list;

	/* callback */
	void	(*add_hbox)	(CondHBox	*hbox);
};

struct _CondHBox {
	GtkWidget *hbox;

	GtkWidget *cond_type_optmenu;
	GtkWidget *match_type_optmenu;
	GtkWidget *size_match_optmenu;
	GtkWidget *age_match_optmenu;
	GtkWidget *status_match_optmenu;
	GtkWidget *key_entry;
	GtkWidget *spin_btn;
	GtkWidget *label;

	GtkWidget *match_menu_in_addr;
	GtkWidget *match_menu_not_in_addr;

	GtkWidget *del_btn;
	GtkWidget *add_btn;

	CondMenuType cur_type;
	gchar *cur_header_name;

	FilterCondEdit *cond_edit;
};

struct _ActionHBox {
	GtkWidget *hbox;

	GtkWidget *action_type_optmenu;
	GtkWidget *label;
	GtkWidget *folder_entry;
	GtkWidget *cmd_entry;
	GtkWidget *address_entry;
	GtkWidget *clabel_optmenu;

	GtkWidget *folder_sel_btn;

	GtkWidget *action_type_menu_items[PF_ACTION_NONE];

	GtkWidget *del_btn;
	GtkWidget *add_btn;
};


FilterRule *prefs_filter_edit_open	(FilterRule	*rule,
					 const gchar	*header,
					 const gchar	*key);

FilterCondEdit *prefs_filter_edit_cond_edit_create	(void);
void prefs_filter_edit_clear_cond_edit	(FilterCondEdit	*cond_edit);

void prefs_filter_edit_set_header_list	(FilterCondEdit	*cond_edit,
					 FilterRule	*rule);

CondHBox *prefs_filter_edit_cond_hbox_create
					(FilterCondEdit	*cond_edit);
ActionHBox *prefs_filter_edit_action_hbox_create	(void);

void prefs_filter_edit_cond_hbox_set	(CondHBox	*hbox,
					 FilterCond	*cond);
void prefs_filter_edit_action_hbox_set	(ActionHBox	*hbox,
					 FilterAction	*action);

void prefs_filter_edit_cond_hbox_select	(CondHBox	*hbox,
					 CondMenuType	 type,
					 const gchar	*header_name);

void prefs_filter_edit_set_cond_hbox_widgets	(CondHBox	*hbox,
						 CondMenuType	 type);
void prefs_filter_edit_set_action_hbox_widgets	(ActionHBox	*hbox,
						 ActionMenuType	 type);

void prefs_filter_edit_insert_cond_hbox		(FilterCondEdit	*cond_edit,
						 CondHBox	*hbox,
						 gint		 pos);

void prefs_filter_edit_add_rule_cond		(FilterCondEdit *cond_edit,
						 FilterRule	*rule);

FilterCond *prefs_filter_edit_cond_hbox_to_cond	(CondHBox	*hbox,
						 gboolean	 case_sens,
						 gchar	       **error_msg);

GSList *prefs_filter_edit_cond_edit_to_list	(FilterCondEdit	*cond_edit,
						 gboolean	 case_sens);

#endif /* __PREFS_FILTER_EDIT_H__ */
