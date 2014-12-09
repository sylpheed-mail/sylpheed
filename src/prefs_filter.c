/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2014 Hiroyuki Yamamoto
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
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "main.h"
#include "prefs.h"
#include "prefs_ui.h"
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
#include "plugin.h"

static struct FilterRuleListWindow {
	GtkWidget *window;

	GtkWidget *treeview;
	GtkListStore *store;
	GtkTreeSelection *selection;

	GtkWidget *add_btn;
	GtkWidget *edit_btn;
	GtkWidget *copy_btn;
	GtkWidget *del_btn;

	GSList *default_hdr_list;
	GSList *user_hdr_list;
	GSList *msg_hdr_list;

	GHashTable *msg_hdr_table;

	GtkWidget *close_btn;

	gboolean on_init;
	gboolean modified;
} rule_list_window;

enum {
	COL_ENABLED,
	COL_NAME,
	COL_FILTER_RULE,
	N_COLS
};

static void prefs_filter_create			(void);

static void prefs_filter_set_dialog		(void);
static void prefs_filter_set_list_row		(GtkTreeIter	*iter,
						 FilterRule	*rule,
						 gboolean	 move_view);

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

static gboolean prefs_filter_select	(GtkTreeSelection	*selection,
					 GtkTreeModel		*model,
					 GtkTreePath		*path,
					 gboolean		 cur_selected,
					 gpointer		 data);
static void prefs_filter_enable_toggled	(GtkCellRenderer	*cell,
					 gchar			*path,
					 gpointer		 data);

static void prefs_filter_row_activated	(GtkTreeView		*treeview,
					 GtkTreePath		*path,
					 GtkTreeViewColumn	*column,
					 gpointer		 data);
static void prefs_filter_row_reordered	(GtkTreeModel		*model,
					 GtkTreePath		*path,
					 GtkTreeIter		*iter,
					 gpointer		 data,
					 gpointer		 user_data);
static void prefs_filter_row_inserted	(GtkTreeModel		*model,
					 GtkTreePath		*path,
					 GtkTreeIter		*iter,
					 gpointer		 user_data);

static gint prefs_filter_deleted	(GtkWidget	*widget,
					 GdkEventAny	*event,
					 gpointer	 data);
static gboolean prefs_filter_key_pressed(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gpointer	 data);
static void prefs_filter_close		(void);


void prefs_filter_open(MsgInfo *msginfo, const gchar *header, const gchar *key)
{
	inc_lock();

	rule_list_window.on_init = TRUE;

	if (!rule_list_window.window)
		prefs_filter_create();

	prefs_filter_set_header_list(msginfo);

	manage_window_set_transient(GTK_WINDOW(rule_list_window.window));
	gtk_widget_grab_focus(rule_list_window.close_btn);

	prefs_filter_set_dialog();

	gtk_widget_show(rule_list_window.window);
	manage_window_focus_in(rule_list_window.window, NULL, NULL);

	rule_list_window.modified = FALSE;

	syl_plugin_signal_emit("prefs-filter-open", rule_list_window.window);

	if (msginfo) {
		FilterRule *rule;

		rule = prefs_filter_edit_open(NULL, header, key);
		gtk_window_present(GTK_WINDOW(rule_list_window.window));

		if (rule) {
			prefs_filter_set_list_row(NULL, rule, TRUE);
			prefs_filter_set_list();
		}
	}

	rule_list_window.on_init = FALSE;
}

static void prefs_filter_create(void)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *close_btn;
	GtkWidget *confirm_area;

	GtkWidget *hbox;
	GtkWidget *scrolledwin;
	GtkWidget *treeview;
	GtkListStore *store;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

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
	GtkWidget *image;

	debug_print("Creating filter setting window...\n");

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_widget_set_size_request(window, 540 * gtkut_get_dpi_multiplier(), 360 * gtkut_get_dpi_multiplier());
	gtk_window_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, FALSE);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	gtkut_stock_button_set_create(&confirm_area,
				      &close_btn, GTK_STOCK_CLOSE,
				      NULL, NULL, NULL, NULL);
	gtk_widget_show(confirm_area);
	gtk_box_pack_end(GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default(close_btn);

	gtk_window_set_title(GTK_WINDOW(window),
			     _("Filter settings"));
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
	gtk_widget_set_size_request(scrolledwin, -1, 150 * gtkut_get_dpi_multiplier());
	gtk_box_pack_start(GTK_BOX(hbox), scrolledwin, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_SHADOW_IN);

	store = gtk_list_store_new
		(N_COLS, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_POINTER);

	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(G_OBJECT(store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), TRUE);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(treeview), COL_NAME);
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(treeview), TRUE);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);
	gtk_tree_selection_set_select_function(selection, prefs_filter_select,
					       NULL, NULL);

	renderer = gtk_cell_renderer_toggle_new();
	g_signal_connect(renderer, "toggled",
			 G_CALLBACK(prefs_filter_enable_toggled), NULL);
	column = gtk_tree_view_column_new_with_attributes
		(_("Enabled"), renderer, "active", COL_ENABLED, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes
		(_("Name"), renderer, "text", COL_NAME, NULL);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	gtk_widget_show(treeview);
	gtk_container_add(GTK_CONTAINER(scrolledwin), treeview);

	g_signal_connect(G_OBJECT(treeview), "row-activated",
			 G_CALLBACK(prefs_filter_row_activated), NULL);
	g_signal_connect_after(G_OBJECT(store), "rows-reordered",
			       G_CALLBACK(prefs_filter_row_reordered), NULL);
	g_signal_connect_after(G_OBJECT(store), "row-inserted",
			       G_CALLBACK(prefs_filter_row_inserted), NULL);

	/* Up / Down */

	btn_vbox = gtk_vbox_new (FALSE, 8);
	gtk_widget_show(btn_vbox);
	gtk_box_pack_start(GTK_BOX(hbox), btn_vbox, FALSE, FALSE, 0);

	top_btn = gtk_button_new_from_stock(GTK_STOCK_GOTO_TOP);
	gtk_widget_show(top_btn);
	gtk_box_pack_start(GTK_BOX(btn_vbox), top_btn, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(top_btn), "clicked",
			 G_CALLBACK(prefs_filter_top), NULL);

	PACK_VSPACER(btn_vbox, spc_vbox, VSPACING_NARROW_2);

	up_btn = gtk_button_new();
	image = gtk_image_new_from_stock(GTK_STOCK_GO_UP, GTK_ICON_SIZE_BUTTON);
	gtk_widget_show(image);
	gtk_button_set_image(GTK_BUTTON(up_btn), image);
	gtk_widget_show(up_btn);
	gtk_box_pack_start(GTK_BOX(btn_vbox), up_btn, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(up_btn), "clicked",
			 G_CALLBACK(prefs_filter_up), NULL);

	down_btn = gtk_button_new();
	image = gtk_image_new_from_stock(GTK_STOCK_GO_DOWN, GTK_ICON_SIZE_BUTTON);
	gtk_widget_show(image);
	gtk_button_set_image(GTK_BUTTON(down_btn), image);
	gtk_widget_show(down_btn);
	gtk_box_pack_start(GTK_BOX(btn_vbox), down_btn, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(down_btn), "clicked",
			 G_CALLBACK(prefs_filter_down), NULL);

	PACK_VSPACER(btn_vbox, spc_vbox, VSPACING_NARROW_2);

	bottom_btn = gtk_button_new_from_stock(GTK_STOCK_GOTO_BOTTOM);
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

	add_btn = gtk_button_new_from_stock(GTK_STOCK_ADD);
	gtk_widget_show(add_btn);
	gtk_box_pack_start(GTK_BOX(btn_hbox), add_btn, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT(add_btn), "clicked",
			 G_CALLBACK(prefs_filter_add_cb), NULL);

#ifdef GTK_STOCK_EDIT
	edit_btn = gtk_button_new_from_stock(GTK_STOCK_EDIT);
#else
	edit_btn = gtk_button_new_with_label(_("Edit"));
#endif
	gtk_widget_show(edit_btn);
	gtk_box_pack_start(GTK_BOX(btn_hbox), edit_btn, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT(edit_btn), "clicked",
			 G_CALLBACK(prefs_filter_edit_cb), NULL);

	copy_btn = gtk_button_new_from_stock(GTK_STOCK_COPY);
	gtk_widget_show(copy_btn);
	gtk_box_pack_start(GTK_BOX(btn_hbox), copy_btn, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT(copy_btn), "clicked",
			 G_CALLBACK(prefs_filter_copy_cb), NULL);

	del_btn = gtk_button_new_from_stock(GTK_STOCK_DELETE);
	gtk_widget_show(del_btn);
	gtk_box_pack_start(GTK_BOX(btn_hbox), del_btn, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT(del_btn), "clicked",
			 G_CALLBACK(prefs_filter_delete_cb), NULL);

	gtk_widget_show_all(window);

	rule_list_window.window = window;
	rule_list_window.close_btn = close_btn;

	rule_list_window.treeview = treeview;
	rule_list_window.store = store;
	rule_list_window.selection = selection;

	rule_list_window.add_btn = add_btn;
	rule_list_window.edit_btn = edit_btn;
	rule_list_window.copy_btn = copy_btn;
	rule_list_window.del_btn = del_btn;

	rule_list_window.default_hdr_list = NULL;
	rule_list_window.user_hdr_list = NULL;
	rule_list_window.msg_hdr_list = NULL;
	rule_list_window.msg_hdr_table = NULL;
}

static void prefs_filter_set_dialog(void)
{
	GSList *cur;

	gtk_list_store_clear(rule_list_window.store);

	for (cur = prefs_common.fltlist; cur != NULL; cur = cur->next) {
		FilterRule *rule = (FilterRule *)cur->data;
		prefs_filter_set_list_row(NULL, rule, FALSE);
	}
}

static void prefs_filter_set_list_row(GtkTreeIter *iter, FilterRule *rule,
				      gboolean move_view)
{
	GtkListStore *store = rule_list_window.store;
	gchar *rule_name;
	GtkTreeIter iter_;

	g_return_if_fail(rule != NULL);

	if (rule->name && *rule->name)
		rule_name = g_strdup(rule->name);
	else
		rule_name = filter_get_str(rule);

	if (!iter) {
		gtk_list_store_append(store, &iter_);
		gtk_list_store_set(store, &iter_,
				   COL_ENABLED, rule->enabled,
				   COL_NAME, rule_name,
				   COL_FILTER_RULE, rule, -1);
	} else {
		FilterRule *prev_rule = NULL;

		iter_ = *iter;
		gtk_tree_model_get(GTK_TREE_MODEL(store), &iter_,
				   COL_FILTER_RULE, &prev_rule, -1);
		if (!prev_rule) {
			g_warning("rule at the row not found\n");
			gtk_list_store_append(store, &iter_);
		}

		gtk_list_store_set(store, &iter_,
				   COL_ENABLED, rule->enabled,
				   COL_NAME, rule_name,
				   COL_FILTER_RULE, rule, -1);

		if (prev_rule && prev_rule != rule)
			filter_rule_free(prev_rule);
	}

	g_free(rule_name);

	if (move_view) {
		GtkTreePath *path;

		path = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &iter_);
		gtk_tree_view_scroll_to_cell
			(GTK_TREE_VIEW(rule_list_window.treeview),
			 path, NULL, TRUE, 0.5, 0.0);
		gtk_tree_path_free(path);
	}

	rule_list_window.modified = TRUE;
}

#define APPEND_HDR_LIST(hdr_list)					  \
	for (cur = hdr_list; cur != NULL; cur = cur->next) {		  \
		header = (Header *)cur->data;				  \
									  \
		if (!g_hash_table_lookup(table, header->name)) {	  \
			g_hash_table_insert(table, header->name, header); \
			list = procheader_add_header_list		  \
				(list, header->name, header->body);	  \
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
		if (!g_ascii_strcasecmp(header->name, "Received") ||
		    !g_ascii_strcasecmp(header->name, "Mime-Version") ||
		    !g_ascii_strcasecmp(header->name, "X-UIDL")) {
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

void prefs_filter_set_header_list(MsgInfo *msginfo)
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
	if ((fp = g_fopen(path, "rb")) != NULL) {
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

void prefs_filter_write_user_header_list(void)
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
	FilterRule *rule;
	GtkTreeIter iter;
	GtkTreeModel *model = GTK_TREE_MODEL(rule_list_window.store);

	g_slist_free(prefs_common.fltlist);
	prefs_common.fltlist = NULL;

	if (!gtk_tree_model_get_iter_first(model, &iter))
		return;

	do {
		gtk_tree_model_get(model, &iter, COL_FILTER_RULE, &rule, -1);
		if (rule)
			prefs_common.fltlist =
				g_slist_append(prefs_common.fltlist, rule);
	} while (gtk_tree_model_iter_next(model, &iter));
}

static void prefs_filter_add_cb(void)
{
	FilterRule *rule;

	rule = prefs_filter_edit_open(NULL, NULL, NULL);
	gtk_window_present(GTK_WINDOW(rule_list_window.window));

	if (rule) {
		prefs_filter_set_list_row(NULL, rule, TRUE);
		prefs_filter_set_list();
	}
}

static void prefs_filter_edit_cb(void)
{
	GtkTreeIter iter;
	FilterRule *rule, *new_rule;

	if (!gtk_tree_selection_get_selected(rule_list_window.selection,
					     NULL, &iter))
		return;

	gtk_tree_model_get(GTK_TREE_MODEL(rule_list_window.store), &iter,
			   COL_FILTER_RULE, &rule, -1);
	g_return_if_fail(rule != NULL);

	new_rule = prefs_filter_edit_open(rule, NULL, NULL);
	gtk_window_present(GTK_WINDOW(rule_list_window.window));

	if (new_rule) {
		prefs_filter_set_list_row(&iter, new_rule, TRUE);
		prefs_filter_set_list();
	}
}

static void prefs_filter_copy_cb(void)
{
	GtkTreeIter iter;
	FilterRule *rule, *new_rule;

	if (!gtk_tree_selection_get_selected(rule_list_window.selection,
					     NULL, &iter))
		return;

	gtk_tree_model_get(GTK_TREE_MODEL(rule_list_window.store), &iter,
			   COL_FILTER_RULE, &rule, -1);
	g_return_if_fail(rule != NULL);

	new_rule = prefs_filter_edit_open(rule, NULL, NULL);
	gtk_window_present(GTK_WINDOW(rule_list_window.window));

	if (new_rule) {
		prefs_filter_set_list_row(NULL, new_rule, TRUE);
		prefs_filter_set_list();
	}
}

static void prefs_filter_delete_cb(void)
{
	GtkTreeIter iter;
	FilterRule *rule;
	gchar buf[BUFFSIZE];
	gboolean valid;

	if (!gtk_tree_selection_get_selected(rule_list_window.selection,
					     NULL, &iter))
		return;

	gtk_tree_model_get(GTK_TREE_MODEL(rule_list_window.store), &iter,
			   COL_FILTER_RULE, &rule, -1);
	g_return_if_fail(rule != NULL);

	g_snprintf(buf, sizeof(buf),
		   _("Do you really want to delete the rule '%s'?"),
		   rule->name ? rule->name : _("(Untitled)"));
	if (alertpanel(_("Delete rule"), buf,
		       GTK_STOCK_YES, GTK_STOCK_NO, NULL) != G_ALERTDEFAULT)
		return;

	valid = gtk_list_store_remove(rule_list_window.store, &iter);
	if (valid)
		gtk_tree_selection_select_iter(rule_list_window.selection,
					       &iter);

	prefs_common.fltlist = g_slist_remove(prefs_common.fltlist, rule);
	filter_rule_free(rule);

	rule_list_window.modified = TRUE;
}

static void prefs_filter_top(void)
{
	GtkTreeIter iter;

	if (!gtk_tree_selection_get_selected(rule_list_window.selection,
					     NULL, &iter))
		return;

	gtk_list_store_move_after(rule_list_window.store, &iter, NULL);
}

static void prefs_filter_up(void)
{
	GtkTreeModel *model = GTK_TREE_MODEL(rule_list_window.store);
	GtkTreeIter iter, prev;
	GtkTreePath *path;

	if (!gtk_tree_selection_get_selected(rule_list_window.selection,
					     NULL, &iter))
		return;

	path = gtk_tree_model_get_path(model, &iter);
	if (gtk_tree_path_prev(path)) {
		gtk_tree_model_get_iter(model, &prev, path);
		gtk_list_store_swap(rule_list_window.store, &iter, &prev);
	}
	gtk_tree_path_free(path);
}

static void prefs_filter_down(void)
{
	GtkTreeIter iter, next;

	if (!gtk_tree_selection_get_selected(rule_list_window.selection,
					     NULL, &iter))
		return;

	next = iter;
	if (gtk_tree_model_iter_next(GTK_TREE_MODEL(rule_list_window.store),
				     &next))
		gtk_list_store_swap(rule_list_window.store, &iter, &next);
}

static void prefs_filter_bottom(void)
{
	GtkTreeIter iter;

	if (!gtk_tree_selection_get_selected(rule_list_window.selection,
					     NULL, &iter))
		return;

	gtk_list_store_move_before(rule_list_window.store, &iter, NULL);
}

static gboolean prefs_filter_select(GtkTreeSelection *selection,
				    GtkTreeModel *model, GtkTreePath *path,
				    gboolean cur_selected, gpointer data)
{
	return TRUE;
}

static void prefs_filter_enable_toggled(GtkCellRenderer *cell, gchar *path_str,
					gpointer data)
{
	FilterRule *rule;
	GtkTreeIter iter;
	GtkTreePath *path;

	path = gtk_tree_path_new_from_string(path_str);
	gtk_tree_model_get_iter(GTK_TREE_MODEL(rule_list_window.store),
				&iter, path);
	gtk_tree_path_free(path);
	gtk_tree_model_get(GTK_TREE_MODEL(rule_list_window.store), &iter,
			   COL_FILTER_RULE, &rule, -1);

	rule->enabled ^= TRUE;

	gtk_list_store_set(rule_list_window.store, &iter,
			   COL_ENABLED, rule->enabled, -1);
	rule_list_window.modified = TRUE;
}

static void prefs_filter_row_activated(GtkTreeView *treeview, GtkTreePath *path,
				       GtkTreeViewColumn *column,
				       gpointer data)
{
	gtk_button_clicked(GTK_BUTTON(rule_list_window.edit_btn));
}

static void prefs_filter_row_reordered(GtkTreeModel *model,
				       GtkTreePath *path, GtkTreeIter *iter,
				       gpointer data, gpointer user_data)
{
	GtkTreeIter iter_;
	GtkTreePath *path_;

	if (!gtk_tree_selection_get_selected(rule_list_window.selection,
					     NULL, &iter_))
		return;
	path_ = gtk_tree_model_get_path
		(GTK_TREE_MODEL(rule_list_window.store), &iter_);
	gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(rule_list_window.treeview),
				     path_, NULL, FALSE, 0.0, 0.0);
	gtk_tree_path_free(path_);
	rule_list_window.modified = TRUE;
}

static void prefs_filter_row_inserted(GtkTreeModel *model, GtkTreePath *path,
				      GtkTreeIter *iter, gpointer user_data)
{
	rule_list_window.modified = TRUE;
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
	if (rule_list_window.on_init)
		return TRUE;

	if (event && event->keyval == GDK_Escape)
		prefs_filter_close();
	return FALSE;
}

static void prefs_filter_close(void)
{
	if (rule_list_window.on_init)
		return;

	prefs_filter_set_msg_header_list(NULL);
	if (rule_list_window.modified) {
		prefs_filter_set_list();
		filter_write_config();
	}
	gtk_widget_hide(rule_list_window.window);
	gtk_list_store_clear(rule_list_window.store);
	main_window_popup(main_window_get());
	inc_unlock();
}
