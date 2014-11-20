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

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtktreestore.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkstock.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#include "main.h"
#include "utils.h"
#include "gtkutils.h"
#include "stock_pixmap.h"
#include "foldersel.h"
#include "alertpanel.h"
#include "manage_window.h"
#include "folderview.h"
#include "inputdialog.h"
#include "folder.h"
#include "prefs_common.h"

enum {
	FOLDERSEL_FOLDERNAME,
	FOLDERSEL_FOLDERITEM,
	FOLDERSEL_PIXBUF,
	FOLDERSEL_PIXBUF_OPEN,
	FOLDERSEL_FOREGROUND,
	FOLDERSEL_BOLD,
	N_FOLDERSEL_COLUMNS
};

typedef struct _FolderItemSearch	FolderItemSearch;

struct _FolderItemSearch
{
	FolderItem *item;
	GtkTreePath *path;
	GtkTreeIter iter;
};

static GdkPixbuf *folder_pixbuf;
static GdkPixbuf *folderopen_pixbuf;
static GdkPixbuf *foldernoselect_pixbuf;

static GtkWidget *window;
static GtkWidget *label;
static GtkWidget *scrolledwin;
static GtkWidget *treeview;
static GtkWidget *entry;
static GtkWidget *confirm_area;
static GtkWidget *ok_button;
static GtkWidget *cancel_button;
static GtkWidget *new_button;

static GtkTreeStore *tree_store;

static FolderItem *folder_item;
static FolderItem *selected_item;

FolderSelectionType sel_type;

static gboolean cancelled;
static gboolean finished;

static void foldersel_create		(void);
static void foldersel_init		(void);

static void foldersel_append_item	(GtkTreeStore	*store,
					 FolderItem	*item,
					 GtkTreeIter	*iter,
					 GtkTreeIter	*parent);

static void foldersel_set_tree		(Folder		*cur_folder);

static gboolean foldersel_selected	(GtkTreeSelection *selection,
					 GtkTreeModel	  *model,
					 GtkTreePath	  *path,
					 gboolean	   currently_selected,
					 gpointer	   data);

static void foldersel_ok		(GtkButton	*button,
					 gpointer	 data);
static void foldersel_cancel		(GtkButton	*button,
					 gpointer	 data);
static void foldersel_new_folder	(GtkButton	*button,
					 gpointer	 data);

static void foldersel_entry_activated	(GtkEntry	*entry,
					 gpointer	 data);

static void foldersel_tree_activated	(GtkTreeView		*treeview,
					 GtkTreePath		*path,
					 GtkTreeViewColumn	*column,
					 gpointer		 data);

static gboolean foldersel_tree_key_pressed	(GtkWidget	*widget,
						 GdkEventKey	*event,
						 gpointer	 data);

static gint delete_event		(GtkWidget	*widget,
					 GdkEventAny	*event,
					 gpointer	 data);
static gboolean key_pressed		(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gpointer	 data);

static gint foldersel_folder_name_compare	(GtkTreeModel	*model,
						 GtkTreeIter	*a,
						 GtkTreeIter	*b,
						 gpointer	 context);

static gboolean tree_view_folder_item_func	(GtkTreeModel	  *model,
						 GtkTreePath	  *path,
						 GtkTreeIter	  *iter,
						 FolderItemSearch *data);

FolderItem *foldersel_folder_sel(Folder *cur_folder, FolderSelectionType type,
				 const gchar *default_folder)
{
	return foldersel_folder_sel_full(cur_folder, type, default_folder,
					 NULL);
}

FolderItem *foldersel_folder_sel_full(Folder *cur_folder,
				      FolderSelectionType type,
				      const gchar *default_folder,
				      const gchar *message)
{
	selected_item = NULL;
	sel_type = type;

	if (!window) {
		foldersel_create();
		foldersel_init();
	}

	if (message) {
		gtk_widget_show(label);
		gtk_label_set_text(GTK_LABEL(label), message);
	} else
		gtk_widget_hide(label);

	foldersel_set_tree(cur_folder);

	/* select current */
	if (folder_item) {
		FolderItemSearch fis;

		fis.item = folder_item;
		fis.path = NULL;

		/* find matching model entry */
		gtk_tree_model_foreach
			(GTK_TREE_MODEL(tree_store),
			 (GtkTreeModelForeachFunc)tree_view_folder_item_func,
			 &fis);

		if (fis.path) {
			GtkTreeSelection *selection;

			selection = gtk_tree_view_get_selection
				(GTK_TREE_VIEW(treeview));
			gtkut_tree_view_expand_parent_all
				(GTK_TREE_VIEW(treeview), &fis.iter);
			gtk_tree_selection_select_iter(selection, &fis.iter);
			gtk_tree_view_set_cursor(GTK_TREE_VIEW(treeview),
						 fis.path, NULL, FALSE);
			gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeview),
						     fis.path,
						     NULL, TRUE, 0.5, 0.0);
			gtk_tree_path_free(fis.path);
		} else
			gtk_tree_view_scroll_to_point
				(GTK_TREE_VIEW(treeview), 0, 0);
	} else
		gtk_tree_view_scroll_to_point(GTK_TREE_VIEW(treeview), 0, 0);

	gtkut_box_set_reverse_order(GTK_BOX(confirm_area),
				    !prefs_common.comply_gnome_hig);
	gtk_widget_grab_focus(ok_button);
	gtk_widget_grab_focus(treeview);

	manage_window_set_transient(GTK_WINDOW(window));
	gtk_widget_show(window);

	cancelled = finished = FALSE;

	while (finished == FALSE)
		gtk_main_iteration();

	gtk_widget_hide(window);
	gtk_label_set_text(GTK_LABEL(label), "");
	gtk_entry_set_text(GTK_ENTRY(entry), "");
	gtk_tree_store_clear(tree_store);

	if (cancelled || !selected_item)
		return NULL;

	if (type == FOLDER_SEL_ALL ||
	    (selected_item->stype != F_VIRTUAL &&
	     (sel_type == FOLDER_SEL_MOVE_FOLDER ||
	      (selected_item->path && !selected_item->no_select)))) {
		folder_item = selected_item;
		return folder_item;
	}

	return NULL;
}

static void foldersel_create(void)
{
	GtkWidget *vbox;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeSelection *selection;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), _("Select folder"));
	gtk_container_set_border_width(GTK_CONTAINER(window), 4);
	gtk_window_set_position(GTK_WINDOW(window),
				GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, FALSE);
	gtk_window_set_wmclass
		(GTK_WINDOW(window), "folder_selection", "Sylpheed");
	gtk_widget_realize(window);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(delete_event), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);

	vbox = gtk_vbox_new(FALSE, 4);
	gtk_widget_set_size_request(vbox, -1, 460 * gtkut_get_dpi_multiplier());
	gtk_container_add(GTK_CONTAINER(window), vbox);

	label = gtk_label_new("");
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 2);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_size_request(scrolledwin, 320 * gtkut_get_dpi_multiplier(), -1);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(vbox), scrolledwin, TRUE, TRUE, 0);

	tree_store = gtk_tree_store_new(N_FOLDERSEL_COLUMNS,
					G_TYPE_STRING,
					G_TYPE_POINTER,
					GDK_TYPE_PIXBUF,
					GDK_TYPE_PIXBUF,
					GDK_TYPE_COLOR,
					G_TYPE_INT);
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(tree_store),
					FOLDERSEL_FOLDERNAME,
					foldersel_folder_name_compare,
					NULL, NULL);

	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(tree_store));
	g_object_unref(G_OBJECT(tree_store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(treeview),
					FOLDERSEL_FOLDERNAME);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);
	gtk_tree_selection_set_select_function(selection, foldersel_selected,
					       NULL, NULL);

	g_signal_connect(G_OBJECT(treeview), "row-activated",
			 G_CALLBACK(foldersel_tree_activated), NULL);
	g_signal_connect(G_OBJECT(treeview), "key_press_event",
			 G_CALLBACK(foldersel_tree_key_pressed), NULL);

	gtk_container_add(GTK_CONTAINER(scrolledwin), treeview);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_spacing(column, 1);
	renderer = gtk_cell_renderer_pixbuf_new();
	g_object_set(renderer, "ypad", 0, NULL);
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_set_attributes
		(column, renderer,
		 "pixbuf", FOLDERSEL_PIXBUF,
		 "pixbuf-expander-open", FOLDERSEL_PIXBUF_OPEN,
		 "pixbuf-expander-closed", FOLDERSEL_PIXBUF,
		 NULL);

	/* create text renderer */
	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "ypad", 0, NULL);
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_set_attributes
		(column, renderer,
		 "text", FOLDERSEL_FOLDERNAME,
		 "foreground-gdk", FOLDERSEL_FOREGROUND,
		 "weight", FOLDERSEL_BOLD,
		 NULL);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);

	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	entry = gtk_entry_new();
	gtk_entry_set_editable(GTK_ENTRY(entry), FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(entry), "activate",
			 G_CALLBACK(foldersel_entry_activated), NULL);

	gtkut_stock_button_set_create(&confirm_area,
				      &ok_button,     GTK_STOCK_OK,
				      &cancel_button, GTK_STOCK_CANCEL,
				      NULL, NULL);

	gtk_box_pack_end(GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default(ok_button);

	new_button = gtk_button_new_from_stock(GTK_STOCK_NEW);
	gtk_widget_show(new_button);
	gtk_box_pack_start(GTK_BOX(confirm_area), new_button, FALSE, FALSE, 0);
	gtk_button_box_set_child_secondary(GTK_BUTTON_BOX(confirm_area),
					   new_button, TRUE);

	g_signal_connect(G_OBJECT(ok_button), "clicked",
			 G_CALLBACK(foldersel_ok), NULL);
	g_signal_connect(G_OBJECT(cancel_button), "clicked",
			 G_CALLBACK(foldersel_cancel), NULL);
	g_signal_connect(G_OBJECT(new_button), "clicked",
			 G_CALLBACK(foldersel_new_folder), NULL);

	gtk_widget_show_all(vbox);
}

static void foldersel_init(void)
{
	stock_pixbuf_gdk(treeview, STOCK_PIXMAP_FOLDER_CLOSE,
			 &folder_pixbuf);
	stock_pixbuf_gdk(treeview, STOCK_PIXMAP_FOLDER_OPEN,
			 &folderopen_pixbuf);
	stock_pixbuf_gdk(treeview, STOCK_PIXMAP_FOLDER_NOSELECT,
			 &foldernoselect_pixbuf);
}

static void foldersel_append_item(GtkTreeStore *store, FolderItem *item,
				  GtkTreeIter *iter, GtkTreeIter *parent)
{
	gchar *name;
	gchar *sub = "";
	GdkPixbuf *pixbuf, *pixbuf_open;
	gboolean use_color;
	gboolean no_select;
	PangoWeight weight = PANGO_WEIGHT_NORMAL;
	GdkColor *foreground = NULL;
	static GdkColor color_noselect = {0, COLOR_DIM, COLOR_DIM, COLOR_DIM};
	static GdkColor color_new = {0, (guint16)55000, 15000, 15000};

	name = item->name;

	if (item->stype != F_NORMAL && FOLDER_IS_LOCAL(item->folder)) {
		switch (item->stype) {
		case F_INBOX:
			if (!strcmp2(item->name, INBOX_DIR))
				name = _("Inbox");
			break;
		case F_OUTBOX:
			if (!strcmp2(item->name, OUTBOX_DIR))
				name = _("Sent");
			break;
		case F_QUEUE:
			if (!strcmp2(item->name, QUEUE_DIR))
				name = _("Queue");
			break;
		case F_TRASH:
			if (!strcmp2(item->name, TRASH_DIR))
				name = _("Trash");
			break;
		case F_DRAFT:
			if (!strcmp2(item->name, DRAFT_DIR))
				name = _("Drafts");
			break;
		case F_JUNK:
			if (!strcmp2(item->name, JUNK_DIR))
				name = _("Junk");
			break;
		default:
			break;
		}
	}

	if (!item->parent) {
		switch (FOLDER_TYPE(item->folder)) {
		case F_MH:
			sub = " (MH)"; break;
		case F_IMAP:
			sub = " (IMAP4)"; break;
		case F_NEWS:
			sub = " (News)"; break;
		default:
			break;
		}
	}

	if (item->stype == F_QUEUE && item->total > 0) {
		name = g_strdup_printf("%s%s (%d)", name, sub, item->total);
	} else if (item->unread > 0) {
		name = g_strdup_printf("%s%s (%d)", name, sub, item->unread);
	} else
		name = g_strdup_printf("%s%s", name, sub);

	no_select = item->no_select ||
		(sel_type != FOLDER_SEL_ALL && item->stype == F_VIRTUAL);

	pixbuf = no_select ? foldernoselect_pixbuf : folder_pixbuf;
	pixbuf_open = no_select ? foldernoselect_pixbuf : folderopen_pixbuf;

	if (item->stype == F_OUTBOX || item->stype == F_DRAFT ||
	    item->stype == F_TRASH || item->stype == F_JUNK) {
		use_color = FALSE;
	} else if (item->stype == F_QUEUE) {
		use_color = (item->total > 0);
		if (item->total > 0)
			weight = PANGO_WEIGHT_BOLD;
	} else {
		use_color = (item->new > 0);
		if (item->unread > 0)
			weight = PANGO_WEIGHT_BOLD;
	}

	if (no_select)
		foreground = &color_noselect;
	else if (use_color)
		foreground = &color_new;

	/* insert this node */
	gtk_tree_store_append(store, iter, parent);
	gtk_tree_store_set(store, iter,
			   FOLDERSEL_FOLDERNAME, name,
			   FOLDERSEL_FOLDERITEM, item,
			   FOLDERSEL_PIXBUF, pixbuf,
			   FOLDERSEL_PIXBUF_OPEN, pixbuf_open,
			   FOLDERSEL_FOREGROUND, foreground,
			   FOLDERSEL_BOLD, weight,
			   -1);
	g_free(name);
}

static void foldersel_insert_gnode_in_store(GtkTreeStore *store, GNode *node,
					    GtkTreeIter *parent)
{
	FolderItem *item;
	GtkTreeIter child;
	GNode *iter;

	g_return_if_fail(node != NULL);
	g_return_if_fail(node->data != NULL);
	g_return_if_fail(store != NULL);

	item = FOLDER_ITEM(node->data);
	foldersel_append_item(store, item, &child, parent);

	if (parent && item->parent && node->parent->children == node &&
	    !item->parent->collapsed) {
		GtkTreePath *path;

		path = gtk_tree_model_get_path(GTK_TREE_MODEL(store), parent);
		gtk_tree_view_expand_row(GTK_TREE_VIEW(treeview), path, FALSE);
		gtk_tree_path_free(path);
	}

	/* insert its children (this node as parent) */
	for (iter = node->children; iter != NULL; iter = iter->next)
		foldersel_insert_gnode_in_store(store, iter, &child);
}

static void foldersel_set_tree(Folder *cur_folder)
{
	Folder *folder;
	GList *list;

	for (list = folder_get_list(); list != NULL; list = list->next) {
		folder = FOLDER(list->data);
		g_return_if_fail(folder != NULL);

		if (sel_type != FOLDER_SEL_ALL) {
			if (FOLDER_TYPE(folder) == F_NEWS)
				continue;
		}
		if (sel_type == FOLDER_SEL_MOVE_FOLDER && folder != cur_folder)
			continue;

		foldersel_insert_gnode_in_store(tree_store, folder->node, NULL);
	}

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(tree_store),
					     FOLDERSEL_FOLDERNAME,
					     GTK_SORT_ASCENDING);
}

static gboolean foldersel_selected(GtkTreeSelection *selection,
				   GtkTreeModel *model, GtkTreePath *path,
				   gboolean currently_selected, gpointer data)
{
	GtkTreeIter iter;
	FolderItem *item = NULL;

	if (currently_selected)
		return TRUE;

	if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, path))
		return TRUE;

	gtk_tree_model_get(GTK_TREE_MODEL(tree_store), &iter,
			   FOLDERSEL_FOLDERITEM, &item, -1);

	selected_item = item;
	if (selected_item &&
	    (sel_type == FOLDER_SEL_ALL ||
	     (selected_item->stype != F_VIRTUAL &&
	      (sel_type == FOLDER_SEL_MOVE_FOLDER ||
	       (selected_item->path && !selected_item->no_select))))) {
		gchar *id;
		id = folder_item_get_identifier(selected_item);
		gtk_entry_set_text(GTK_ENTRY(entry), id);
		g_free(id);
	} else
		gtk_entry_set_text(GTK_ENTRY(entry), "");

	return TRUE;
}

static void foldersel_ok(GtkButton *button, gpointer data)
{
	finished = TRUE;
}

static void foldersel_cancel(GtkButton *button, gpointer data)
{
	cancelled = TRUE;
	finished = TRUE;
}

static void foldersel_new_folder(GtkButton *button, gpointer data)
{
	FolderItem *new_item;
	gchar *new_folder;
	gchar *disp_name;
	gchar *p;
	GtkTreeIter selected, new_child;
	GtkTreePath *selected_p, *new_child_p;
	GtkTreeStore *store;
	GtkTreeModel *model;
	GtkTreeSelection *selection;

	if (!selected_item || FOLDER_TYPE(selected_item->folder) == F_NEWS)
		return;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	if (!gtk_tree_selection_get_selected(selection, &model, &selected))
		return;
	store = GTK_TREE_STORE(model);

	new_folder = input_dialog(_("New folder"),
				  _("Input the name of new folder:"),
				  _("NewFolder"));
	if (!new_folder) return;
	AUTORELEASE_STR(new_folder, {g_free(new_folder); return;});

	p = strchr(new_folder, G_DIR_SEPARATOR);
	if ((p && FOLDER_TYPE(selected_item->folder) != F_IMAP) ||
	    (p && FOLDER_TYPE(selected_item->folder) == F_IMAP &&
	     *(p + 1) != '\0')) {
		alertpanel_error(_("`%c' can't be included in folder name."),
				G_DIR_SEPARATOR);
		return;
	}

	disp_name = trim_string(new_folder, 32);
	AUTORELEASE_STR(disp_name, {g_free(disp_name); return;});

	/* find whether the directory already exists */
	if (folder_find_child_item_by_name(selected_item, new_folder)) {
		alertpanel_error(_("The folder `%s' already exists."),
				 disp_name);
		return;
	}

	new_item = selected_item->folder->klass->create_folder
		(selected_item->folder, selected_item, new_folder);
	if (!new_item) {
		alertpanel_error(_("Can't create the folder `%s'."), disp_name);
		return;
	}

	/* add new child */
	foldersel_append_item(store, new_item, &new_child, &selected);
	selected_p = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &selected);
	new_child_p = gtk_tree_model_get_path(GTK_TREE_MODEL(store),
					      &new_child);

	gtk_tree_view_expand_row(GTK_TREE_VIEW(treeview), selected_p, FALSE);
	gtk_tree_selection_select_iter(selection, &new_child);
	gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeview), new_child_p,
				     NULL, TRUE, 0.5, 0.0);
	gtk_tree_path_free(new_child_p);
	gtk_tree_path_free(selected_p);

	folderview_append_item(folderview_get(), NULL, new_item, TRUE);
	folder_write_list();
}

static void foldersel_entry_activated(GtkEntry *entry, gpointer data)
{
	gtk_button_clicked(GTK_BUTTON(ok_button));
}

static void foldersel_tree_activated(GtkTreeView *treeview, GtkTreePath *path,
				     GtkTreeViewColumn *column, gpointer data)
{
	gtk_button_clicked(GTK_BUTTON(ok_button));
}

static gboolean foldersel_tree_key_pressed(GtkWidget *widget,
					   GdkEventKey *event, gpointer data)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreePath *selected;
	GtkAdjustment *adj;
	gboolean moved;

	if (!event)
		return FALSE;

	switch (event->keyval) {
	case GDK_Left:
	case GDK_KP_Left:
		if ((event->state &
		     (GDK_SHIFT_MASK|GDK_MOD1_MASK|GDK_CONTROL_MASK)) != 0)
			return FALSE;
		adj = gtk_scrolled_window_get_hadjustment
			(GTK_SCROLLED_WINDOW(scrolledwin));
		if (adj->lower < adj->value)
			return FALSE;
		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
		if (!gtk_tree_selection_get_selected(selection, &model, &iter))
			return FALSE;
		selected = gtk_tree_model_get_path(model, &iter);
		if (gtk_tree_view_row_expanded(GTK_TREE_VIEW(treeview), selected)) {
			gtk_tree_view_collapse_row(GTK_TREE_VIEW(treeview), selected);
			gtk_tree_path_free(selected);
			return TRUE;
		}
		gtk_tree_path_free(selected);
		g_signal_emit_by_name(G_OBJECT(treeview),
				      "select-cursor-parent", &moved);
		return TRUE;
	case GDK_Right:
	case GDK_KP_Right:
		if ((event->state &
		     (GDK_SHIFT_MASK|GDK_MOD1_MASK|GDK_CONTROL_MASK)) != 0)
			return FALSE;
		adj = gtk_scrolled_window_get_hadjustment
			(GTK_SCROLLED_WINDOW(scrolledwin));
		if (adj->lower - adj->page_size > adj->value)
			return FALSE;
		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
		if (!gtk_tree_selection_get_selected(selection, &model, &iter))
			return FALSE;
		selected = gtk_tree_model_get_path(model, &iter);
		if (!gtk_tree_view_row_expanded(GTK_TREE_VIEW(treeview), selected)) {
			gtk_tree_view_expand_row(GTK_TREE_VIEW(treeview), selected, FALSE);
			gtk_tree_path_free(selected);
			return TRUE;
		}
		gtk_tree_path_free(selected);
		break;
	default:
		break;
	}

	return FALSE;
}

static gint delete_event(GtkWidget *widget, GdkEventAny *event, gpointer data)
{
	foldersel_cancel(NULL, NULL);
	return TRUE;
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		foldersel_cancel(NULL, NULL);
	return FALSE;
}

static gint foldersel_folder_name_compare(GtkTreeModel *model, GtkTreeIter *a,
					  GtkTreeIter *b, gpointer context)
{
	FolderItem *item_a = NULL, *item_b = NULL;

	gtk_tree_model_get(model, a, FOLDERSEL_FOLDERITEM, &item_a, -1);
	gtk_tree_model_get(model, b, FOLDERSEL_FOLDERITEM, &item_b, -1);

	return folder_item_compare(item_a, item_b);
}

static gboolean tree_view_folder_item_func(GtkTreeModel *model,
					   GtkTreePath *path,
					   GtkTreeIter *iter,
					   FolderItemSearch *data)
{
	FolderItem *item = NULL;

	gtk_tree_model_get(model, iter, FOLDERSEL_FOLDERITEM, &item, -1);

	if (data->item == item) {
		data->path = gtk_tree_path_copy(path);
		data->iter = *iter;
		return TRUE;
	}

	return FALSE;
}
