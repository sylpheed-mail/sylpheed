/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2004 Hiroyuki Yamamoto
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
#include <regex.h>
#include <time.h>

#include "procheader.h"
#include "filter.h"
#include "folder.h"
#include "utils.h"
#include "xml.h"
#include "prefs.h"
#include "prefs_account.h"

typedef enum
{
	FLT_O_CONTAIN	= 1 << 0,
	FLT_O_CASE_SENS	= 1 << 1,
	FLT_O_REGEX	= 1 << 2
} FilterOldFlag;

static gboolean filter_match_cond	(FilterCond	*cond,
					 MsgInfo	*msginfo,
					 GSList		*hlist,
					 FilterInfo	*fltinfo);
static gboolean filter_match_header_cond(FilterCond	*cond,
					 GSList		*hlist);

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

	if (!fltlist) return 0;

	file = procmsg_get_message_file(msginfo);
	hlist = procheader_get_header_list_from_file(file);
	if (!hlist) {
		g_free(file);
		return 0;
	}

	for (cur = fltlist; cur != NULL; cur = cur->next) {
		rule = (FilterRule *)cur->data;
		if (!rule->enabled) continue;
		if (filter_match_rule(rule, msginfo, hlist, fltinfo)) {
			ret = filter_action_exec(rule, msginfo, file, fltinfo);
			if (ret < 0) {
				g_warning("filter_action_exec() returned error\n");
				break;
			}
			if (fltinfo->drop_done == TRUE ||
			    fltinfo->actions[FLT_ACTION_STOP_EVAL] == TRUE)
				break;
		}
	}

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
			cmdline = g_strconcat(action->str_value, " ", file,
					      NULL);
			execute_command_line(cmdline, FALSE);
			g_free(cmdline);
			fltinfo->actions[action->type] = TRUE;
			break;
		case FLT_ACTION_EXEC_ASYNC:
			cmdline = g_strconcat(action->str_value, " ", file,
					      NULL);
			execute_command_line(cmdline, TRUE);
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
						val = folder_item_copy_msg
							(dest_folder, msginfo);
						if (val == -1)
							return -1;
					}
					fltinfo->actions[action->type] = TRUE;
				}
			} else {
				if (folder_item_add_msg(dest_folder, file,
							&fltinfo->flags,
							FALSE) < 0)
					return -1;
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
	gint ret = 0;
	regex_t preg;
	regmatch_t pmatch[1];

	ret = regcomp(&preg, needle, REG_EXTENDED|REG_ICASE);
	if (ret != 0) return FALSE;

	ret = regexec(&preg, haystack, 1, pmatch, 0);
	regfree(&preg);

	if (ret == REG_NOMATCH) return FALSE;

	if (pmatch[0].rm_so != -1)
		return TRUE;
	else
		return FALSE;
}

gboolean filter_match_rule(FilterRule *rule, MsgInfo *msginfo, GSList *hlist,
			   FilterInfo *fltinfo)
{
	FilterCond *cond;
	GSList *cur;
	gboolean matched;

	g_return_val_if_fail(rule->cond_list != NULL, FALSE);
	g_return_val_if_fail(rule->action_list != NULL, FALSE);

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
			matched = filter_match_cond(cond, msginfo, hlist,
						    fltinfo);
			if (matched == FALSE)
				return FALSE;
		}

		return TRUE;
	} else if (rule->bool_op == FLT_OR) {
		for (cur = rule->cond_list; cur != NULL; cur = cur->next) {
			cond = (FilterCond *)cur->data;
			matched = filter_match_cond(cond, msginfo, hlist,
						    fltinfo);
			if (matched == TRUE)
				return TRUE;
		}

		return FALSE;
	}

	return FALSE;
}

static gboolean filter_match_cond(FilterCond *cond, MsgInfo *msginfo,
				  GSList *hlist, FilterInfo *fltinfo)
{
	gboolean matched = FALSE;
	gchar *file;
	gchar *cmdline;
	PrefsAccount *cond_ac;

	switch (cond->type) {
	case FLT_COND_HEADER:
	case FLT_COND_ANY_HEADER:
	case FLT_COND_TO_OR_CC:
		return filter_match_header_cond(cond, hlist);
	case FLT_COND_BODY:
		matched = procmime_find_string(msginfo, cond->str_value,
					       cond->match_func);
		break;
	case FLT_COND_CMD_TEST:
		file = procmsg_get_message_file(msginfo);
		cmdline = g_strconcat(cond->str_value, " ", file, NULL);
		matched = (execute_command_line(cmdline, FALSE) == 0);
		g_free(cmdline);
		g_free(file);
		break;
	case FLT_COND_SIZE_GREATER:
		matched = (msginfo->size > cond->int_value * 1024);
		break;
	case FLT_COND_AGE_GREATER:
		matched = (time(NULL) - msginfo->date_t >
				cond->int_value * 24 * 60 * 60);
		break;
	case FLT_COND_ACCOUNT:
		cond_ac = account_find_from_id(cond->int_value);
		matched = (cond_ac != NULL && cond_ac == fltinfo->account);
		break;
	default:
		g_warning("filter_match_cond(): unknown condition: %d\n",
			  cond->type);
		return FALSE;
	}

	if (FLT_IS_NOT_MATCH(cond->match_flag))
		matched = !matched;

	return matched;
}

static gboolean filter_match_header_cond(FilterCond *cond, GSList *hlist)
{
	gboolean matched = FALSE;
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

	if (FLT_IS_NOT_MATCH(cond->match_flag))
		matched = !matched;

	return matched;
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
		FilterCond *cond;
		FilterCondType cond_type = FLT_COND_HEADER;
		FilterMatchType match_type = FLT_CONTAIN;
		FilterMatchFlag match_flag = 0;

		xmlnode = cond_child->data;
		if (!xmlnode || !xmlnode->tag || !xmlnode->tag->tag) continue;

		for (list = xmlnode->tag->attr; list != NULL; list = list->next) {
			XMLAttr *attr = (XMLAttr *)list->data;

			if (!attr || !attr->name || !attr->value) continue;
			if (!strcmp(attr->name, "type"))
				type = attr->value;
			else if (!strcmp(attr->name, "name"))
				name = attr->value;
		}

		if (type) {
			filter_rule_match_type_str_to_enum
				(type, &match_type, &match_flag);
		}
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
		STR_CASE("account-id")
			cond_type = FLT_COND_ACCOUNT;
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

	if (name && cond_list && action_list) {
		rule = filter_rule_new(name, bool_op, cond_list, action_list);
		rule->timing = timing;
		rule->enabled = enabled;
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

#define NODE_NEW(tag, text) \
	node = xml_node_new(xml_tag_new(tag), text)
#define ADD_ATTR(name, value) \
	xml_tag_add_attr(node->tag, xml_attr_new(name, value))

void filter_write_config(GSList *fltlist)
{
	gchar *rcpath;
	PrefFile *pfile;
	GSList *cur;

	debug_print("Writing filter configuration...\n");

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, FILTER_LIST,
			     NULL);
	if ((pfile = prefs_file_open(rcpath)) == NULL) {
		g_warning("failed to write filter configuration to file\n");
		g_free(rcpath);
		return;
	}

	xml_file_put_xml_decl(pfile->fp);
	fputs("\n<filter>\n", pfile->fp);

	for (cur = fltlist; cur != NULL; cur = cur->next) {
		FilterRule *rule = (FilterRule *)cur->data;
		GSList *cur_cond;
		GSList *cur_action;
		gchar match_type[16];

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
				strcpy(match_type,
				       FLT_IS_NOT_MATCH(cond->match_flag)
				       ? "not-contain" : "contains");
				break;
			case FLT_EQUAL:
				strcpy(match_type,
				       FLT_IS_NOT_MATCH(cond->match_flag)
				       ? "is-not" : "is");
				break;
			case FLT_REGEX:
				strcpy(match_type,
				       FLT_IS_NOT_MATCH(cond->match_flag)
				       ? "not-regex" : "regex");
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
				break;
			case FLT_COND_ANY_HEADER:
				NODE_NEW("match-any-header", cond->str_value);
				ADD_ATTR("type", match_type);
				break;
			case FLT_COND_TO_OR_CC:
				NODE_NEW("match-to-or-cc", cond->str_value);
				ADD_ATTR("type", match_type);
				break;
			case FLT_COND_BODY:
				NODE_NEW("match-body-text", cond->str_value);
				ADD_ATTR("type", match_type);
				break;
			case FLT_COND_CMD_TEST:
				NODE_NEW("command-test", cond->str_value);
				break;
			case FLT_COND_SIZE_GREATER:
				NODE_NEW("size", itos(cond->int_value));
				ADD_ATTR("type",
					 FLT_IS_NOT_MATCH(cond->match_flag)
					 ? "lt" : "gt");
				break;
			case FLT_COND_AGE_GREATER:
				NODE_NEW("age", itos(cond->int_value));
				ADD_ATTR("type",
					 FLT_IS_NOT_MATCH(cond->match_flag)
					 ? "lt" : "gt");
				break;
			case FLT_COND_ACCOUNT:
				 NODE_NEW("account-id", itos(cond->int_value));
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

	g_free(rcpath);

	if (prefs_file_close(pfile) < 0) {
		g_warning(_("failed to write configuration to file\n"));
		return;
	}
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
	if (type == FLT_COND_SIZE_GREATER || type == FLT_COND_AGE_GREATER)
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

	return fltinfo;
}

void filter_rule_rename_dest_path(FilterRule *rule, const gchar *old_path,
				  const gchar *new_path)
{
	FilterAction *action;
	GSList *cur;
	gchar *base;
	gchar *dest_path;
	gint oldpathlen;

	for (cur = rule->action_list; cur != NULL; cur = cur->next) {
		action = (FilterAction *)cur->data;

		if (action->type != FLT_ACTION_MOVE &&
		    action->type != FLT_ACTION_COPY)
			continue;

		oldpathlen = strlen(old_path);
		if (action->str_value &&
		    !strncmp(old_path, action->str_value, oldpathlen)) {
			base = action->str_value + oldpathlen;
			while (*base == G_DIR_SEPARATOR) base++;
			if (*base == '\0')
				dest_path = g_strdup(new_path);
			else
				dest_path = g_strconcat(new_path,
							G_DIR_SEPARATOR_S,
							base, NULL);
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

	for (cur = rule->action_list; cur != NULL; cur = next) {
		action = (FilterAction *)cur->data;
		next = cur->next;

		if (action->type != FLT_ACTION_MOVE &&
		    action->type != FLT_ACTION_COPY)
			continue;

		if (action->str_value &&
		    !strncmp(path, action->str_value, strlen(path))) {
			rule->action_list = g_slist_remove
				(rule->action_list, action);
			filter_action_free(action);
		}
	}
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
	} else if (!strcmp(match_type, "gt")) {
	} else if (!strcmp(match_type, "lt")) {
		*flag = FLT_NOT_MATCH;
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
