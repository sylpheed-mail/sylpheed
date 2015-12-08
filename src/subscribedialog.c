/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2015 Hiroyuki Yamamoto
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
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtktreestore.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkcellrenderertoggle.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkstock.h>
#include <string.h>

#include "subscribedialog.h"
#include "manage_window.h"
#include "mainwindow.h"
#include "gtkutils.h"
#include "utils.h"
#include "news.h"
#include "folder.h"
#include "alertpanel.h"
#include "statusbar.h"
#include "recv.h"
#include "socket.h"

enum {
	SUBSCRIBE_TOGGLE,
	SUBSCRIBE_NAME,
	SUBSCRIBE_NUM,
	SUBSCRIBE_TYPE,
	SUBSCRIBE_INFO,
	SUBSCRIBE_CAN_TOGGLE,
	N_SUBSCRIBE_COLUMNS
};

#define SUBSCRIBE_DIALOG_WIDTH		450
#define SUBSCRIBE_DIALOG_HEIGHT		400
#define SUBSCRIBE_COL_NAME_WIDTH	250

static gboolean ack;
static gboolean locked;

static GtkWidget *dialog;
static GtkWidget *entry;
static GtkWidget *treeview;
static GtkWidget *status_label;
static GtkWidget *ok_button;

static GtkTreeStore *tree_store;

static GSList *group_list;
static GSList *subscribe_list;
static Folder *news_folder;

static void subscribe_dialog_create	(void);
static void subscribe_dialog_set_list	(const gchar	*pattern,
					 gboolean	 refresh);
static void subscribe_search		(void);
static void subscribe_clear		(void);

static gboolean subscribe_recv_func	(SockInfo	*sock,
					 gint		 count,
					 gint		 read_bytes,
					 gpointer	 data);

static gint window_deleted	(GtkWidget	*widget,
				 GdkEventAny	*event,
				 gpointer	 data);
static void ok_clicked		(GtkWidget	*widget,
				 gpointer	 data);
static void cancel_clicked	(GtkWidget	*widget,
				 gpointer	 data);
static void refresh_clicked	(GtkWidget	*widget,
				 gpointer	 data);
static gboolean key_pressed	(GtkWidget	*widget,
				 GdkEventKey	*event,
				 gpointer	 data);

static gboolean subscribe_selected	(GtkTreeSelection	*selection,
					 GtkTreeModel		*model,
					 GtkTreePath		*path,
					 gboolean		 cur_selected,
					 gpointer		 data);

static void subscribe_toggled	(GtkCellRenderer	*cell,
				 gchar			*path,
				 gpointer		 data);

static void entry_activated	(GtkEditable	*editable);
static void search_clicked	(GtkWidget	*widget,
				 gpointer	 data);

GSList *subscribe_dialog(Folder *folder)
{
	GNode *node;
	FolderItem *item;

	if (dialog && GTK_WIDGET_VISIBLE(dialog)) return NULL;

	if (!dialog)
		subscribe_dialog_create();

	news_folder = folder;

	gtk_widget_show(dialog);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	manage_window_set_transient(GTK_WINDOW(dialog));
	gtk_widget_grab_focus(ok_button);
	gtk_widget_grab_focus(treeview);
	GTK_EVENTS_FLUSH();

	subscribe_list = NULL;
	for (node = folder->node->children; node != NULL; node = node->next) {
		item = FOLDER_ITEM(node->data);
		subscribe_list = g_slist_append(subscribe_list,
						g_strdup(item->path));
	}

	subscribe_dialog_set_list(NULL, TRUE);

	if (ack) gtk_main();

	manage_window_focus_out(dialog, NULL, NULL);
	gtk_widget_hide(dialog);
	main_window_popup(main_window_get());

	if (ack) {
		GSList *cur;

		slist_free_strings(subscribe_list);
		subscribe_list = NULL;
		for (cur = group_list; cur != NULL; cur = cur->next) {
			NewsGroupInfo *ginfo = (NewsGroupInfo *)cur->data;

			if (ginfo->subscribed)
				subscribe_list = g_slist_append
					(subscribe_list, g_strdup(ginfo->name));
		}
	}

	subscribe_clear();

	return subscribe_list;
}

static void subscribe_dialog_create(void)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *msg_label;
	GtkWidget *search_button;
	GtkWidget *confirm_area;
	GtkWidget *cancel_button;	
	GtkWidget *refresh_button;	
	GtkWidget *scrolledwin;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeSelection *selection;

	dialog = gtk_dialog_new();
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, TRUE, FALSE);
	gtk_widget_set_size_request(dialog,
				    SUBSCRIBE_DIALOG_WIDTH * gtkut_get_dpi_multiplier(),
				    SUBSCRIBE_DIALOG_HEIGHT * gtkut_get_dpi_multiplier());
	gtk_container_set_border_width
		(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area), 5);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_title(GTK_WINDOW(dialog), _("Subscribe to newsgroup"));
	g_signal_connect(G_OBJECT(dialog), "delete_event",
			 G_CALLBACK(window_deleted), NULL);
	g_signal_connect(G_OBJECT(dialog), "key_press_event",
			 G_CALLBACK(key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT(dialog);

	gtk_widget_realize(dialog);

	vbox = gtk_vbox_new(FALSE, 8);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), vbox);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	msg_label = gtk_label_new(_("Select newsgroups to subscribe."));
	gtk_box_pack_start(GTK_BOX(hbox), msg_label, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 8);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	msg_label = gtk_label_new(_("Find groups:"));
	gtk_box_pack_start(GTK_BOX(hbox), msg_label, FALSE, FALSE, 0);

	entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(entry), "activate",
			 G_CALLBACK(entry_activated), NULL);

	search_button = gtk_button_new_with_label(_(" Search "));
	gtk_box_pack_start(GTK_BOX(hbox), search_button, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(search_button), "clicked",
			 G_CALLBACK(search_clicked), NULL);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_box_pack_start(GTK_BOX (vbox), scrolledwin, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_SHADOW_IN);

	tree_store = gtk_tree_store_new(N_SUBSCRIBE_COLUMNS,
					G_TYPE_BOOLEAN,
					G_TYPE_STRING,
					G_TYPE_STRING,
					G_TYPE_STRING,
					G_TYPE_POINTER,
					G_TYPE_BOOLEAN);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(tree_store),
					     SUBSCRIBE_NAME,
					     GTK_SORT_ASCENDING);

	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(tree_store));
	g_object_unref(G_OBJECT(tree_store));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(treeview),
					SUBSCRIBE_NAME);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);
	gtk_tree_selection_set_select_function(selection, subscribe_selected,
					       NULL, NULL);

	gtk_container_add(GTK_CONTAINER(scrolledwin), treeview);

	renderer = gtk_cell_renderer_toggle_new();
	column = gtk_tree_view_column_new_with_attributes
		(NULL, renderer, "active", SUBSCRIBE_TOGGLE,
		 "activatable", SUBSCRIBE_CAN_TOGGLE,
		 "visible", SUBSCRIBE_CAN_TOGGLE, NULL);
	gtk_tree_view_column_set_min_width(column, 20);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
	g_signal_connect(renderer, "toggled", G_CALLBACK(subscribe_toggled),
			 NULL);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes
		(_("Newsgroup name"), renderer, "text", SUBSCRIBE_NAME, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
	gtk_tree_view_set_expander_column(GTK_TREE_VIEW(treeview), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes
		(_("Messages"), renderer, "text", SUBSCRIBE_NUM, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes
		(_("Type"), renderer, "text", SUBSCRIBE_TYPE, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	status_label = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(hbox), status_label, FALSE, FALSE, 0);

	gtkut_stock_button_set_create(&confirm_area,
				      &ok_button, GTK_STOCK_OK,
				      &cancel_button, GTK_STOCK_CANCEL,
				      &refresh_button, GTK_STOCK_REFRESH);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area),
			  confirm_area);
	gtk_widget_grab_default(ok_button);

	g_signal_connect(G_OBJECT(ok_button), "clicked",
			 G_CALLBACK(ok_clicked), NULL);
	g_signal_connect(G_OBJECT(cancel_button), "clicked",
			 G_CALLBACK(cancel_clicked), NULL);
	g_signal_connect(G_OBJECT(refresh_button), "clicked",
			 G_CALLBACK(refresh_clicked), NULL);

	gtk_widget_show_all(GTK_DIALOG(dialog)->vbox);
}

static GHashTable *branch_node_table;

static void subscribe_hash_init(void)
{
	branch_node_table = g_hash_table_new_full
		(g_str_hash, g_str_equal, NULL,
		 (GDestroyNotify)gtk_tree_iter_free);
}

static void subscribe_hash_free(void)
{
	hash_free_strings(branch_node_table);
	g_hash_table_destroy(branch_node_table);
}

static gboolean subscribe_hash_get_branch_node(const gchar *name,
					       GtkTreeIter *iter)
{
	GtkTreeIter *iter_;

	iter_ = g_hash_table_lookup(branch_node_table, name);
	if (iter_) {
		*iter = *iter_;
		return TRUE;
	}

	return FALSE;
}

static void subscribe_hash_set_branch_node(const gchar *name,
					   GtkTreeIter *iter)
{
	GtkTreeIter *iter_;

	iter_ = gtk_tree_iter_copy(iter);
	g_hash_table_replace(branch_node_table, g_strdup(name), iter_);
}

static gchar *subscribe_get_parent_name(const gchar *name)
{
	gchar *p;

	p = strrchr(name, '.');
	if (!p)
		return g_strdup("");
	return g_strndup(name, p - name);
}

static gboolean subscribe_create_parent(const gchar *name,
					const gchar *pattern,
					GtkTreeIter *parent)
{
	GtkTreeIter parent_;
	GtkTreeIter iter;
	gchar *parent_name;
	gboolean has_parent;

	if (*name == '\0') return FALSE;
	if (subscribe_hash_get_branch_node(name, &iter)) {
		*parent = iter;
		return TRUE;
	}

	parent_name = subscribe_get_parent_name(name);
	has_parent = subscribe_create_parent(parent_name, pattern, &parent_);

	if (has_parent) {
		gtk_tree_store_append(tree_store, &iter, &parent_);
		if (g_pattern_match_simple(pattern, parent_name) == FALSE)
			gtkut_tree_view_expand_parent_all
				(GTK_TREE_VIEW(treeview), &iter);
	} else
		gtk_tree_store_append(tree_store, &iter, NULL);
	gtk_tree_store_set(tree_store, &iter, SUBSCRIBE_NAME, name, -1);

	subscribe_hash_set_branch_node(name, &iter);

	g_free(parent_name);

	*parent = iter;

	return TRUE;
}

static gboolean subscribe_create_branch(NewsGroupInfo *ginfo,
					const gchar *pattern,
					GtkTreeIter *iter)
{
	GtkTreeIter iter_;
	GtkTreeIter parent;
	const gchar *name = ginfo->name;
	gchar *parent_name;
	gint count;
	const gchar *count_str;
	const gchar *type_str;
	gboolean has_parent;

	count = ginfo->last - ginfo->first;
	if (count < 0)
		count = 0;
	count_str = itos(count);

	if (ginfo->type == 'y')
		type_str = "";
	else if (ginfo->type == 'm')
		type_str = _("moderated");
	else if (ginfo->type == 'n')
		type_str = _("readonly");
	else
		type_str = _("unknown");

	parent_name = subscribe_get_parent_name(name);
	has_parent = subscribe_create_parent(parent_name, pattern, &parent);
	if (!subscribe_hash_get_branch_node(name, &iter_)) {
		if (has_parent) {
			gtk_tree_store_append(tree_store, &iter_, &parent);
			if (g_pattern_match_simple(pattern, parent_name) == FALSE)
				gtkut_tree_view_expand_parent_all
					(GTK_TREE_VIEW(treeview), &iter_);
		} else
			gtk_tree_store_append(tree_store, &iter_, NULL);
	}

	gtk_tree_store_set(tree_store, &iter_,
			   SUBSCRIBE_NAME, name,
			   SUBSCRIBE_NUM, count_str,
			   SUBSCRIBE_TYPE, type_str,
			   SUBSCRIBE_INFO, ginfo,
			   SUBSCRIBE_CAN_TOGGLE, TRUE,
			   -1);

	g_free(parent_name);

	*iter = iter_;

	return TRUE;
}

static void subscribe_dialog_set_list(const gchar *pattern, gboolean refresh)
{
	gchar *pattern_;
	GSList *cur;
	GPatternSpec *pspec;

	if (locked) return;
	locked = TRUE;

	if (!pattern || *pattern == '\0')
		pattern_ = g_strdup("*");
	else if (strchr(pattern, '*') == NULL)
		pattern_ = g_strconcat("*", pattern, "*", NULL);
	else
		pattern_ = g_strdup(pattern);

	if (refresh) {
		ack = TRUE;
		subscribe_clear();
		if (pattern)
			gtk_entry_set_text(GTK_ENTRY(entry), pattern);
		gtk_label_set_text(GTK_LABEL(status_label),
				   _("Getting newsgroup list..."));
		GTK_EVENTS_FLUSH();
		recv_set_ui_func(subscribe_recv_func, NULL);
		group_list = news_get_group_list(news_folder);
		group_list = g_slist_reverse(group_list);
		recv_set_ui_func(NULL, NULL);
		statusbar_pop_all();
		if (group_list == NULL && ack == TRUE) {
			alertpanel_error(_("Can't retrieve newsgroup list."));
			g_free(pattern_);
			locked = FALSE;
			return;
		}
	} else {
		gtk_tree_store_clear(tree_store);
	}

	subscribe_hash_init();

	pspec = g_pattern_spec_new(pattern_);

	for (cur = group_list; cur != NULL ; cur = cur->next) {
		NewsGroupInfo *ginfo = (NewsGroupInfo *)cur->data;
		GtkTreeIter iter;

		if (!ginfo->name || !is_ascii_str(ginfo->name))
			continue;

		if (g_slist_find_custom(subscribe_list, ginfo->name,
					(GCompareFunc)g_ascii_strcasecmp)
		    != NULL)
			ginfo->subscribed = TRUE;

		if (g_pattern_match_string(pspec, ginfo->name)) {
			subscribe_create_branch(ginfo, pattern_, &iter);
			if (ginfo->subscribed)
				gtk_tree_store_set(tree_store, &iter,
						   SUBSCRIBE_TOGGLE, TRUE, -1);
		}
	}

	g_pattern_spec_free(pspec);
	subscribe_hash_free();
	g_free(pattern_);

	gtk_label_set_text(GTK_LABEL(status_label), _("Done."));

	locked = FALSE;
}

static void subscribe_search(void)
{
	gchar *str;

	if (locked) return;

	str = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
	subscribe_dialog_set_list(str, FALSE);
	g_free(str);
}

static void subscribe_clear(void)
{
	gtk_tree_store_clear(tree_store);
	gtk_entry_set_text(GTK_ENTRY(entry), "");
	news_group_list_free(group_list);
	group_list = NULL;
}

static gboolean subscribe_recv_func(SockInfo *sock, gint count, gint read_bytes,
				    gpointer data)
{
	gchar buf[BUFFSIZE];

	g_snprintf(buf, sizeof(buf),
		   _("%d newsgroups received (%s read)"),
		   count, to_human_readable(read_bytes));
	gtk_label_set_text(GTK_LABEL(status_label), buf);
	GTK_EVENTS_FLUSH();
	if (ack == FALSE)
		return FALSE;
	else
		return TRUE;
}

static gint window_deleted(GtkWidget *widget, GdkEventAny *event, gpointer data)
{
	ack = FALSE;
	if (gtk_main_level() > 1)
		gtk_main_quit();

	return TRUE;
}

static void ok_clicked(GtkWidget *widget, gpointer data)
{
	ack = TRUE;
	if (gtk_main_level() > 1)
		gtk_main_quit();
}

static void cancel_clicked(GtkWidget *widget, gpointer data)
{
	ack = FALSE;
	if (gtk_main_level() > 1)
		gtk_main_quit();
}

static void refresh_clicked(GtkWidget *widget, gpointer data)
{ 
	gchar *str;

	if (locked) return;

	news_remove_group_list_cache(news_folder);

	str = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
	subscribe_dialog_set_list(str, TRUE);
	g_free(str);
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event,
			    gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		cancel_clicked(NULL, NULL);
	return FALSE;
}

static gboolean subscribe_selected(GtkTreeSelection *selection,
				   GtkTreeModel *model, GtkTreePath *path,
				   gboolean cur_selected, gpointer data)
{
	GtkTreeIter iter;
	NewsGroupInfo *ginfo;

	if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(tree_store), &iter, path))
		return FALSE;

	gtk_tree_model_get(GTK_TREE_MODEL(tree_store), &iter,
			   SUBSCRIBE_INFO, &ginfo, -1);
	if (!ginfo)
		return FALSE;

	return TRUE;
}

static void subscribe_toggled(GtkCellRenderer *cell, gchar *path_str,
			      gpointer data)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	gboolean enabled;
	NewsGroupInfo *ginfo;
	gboolean can_toggle;

	path = gtk_tree_path_new_from_string(path_str);
	gtk_tree_model_get_iter(GTK_TREE_MODEL(tree_store), &iter, path);
	gtk_tree_path_free(path);

	gtk_tree_model_get(GTK_TREE_MODEL(tree_store), &iter,
			   SUBSCRIBE_TOGGLE, &enabled,
			   SUBSCRIBE_INFO, &ginfo,
			   SUBSCRIBE_CAN_TOGGLE, &can_toggle,
			   -1);
	if (ginfo && can_toggle) {
		ginfo->subscribed = !enabled;
		gtk_tree_store_set(tree_store, &iter,
				   SUBSCRIBE_TOGGLE, !enabled, -1);
	}
}

static void entry_activated(GtkEditable *editable)
{
	subscribe_search();
}

static void search_clicked(GtkWidget *widget, gpointer data)
{
	subscribe_search();
}
