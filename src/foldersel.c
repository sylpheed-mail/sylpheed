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

#include "defs.h"

#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkctree.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtksignal.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#include "intl.h"
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

static GdkPixmap *folderxpm;
static GdkBitmap *folderxpmmask;
static GdkPixmap *folderopenxpm;
static GdkBitmap *folderopenxpmmask;
static GdkPixmap *foldernoselectxpm;
static GdkBitmap *foldernoselectxpmmask;

static GtkWidget *window;
static GtkWidget *ctree;
static GtkWidget *entry;
static GtkWidget *ok_button;
static GtkWidget *cancel_button;
static GtkWidget *new_button;

static FolderItem *folder_item;
static FolderItem *selected_item;

static gboolean cancelled;
static gboolean finished;

static void foldersel_create	(void);
static void foldersel_init	(void);
static void foldersel_set_tree	(Folder			*cur_folder,
				 FolderSelectionType	 type);

static void foldersel_selected	(GtkCList	*clist,
				 gint		 row,
				 gint		 column,
				 GdkEvent	*event,
				 gpointer	 data);

static void foldersel_ok	(GtkButton	*button,
				 gpointer	 data);
static void foldersel_cancel	(GtkButton	*button,
				 gpointer	 data);
static void foldersel_new_folder(GtkButton	*button,
				 gpointer	 data);
static void foldersel_activated	(void);
static gint delete_event	(GtkWidget	*widget,
				 GdkEventAny	*event,
				 gpointer	 data);
static gboolean key_pressed	(GtkWidget	*widget,
				 GdkEventKey	*event,
				 gpointer	 data);

static gint foldersel_clist_compare	(GtkCList	*clist,
					 gconstpointer	 ptr1,
					 gconstpointer	 ptr2);

FolderItem *foldersel_folder_sel(Folder *cur_folder,
				 FolderSelectionType type,
				 const gchar *default_folder)
{
	GtkCTreeNode *node;

	selected_item = NULL;

	if (!window) {
		foldersel_create();
		foldersel_init();
	} else
		gtk_widget_show(window);
	manage_window_set_transient(GTK_WINDOW(window));

	foldersel_set_tree(cur_folder, type);

	if (folder_item) {
		node = gtk_ctree_find_by_row_data
			(GTK_CTREE(ctree), NULL, folder_item);
		if (node) {
			gint row;

			row = gtkut_ctree_get_nth_from_node
				(GTK_CTREE(ctree), node);
			gtk_clist_select_row(GTK_CLIST(ctree), row, -1);
			gtkut_clist_set_focus_row(GTK_CLIST(ctree), row);
			gtk_ctree_node_moveto(GTK_CTREE(ctree), node, -1,
					      0.5, 0);
		}
	}
	gtk_widget_grab_focus(ok_button);
	gtk_widget_grab_focus(ctree);

	cancelled = finished = FALSE;

	while (finished == FALSE)
		gtk_main_iteration();

	gtk_widget_hide(window);
	gtk_entry_set_text(GTK_ENTRY(entry), "");
	gtk_clist_clear(GTK_CLIST(ctree));

	if (!cancelled &&
	    selected_item && selected_item->path && !selected_item->no_select) {
		folder_item = selected_item;
		return folder_item;
	} else
		return NULL;
}

static void foldersel_create(void)
{
	GtkWidget *vbox;
	GtkWidget *scrolledwin;
	GtkWidget *confirm_area;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), _("Select folder"));
	gtk_container_set_border_width(GTK_CONTAINER(window), 4);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, FALSE);
	gtk_window_set_wmclass
		(GTK_WINDOW(window), "folder_selection", "Sylpheed");
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(delete_event), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);

	vbox = gtk_vbox_new(FALSE, 4);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_size_request(scrolledwin, 300, 360);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_box_pack_start(GTK_BOX(vbox), scrolledwin, TRUE, TRUE, 0);

	ctree = gtk_ctree_new(1, 0);
	gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_CLIST(ctree)->vadjustment);
	gtk_container_add(GTK_CONTAINER(scrolledwin), ctree);
	gtk_clist_set_selection_mode(GTK_CLIST(ctree), GTK_SELECTION_BROWSE);
	gtk_ctree_set_line_style(GTK_CTREE(ctree), GTK_CTREE_LINES_DOTTED);
	gtk_ctree_set_expander_style(GTK_CTREE(ctree),
				     GTK_CTREE_EXPANDER_SQUARE);
	gtk_ctree_set_indent(GTK_CTREE(ctree), CTREE_INDENT);
	gtk_clist_set_compare_func(GTK_CLIST(ctree), foldersel_clist_compare);
	GTK_WIDGET_UNSET_FLAGS(GTK_CLIST(ctree)->column[0].button,
			       GTK_CAN_FOCUS);
	/* g_signal_connect(G_OBJECT(ctree), "tree_select_row",
			    G_CALLBACK(foldersel_selected), NULL); */
	g_signal_connect(G_OBJECT(ctree), "select_row",
			 G_CALLBACK(foldersel_selected), NULL);

	entry = gtk_entry_new();
	gtk_entry_set_editable(GTK_ENTRY(entry), FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(entry), "activate",
			 G_CALLBACK(foldersel_activated), NULL);

	gtkut_button_set_create(&confirm_area,
				&ok_button,	_("OK"),
				&cancel_button,	_("Cancel"),
				&new_button,    _("New folder"));

	gtk_box_pack_end(GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default(ok_button);

	g_signal_connect(G_OBJECT(ok_button), "clicked",
			 G_CALLBACK(foldersel_ok), NULL);
	g_signal_connect(G_OBJECT(cancel_button), "clicked",
			 G_CALLBACK(foldersel_cancel), NULL);
	g_signal_connect(G_OBJECT(new_button), "clicked",
			 G_CALLBACK(foldersel_new_folder), NULL);

	gtk_widget_show_all(window);
}

static void foldersel_init(void)
{
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_DIR_CLOSE,
			 &folderxpm, &folderxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_DIR_OPEN,
			 &folderopenxpm, &folderopenxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_DIR_NOSELECT,
			 &foldernoselectxpm, &foldernoselectxpmmask);
}

static gboolean foldersel_gnode_func(GtkCTree *ctree, guint depth,
				     GNode *gnode, GtkCTreeNode *cnode,
				     gpointer data)
{
	FolderItem *item = FOLDER_ITEM(gnode->data);
	gchar *name;
	GdkPixmap *xpm, *openxpm;
	GdkBitmap *mask, *openmask;

	switch (item->stype) {
	case F_INBOX:
		name = _("Inbox");
		break;
	case F_OUTBOX:
		name = _("Sent");
		break;
	case F_QUEUE:
		name = _("Queue");
		break;
	case F_TRASH:
		name = _("Trash");
		break;
	case F_DRAFT:
		name = _("Drafts");
		break;
	default:
		name = item->name;

		if (!item->parent) {
			switch (FOLDER_TYPE(item->folder)) {
			case F_MH:
				Xstrcat_a(name, name, " (MH)", ); break;
			case F_IMAP:
				Xstrcat_a(name, name, " (IMAP4)", ); break;
			case F_NEWS:
				Xstrcat_a(name, name, " (News)", ); break;
			default:
				break;
			}
		}
	}

	if (item->no_select) {
		GdkColor color_noselect = {0, COLOR_DIM, COLOR_DIM, COLOR_DIM};
		xpm = openxpm = foldernoselectxpm;
		mask = openmask = foldernoselectxpmmask;
		gtk_ctree_node_set_foreground(ctree, cnode, &color_noselect);
	} else {
		xpm = folderxpm;
		mask = folderxpmmask;
		openxpm = folderopenxpm;
		openmask = folderopenxpmmask;
	}

	gtk_ctree_node_set_row_data(ctree, cnode, item);
	gtk_ctree_set_node_info(ctree, cnode, name,
				FOLDER_SPACING,
				xpm, mask, openxpm, openmask,
				FALSE, FALSE);

	return TRUE;
}

static void foldersel_expand_func(GtkCTree *ctree, GtkCTreeNode *node,
				  gpointer data)
{
	if (GTK_CTREE_ROW(node)->children)
		gtk_ctree_expand(ctree, node);
}

#define SET_SPECIAL_FOLDER(item)					   \
{									   \
	if (item) {							   \
		GtkCTreeNode *node_, *parent, *sibling;			   \
									   \
		node_ = gtk_ctree_find_by_row_data			   \
			(GTK_CTREE(ctree), node, item);			   \
		if (!node_)						   \
			g_warning("%s not found.\n", item->path);	   \
		else {							   \
			parent = GTK_CTREE_ROW(node_)->parent;		   \
			if (prev && parent == GTK_CTREE_ROW(prev)->parent) \
				sibling = GTK_CTREE_ROW(prev)->sibling;	   \
			else						   \
				sibling = GTK_CTREE_ROW(parent)->children; \
			if (node_ != sibling)				   \
				gtk_ctree_move(GTK_CTREE(ctree),	   \
					       node_, parent, sibling);	   \
		}							   \
									   \
		prev = node_;						   \
	}								   \
}

static void foldersel_set_tree(Folder *cur_folder, FolderSelectionType type)
{
	Folder *folder;
	GtkCTreeNode *node;
	GList *list;

	list = folder_get_list();

	gtk_clist_freeze(GTK_CLIST(ctree));

	for (; list != NULL; list = list->next) {
		GtkCTreeNode *prev = NULL;

		folder = FOLDER(list->data);
		g_return_if_fail(folder != NULL);

		if (type != FOLDER_SEL_ALL) {
			if (FOLDER_TYPE(folder) == F_NEWS)
				continue;
		}

		node = gtk_ctree_insert_gnode(GTK_CTREE(ctree), NULL, NULL,
					      folder->node,
					      foldersel_gnode_func,
					      NULL);
		gtk_ctree_sort_recursive(GTK_CTREE(ctree), node);
		SET_SPECIAL_FOLDER(folder->inbox);
		SET_SPECIAL_FOLDER(folder->outbox);
		SET_SPECIAL_FOLDER(folder->draft);
		SET_SPECIAL_FOLDER(folder->queue);
		SET_SPECIAL_FOLDER(folder->trash);
		gtk_ctree_pre_recursive(GTK_CTREE(ctree), node,
					foldersel_expand_func,
					NULL);
	}

	gtk_clist_thaw(GTK_CLIST(ctree));
}

static void foldersel_selected(GtkCList *clist, gint row, gint column,
			       GdkEvent *event, gpointer data)
{
	GdkEventButton *ev = (GdkEventButton *)event;

	selected_item = gtk_clist_get_row_data(clist, row);
	if (selected_item && selected_item->path && !selected_item->no_select) {
		gchar *id;
		id = folder_item_get_identifier(selected_item);
		gtk_entry_set_text(GTK_ENTRY(entry), id);
		g_free(id);
	} else
		gtk_entry_set_text(GTK_ENTRY(entry), "");

	if (ev && GDK_2BUTTON_PRESS == ev->type)
		gtk_button_clicked(GTK_BUTTON(ok_button));
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
	gchar *text[1] = {NULL};
	GtkCTreeNode *selected_node;
	GtkCTreeNode *node;
	gint row;

	if (!selected_item || FOLDER_TYPE(selected_item->folder) == F_NEWS)
		return;
	selected_node = gtk_ctree_find_by_row_data(GTK_CTREE(ctree), NULL,
						   selected_item);
	if (!selected_node) return;

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

	text[0] = new_item->name;
	node = gtk_ctree_insert_node(GTK_CTREE(ctree), selected_node,
				     NULL, text, FOLDER_SPACING,
				     folderxpm, folderxpmmask,
				     folderopenxpm, folderopenxpmmask,
				     FALSE, FALSE);
	gtk_ctree_expand(GTK_CTREE(ctree), selected_node);
	gtk_ctree_node_set_row_data(GTK_CTREE(ctree), node, new_item);
	gtk_ctree_sort_recursive(GTK_CTREE(ctree), selected_node);

	row = gtkut_ctree_get_nth_from_node(GTK_CTREE(ctree), node);
	gtk_clist_select_row(GTK_CLIST(ctree), row, -1);
	gtkut_clist_set_focus_row(GTK_CLIST(ctree), row);
	gtk_ctree_node_moveto(GTK_CTREE(ctree), node, -1, 0.5, 0);

	folderview_append_item(new_item);
	folder_write_list();
}

static void foldersel_activated(void)
{
	gtk_button_clicked(GTK_BUTTON(ok_button));
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

static gint foldersel_clist_compare(GtkCList *clist,
				    gconstpointer ptr1, gconstpointer ptr2)
{
	FolderItem *item1 = ((GtkCListRow *)ptr1)->data;
	FolderItem *item2 = ((GtkCListRow *)ptr2)->data;

	if (!item1->name)
		return (item2->name != NULL);
	if (!item2->name)
		return -1;

	return g_strcasecmp(item1->name, item2->name);
}
