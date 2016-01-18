/*
 * LibSylph -- E-Mail client library
 * Copyright (C) 1999-2015 Hiroyuki Yamamoto
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
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/types.h>
#if USE_ONIGURUMA
#  include <oniguruma.h>
#elif HAVE_REGEX_H
#  include <regex.h>
#endif
#include <time.h>

#include "filter.h"
#include "procmsg.h"
#include "procheader.h"
#include "folder.h"
#include "utils.h"
#include "xml.h"
#include "prefs.h"
#include "prefs_common.h"
#include "prefs_account.h"
#include "account.h"

typedef enum
{
	FLT_O_CONTAIN	= 1 << 0,
	FLT_O_CASE_SENS	= 1 << 1,
	FLT_O_REGEX	= 1 << 2
} FilterOldFlag;

static FilterInAddressBookFunc default_addrbook_func = NULL;

static gboolean filter_match_cond	(FilterCond	*cond,
					 MsgInfo	*msginfo,
					 GSList		*hlist,
					 FilterInfo	*fltinfo);
static gboolean filter_match_header_cond(FilterCond	*cond,
					 GSList		*hlist);
static gboolean filter_match_in_addressbook
					(FilterCond	*cond,
					 GSList		*hlist,
					 FilterInfo	*fltinfo);

static void filter_cond_free		(FilterCond	*cond);
static void filter_action_free		(FilterAction	*action);


gint filter_apply(GSList *fltlist, const gchar *file, FilterInfo *fltinfo)
{
	MsgInfo *msginfo;
	gint ret = 0;

	g_return_val_if_fail(file != NULL, -1);
	g_return_val_if_fail(fltinfo != NULL, -1);

	if (!fltlist) return 0;

	msginfo = procheader_parse_file(file, fltinfo->flags, FALSE);
	if (!msginfo) return 0;
	msginfo->file_path = g_strdup(file);

	/* inherit MIME flag */
	fltinfo->flags.tmp_flags =
		(fltinfo->flags.tmp_flags & ~MSG_CACHED_FLAG_MASK) |
		(msginfo->flags.tmp_flags & MSG_CACHED_FLAG_MASK);

	ret = filter_apply_msginfo(fltlist, msginfo, fltinfo);

	procmsg_msginfo_free(msginfo);

	return ret;
}

gint filter_apply_msginfo(GSList *fltlist, MsgInfo *msginfo,
			  FilterInfo *fltinfo)
{
	gchar *file;
	GSList *hlist, *cur;
	FilterRule *rule;
	gint ret = 0;

	g_return_val_if_fail(msginfo != NULL, -1);
	g_return_val_if_fail(fltinfo != NULL, -1);

	fltinfo->error = FLT_ERROR_OK;

	if (!fltlist) return 0;

	file = procmsg_get_message_file(msginfo);
	if (!file)
		return -1;
	hlist = procheader_get_header_list_from_file(file);
	if (!hlist) {
		g_free(file);
		return 0;
	}

	procmsg_set_auto_decrypt_message(FALSE);

	for (cur = fltlist; cur != NULL; cur = cur->next) {
		gboolean matched;

		rule = (FilterRule *)cur->data;
		if (!rule->enabled) continue;
		matched = filter_match_rule(rule, msginfo, hlist, fltinfo);
		if (fltinfo->error != FLT_ERROR_OK) {
			g_warning("filter_match_rule() returned error (code: %d)\n", fltinfo->error);
		}
		if (matched) {
			debug_print("filter-log: %s: rule [%s] matched\n",
				    G_STRFUNC, rule->name ? rule->name : "(No name)");
			ret = filter_action_exec(rule, msginfo, file, fltinfo);
			if (ret < 0) {
				g_warning("filter_action_exec() returned error (code: %d)\n", fltinfo->error);
				break;
			}
			if (fltinfo->drop_done == TRUE ||
			    fltinfo->actions[FLT_ACTION_STOP_EVAL] == TRUE)
				break;
		}
	}

	procmsg_set_auto_decrypt_message(TRUE);

	procheader_header_list_destroy(hlist);
	g_free(file);

	return ret;
}

gint filter_action_exec(FilterRule *rule, MsgInfo *msginfo, const gchar *file,
			FilterInfo *fltinfo)
{
	FolderItem *dest_folder = NULL;
	FilterAction *action;
	GSList *cur;
	gchar *cmdline;
	gboolean copy_to_self = FALSE;
	gint ret;

	g_return_val_if_fail(rule != NULL, -1);
	g_return_val_if_fail(msginfo != NULL, -1);
	g_return_val_if_fail(file != NULL, -1);
	g_return_val_if_fail(fltinfo != NULL, -1);

	for (cur = rule->action_list; cur != NULL; cur = cur->next) {
		action = (FilterAction *)cur->data;

		switch (action->type) {
		case FLT_ACTION_MARK:
			debug_print("filter_action_exec(): mark\n");
			MSG_SET_PERM_FLAGS(fltinfo->flags, MSG_MARKED);
			fltinfo->actions[action->type] = TRUE;
			break;
		case FLT_ACTION_COLOR_LABEL:
			debug_print("filter_action_exec(): color label: %d\n",
				    action->int_value);
			MSG_UNSET_PERM_FLAGS(fltinfo->flags,
					     MSG_CLABEL_FLAG_MASK);
			MSG_SET_COLORLABEL_VALUE(fltinfo->flags,
						 action->int_value);
			fltinfo->actions[action->type] = TRUE;
			break;
		case FLT_ACTION_MARK_READ:
			debug_print("filter_action_exec(): mark as read\n");
			if (msginfo->folder) {
				if (MSG_IS_NEW(fltinfo->flags))
					msginfo->folder->new--;
				if (MSG_IS_UNREAD(fltinfo->flags))
					msginfo->folder->unread--;
			}
			MSG_UNSET_PERM_FLAGS(fltinfo->flags, MSG_NEW|MSG_UNREAD);
			fltinfo->actions[action->type] = TRUE;
			break;
		case FLT_ACTION_EXEC:
			cmdline = g_strconcat(action->str_value, " \"", file,
					      "\"", NULL);
			ret = execute_command_line(cmdline, FALSE);
			fltinfo->last_exec_exit_status = ret;
			if (ret == -1) {
				fltinfo->error = FLT_ERROR_EXEC_FAILED;
				g_warning("filter_action_exec: cannot execute command: %s", cmdline);
				g_free(cmdline);
				return -1;
			}
			g_free(cmdline);
			fltinfo->actions[action->type] = TRUE;
			break;
		case FLT_ACTION_EXEC_ASYNC:
			cmdline = g_strconcat(action->str_value, " \"", file,
					      "\"", NULL);
			ret = execute_command_line(cmdline, TRUE);
			fltinfo->last_exec_exit_status = ret;
			if (ret == -1) {
				fltinfo->error = FLT_ERROR_EXEC_FAILED;
				g_warning("filter_action_exec: cannot execute command: %s", cmdline);
				g_free(cmdline);
				return -1;
			}
			g_free(cmdline);
			fltinfo->actions[action->type] = TRUE;
			break;
		default:
			break;
		}
	}

	for (cur = rule->action_list; cur != NULL; cur = cur->next) {
		action = (FilterAction *)cur->data;

		switch (action->type) {
		case FLT_ACTION_MOVE:
		case FLT_ACTION_COPY:
			dest_folder = folder_find_item_from_identifier
				(action->str_value);
			if (!dest_folder) {
				g_warning("dest folder '%s' not found\n",
					  action->str_value);
				fltinfo->error = FLT_ERROR_ERROR;
				return -1;
			}
			debug_print("filter_action_exec(): %s: dest_folder = %s\n",
				    action->type == FLT_ACTION_COPY ?
				    "copy" : "move", action->str_value);

			if (msginfo->folder) {
				gint val;

				/* local filtering */
				if (msginfo->folder == dest_folder)
					copy_to_self = TRUE;
				else {
					if (action->type == FLT_ACTION_COPY) {
						MsgFlags save_flags;

						save_flags = msginfo->flags;
						msginfo->flags = fltinfo->flags;
						val = folder_item_copy_msg
							(dest_folder, msginfo);
						msginfo->flags = save_flags;
						if (val == -1) {
							fltinfo->error = FLT_ERROR_ERROR;
							return -1;
						}
					}
					fltinfo->actions[action->type] = TRUE;
				}
			} else {
				MsgFlags save_flags;

				save_flags = msginfo->flags;
				msginfo->flags = fltinfo->flags;
				if (folder_item_add_msg_msginfo
					(dest_folder, msginfo, FALSE) < 0) {
					msginfo->flags = save_flags;
					fltinfo->error = FLT_ERROR_ERROR;
					return -1;
				}
				msginfo->flags = save_flags;
				fltinfo->actions[action->type] = TRUE;
			}

			fltinfo->dest_list = g_slist_append(fltinfo->dest_list,
							    dest_folder);
			if (action->type == FLT_ACTION_MOVE) {
				fltinfo->move_dest = dest_folder;
				fltinfo->drop_done = TRUE;
			}
			break;
		default:
			break;
		}
	}

	if (fltinfo->drop_done == TRUE)
		return 0;

	for (cur = rule->action_list; cur != NULL; cur = cur->next) {
		action = (FilterAction *)cur->data;

		switch (action->type) {
		case FLT_ACTION_NOT_RECEIVE:
			debug_print("filter_action_exec(): don't receive\n");
			fltinfo->drop_done = TRUE;
			fltinfo->actions[action->type] = TRUE;
			return 0;
		case FLT_ACTION_DELETE:
			debug_print("filter_action_exec(): delete\n");
			if (msginfo->folder) {
				/* local filtering */
				if (copy_to_self == FALSE)
					fltinfo->actions[action->type] = TRUE;
			} else
				fltinfo->actions[action->type] = TRUE;

			fltinfo->drop_done = TRUE;
			return 0;
		case FLT_ACTION_STOP_EVAL:
			debug_print("filter_action_exec(): stop evaluation\n");
			fltinfo->actions[action->type] = TRUE;
			return 0;
		default:
			break;
		}
	}

	return 0;
}

static gboolean strmatch_regex(const gchar *haystack, const gchar *needle)
{
#ifdef USE_ONIGURUMA
	gint ret = 0;
	OnigRegex reg;
	OnigErrorInfo err_info;
	const UChar *ptn = (const UChar *)needle;
	const UChar *str = (const UChar *)haystack;
	size_t haystack_len;

	ret = onig_new(&reg, ptn, ptn + strlen(needle),
		       /* ONIG_OPTION_EXTEND requires spaces to be escaped */
		       /* ONIG_OPTION_IGNORECASE|ONIG_OPTION_EXTEND, */
		       ONIG_OPTION_IGNORECASE,
		       ONIG_ENCODING_UTF8, ONIG_SYNTAX_POSIX_EXTENDED,
		       &err_info);
	if (ret != ONIG_NORMAL) {
		g_warning("strmatch_regex: onig_new() failed: %d", ret);
		return FALSE;
	}

	haystack_len = strlen(haystack);
	ret = onig_search(reg, str, str + haystack_len,
			  str, str + haystack_len, NULL, 0);
	onig_free(reg);

	if (ret >= 0)
		return TRUE;
	else
		return FALSE;
#elif defined(HAVE_REGCOMP)
	gint ret = 0;
	regex_t preg;

	ret = regcomp(&preg, needle, REG_ICASE|REG_EXTENDED);
	if (ret != 0)
		return FALSE;

	ret = regexec(&preg, haystack, 0, NULL, 0);
	regfree(&preg);

	if (ret == 0)
		return TRUE;
	else
		return FALSE;
#else
	return FALSE;
#endif
}

gboolean filter_match_rule(FilterRule *rule, MsgInfo *msginfo, GSList *hlist,
			   FilterInfo *fltinfo)
{
	FilterCond *cond;
	GSList *cur;
	gboolean matched;

	g_return_val_if_fail(rule->cond_list != NULL, FALSE);

	switch (rule->timing) {
	case FLT_TIMING_ANY:
		break;
	case FLT_TIMING_ON_RECEIVE:
		if (msginfo->folder != NULL)
			return FALSE;
		break;
	case FLT_TIMING_MANUAL:
		if (msginfo->folder == NULL)
			return FALSE;
		break;
	default:
		break;
	}

	if (rule->bool_op == FLT_AND) {
		for (cur = rule->cond_list; cur != NULL; cur = cur->next) {
			cond = (FilterCond *)cur->data;
			if (cond->type >= FLT_COND_SIZE_GREATER) {
				matched = filter_match_cond
					(cond, msginfo, hlist, fltinfo);
				if (matched == FALSE)
					return FALSE;
			}
		}

		for (cur = rule->cond_list; cur != NULL; cur = cur->next) {
			cond = (FilterCond *)cur->data;
			if (cond->type <= FLT_COND_TO_OR_CC) {
				matched = filter_match_cond
					(cond, msginfo, hlist, fltinfo);
				if (matched == FALSE)
					return FALSE;
			}
		}

		for (cur = rule->cond_list; cur != NULL; cur = cur->next) {
			cond = (FilterCond *)cur->data;
			if (cond->type == FLT_COND_BODY ||
			    cond->type == FLT_COND_CMD_TEST) {
				matched = filter_match_cond
					(cond, msginfo, hlist, fltinfo);
				if (matched == FALSE)
					return FALSE;
			}
		}

		return TRUE;
	} else if (rule->bool_op == FLT_OR) {
		for (cur = rule->cond_list; cur != NULL; cur = cur->next) {
			cond = (FilterCond *)cur->data;
			if (cond->type >= FLT_COND_SIZE_GREATER) {
				matched = filter_match_cond
					(cond, msginfo, hlist, fltinfo);
				if (matched == TRUE)
					return TRUE;
			}
		}

		for (cur = rule->cond_list; cur != NULL; cur = cur->next) {
			cond = (FilterCond *)cur->data;
			if (cond->type <= FLT_COND_TO_OR_CC) {
				matched = filter_match_cond
					(cond, msginfo, hlist, fltinfo);
				if (matched == TRUE)
					return TRUE;
			}
		}

		for (cur = rule->cond_list; cur != NULL; cur = cur->next) {
			cond = (FilterCond *)cur->data;
			if (cond->type == FLT_COND_BODY ||
			    cond->type == FLT_COND_CMD_TEST) {
				matched = filter_match_cond
					(cond, msginfo, hlist, fltinfo);
				if (matched == TRUE)
					return TRUE;
			}
		}

		return FALSE;
	}

	return FALSE;
}

static gboolean filter_match_cond(FilterCond *cond, MsgInfo *msginfo,
				  GSList *hlist, FilterInfo *fltinfo)
{
	gint ret;
	gboolean matched = FALSE;
	gboolean not_match = FALSE;
	gint64 timediff = 0;
	gchar *file;
	gchar *cmdline;
	PrefsAccount *cond_ac;

	switch (cond->type) {
	case FLT_COND_HEADER:
		if (cond->match_type == FLT_IN_ADDRESSBOOK)
			return filter_match_in_addressbook(cond, hlist, fltinfo);
		else
			return filter_match_header_cond(cond, hlist);
	case FLT_COND_ANY_HEADER:
		return filter_match_header_cond(cond, hlist);
	case FLT_COND_TO_OR_CC:
		if (cond->match_type == FLT_IN_ADDRESSBOOK)
			return filter_match_in_addressbook(cond, hlist, fltinfo);
		else
			return filter_match_header_cond(cond, hlist);
	case FLT_COND_BODY:
		matched = procmime_find_string(msginfo, cond->str_value,
					       cond->match_func);
		break;
	case FLT_COND_CMD_TEST:
		file = procmsg_get_message_file(msginfo);
		if (!file)
			return FALSE;
		cmdline = g_strconcat(cond->str_value, " \"", file, "\"", NULL);
		ret = execute_command_line_async_wait(cmdline);
		fltinfo->last_exec_exit_status = ret;
		matched = (ret == 0);
		if (ret == -1)
			fltinfo->error = FLT_ERROR_EXEC_FAILED;
		g_free(cmdline);
		g_free(file);
		break;
	case FLT_COND_SIZE_GREATER:
		matched = (msginfo->size > cond->int_value * 1024);
		break;
	case FLT_COND_AGE_GREATER:
		timediff = time(NULL) - msginfo->date_t;
		matched = (timediff > cond->int_value * 24 * 60 * 60);
		break;
	case FLT_COND_UNREAD:
		matched = MSG_IS_UNREAD(msginfo->flags);
		break;
	case FLT_COND_MARK:
		matched = MSG_IS_MARKED(msginfo->flags);
		break;
	case FLT_COND_COLOR_LABEL:
		matched = (MSG_GET_COLORLABEL_VALUE(msginfo->flags) != 0);
		break;
	case FLT_COND_MIME:
		matched = MSG_IS_MIME(msginfo->flags);
		break;
	case FLT_COND_ACCOUNT:
		cond_ac = account_find_from_id(cond->int_value);
		matched = (cond_ac != NULL && cond_ac == fltinfo->account);
		break;
	default:
		g_warning("filter_match_cond(): unknown condition: %d\n",
			  cond->type);
		fltinfo->error = FLT_ERROR_ERROR;
		return FALSE;
	}

	if (FLT_IS_NOT_MATCH(cond->match_flag)) {
		not_match = TRUE;
		matched = !matched;
	}

	if (matched && get_debug_mode()) {
		gchar *sv = cond->str_value ? cond->str_value : "";
		gchar *nm = not_match ? " (reverse match)" : "";

		switch (cond->type) {
		case FLT_COND_BODY:
			debug_print("filter-log: %s: BODY, str_value: [%s]%s\n", G_STRFUNC, sv, nm);
			break;
		case FLT_COND_CMD_TEST:
			debug_print("filter-log: %s: CMD_TEST, str_value: [%s]%s\n", G_STRFUNC, sv, nm);
			break;
		case FLT_COND_SIZE_GREATER:
			debug_print("filter-log: %s: SIZE_GREATER: %u %s %d (KB)%s\n", G_STRFUNC, msginfo->size, not_match ? "<=" : ">", cond->int_value, nm);
			break;
		case FLT_COND_AGE_GREATER:
			debug_print("filter-log: %s: AGE_GREATER: %lld (sec) %s %d (day)%s\n", G_STRFUNC, timediff, not_match ? "<=" : ">", cond->int_value, nm);
			break;
		case FLT_COND_UNREAD:
			debug_print("filter-log: %s: UNREAD%s\n", G_STRFUNC, nm);
			break;
		case FLT_COND_MARK:
			debug_print("filter-log: %s: MARK%s\n", G_STRFUNC, nm);
			break;
		case FLT_COND_COLOR_LABEL:
			debug_print("filter-log: %s: COLOR_LABEL%s\n", G_STRFUNC, nm);
			break;
		case FLT_COND_MIME:
			debug_print("filter-log: %s: MIME%s\n", G_STRFUNC, nm);
			break;
		case FLT_COND_ACCOUNT:
			debug_print("filter-log: %s: ACCOUNT [%d]%s\n", G_STRFUNC, cond->int_value, nm);
			break;
		default:
			break;
		}
	}

	return matched;
}

static gboolean filter_match_header_cond(FilterCond *cond, GSList *hlist)
{
	gboolean matched = FALSE;
	gboolean not_match = FALSE;
	GSList *cur;
	Header *header;

	for (cur = hlist; cur != NULL; cur = cur->next) {
		header = (Header *)cur->data;

		switch (cond->type) {
		case FLT_COND_HEADER:
			if (!g_ascii_strcasecmp
				(header->name, cond->header_name)) {
				if (!cond->str_value ||
				    cond->match_func(header->body,
						     cond->str_value))
					matched = TRUE;
			}
			break;
		case FLT_COND_ANY_HEADER:
			if (!cond->str_value ||
			    cond->match_func(header->body, cond->str_value))
				matched = TRUE;
			break;
		case FLT_COND_TO_OR_CC:
			if (!g_ascii_strcasecmp(header->name, "To") ||
			    !g_ascii_strcasecmp(header->name, "Cc")) {
				if (!cond->str_value ||
				    cond->match_func(header->body,
						     cond->str_value))
					matched = TRUE;
			}
			break;
		default:
			break;
		}

		if (matched == TRUE)
			break;
	}

	if (FLT_IS_NOT_MATCH(cond->match_flag)) {
		not_match = TRUE;
		matched = !matched;
	}

	if (matched && get_debug_mode()) {
		gchar *sv = cond->str_value ? cond->str_value : "";
		gchar *nm = not_match ? " (reverse match)" : "";

		switch (cond->type) {
		case FLT_COND_HEADER:
			debug_print("filter-log: %s: HEADER [%s], str_value: [%s]%s\n", G_STRFUNC, cond->header_name, sv, nm);
			break;
		case FLT_COND_ANY_HEADER:
			debug_print("filter-log: %s: ANY_HEADER, str_value: [%s]%s\n", G_STRFUNC, sv, nm);
			break;
		case FLT_COND_TO_OR_CC:
			debug_print("filter-log: %s: TO_OR_CC, str_value: [%s]%s\n", G_STRFUNC, sv, nm);
			break;
		default:
			break;
		}
	}

	return matched;
}

static gboolean filter_match_in_addressbook(FilterCond *cond, GSList *hlist,
					    FilterInfo *fltinfo)
{
	gboolean matched = FALSE;
	gboolean not_match = FALSE;
	GSList *cur;
	Header *header;

	if (!default_addrbook_func)
		return FALSE;
	if (cond->type != FLT_COND_HEADER && cond->type != FLT_COND_TO_OR_CC)
		return FALSE;

	for (cur = hlist; cur != NULL; cur = cur->next) {
		header = (Header *)cur->data;

		if (cond->type == FLT_COND_HEADER) {
			if (!g_ascii_strcasecmp
				(header->name, cond->header_name)) {
				if (default_addrbook_func(header->body))
					matched = TRUE;
			}
		} else if (cond->type == FLT_COND_TO_OR_CC) {
			if (!g_ascii_strcasecmp(header->name, "To") ||
			    !g_ascii_strcasecmp(header->name, "Cc")) {
				if (default_addrbook_func(header->body))
					matched = TRUE;
			}
		}

		if (matched == TRUE)
			break;
	}

	if (FLT_IS_NOT_MATCH(cond->match_flag)) {
		not_match = TRUE;
		matched = !matched;
	}

	if (matched && get_debug_mode()) {
		gchar *nm = not_match ? " (reverse match)" : "";

		switch (cond->type) {
		case FLT_COND_HEADER:
			debug_print("filter-log: %s: HEADER [%s], IN_ADDRESSBOOK%s\n", G_STRFUNC, cond->header_name, nm);
			break;
		case FLT_COND_TO_OR_CC:
			debug_print("filter-log: %s: TO_OR_CC, IN_ADDRESSBOOK%s\n", G_STRFUNC, nm);
			break;
		default:
			break;
		}
	}

	return matched;
}

gboolean filter_rule_requires_full_headers(FilterRule *rule)
{
	GSList *cur;

	for (cur = rule->cond_list; cur != NULL; cur = cur->next) {
		FilterCond *cond = (FilterCond *)cur->data;
		const gchar *name = cond->header_name;

		if (cond->type == FLT_COND_HEADER && name) {
			if (g_ascii_strcasecmp(name, "Date") != 0 &&
			    g_ascii_strcasecmp(name, "From") != 0 &&
			    g_ascii_strcasecmp(name, "To") != 0 &&
			    g_ascii_strcasecmp(name, "Newsgroups") != 0 &&
			    g_ascii_strcasecmp(name, "Subject") != 0)
				return TRUE;
		} else if (cond->type == FLT_COND_ANY_HEADER ||
			   cond->type == FLT_COND_TO_OR_CC)
			return TRUE;
	}

	return FALSE;
}

#define RETURN_IF_TAG_NOT_MATCH(tag_name)			\
	if (strcmp2(xmlnode->tag->tag, tag_name) != 0) {	\
		g_warning("tag name != \"" tag_name "\"\n");	\
		filter_cond_list_free(cond_list);		\
		filter_action_list_free(cond_list);		\
		return FALSE;					\
	}							\

#define STR_SWITCH(sw)		{ const gchar *sw_str = sw;
#define STR_CASE_BEGIN(str)	if (!strcmp(sw_str, str)) {
#define STR_CASE(str)		} else if (!strcmp(sw_str, str)) {
#define STR_CASE_END		} }

static gboolean filter_xml_node_func(GNode *node, gpointer data)
{
	XMLNode *xmlnode;
	GList *list;
	const gchar *name = NULL;
	FilterTiming timing = FLT_TIMING_ANY;
	gboolean enabled = TRUE;
	FilterRule *rule;
	FilterBoolOp bool_op = FLT_OR;
	const gchar *target_folder = NULL;
	gboolean recursive = FALSE;
	GSList *cond_list = NULL;
	GSList *action_list = NULL;
	GNode *child, *cond_child, *action_child;
	GSList **fltlist = (GSList **)data;

	if (g_node_depth(node) != 2) return FALSE;
	g_return_val_if_fail(node->data != NULL, FALSE);

	xmlnode = node->data;
	RETURN_IF_TAG_NOT_MATCH("rule");

	for (list = xmlnode->tag->attr; list != NULL; list = list->next) {
		XMLAttr *attr = (XMLAttr *)list->data;

		if (!attr || !attr->name || !attr->value) continue;
		if (!strcmp(attr->name, "name"))
			name = attr->value;
		else if (!strcmp(attr->name, "timing")) {
			if (!strcmp(attr->value, "any"))
				timing = FLT_TIMING_ANY;
			else if (!strcmp(attr->value, "receive"))
				timing = FLT_TIMING_ON_RECEIVE;
			else if (!strcmp(attr->value, "manual"))
				timing = FLT_TIMING_MANUAL;
		} else if (!strcmp(attr->name, "enabled")) {
			if (!strcmp(attr->value, "true"))
				enabled = TRUE;
			else
				enabled = FALSE;
		}
	}

	/* condition list */
	child = node->children;
	if (!child) {
		g_warning("<rule> must have children\n");
		return FALSE;
	}
	xmlnode = child->data;
	RETURN_IF_TAG_NOT_MATCH("condition-list");
	for (list = xmlnode->tag->attr; list != NULL; list = list->next) {
		XMLAttr *attr = (XMLAttr *)list->data;

		if (attr && attr->name && attr->value &&
		    !strcmp(attr->name, "bool")) {
			if (!strcmp(attr->value, "or"))
				bool_op = FLT_OR;
			else
				bool_op = FLT_AND;
		}
	}

	for (cond_child = child->children; cond_child != NULL;
	     cond_child = cond_child->next) {
		const gchar *type = NULL;
		const gchar *name = NULL;
		const gchar *value = NULL;
		gboolean case_sens = FALSE;
		FilterCond *cond;
		FilterCondType cond_type = FLT_COND_HEADER;
		FilterMatchType match_type = FLT_CONTAIN;
		FilterMatchFlag match_flag = 0;

		xmlnode = cond_child->data;
		if (!xmlnode || !xmlnode->tag || !xmlnode->tag->tag) continue;

		for (list = xmlnode->tag->attr; list != NULL; list = list->next) {
			XMLAttr *attr = (XMLAttr *)list->data;

			if (!attr || !attr->name || !attr->value) continue;

			STR_SWITCH(attr->name)
			STR_CASE_BEGIN("type")
				type = attr->value;
			STR_CASE("name")
				name = attr->value;
			STR_CASE("case")
				case_sens = TRUE;
			STR_CASE("recursive")
				if (!strcmp(attr->value, "true"))
					recursive = TRUE;
				else
					recursive = FALSE;
			STR_CASE_END
		}

		if (type) {
			filter_rule_match_type_str_to_enum
				(type, &match_type, &match_flag);
		}
		if (case_sens)
			match_flag |= FLT_CASE_SENS;
		value = xmlnode->element;

		STR_SWITCH(xmlnode->tag->tag)
		STR_CASE_BEGIN("match-header")
			cond_type = FLT_COND_HEADER;
		STR_CASE("match-any-header")
			cond_type = FLT_COND_ANY_HEADER;
		STR_CASE("match-to-or-cc")
			cond_type = FLT_COND_TO_OR_CC;
		STR_CASE("match-body-text")
			cond_type = FLT_COND_BODY;
		STR_CASE("command-test")
			cond_type = FLT_COND_CMD_TEST;
		STR_CASE("size")
			cond_type = FLT_COND_SIZE_GREATER;
		STR_CASE("age")
			cond_type = FLT_COND_AGE_GREATER;
		STR_CASE("unread")
			cond_type = FLT_COND_UNREAD;
		STR_CASE("mark")
			cond_type = FLT_COND_MARK;
		STR_CASE("color-label")
			cond_type = FLT_COND_COLOR_LABEL;
		STR_CASE("mime")
			cond_type = FLT_COND_MIME;
		STR_CASE("account-id")
			cond_type = FLT_COND_ACCOUNT;
		STR_CASE("target-folder")
			target_folder = value;
			continue;
		STR_CASE_END

		cond = filter_cond_new(cond_type, match_type, match_flag,
				       name, value);
		cond_list = g_slist_append(cond_list, cond);
	}

	/* action list */
	child = child->next;
	if (!child) {
		g_warning("<rule> must have multiple children\n");
		filter_cond_list_free(cond_list);
		return FALSE;
	}
	xmlnode = child->data;
	RETURN_IF_TAG_NOT_MATCH("action-list");

	for (action_child = child->children; action_child != NULL;
	     action_child = action_child->next) {
		FilterAction *action;
		FilterActionType action_type = FLT_ACTION_NONE;

		xmlnode = action_child->data;
		if (!xmlnode || !xmlnode->tag || !xmlnode->tag->tag) continue;

		STR_SWITCH(xmlnode->tag->tag)
		STR_CASE_BEGIN("move")
			action_type = FLT_ACTION_MOVE;
		STR_CASE("copy")
			action_type = FLT_ACTION_COPY;
		STR_CASE("not-receive")
			action_type = FLT_ACTION_NOT_RECEIVE;
		STR_CASE("delete")
			action_type = FLT_ACTION_DELETE;
		STR_CASE("exec")
			action_type = FLT_ACTION_EXEC;
		STR_CASE("exec-async")
			action_type = FLT_ACTION_EXEC_ASYNC;
		STR_CASE("mark")
			action_type = FLT_ACTION_MARK;
		STR_CASE("color-label")
			action_type = FLT_ACTION_COLOR_LABEL;
		STR_CASE("mark-as-read")
			action_type = FLT_ACTION_MARK_READ;
		STR_CASE("forward")
			action_type = FLT_ACTION_FORWARD;
		STR_CASE("forward-as-attachment")
			action_type = FLT_ACTION_FORWARD_AS_ATTACHMENT;
		STR_CASE("redirect")
			action_type = FLT_ACTION_REDIRECT;
		STR_CASE("stop-eval")
			action_type = FLT_ACTION_STOP_EVAL;
		STR_CASE_END

		action = filter_action_new(action_type, xmlnode->element);
		action_list = g_slist_append(action_list, action);
	}

	if (name && cond_list) {
		rule = filter_rule_new(name, bool_op, cond_list, action_list);
		rule->timing = timing;
		rule->enabled = enabled;
		if (target_folder) {
			rule->target_folder = g_strdup(target_folder);
			rule->recursive = recursive;
		}
		*fltlist = g_slist_prepend(*fltlist, rule);
	}

	return FALSE;
}

#undef RETURN_IF_TAG_NOT_MATCH
#undef STR_SWITCH
#undef STR_CASE_BEGIN
#undef STR_CASE
#undef STR_CASE_END

GSList *filter_xml_node_to_filter_list(GNode *node)
{
	GSList *fltlist = NULL;

	g_return_val_if_fail(node != NULL, NULL);

	g_node_traverse(node, G_PRE_ORDER, G_TRAVERSE_ALL, 2,
			filter_xml_node_func, &fltlist);
	fltlist = g_slist_reverse(fltlist);

	return fltlist;
}

GSList *filter_read_file(const gchar *file)
{
	GNode *node;
	GSList *list;

	g_return_val_if_fail(file != NULL, NULL);

	debug_print("Reading %s\n", file);

	if (!is_file_exist(file))
		return NULL;

	node = xml_parse_file(file);
	if (!node) {
		g_warning("Can't parse %s\n", file);
		return NULL;
	}

	list = filter_xml_node_to_filter_list(node);

	xml_free_tree(node);

	return list;
}

void filter_read_config(void)
{
	gchar *rcpath;

	debug_print("Reading filter configuration...\n");

	/* remove all previous filter list */
	while (prefs_common.fltlist != NULL) {
		FilterRule *rule = (FilterRule *)prefs_common.fltlist->data;

		filter_rule_free(rule);
		prefs_common.fltlist = g_slist_remove(prefs_common.fltlist,
						      rule);
	}

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, FILTER_LIST,
			     NULL);
	prefs_common.fltlist = filter_read_file(rcpath);
	g_free(rcpath);
}

#define NODE_NEW(tag, text) \
	node = xml_node_new(xml_tag_new(tag), text)
#define ADD_ATTR(name, value) \
	xml_tag_add_attr(node->tag, xml_attr_new(name, value))

void filter_write_file(GSList *list, const gchar *file)
{
	PrefFile *pfile;
	GSList *cur;

	g_return_if_fail(file != NULL);

	if ((pfile = prefs_file_open(file)) == NULL) {
		g_warning("failed to write filter configuration to file: %s\n",
			  file);
		return;
	}

	xml_file_put_xml_decl(pfile->fp);
	fputs("\n<filter>\n", pfile->fp);

	for (cur = list; cur != NULL; cur = cur->next) {
		FilterRule *rule = (FilterRule *)cur->data;
		GSList *cur_cond;
		GSList *cur_action;
		gchar match_type[64];
		gchar nstr[16];

		fputs("    <rule name=\"", pfile->fp);
		xml_file_put_escape_str(pfile->fp, rule->name);
		fprintf(pfile->fp, "\" timing=\"%s\"",
			rule->timing == FLT_TIMING_ON_RECEIVE ? "receive" :
			rule->timing == FLT_TIMING_MANUAL ? "manual" : "any");
		fprintf(pfile->fp, " enabled=\"%s\">\n",
			rule->enabled ? "true" : "false");

		fprintf(pfile->fp, "        <condition-list bool=\"%s\">\n",
			rule->bool_op == FLT_OR ? "or" : "and");

		for (cur_cond = rule->cond_list; cur_cond != NULL;
		     cur_cond = cur_cond->next) {
			FilterCond *cond = (FilterCond *)cur_cond->data;
			XMLNode *node = NULL;

			switch (cond->match_type) {
			case FLT_CONTAIN:
				strncpy2(match_type,
					 FLT_IS_NOT_MATCH(cond->match_flag)
					 ? "not-contain" : "contains",
					 sizeof(match_type));
				break;
			case FLT_EQUAL:
				strncpy2(match_type,
					 FLT_IS_NOT_MATCH(cond->match_flag)
					 ? "is-not" : "is", sizeof(match_type));
				break;
			case FLT_REGEX:
				strncpy2(match_type,
					 FLT_IS_NOT_MATCH(cond->match_flag)
					 ? "not-regex" : "regex",
					 sizeof(match_type));
				break;
			case FLT_IN_ADDRESSBOOK:
				strncpy2(match_type,
					 FLT_IS_NOT_MATCH(cond->match_flag)
					 ? "not-in-addressbook" : "in-addressbook",
					 sizeof(match_type));
				break;
			default:
				match_type[0] = '\0';
				break;
			}

			switch (cond->type) {
			case FLT_COND_HEADER:
				NODE_NEW("match-header", cond->str_value);
				ADD_ATTR("type", match_type);
				ADD_ATTR("name", cond->header_name);
				if (FLT_IS_CASE_SENS(cond->match_flag))
					ADD_ATTR("case", "true");
				break;
			case FLT_COND_ANY_HEADER:
				NODE_NEW("match-any-header", cond->str_value);
				ADD_ATTR("type", match_type);
				if (FLT_IS_CASE_SENS(cond->match_flag))
					ADD_ATTR("case", "true");
				break;
			case FLT_COND_TO_OR_CC:
				NODE_NEW("match-to-or-cc", cond->str_value);
				ADD_ATTR("type", match_type);
				if (FLT_IS_CASE_SENS(cond->match_flag))
					ADD_ATTR("case", "true");
				break;
			case FLT_COND_BODY:
				NODE_NEW("match-body-text", cond->str_value);
				ADD_ATTR("type", match_type);
				if (FLT_IS_CASE_SENS(cond->match_flag))
					ADD_ATTR("case", "true");
				break;
			case FLT_COND_CMD_TEST:
				NODE_NEW("command-test", cond->str_value);
				break;
			case FLT_COND_SIZE_GREATER:
				NODE_NEW("size", itos_buf(nstr, cond->int_value));
				ADD_ATTR("type",
					 FLT_IS_NOT_MATCH(cond->match_flag)
					 ? "lt" : "gt");
				break;
			case FLT_COND_AGE_GREATER:
				NODE_NEW("age", itos_buf(nstr, cond->int_value));
				ADD_ATTR("type",
					 FLT_IS_NOT_MATCH(cond->match_flag)
					 ? "lt" : "gt");
				break;
			case FLT_COND_UNREAD:
				NODE_NEW("unread", NULL);
				ADD_ATTR("type",
					 FLT_IS_NOT_MATCH(cond->match_flag)
					 ? "is-not" : "is");
				break;
			case FLT_COND_MARK:
				NODE_NEW("mark", NULL);
				ADD_ATTR("type",
					 FLT_IS_NOT_MATCH(cond->match_flag)
					 ? "is-not" : "is");
				break;
			case FLT_COND_COLOR_LABEL:
				NODE_NEW("color-label", NULL);
				ADD_ATTR("type",
					 FLT_IS_NOT_MATCH(cond->match_flag)
					 ? "is-not" : "is");
				break;
			case FLT_COND_MIME:
				NODE_NEW("mime", NULL);
				ADD_ATTR("type",
					 FLT_IS_NOT_MATCH(cond->match_flag)
					 ? "is-not" : "is");
				break;
			case FLT_COND_ACCOUNT:
				 NODE_NEW("account-id", itos_buf(nstr, cond->int_value));
				 break;
			default:
				 break;
			}

			if (node) {
				fputs("            ", pfile->fp);
				xml_file_put_node(pfile->fp, node);
				xml_free_node(node);
			}
		}

		if (rule->target_folder) {
			XMLNode *node;

			NODE_NEW("target-folder", rule->target_folder);
			ADD_ATTR("recursive", rule->recursive
				 ? "true" : "false");
			fputs("            ", pfile->fp);
			xml_file_put_node(pfile->fp, node);
			xml_free_node(node);
		}

		fputs("        </condition-list>\n", pfile->fp);

		fputs("        <action-list>\n", pfile->fp);

		for (cur_action = rule->action_list; cur_action != NULL;
		     cur_action = cur_action->next) {
			FilterAction *action = (FilterAction *)cur_action->data;
			XMLNode *node = NULL;

			switch (action->type) {
			case FLT_ACTION_MOVE:
				NODE_NEW("move", action->str_value);
				break;
			case FLT_ACTION_COPY:
				NODE_NEW("copy", action->str_value);
				break;
			case FLT_ACTION_NOT_RECEIVE:
				NODE_NEW("not-receive", NULL);
				break;
			case FLT_ACTION_DELETE:
				NODE_NEW("delete", NULL);
				break;
			case FLT_ACTION_EXEC:
				NODE_NEW("exec", action->str_value);
				break;
			case FLT_ACTION_EXEC_ASYNC:
				NODE_NEW("exec-async", action->str_value);
				break;
			case FLT_ACTION_MARK:
				NODE_NEW("mark", NULL);
				break;
			case FLT_ACTION_COLOR_LABEL:
				NODE_NEW("color-label", action->str_value);
				break;
			case FLT_ACTION_MARK_READ:
				NODE_NEW("mark-as-read", NULL);
				break;
			case FLT_ACTION_FORWARD:
				NODE_NEW("forward", action->str_value);
				break;
			case FLT_ACTION_FORWARD_AS_ATTACHMENT:
				NODE_NEW("forward-as-attachment",
					 action->str_value);
				break;
			case FLT_ACTION_REDIRECT:
				NODE_NEW("redirect", action->str_value);
				break;
			case FLT_ACTION_STOP_EVAL:
				NODE_NEW("stop-eval", NULL);
				break;
			default:
				break;
			}

			if (node) {
				fputs("            ", pfile->fp);
				xml_file_put_node(pfile->fp, node);
				xml_free_node(node);
			}
		}

		fputs("        </action-list>\n", pfile->fp);

		fputs("    </rule>\n", pfile->fp);
	}

	fputs("</filter>\n", pfile->fp);

	if (prefs_file_close(pfile) < 0) {
		g_warning("failed to write filter configuration to file: %s\n",
			  file);
		return;
	}
}

void filter_write_config(void)
{
	gchar *rcpath;

	debug_print("Writing filter configuration...\n");

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, FILTER_LIST,
			     NULL);
	filter_write_file(prefs_common.fltlist, rcpath);
	g_free(rcpath);
}

#undef NODE_NEW
#undef ADD_ATTR

gchar *filter_get_str(FilterRule *rule)
{
	gchar *str;
	FilterCond *cond1, *cond2;
	FilterAction *action = NULL;
	GSList *cur;
	gint flag1 = 0, flag2 = 0;

	cond1 = (FilterCond *)rule->cond_list->data;
	if (rule->cond_list->next)
		cond2 = (FilterCond *)rule->cond_list->next->data;
	else
		cond2 = NULL;

	/* new -> old flag conversion */
	switch (cond1->match_type) {
	case FLT_CONTAIN:
	case FLT_EQUAL:
		flag1 = FLT_IS_NOT_MATCH(cond1->match_flag) ? 0 : FLT_O_CONTAIN;
		if (FLT_IS_CASE_SENS(cond1->match_flag))
			flag1 |= FLT_O_CASE_SENS;
		break;
	case FLT_REGEX:
		flag1 = FLT_O_REGEX; break;
	default:
		break;
	}
	if (cond2) {
		switch (cond2->match_type) {
		case FLT_CONTAIN:
		case FLT_EQUAL:
			flag2 = FLT_IS_NOT_MATCH(cond2->match_flag) ? 0 : FLT_O_CONTAIN;
			if (FLT_IS_CASE_SENS(cond2->match_flag))
				flag2 |= FLT_O_CASE_SENS;
			break;
		case FLT_REGEX:
			flag2 = FLT_O_REGEX; break;
		default:
			break;
		}
	} else
		flag2 = FLT_O_CONTAIN;

	for (cur = rule->action_list; cur != NULL; cur = cur->next) {
		action = (FilterAction *)cur->data;
		if (action->type == FLT_ACTION_MOVE ||
		    action->type == FLT_ACTION_NOT_RECEIVE ||
		    action->type == FLT_ACTION_DELETE)
			break;
	}

	str = g_strdup_printf
		("%s:%s:%c:%s:%s:%s:%d:%d:%c",
		 cond1->header_name, cond1->str_value ? cond1->str_value : "",
		 (cond2 && cond2->header_name) ?
		 	(rule->bool_op == FLT_AND ? '&' : '|') : ' ',
		 (cond2 && cond2->header_name) ? cond2->header_name : "",
		 (cond2 && cond2->str_value) ? cond2->str_value : "",
		 (action && action->str_value) ? action->str_value : "",
		 flag1, flag2,
		 (action && action->type == FLT_ACTION_MOVE) ? 'm' :
		 (action && action->type == FLT_ACTION_NOT_RECEIVE) ? 'n' :
		 (action && action->type == FLT_ACTION_DELETE) ? 'd' : ' ');

	return str;
}

#define PARSE_ONE_PARAM(p, srcp) \
{ \
	p = strchr(srcp, '\t'); \
	if (!p) return NULL; \
	else \
		*p++ = '\0'; \
}

FilterRule *filter_read_str(const gchar *str)
{
	FilterRule *rule;
	FilterBoolOp bool_op;
	gint flag;
	FilterCond *cond;
	FilterMatchType match_type;
	FilterMatchFlag match_flag;
	FilterAction *action;
	GSList *cond_list = NULL;
	GSList *action_list = NULL;
	gchar *tmp;
	gchar *rule_name;
	gchar *name1, *body1, *op, *name2, *body2, *dest;
	gchar *flag1 = NULL, *flag2 = NULL, *action1 = NULL;

	Xstrdup_a(tmp, str, return NULL);

	name1 = tmp;
	PARSE_ONE_PARAM(body1, name1);
	PARSE_ONE_PARAM(op, body1);
	PARSE_ONE_PARAM(name2, op);
	PARSE_ONE_PARAM(body2, name2);
	PARSE_ONE_PARAM(dest, body2);
	if (strchr(dest, '\t')) {
		gchar *p;

		PARSE_ONE_PARAM(flag1, dest);
		PARSE_ONE_PARAM(flag2, flag1);
		PARSE_ONE_PARAM(action1, flag2);
		if ((p = strchr(action1, '\t'))) *p = '\0';
	}

	bool_op = (*op == '&') ? FLT_AND : FLT_OR;

	if (*name1) {
		if (flag1)
			flag = (FilterOldFlag)strtoul(flag1, NULL, 10);
		else
			flag = FLT_O_CONTAIN;
		match_type = FLT_CONTAIN;
		match_flag = 0;
		if (flag & FLT_O_REGEX)
			match_type = FLT_REGEX;
		else if (!(flag & FLT_O_CONTAIN))
			match_flag = FLT_NOT_MATCH;
		if (flag & FLT_O_CASE_SENS)
			match_flag |= FLT_CASE_SENS;
		cond = filter_cond_new(FLT_COND_HEADER, match_type, match_flag,
				       name1, body1);
		cond_list = g_slist_append(cond_list, cond);
	}
	if (*name2) {
		if (flag2)
			flag = (FilterOldFlag)strtoul(flag2, NULL, 10);
		else
			flag = FLT_O_CONTAIN;
		match_type = FLT_CONTAIN;
		match_flag = 0;
		if (flag & FLT_O_REGEX)
			match_type = FLT_REGEX;
		else if (!(flag & FLT_O_CONTAIN))
			match_flag = FLT_NOT_MATCH;
		if (flag & FLT_O_CASE_SENS)
			match_flag |= FLT_CASE_SENS;
		cond = filter_cond_new(FLT_COND_HEADER, match_type, match_flag,
				       name2, body2);
		cond_list = g_slist_append(cond_list, cond);
	}

	action = filter_action_new(FLT_ACTION_MOVE,
				   *dest ? g_strdup(dest) : NULL);
	if (action1) {
		switch (*action1) {
		case 'm': action->type = FLT_ACTION_MOVE;		break;
		case 'n': action->type = FLT_ACTION_NOT_RECEIVE;	break;
		case 'd': action->type = FLT_ACTION_DELETE;		break;
		default:  g_warning("Invalid action: `%c'\n", *action1);
		}
	}
	action_list = g_slist_append(action_list, action);

	Xstrdup_a(rule_name, str, return NULL);
	subst_char(rule_name, '\t', ':');

	rule = filter_rule_new(rule_name, bool_op, cond_list, action_list);

	return rule;
}

void filter_set_addressbook_func(FilterInAddressBookFunc func)
{
	default_addrbook_func = func;
}

FilterInAddressBookFunc filter_get_addressbook_func(void)
{
	return default_addrbook_func;
}

FilterRule *filter_rule_new(const gchar *name, FilterBoolOp bool_op,
			    GSList *cond_list, GSList *action_list)
{
	FilterRule *rule;

	rule = g_new0(FilterRule, 1);
	rule->name = g_strdup(name);
	rule->bool_op = bool_op;
	rule->cond_list = cond_list;
	rule->action_list = action_list;
	rule->timing = FLT_TIMING_ANY;
	rule->enabled = TRUE;

	return rule;
}

FilterCond *filter_cond_new(FilterCondType type,
			    FilterMatchType match_type,
			    FilterMatchFlag match_flag,
			    const gchar *header, const gchar *value)
{
	FilterCond *cond;

	cond = g_new0(FilterCond, 1);
	cond->type = type;
	cond->match_type = match_type;
	cond->match_flag = match_flag;

	if (type == FLT_COND_HEADER)
		cond->header_name =
			(header && *header) ? g_strdup(header) : NULL;
	else
		cond->header_name = NULL;

	cond->str_value = (value && *value) ? g_strdup(value) : NULL;
	if (type == FLT_COND_SIZE_GREATER || type == FLT_COND_AGE_GREATER ||
	    type == FLT_COND_ACCOUNT)
		cond->int_value = atoi(value);
	else
		cond->int_value = 0;

	if (match_type == FLT_REGEX)
		cond->match_func = strmatch_regex;
	else if (match_type == FLT_EQUAL) {
		if (FLT_IS_CASE_SENS(match_flag))
			cond->match_func = str_find_equal;
		else
			cond->match_func = str_case_find_equal;
	} else if (match_type == FLT_IN_ADDRESSBOOK) {
		cond->match_func = str_case_find_equal;
	} else {
		if (FLT_IS_CASE_SENS(match_flag))
			cond->match_func = str_find;
		else
			cond->match_func = str_case_find;
	}

	return cond;
}

FilterAction *filter_action_new(FilterActionType type, const gchar *str)
{
	FilterAction *action;

	action = g_new0(FilterAction, 1);
	action->type = type;

	action->str_value = (str && *str) ? g_strdup(str) : NULL;
	if (type == FLT_ACTION_COLOR_LABEL && str)
		action->int_value = atoi(str);
	else
		action->int_value = 0;

	return action;
}

FilterInfo *filter_info_new(void)
{
	FilterInfo *fltinfo;

	fltinfo = g_new0(FilterInfo, 1);
	fltinfo->dest_list = NULL;
	fltinfo->move_dest = NULL;
	fltinfo->drop_done = FALSE;
	fltinfo->error = FLT_ERROR_OK;
	fltinfo->last_exec_exit_status = 0;

	return fltinfo;
}

FilterRule *filter_junk_rule_create(PrefsAccount *account,
				    FolderItem *default_junk,
				    gboolean is_manual)
{
	FilterRule *rule;
	FilterCond *cond;
	FilterAction *action;
	GSList *cond_list = NULL, *action_list = NULL;
	gchar *junk_id = NULL;
	FolderItem *item = NULL;

	if (!prefs_common.junk_classify_cmd)
		return NULL;

	if (prefs_common.junk_folder)
		item = folder_find_item_from_identifier(prefs_common.junk_folder);

	if (!item && account) {
		Folder *folder = NULL;
		GList *list;

		/* find most suitable Junk folder for account */

		if (account->inbox && *account->inbox == '#') {
			FolderItem *inbox;
			inbox = folder_find_item_from_identifier(account->inbox);
			if (inbox) {
				folder = inbox->folder;
				if (folder)
					item = folder_get_junk(folder);
			}
		}
		if (!item) {
			folder = FOLDER(account->folder);
			if (folder)
				item = folder_get_junk(folder);
		}
		if (!item) {
			for (list = folder_get_list(); list != NULL; list = list->next) {
				folder = FOLDER(list->data);
				if (FOLDER_IS_LOCAL(folder)) {
					if (folder->account == account)
						item = folder_get_junk(folder);
					if (!item && folder->node) {
						item = FOLDER_ITEM(folder->node->data);
						if (item && item->account == account && item->folder)
							item = folder_get_junk(item->folder);
						else
							item = NULL;
					}
				}
				if (item)
					break;
			}
		}
	}

	if (!item)
		item = default_junk;
	if (!item)
		item = folder_get_default_junk();
	if (!item)
		return NULL;
	junk_id = folder_item_get_identifier(item);
	if (!junk_id)
		return NULL;

	debug_print("filter_junk_rule_create: junk folder: %s\n",
		    junk_id);

	if (prefs_common.nofilter_junk_sender_in_book) {
		cond = filter_cond_new(FLT_COND_HEADER, FLT_IN_ADDRESSBOOK,
				       FLT_NOT_MATCH, "From", NULL);
		cond_list = g_slist_append(cond_list, cond);
	}

	cond = filter_cond_new(FLT_COND_CMD_TEST, 0, 0, NULL,
			       prefs_common.junk_classify_cmd);
	cond_list = g_slist_append(cond_list, cond);

	if (prefs_common.delete_junk_on_recv && !is_manual) {
		action = filter_action_new(FLT_ACTION_COPY, junk_id);
		action_list = g_slist_append(NULL, action);
		action = filter_action_new(FLT_ACTION_DELETE, NULL);
		action_list = g_slist_append(action_list, action);
	} else {
		action = filter_action_new(FLT_ACTION_MOVE, junk_id);
		action_list = g_slist_append(NULL, action);
	}

	if (prefs_common.mark_junk_as_read) {
		action = filter_action_new(FLT_ACTION_MARK_READ, NULL);
		action_list = g_slist_append(action_list, action);
	}

	if (is_manual)
		rule = filter_rule_new(_("Junk mail filter (manual)"), FLT_AND,
				       cond_list, action_list);
	else
		rule = filter_rule_new(_("Junk mail filter"), FLT_AND,
				       cond_list, action_list);

	g_free(junk_id);

        return rule;
}

void filter_rule_rename_dest_path(FilterRule *rule, const gchar *old_path,
				  const gchar *new_path)
{
	FilterAction *action;
	GSList *cur;
	gchar *base;
	gchar *dest_path;
	gint oldpathlen;

	oldpathlen = strlen(old_path);

	for (cur = rule->action_list; cur != NULL; cur = cur->next) {
		action = (FilterAction *)cur->data;

		if (action->type != FLT_ACTION_MOVE &&
		    action->type != FLT_ACTION_COPY)
			continue;

		if (action->str_value &&
		    !strncmp(old_path, action->str_value, oldpathlen)) {
			base = action->str_value + oldpathlen;
			if (*base != '/' && *base != '\0')
				continue;
			while (*base == '/') base++;
			if (*base == '\0')
				dest_path = g_strdup(new_path);
			else
				dest_path = g_strconcat(new_path, "/", base,
							NULL);
			debug_print("filter_rule_rename_dest_path(): "
				    "renaming %s -> %s\n",
				    action->str_value, dest_path);
			g_free(action->str_value);
			action->str_value = dest_path;
		}
	}
}

void filter_rule_delete_action_by_dest_path(FilterRule *rule, const gchar *path)
{
	FilterAction *action;
	GSList *cur;
	GSList *next;
	gint pathlen;

	pathlen = strlen(path);

	for (cur = rule->action_list; cur != NULL; cur = next) {
		action = (FilterAction *)cur->data;
		next = cur->next;

		if (action->type != FLT_ACTION_MOVE &&
		    action->type != FLT_ACTION_COPY)
			continue;

		if (action->str_value &&
		    !strncmp(path, action->str_value, pathlen) &&
		    (action->str_value[pathlen] == '/' ||
		     action->str_value[pathlen] == '\0')) {
			debug_print("filter_rule_delete_action_by_dest_path(): "
				    "deleting %s\n", action->str_value);
			rule->action_list = g_slist_remove
				(rule->action_list, action);
			filter_action_free(action);
		}
	}
}

void filter_list_rename_path(const gchar *old_path, const gchar *new_path)
{
	GSList *cur;

	g_return_if_fail(old_path != NULL);
	g_return_if_fail(new_path != NULL);

	for (cur = prefs_common.fltlist; cur != NULL; cur = cur->next) {
		FilterRule *rule = (FilterRule *)cur->data;
		filter_rule_rename_dest_path(rule, old_path, new_path);
	}

	filter_write_config();
}

void filter_list_delete_path(const gchar *path)
{
	GSList *cur;
	GSList *next;

	g_return_if_fail(path != NULL);

	for (cur = prefs_common.fltlist; cur != NULL; cur = next) {
		FilterRule *rule = (FilterRule *)cur->data;
		next = cur->next;

		filter_rule_delete_action_by_dest_path(rule, path);
		if (!rule->action_list) {
			prefs_common.fltlist =
				g_slist_remove(prefs_common.fltlist, rule);
			filter_rule_free(rule);
		}
	}

	filter_write_config();
}

void filter_rule_match_type_str_to_enum(const gchar *match_type,
					FilterMatchType *type,
					FilterMatchFlag *flag)
{
	g_return_if_fail(match_type != NULL);

	*type = FLT_CONTAIN;
	*flag = 0;

	if (!strcmp(match_type, "contains")) {
		*type = FLT_CONTAIN;
	} else if (!strcmp(match_type, "not-contain")) {
		*type = FLT_CONTAIN;
		*flag = FLT_NOT_MATCH;
	} else if (!strcmp(match_type, "is")) {
		*type = FLT_EQUAL;
	} else if (!strcmp(match_type, "is-not")) {
		*type = FLT_EQUAL;
		*flag = FLT_NOT_MATCH;
	} else if (!strcmp(match_type, "regex")) {
		*type = FLT_REGEX;
	} else if (!strcmp(match_type, "not-regex")) {
		*type = FLT_REGEX;
		*flag = FLT_NOT_MATCH;
	} else if (!strcmp(match_type, "in-addressbook")) {
		*type = FLT_IN_ADDRESSBOOK;
	} else if (!strcmp(match_type, "not-in-addressbook")) {
		*type = FLT_IN_ADDRESSBOOK;
		*flag = FLT_NOT_MATCH;
	} else if (!strcmp(match_type, "gt")) {
	} else if (!strcmp(match_type, "lt")) {
		*flag = FLT_NOT_MATCH;
	}
}

void filter_get_keyword_from_msg(MsgInfo *msginfo, gchar **header, gchar **key,
				 FilterCreateType type)
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
	case FLT_BY_NONE:
		return;
	case FLT_BY_AUTO:
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
			gchar *p;

			SET_FILTER_KEY("X-Sequence", H_X_SEQUENCE);
			p = *key;
			while (*p != '\0') {
				while (*p != '\0' && !g_ascii_isspace(*p)) p++;
				while (g_ascii_isspace(*p)) p++;
				if (g_ascii_isdigit(*p)) {
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
	case FLT_BY_FROM:
		*header = g_strdup("From");
		*key = g_strdup(msginfo->from);
		break;
	case FLT_BY_TO:
		*header = g_strdup("To");
		*key = g_strdup(msginfo->to);
		break;
	case FLT_BY_SUBJECT:
		*header = g_strdup("Subject");
		*key = g_strdup(msginfo->subject);
		break;
	default:
		break;
	}
}

void filter_rule_list_free(GSList *fltlist)
{
	GSList *cur;

	for (cur = fltlist; cur != NULL; cur = cur->next)
		filter_rule_free((FilterRule *)cur->data);
	g_slist_free(fltlist);
}

void filter_rule_free(FilterRule *rule)
{
	if (!rule) return;

	g_free(rule->name);
	g_free(rule->target_folder);

	filter_cond_list_free(rule->cond_list);
	filter_action_list_free(rule->action_list);

	g_free(rule);
}

void filter_cond_list_free(GSList *cond_list)
{
	GSList *cur;

	for (cur = cond_list; cur != NULL; cur = cur->next)
		filter_cond_free((FilterCond *)cur->data);
	g_slist_free(cond_list);
}

void filter_action_list_free(GSList *action_list)
{
	GSList *cur;

	for (cur = action_list; cur != NULL; cur = cur->next)
		filter_action_free((FilterAction *)cur->data);
	g_slist_free(action_list);
}

static void filter_cond_free(FilterCond *cond)
{
	g_free(cond->header_name);
	g_free(cond->str_value);
	g_free(cond);
}

static void filter_action_free(FilterAction *action)
{
	g_free(action->str_value);
	g_free(action);
}

void filter_info_free(FilterInfo *fltinfo)
{
	g_slist_free(fltinfo->dest_list);
	g_free(fltinfo);
}
