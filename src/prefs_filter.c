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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "intl.h"
#include "main.h"
#include "prefs.h"
#include "prefs_filter.h"
#include "prefs_filter_edit.h"
#include "prefs_common.h"
#include "mainwindow.h"
#include "foldersel.h"
#include "manage_window.h"
#include "stock_pixmap.h"
#include "inc.h"
#include "procheader.h"
#include "menu.h"
#include "filter.h"
#include "utils.h"
#include "gtkutils.h"
#include "alertpanel.h"
#include "xml.h"

static struct FilterRuleListWindow {
	GtkWidget *window;

	GtkWidget *clist;

	GtkWidget *add_btn;
	GtkWidget *edit_btn;
	GtkWidget *copy_btn;
	GtkWidget *del_btn;

	GSList *default_hdr_list;
	GSList *user_hdr_list;
	GSList *msg_hdr_list;

	GHashTable *msg_hdr_table;

	GtkWidget *close_btn;
} rule_list_window;

static GdkPixmap *markxpm;
static GdkBitmap *markxpmmask;

static void prefs_filter_create			(void);

static void prefs_filter_set_dialog		(void);
static void prefs_filter_set_list_row		(gint		 row,
						 FilterRule	*rule,
						 gboolean	 move_view);

static void prefs_filter_set_header_list	(MsgInfo	*msginfo);

static void prefs_filter_write_user_header_list	(void);

static void prefs_filter_set_list		(void);

/* callback functions */
static void prefs_filter_add_cb		(void);
static void prefs_filter_edit_cb	(void);
static void prefs_filter_copy_cb	(void);
static void prefs_filter_delete_cb	(void);
static void prefs_filter_top		(void);
static void prefs_filter_up		(void);
static void prefs_filter_down		(void);
static void prefs_filter_bottom		(void);

static void prefs_filter_select		(GtkCList	*clist,
					 gint		 row,
					 gint		 column,
					 GdkEvent	*event);
static void prefs_filter_row_move	(GtkCList	*clist,
					 gint		 source_row,
					 gint		 dest_row);

static gint prefs_filter_deleted	(GtkWidget	*widget,
					 GdkEventAny	*event,
					 gpointer	 data);
static gboolean prefs_filter_key_pressed(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gpointer	 data);
static void prefs_filter_close		(void);


void prefs_filter_open(MsgInfo *msginfo, const gchar *header)
{
	inc_lock();

	if (!rule_list_window.window)
		prefs_filter_create();

	prefs_filter_set_header_list(msginfo);

	manage_window_set_transient(GTK_WINDOW(rule_list_window.window));
	gtk_widget_grab_focus(rule_list_window.close_btn);

	prefs_filter_set_dialog();

	gtk_widget_show(rule_list_window.window);

	if (msginfo) {
		FilterRule *rule;

		rule = prefs_filter_edit_open(NULL, header);

		if (rule) {
			prefs_filter_set_list_row(-1, rule, TRUE);
			prefs_filter_set_list();
		}
	}
}

static void prefs_filter_create(void)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *close_btn;
	GtkWidget *confirm_area;

	GtkWidget *hbox;
	GtkWidget *scrolledwin;
	GtkWidget *clist;

	GtkWidget *btn_vbox;
	GtkWidget *spc_vbox;
	GtkWidget *top_btn;
	GtkWidget *up_btn;
	GtkWidget *down_btn;
	GtkWidget *bottom_btn;

	GtkWidget *btn_hbox;
	GtkWidget *add_btn;
	GtkWidget *edit_btn;
	GtkWidget *copy_btn;
	GtkWidget *del_btn;

	gchar *title[2];

	debug_print("Creating filter setting window...\n");

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_widget_set_size_request(window, 540, 360);
	gtk_window_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, FALSE);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	gtkut_button_set_create(&confirm_area, &close_btn, _("Close"),
				NULL, NULL, NULL, NULL);
	gtk_widget_show(confirm_area);
	gtk_box_pack_end(GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default(close_btn);

	gtk_window_set_title(GTK_WINDOW(window),
			     _("Filter setting"));
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(prefs_filter_deleted), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(prefs_filter_key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT (window);
	g_signal_connect(G_OBJECT(close_btn), "clicked",
			 G_CALLBACK(prefs_filter_close), NULL);

	/* Rule list */

	hbox = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scrolledwin);
	gtk_widget_set_size_request(scrolledwin, -1, 150);
	gtk_box_pack_start(GTK_BOX(hbox), scrolledwin, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);

	title[0] = _("Enabled");
	title[1] = _("Name");
	clist = gtk_clist_new_with_titles(2, title);
	gtk_widget_show(clist);
	gtk_container_add (GTK_CONTAINER(scrolledwin), clist);
	gtk_clist_set_column_width(GTK_CLIST(clist), 0, 64);
	gtk_clist_set_column_justification(GTK_CLIST(clist), 0,
					   GTK_JUSTIFY_CENTER);
	gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_BROWSE);
	GTK_WIDGET_UNSET_FLAGS(GTK_CLIST(clist)->column[0].button,
			       GTK_CAN_FOCUS);
	GTK_WIDGET_UNSET_FLAGS(GTK_CLIST(clist)->column[1].button,
			       GTK_CAN_FOCUS);
	g_signal_connect(G_OBJECT(clist), "select_row",
			 G_CALLBACK(prefs_filter_select), NULL);
	g_signal_connect_after(G_OBJECT(clist), "row_move",
			       G_CALLBACK(prefs_filter_row_move), NULL);

	/* Up / Down */

	btn_vbox = gtk_vbox_new (FALSE, 8);
	gtk_widget_show(btn_vbox);
	gtk_box_pack_start(GTK_BOX(hbox), btn_vbox, FALSE, FALSE, 0);

	top_btn = gtk_button_new_with_label(_("Top"));
	gtk_widget_show(top_btn);
	gtk_box_pack_start(GTK_BOX(btn_vbox), top_btn, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(top_btn), "clicked",
			 G_CALLBACK(prefs_filter_top), NULL);

	PACK_VSPACER(btn_vbox, spc_vbox, VSPACING_NARROW_2);

	up_btn = gtk_button_new_with_label(_("Up"));
	gtk_widget_show(up_btn);
	gtk_box_pack_start(GTK_BOX(btn_vbox), up_btn, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(up_btn), "clicked",
			 G_CALLBACK(prefs_filter_up), NULL);

	down_btn = gtk_button_new_with_label(_("Down"));
	gtk_widget_show(down_btn);
	gtk_box_pack_start(GTK_BOX(btn_vbox), down_btn, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(down_btn), "clicked",
			 G_CALLBACK(prefs_filter_down), NULL);

	PACK_VSPACER(btn_vbox, spc_vbox, VSPACING_NARROW_2);

	bottom_btn = gtk_button_new_with_label(_("Bottom"));
	gtk_widget_show(bottom_btn);
	gtk_box_pack_start(GTK_BOX(btn_vbox), bottom_btn, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(bottom_btn), "clicked",
			 G_CALLBACK(prefs_filter_bottom), NULL);

	/* add / edit / copy / delete */

	hbox = gtk_hbox_new(FALSE, 4);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	btn_hbox = gtk_hbox_new(TRUE, 4);
	gtk_widget_show(btn_hbox);
	gtk_box_pack_start(GTK_BOX(hbox), btn_hbox, FALSE, FALSE, 0);

	add_btn = gtk_button_new_with_label(_("Add"));
	gtk_widget_show(add_btn);
	gtk_box_pack_start(GTK_BOX(btn_hbox), add_btn, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT(add_btn), "clicked",
			 G_CALLBACK(prefs_filter_add_cb), NULL);

	edit_btn = gtk_button_new_with_label(_("Edit"));
	gtk_widget_show(edit_btn);
	gtk_box_pack_start(GTK_BOX(btn_hbox), edit_btn, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT(edit_btn), "clicked",
			 G_CALLBACK(prefs_filter_edit_cb), NULL);

	copy_btn = gtk_button_new_with_label(_("Copy"));
	gtk_widget_show(copy_btn);
	gtk_box_pack_start(GTK_BOX(btn_hbox), copy_btn, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT(copy_btn), "clicked",
			 G_CALLBACK(prefs_filter_copy_cb), NULL);

	del_btn = gtk_button_new_with_label(_(" Delete "));
	gtk_widget_show(del_btn);
	gtk_box_pack_start(GTK_BOX(btn_hbox), del_btn, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT(del_btn), "clicked",
			 G_CALLBACK(prefs_filter_delete_cb), NULL);

	gtk_widget_show_all(window);

	stock_pixmap_gdk(clist, STOCK_PIXMAP_MARK, &markxpm, &markxpmmask);

	rule_list_window.window = window;
	rule_list_window.close_btn = close_btn;

	rule_list_window.clist = clist;

	rule_list_window.default_hdr_list  = NULL;
	rule_list_window.user_hdr_list  = NULL;
	rule_list_window.msg_hdr_list  = NULL;
	rule_list_window.msg_hdr_table = NULL;
}

void prefs_filter_read_config(void)
{
	gchar *rcpath;
	GNode *node;
	FilterRule *rule;

	debug_print("Reading filter configuration...\n");

	/* remove all previous filter list */
	while (prefs_common.fltlist != NULL) {
		rule = (FilterRule *)prefs_common.fltlist->data;
		filter_rule_free(rule);
		prefs_common.fltlist = g_slist_remove(prefs_common.fltlist,
						      rule);
	}

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, FILTER_LIST,
			     NULL);
	if (!is_file_exist(rcpath)) {
		g_free(rcpath);
		return;
	}

	node = xml_parse_file(rcpath);
	if (!node) {
		g_warning("Can't parse %s\n", rcpath);
		g_free(rcpath);
		return;
	}
	g_free(rcpath);

	prefs_common.fltlist = filter_xml_node_to_filter_list(node);

	xml_free_tree(node);
}

void prefs_filter_write_config(void)
{
	filter_write_config(prefs_common.fltlist);
}

void prefs_filter_rename_path(const gchar *old_path, const gchar *new_path)
{
	GSList *cur;

	g_return_if_fail(old_path != NULL);
	g_return_if_fail(new_path != NULL);

	for (cur = prefs_common.fltlist; cur != NULL; cur = cur->next) {
		FilterRule *rule = (FilterRule *)cur->data;
		filter_rule_rename_dest_path(rule, old_path, new_path);
	}

	filter_write_config(prefs_common.fltlist);
}

void prefs_filter_delete_path(const gchar *path)
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

	filter_write_config(prefs_common.fltlist);
}

static void prefs_filter_set_dialog(void)
{
	GtkCList *clist = GTK_CLIST(rule_list_window.clist);
	GSList *cur;

	gtk_clist_freeze(clist);
	gtk_clist_clear(clist);

	for (cur = prefs_common.fltlist; cur != NULL; cur = cur->next) {
		FilterRule *rule = (FilterRule *)cur->data;
		prefs_filter_set_list_row(-1, rule, FALSE);
	}

	gtk_clist_thaw(clist);
}

static void prefs_filter_set_list_row(gint row, FilterRule *rule,
				      gboolean move_view)
{
	GtkCList *clist = GTK_CLIST(rule_list_window.clist);
	gchar *cond_str[2] = {"", NULL};

	if (!rule)
		rule = gtk_clist_get_row_data(clist, row);

	g_return_if_fail(rule != NULL);

	if (rule->name && *rule->name)
		cond_str[1] = g_strdup(rule->name);
	else {
		cond_str[1] = filter_get_str(rule);
	}

	if (row < 0)
		row = gtk_clist_append(clist, cond_str);
	else {
		FilterRule *prev_rule;

		prev_rule = gtk_clist_get_row_data(clist, row);
		if (rule == prev_rule)
			gtk_clist_set_text(clist, row, 1, cond_str[1]);
		else if (prev_rule) {
			gtk_clist_set_text(clist, row, 1, cond_str[1]);
			filter_rule_free(prev_rule);
		} else
			row = gtk_clist_append(clist, cond_str);
	}

	if (rule->enabled)
		gtk_clist_set_pixmap(clist, row, 0, markxpm, markxpmmask);
	else
		gtk_clist_set_text(clist, row, 0, "");

	gtk_clist_set_row_data(clist, row, rule);
	g_free(cond_str[1]);

	if (move_view &&
	    gtk_clist_row_is_visible(clist, row) != GTK_VISIBILITY_FULL)
		gtk_clist_moveto(clist, row, -1, 0.5, 0.0);
}

#define APPEND_HDR_LIST(hdr_list)					  \
	for (cur = hdr_list; cur != NULL; cur = cur->next) {		  \
		header = (Header *)cur->data;				  \
									  \
		if (!g_hash_table_lookup(table, header->name)) {	  \
			g_hash_table_insert(table, header->name, header); \
			list = g_slist_append(list, header);		  \
		}							  \
	}

GSList *prefs_filter_get_header_list(void)
{
	GSList *list = NULL;
	GSList *cur;
	GHashTable *table;
	Header *header;

	table = g_hash_table_new(str_case_hash, str_case_equal);

	APPEND_HDR_LIST(rule_list_window.default_hdr_list)
	APPEND_HDR_LIST(rule_list_window.user_hdr_list);
	APPEND_HDR_LIST(rule_list_window.msg_hdr_list);

	g_hash_table_destroy(table);

	return list;
}

#undef APPEND_HDR_LIST

GSList *prefs_filter_get_user_header_list(void)
{
	return rule_list_window.user_hdr_list;
}

gchar *prefs_filter_get_msg_header_field(const gchar *header_name)
{
	if (!rule_list_window.msg_hdr_table)
		return NULL;

	return (gchar *)g_hash_table_lookup
		(rule_list_window.msg_hdr_table, header_name);
}

void prefs_filter_set_user_header_list(GSList *list)
{
	procheader_header_list_destroy(rule_list_window.user_hdr_list);
	rule_list_window.user_hdr_list = list;
}

void prefs_filter_set_msg_header_list(MsgInfo *msginfo)
{
	gchar *file;
	GSList *cur;
	GSList *next;
	Header *header;

	if (rule_list_window.msg_hdr_table) {
		g_hash_table_destroy(rule_list_window.msg_hdr_table);
		rule_list_window.msg_hdr_table = NULL;
	}
	if (rule_list_window.msg_hdr_list) {
		procheader_header_list_destroy(rule_list_window.msg_hdr_list);
		rule_list_window.msg_hdr_list = NULL;
	}

	if (!msginfo)
		return;

	file = procmsg_get_message_file(msginfo);
	g_return_if_fail(file != NULL);

	rule_list_window.msg_hdr_list =
		procheader_get_header_list_from_file(file);

	g_free(file);

	rule_list_window.msg_hdr_table =
		g_hash_table_new(str_case_hash, str_case_equal);

	for (cur = rule_list_window.msg_hdr_list; cur != NULL;
	     cur = next) {
		next = cur->next;
		header = (Header *)cur->data;
		if (!g_strcasecmp(header->name, "Received") ||
		    !g_strcasecmp(header->name, "Mime-Version") ||
		    !g_strcasecmp(header->name, "X-UIDL")) {
			procheader_header_free(header);
			rule_list_window.msg_hdr_list =
				g_slist_remove(rule_list_window.msg_hdr_list,
					       header);
			continue;
		}
		if (!g_hash_table_lookup(rule_list_window.msg_hdr_table,
					 header->name)) {
			g_hash_table_insert(rule_list_window.msg_hdr_table,
					    header->name, header->body);
		}
	}
}

static void prefs_filter_set_header_list(MsgInfo *msginfo)
{
	GSList *list = NULL;
	gchar *path;
	FILE *fp;

	list = procheader_add_header_list(list, "From", NULL);
	list = procheader_add_header_list(list, "To", NULL);
	list = procheader_add_header_list(list, "Cc", NULL);
	list = procheader_add_header_list(list, "Subject", NULL);
	list = procheader_add_header_list(list, "Reply-To", NULL);
	list = procheader_add_header_list(list, "List-Id", NULL);
	list = procheader_add_header_list(list, "X-ML-Name", NULL);

	procheader_header_list_destroy(rule_list_window.default_hdr_list);
	rule_list_window.default_hdr_list = list;

	list = NULL;
	path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, FILTER_HEADER_RC,
			   NULL);
	if ((fp = fopen(path, "rb")) != NULL) {
		gchar buf[PREFSBUFSIZE];

		while (fgets(buf, sizeof(buf), fp) != NULL) {
			g_strstrip(buf);
			if (buf[0] == '\0') continue;
			list = procheader_add_header_list(list, buf, NULL);
		}

		fclose(fp);
	} else
		if (ENOENT != errno) FILE_OP_ERROR(path, "fopen");
	g_free(path);

	prefs_filter_set_user_header_list(list);

	prefs_filter_set_msg_header_list(msginfo);
}

static void prefs_filter_write_user_header_list(void)
{
	gchar *path;
	PrefFile *pfile;
	GSList *cur;

	path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, FILTER_HEADER_RC,
			   NULL);

	if ((pfile = prefs_file_open(path)) == NULL) {
		g_warning("failed to write filter user header list\n");
		g_free(path);
		return;
	}
	g_free(path);

	for (cur = rule_list_window.user_hdr_list; cur != NULL;
	     cur = cur->next) {
		Header *header = (Header *)cur->data;
		fputs(header->name, pfile->fp);
		fputc('\n', pfile->fp);
	}

	if (prefs_file_close(pfile) < 0)
		g_warning("failed to write filter user header list\n");
}

static void prefs_filter_set_list(void)
{
	gint row = 0;
	FilterRule *rule;

	g_slist_free(prefs_common.fltlist);
	prefs_common.fltlist = NULL;

	while ((rule = gtk_clist_get_row_data
		(GTK_CLIST(rule_list_window.clist), row)) != NULL) {
		prefs_common.fltlist = g_slist_append(prefs_common.fltlist,
						      rule);
		row++;
	}
}

static void prefs_filter_add_cb(void)
{
	FilterRule *rule;

	rule = prefs_filter_edit_open(NULL, NULL);

	if (rule) {
		prefs_filter_set_list_row(-1, rule, TRUE);
		prefs_filter_set_list();
	}
}

static void prefs_filter_edit_cb(void)
{
	GtkCList *clist = GTK_CLIST(rule_list_window.clist);
	FilterRule *rule, *new_rule;
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);

	rule = gtk_clist_get_row_data(clist, row);
	g_return_if_fail(rule != NULL);

	new_rule = prefs_filter_edit_open(rule, NULL);

	if (new_rule) {
		prefs_filter_set_list_row(row, new_rule, TRUE);
		prefs_filter_set_list();
	}
}

static void prefs_filter_copy_cb(void)
{
	GtkCList *clist = GTK_CLIST(rule_list_window.clist);
	FilterRule *rule, *new_rule;
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);

	rule = gtk_clist_get_row_data(clist, row);
	g_return_if_fail(rule != NULL);

	new_rule = prefs_filter_edit_open(rule, NULL);

	if (new_rule) {
		prefs_filter_set_list_row(-1, new_rule, TRUE);
		prefs_filter_set_list();
	}
}

static void prefs_filter_delete_cb(void)
{
	GtkCList *clist = GTK_CLIST(rule_list_window.clist);
	FilterRule *rule;
	gint row;

	if (!clist->selection) return;
	row = GPOINTER_TO_INT(clist->selection->data);

	if (alertpanel(_("Delete rule"),
		       _("Do you really want to delete this rule?"),
		       _("Yes"), _("No"), NULL) != G_ALERTDEFAULT)
		return;

	rule = gtk_clist_get_row_data(clist, row);
	filter_rule_free(rule);
	gtk_clist_remove(clist, row);
	prefs_common.fltlist = g_slist_remove(prefs_common.fltlist, rule);
	if (!clist->selection)
		gtk_clist_select_row(clist, row - 1, -1);
}

static void prefs_filter_top(void)
{
	GtkCList *clist = GTK_CLIST(rule_list_window.clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row > 0)
		gtk_clist_row_move(clist, row, 0);
}

static void prefs_filter_up(void)
{
	GtkCList *clist = GTK_CLIST(rule_list_window.clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row > 0)
		gtk_clist_row_move(clist, row, row - 1);
}

static void prefs_filter_down(void)
{
	GtkCList *clist = GTK_CLIST(rule_list_window.clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row < clist->rows - 1)
		gtk_clist_row_move(clist, row, row + 1);
}

static void prefs_filter_bottom(void)
{
	GtkCList *clist = GTK_CLIST(rule_list_window.clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row < clist->rows - 1)
		gtk_clist_row_move(clist, row, clist->rows - 1);
}

static void prefs_filter_select(GtkCList *clist, gint row, gint column,
				GdkEvent *event)
{
	if (event && event->type == GDK_2BUTTON_PRESS) {
		prefs_filter_edit_cb();
		return;
	}

	if (column == 0) {
		FilterRule *rule;
		rule = gtk_clist_get_row_data(clist, row);
		rule->enabled ^= TRUE;
		prefs_filter_set_list_row(row, rule, FALSE);
	}
}

static void prefs_filter_row_move(GtkCList *clist, gint source_row,
				  gint dest_row)
{
	prefs_filter_set_list();
	if (gtk_clist_row_is_visible(clist, dest_row) != GTK_VISIBILITY_FULL)
		gtk_clist_moveto(clist, dest_row, -1, 0.5, 0.0);
}

static gint prefs_filter_deleted(GtkWidget *widget, GdkEventAny *event,
				 gpointer data)
{
	prefs_filter_close();
	return TRUE;
}

static gboolean prefs_filter_key_pressed(GtkWidget *widget, GdkEventKey *event,
					 gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		prefs_filter_close();
	return FALSE;
}

static void prefs_filter_close(void)
{
	prefs_filter_set_msg_header_list(NULL);
	prefs_filter_write_user_header_list();
	filter_write_config(prefs_common.fltlist);
	gtk_widget_hide(rule_list_window.window);
	gtk_clist_clear(GTK_CLIST(rule_list_window.clist));
	inc_unlock();
}
