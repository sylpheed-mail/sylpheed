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
#include <gtk/gtkctree.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkstock.h>
#include <string.h>
#include <fnmatch.h>

#include "intl.h"
#include "grouplistdialog.h"
#include "manage_window.h"
#include "gtkutils.h"
#include "utils.h"
#include "news.h"
#include "folder.h"
#include "alertpanel.h"
#include "recv.h"
#include "socket.h"

#define GROUPLIST_DIALOG_WIDTH		450
#define GROUPLIST_DIALOG_HEIGHT		400
#define GROUPLIST_COL_NAME_WIDTH	250

static gboolean ack;
static gboolean locked;

static GtkWidget *dialog;
static GtkWidget *entry;
static GtkWidget *ctree;
static GtkWidget *status_label;
static GtkWidget *ok_button;
static GSList *group_list;
static Folder *news_folder;

static GSList *subscribed;

static void grouplist_dialog_create	(void);
static void grouplist_dialog_set_list	(const gchar	*pattern,
					 gboolean	 refresh);
static void grouplist_search		(void);
static void grouplist_clear		(void);
static gboolean grouplist_recv_func	(SockInfo	*sock,
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
static void ctree_selected	(GtkCTree	*ctree,
				 GtkCTreeNode	*node,
				 gint		 column,
				 gpointer	 data);
static void ctree_unselected	(GtkCTree	*ctree,
				 GtkCTreeNode	*node,
				 gint		 column,
				 gpointer	 data);
static void entry_activated	(GtkEditable	*editable);
static void search_clicked	(GtkWidget	*widget,
				 gpointer	 data);

GSList *grouplist_dialog(Folder *folder)
{
	GNode *node;
	FolderItem *item;

	if (dialog && GTK_WIDGET_VISIBLE(dialog)) return NULL;

	if (!dialog)
		grouplist_dialog_create();

	news_folder = folder;

	gtk_widget_show(dialog);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	manage_window_set_transient(GTK_WINDOW(dialog));
	gtk_widget_grab_focus(ok_button);
	gtk_widget_grab_focus(ctree);
	GTK_EVENTS_FLUSH();

	subscribed = NULL;
	for (node = folder->node->children; node != NULL; node = node->next) {
		item = FOLDER_ITEM(node->data);
		subscribed = g_slist_append(subscribed, g_strdup(item->path));
	}

	grouplist_dialog_set_list(NULL, TRUE);

	if (ack) gtk_main();

	manage_window_focus_out(dialog, NULL, NULL);
	gtk_widget_hide(dialog);

	if (!ack) {
		slist_free_strings(subscribed);
		g_slist_free(subscribed);
		subscribed = NULL;

		for (node = folder->node->children; node != NULL;
		     node = node->next) {
			item = FOLDER_ITEM(node->data);
			subscribed = g_slist_append(subscribed,
						    g_strdup(item->path));
		}
	}

	grouplist_clear();

	return subscribed;
}

static void grouplist_dialog_create(void)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *msg_label;
	GtkWidget *search_button;
	GtkWidget *confirm_area;
	GtkWidget *cancel_button;	
	GtkWidget *refresh_button;	
	GtkWidget *scrolledwin;
	gchar *titles[3];
	gint i;

	dialog = gtk_dialog_new();
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, TRUE, FALSE);
	gtk_widget_set_size_request(dialog,
				    GROUPLIST_DIALOG_WIDTH,
				    GROUPLIST_DIALOG_HEIGHT);
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
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);

	titles[0] = _("Newsgroup name");
	titles[1] = _("Messages");
	titles[2] = _("Type");
	ctree = gtk_ctree_new_with_titles(3, 0, titles);
	gtk_container_add(GTK_CONTAINER(scrolledwin), ctree);
	gtk_clist_set_column_width
		(GTK_CLIST(ctree), 0, GROUPLIST_COL_NAME_WIDTH);
	gtk_clist_set_selection_mode(GTK_CLIST(ctree), GTK_SELECTION_MULTIPLE);
	gtk_ctree_set_line_style(GTK_CTREE(ctree), GTK_CTREE_LINES_DOTTED);
	gtk_ctree_set_expander_style(GTK_CTREE(ctree),
				     GTK_CTREE_EXPANDER_SQUARE);
	for (i = 0; i < 3; i++)
		GTK_WIDGET_UNSET_FLAGS(GTK_CLIST(ctree)->column[i].button,
				       GTK_CAN_FOCUS);
	g_signal_connect(G_OBJECT(ctree), "tree_select_row",
			 G_CALLBACK(ctree_selected), NULL);
	g_signal_connect(G_OBJECT(ctree), "tree_unselect_row",
			 G_CALLBACK(ctree_unselected), NULL);

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

static void grouplist_hash_init(void)
{
	branch_node_table = g_hash_table_new(g_str_hash, g_str_equal);
}

static void grouplist_hash_done(void)
{
	hash_free_strings(branch_node_table);
	g_hash_table_destroy(branch_node_table);
}

static GtkCTreeNode *grouplist_hash_get_branch_node(const gchar *name)
{
	return g_hash_table_lookup(branch_node_table, name);
}

static void grouplist_hash_set_branch_node(const gchar *name,
					   GtkCTreeNode *node)
{
	g_hash_table_insert(branch_node_table, g_strdup(name), node);
}

static gchar *grouplist_get_parent_name(const gchar *name)
{
	gchar *p;

	p = strrchr(name, '.');
	if (!p)
		return g_strdup("");
	return g_strndup(name, p - name);
}

static GtkCTreeNode *grouplist_create_parent(const gchar *name,
					     const gchar *pattern)
{
	GtkCTreeNode *parent;
	GtkCTreeNode *node;
	gchar *cols[3];
	gchar *parent_name;

	if (*name == '\0') return NULL;
	node = grouplist_hash_get_branch_node(name);
	if (node != NULL) return node;

	cols[0] = (gchar *)name;
	cols[1] = cols[2] = "";

	parent_name = grouplist_get_parent_name(name);
	parent = grouplist_create_parent(parent_name, pattern);

	node = parent ? GTK_CTREE_ROW(parent)->children
		: GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);
	node = gtk_ctree_insert_node(GTK_CTREE(ctree), parent, node,
				     cols, 0, NULL, NULL, NULL, NULL,
				     FALSE, FALSE);
	if (parent && fnmatch(pattern, parent_name, 0) != 0)
		gtk_ctree_expand(GTK_CTREE(ctree), parent);
	gtk_ctree_node_set_selectable(GTK_CTREE(ctree), node, FALSE);

	grouplist_hash_set_branch_node(name, node);

	g_free(parent_name);

	return node;
}

static GtkCTreeNode *grouplist_create_branch(NewsGroupInfo *ginfo,
					     const gchar *pattern)
{
	GtkCTreeNode *node;
	GtkCTreeNode *parent;
	gchar *name = (gchar *)ginfo->name;
	gchar *parent_name;
	gchar *count_str;
	gchar *cols[3];
	gint count;

	count = ginfo->last - ginfo->first;
	if (count < 0)
		count = 0;
	count_str = itos(count);

	cols[0] = ginfo->name;
	cols[1] = count_str;
	if (ginfo->type == 'y')
		cols[2] = "";
	else if (ginfo->type == 'm')
		cols[2] = _("moderated");
	else if (ginfo->type == 'n')
		cols[2] = _("readonly");
	else
		cols[2] = _("unknown");

	parent_name = grouplist_get_parent_name(name);
	parent = grouplist_create_parent(parent_name, pattern);
	node = grouplist_hash_get_branch_node(name);
	if (node) {
		gtk_ctree_set_node_info(GTK_CTREE(ctree), node, cols[0], 0,
					NULL, NULL, NULL, NULL, FALSE, FALSE);
		gtk_ctree_node_set_text(GTK_CTREE(ctree), node, 1, cols[1]);
		gtk_ctree_node_set_text(GTK_CTREE(ctree), node, 2, cols[2]);
	} else {
		node = parent ? GTK_CTREE_ROW(parent)->children
			: GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);
		node = gtk_ctree_insert_node(GTK_CTREE(ctree), parent, node,
					     cols, 0, NULL, NULL, NULL, NULL,
					     TRUE, FALSE);
		if (parent && fnmatch(pattern, parent_name, 0) != 0)
			gtk_ctree_expand(GTK_CTREE(ctree), parent);
	}
	gtk_ctree_node_set_selectable(GTK_CTREE(ctree), node, TRUE);
	if (node)
		gtk_ctree_node_set_row_data(GTK_CTREE(ctree), node, ginfo);

	g_free(parent_name);

	return node;
}

static void grouplist_dialog_set_list(const gchar *pattern, gboolean refresh)
{
	GSList *cur;
	GtkCTreeNode *node;

	if (locked) return;
	locked = TRUE;

	if (!pattern || *pattern == '\0')
		pattern = "*";

	if (refresh) {
		ack = TRUE;
		grouplist_clear();
		recv_set_ui_func(grouplist_recv_func, NULL);
		group_list = news_get_group_list(news_folder);
		group_list = g_slist_reverse(group_list);
		recv_set_ui_func(NULL, NULL);
		if (group_list == NULL && ack == TRUE) {
			alertpanel_error(_("Can't retrieve newsgroup list."));
			locked = FALSE;
			return;
		}
	} else {
		g_signal_handlers_block_by_func
			(G_OBJECT(ctree), G_CALLBACK(ctree_unselected),
			 NULL);
		gtk_clist_clear(GTK_CLIST(ctree));
		g_signal_handlers_unblock_by_func
			(G_OBJECT(ctree), G_CALLBACK(ctree_unselected),
			 NULL);
	}
	gtk_entry_set_text(GTK_ENTRY(entry), pattern);

	grouplist_hash_init();

	gtk_clist_freeze(GTK_CLIST(ctree));

	g_signal_handlers_block_by_func(G_OBJECT(ctree),
					G_CALLBACK(ctree_selected), NULL);

	for (cur = group_list; cur != NULL ; cur = cur->next) {
		NewsGroupInfo *ginfo = (NewsGroupInfo *)cur->data;

		if (fnmatch(pattern, ginfo->name, 0) == 0) {
			node = grouplist_create_branch(ginfo, pattern);
			if (g_slist_find_custom(subscribed, ginfo->name,
						(GCompareFunc)g_strcasecmp)
			    != NULL)
				gtk_ctree_select(GTK_CTREE(ctree), node);
		}
	}

	g_signal_handlers_unblock_by_func(G_OBJECT(ctree),
					  G_CALLBACK(ctree_selected), NULL);

	gtk_clist_thaw(GTK_CLIST(ctree));

	grouplist_hash_done();

	gtk_label_set_text(GTK_LABEL(status_label), _("Done."));

	locked = FALSE;
}

static void grouplist_search(void)
{
	gchar *str;

	if (locked) return;

	str = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
	grouplist_dialog_set_list(str, FALSE);
	g_free(str);
}

static void grouplist_clear(void)
{
	g_signal_handlers_block_by_func(G_OBJECT(ctree),
					G_CALLBACK(ctree_unselected), NULL);
	gtk_clist_clear(GTK_CLIST(ctree));
	gtk_entry_set_text(GTK_ENTRY(entry), "");
	news_group_list_free(group_list);
	group_list = NULL;
	g_signal_handlers_unblock_by_func(G_OBJECT(ctree),
					  G_CALLBACK(ctree_unselected), NULL);
}

static gboolean grouplist_recv_func(SockInfo *sock, gint count, gint read_bytes,
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
	grouplist_dialog_set_list(str, TRUE);
	g_free(str);
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event,
			    gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		cancel_clicked(NULL, NULL);
	return FALSE;
}

static void ctree_selected(GtkCTree *ctree, GtkCTreeNode *node, gint column,
			   gpointer data)
{
	NewsGroupInfo *ginfo;

	ginfo = gtk_ctree_node_get_row_data(ctree, node);
	if (!ginfo) return;

	subscribed = g_slist_append(subscribed, g_strdup(ginfo->name));
}

static void ctree_unselected(GtkCTree *ctree, GtkCTreeNode *node, gint column,
			     gpointer data)
{
	NewsGroupInfo *ginfo;
	GSList *list;

	ginfo = gtk_ctree_node_get_row_data(ctree, node);
	if (!ginfo) return;

	list = g_slist_find_custom(subscribed, ginfo->name,
				   (GCompareFunc)g_strcasecmp);
	if (list) {
		g_free(list->data);
		subscribed = g_slist_remove(subscribed, list->data);
	}
}

static void entry_activated(GtkEditable *editable)
{
	grouplist_search();
}

static void search_clicked(GtkWidget *widget, gpointer data)
{
	grouplist_search();
}
