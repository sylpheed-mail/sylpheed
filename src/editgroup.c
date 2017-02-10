/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2017 Hiroyuki Yamamoto
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
#include <gtk/gtk.h>

#include "addressbook.h"
#include "addressitem.h"
#include "addrbook.h"
#include "addritem.h"

#include "mgutils.h"

#include "prefs_common.h"

#include "alertpanel.h"
#include "inputdialog.h"
#include "manage_window.h"
#include "gtkutils.h"

#define ADDRESSBOOK_GUESS_FOLDER_NAME	"NewFolder"
#define ADDRESSBOOK_GUESS_GROUP_NAME	"NewGroup"

#define EDITGROUP_WIDTH      600
#define EDITGROUP_HEIGHT     340

typedef enum {
	GROUP_COL_NAME    = 0,
	GROUP_COL_EMAIL   = 1,
	GROUP_COL_REMARKS = 2,
	GROUP_COL_DATA    = 3,
	GROUP_N_COLS
} GroupEditEMailColumns;

#define GROUP_COL_WIDTH_NAME    160
#define GROUP_COL_WIDTH_EMAIL   140
#define GROUP_COL_WIDTH_REMARKS 80

static struct _GroupEdit_dlg {
	GtkWidget *window;
	GtkWidget *hbbox;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	GtkWidget *statusbar;
	gint status_cid;

	/* Basic data tab */
	GtkWidget *entry_name;
	GtkTreeView *treeview_avail;
	GtkTreeView *treeview_group;
} groupeditdlg;


static gchar *_edit_group_dfl_message_ = NULL;

static void edit_group_status_show( gchar *msg ) {
	if( groupeditdlg.statusbar != NULL ) {
		gtk_statusbar_pop( GTK_STATUSBAR(groupeditdlg.statusbar), groupeditdlg.status_cid );
		if( msg ) {
			gtk_statusbar_push( GTK_STATUSBAR(groupeditdlg.statusbar), groupeditdlg.status_cid, msg );
		}
	}
}

static void edit_group_ok(GtkWidget *widget, gboolean *cancelled) {
	gchar *sName;
	gboolean errFlag = TRUE;

	sName = gtk_editable_get_chars( GTK_EDITABLE(groupeditdlg.entry_name), 0, -1 );
	if( sName ) {
		g_strstrip( sName );
		if( *sName != '\0' ) {
			gtk_entry_set_text(GTK_ENTRY(groupeditdlg.entry_name), sName );
			*cancelled = FALSE;
			gtk_main_quit();
			errFlag = FALSE;
		}
	}
	if( errFlag ) {
		edit_group_status_show( _( "A Group Name must be supplied." ) );
	}
	g_free( sName );
}
	
static void edit_group_cancel(GtkWidget *widget, gboolean *cancelled) {
	*cancelled = TRUE;
	gtk_main_quit();
}

static gint edit_group_delete_event(GtkWidget *widget, GdkEventAny *event, gboolean *cancelled) {
	*cancelled = TRUE;
	gtk_main_quit();
	return TRUE;
}

static gboolean edit_group_key_pressed(GtkWidget *widget, GdkEventKey *event, gboolean *cancelled) {
	if (event && event->keyval == GDK_Escape) {
		*cancelled = TRUE;
		gtk_main_quit();
	}
	return FALSE;
}

static gchar *edit_group_format_item( ItemPerson *person, ItemEMail *email ) {
	gchar *str = NULL;
	gchar *aName = ADDRITEM_NAME(email);
	if( aName == NULL || *aName == '\0' ) return str;
	if( person ) {
		str = g_strdup_printf( "%s - %s", ADDRITEM_NAME(person), aName );
	}
	else {
		str = g_strdup( aName );
	}
	return str;
}

static void edit_group_list_add_email( GtkTreeView *treeview, ItemEMail *email ) {
	ItemPerson *person = ( ItemPerson * ) ADDRITEM_PARENT(email);
	gchar *str = edit_group_format_item( person, email );
	gchar *text[ GROUP_N_COLS ];
	GtkTreeModel *model;
	GtkTreeStore *store;
	GtkTreeIter iter;

	model = gtk_tree_view_get_model(treeview);
	store = GTK_TREE_STORE(model);

	if( str ) {
		text[ GROUP_COL_NAME ] = str;
	}
	else {
		text[ GROUP_COL_NAME ] = ADDRITEM_NAME(person);
	}
	text[ GROUP_COL_EMAIL   ] = email->address;
	text[ GROUP_COL_REMARKS ] = email->remarks;
	gtk_tree_store_append(store, &iter, NULL);
	gtk_tree_store_set(store, &iter, GROUP_COL_NAME, text[ GROUP_COL_NAME ], GROUP_COL_EMAIL, text[ GROUP_COL_EMAIL ], GROUP_COL_REMARKS, text[ GROUP_COL_REMARKS ], GROUP_COL_DATA, email, -1);
}

static void edit_group_load_list( GtkTreeView *treeview, GList *listEMail ) {
	GList *node = listEMail;
	while( node ) {
		ItemEMail *email = node->data;
		edit_group_list_add_email( treeview, email );
		node = g_list_next( node );
	}
}

static void edit_group_move_email( GtkTreeView *treeview_from, GtkTreeView *treeview_to ) {
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GList *rows;
	GList *cur;
	GtkTreePath *path;
	ItemEMail *email;

	model = gtk_tree_view_get_model(treeview_from);
	selection = gtk_tree_view_get_selection(treeview_from);
	rows = gtk_tree_selection_get_selected_rows(selection, NULL);
	rows = g_list_reverse(rows);

	for (cur = rows; cur != NULL; cur = cur->next) {
		path = (GtkTreePath *)cur->data;
		gtk_tree_model_get_iter(model, &iter, path);
		gtk_tree_model_get(model, &iter, GROUP_COL_DATA, &email, -1);
		edit_group_list_add_email(treeview_to, email);
		gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
		if (cur->next == NULL) {
			if (gtk_tree_store_iter_is_valid(GTK_TREE_STORE(model), &iter)) {
				gtk_tree_selection_select_iter(selection, &iter);
			} else {
				if (gtkut_tree_model_get_iter_last(model, &iter)) {
					gtk_tree_selection_select_iter(selection, &iter);
				}
			}
		}
		gtk_tree_path_free(path);
	}
	g_list_free(rows);
}

static void edit_group_to_group( GtkWidget *widget, gpointer data ) {
	edit_group_move_email( groupeditdlg.treeview_avail, groupeditdlg.treeview_group );
}

static void edit_group_to_avail( GtkWidget *widget, gpointer data ) {
	edit_group_move_email( groupeditdlg.treeview_group, groupeditdlg.treeview_avail );
}

static gboolean edit_group_list_group_button( GtkWidget *treeview, GdkEventButton *event, gpointer data ) {
	if( ! event ) return FALSE;
	if( event->button == 1 ) {
		if( event->type == GDK_2BUTTON_PRESS ) {
			edit_group_to_avail( NULL, NULL );
		}
	}
	return FALSE;
}

static gboolean edit_group_list_avail_button( GtkWidget *treeview, GdkEventButton *event, gpointer data ) {
	if( ! event ) return FALSE;
	if( event->button == 1 ) {
		if( event->type == GDK_2BUTTON_PRESS ) {
			edit_group_to_group( NULL, NULL );
		}
	}
	return FALSE;
}

static gint edit_group_list_col_compare(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gint col)
{
	gchar *name1 = NULL, *name2 = NULL;
	gint ret;

	gtk_tree_model_get(model, a, col, &name1, -1);
	gtk_tree_model_get(model, b, col, &name2, -1);

	if (!name1) {
		name1 = g_strdup("");
	}
	if (!name2) {
		name2 = g_strdup("");
	}

	ret = g_ascii_strcasecmp(name1, name2);
	g_free(name2);
	g_free(name1);

	return ret;
}

static gint edit_group_list_name_compare_func(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer data) {
	gint ret;

	ret = edit_group_list_col_compare(model, a, b, GROUP_COL_NAME);
	if (ret == 0)
		ret = edit_group_list_col_compare(model, a, b, GROUP_COL_EMAIL);
	if (ret == 0)
		ret = edit_group_list_col_compare(model, a, b, GROUP_COL_REMARKS);

	return ret;
}

static gint edit_group_list_email_compare_func(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer data) {
	gint ret;

	ret = edit_group_list_col_compare(model, a, b, GROUP_COL_EMAIL);
	if (ret == 0)
		ret = edit_group_list_col_compare(model, a, b, GROUP_COL_NAME);
	if (ret == 0)
		ret = edit_group_list_col_compare(model, a, b, GROUP_COL_REMARKS);

	return ret;
}

static gint edit_group_list_remarks_compare_func(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer data) {
	gint ret;

	ret = edit_group_list_col_compare(model, a, b, GROUP_COL_REMARKS);
	if (ret == 0)
		ret = edit_group_list_col_compare(model, a, b, GROUP_COL_NAME);
	if (ret == 0)
		ret = edit_group_list_col_compare(model, a, b, GROUP_COL_EMAIL);

	return ret;
}

static void addressbook_edit_group_create( gboolean *cancelled ) {
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *vbox1;
	GtkWidget *hbbox;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	GtkWidget *hsbox;
	GtkWidget *statusbar;

	GtkWidget *hboxg;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *entry_name;
	GtkWidget *hboxl;
	GtkWidget *vboxl;
	GtkWidget *hboxh;
	GtkWidget *vboxb;
	GtkWidget *vboxb1;
	GtkWidget *hboxb;

	GtkWidget *treeview_swin;
	GtkWidget *treeview_avail;
	GtkWidget *treeview_group;
	GtkTreeStore *store;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeSelection *selection;

	GtkWidget *button_add;
	GtkWidget *button_remove;
	gint top;

	gchar *titles[ GROUP_N_COLS ];

	titles[ GROUP_COL_NAME    ] = _( "Name" );
	titles[ GROUP_COL_EMAIL   ] = _("E-Mail Address");
	titles[ GROUP_COL_REMARKS ] = _("Remarks");

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_size_request(window, EDITGROUP_WIDTH, EDITGROUP_HEIGHT);
	gtk_window_set_title(GTK_WINDOW(window), _("Edit Group Data"));
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);	
	gtk_widget_realize(window);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(edit_group_delete_event),
			 cancelled);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(edit_group_key_pressed),
			 cancelled);

	vbox = gtk_vbox_new( FALSE, 4 );
	gtk_container_add( GTK_CONTAINER( window ), vbox );

	vbox1 = gtk_vbox_new( FALSE, 10 );
	gtk_box_pack_start(GTK_BOX(vbox), vbox1, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox1), 6);

	/* Group area */
	hboxg = gtk_hbox_new( FALSE, 0 );
	gtk_box_pack_start(GTK_BOX(vbox1), hboxg, FALSE, FALSE, 0);

	/* Data entry area */
	table = gtk_table_new( 1, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(hboxg), table, TRUE, TRUE, 0);
	gtk_table_set_row_spacings(GTK_TABLE(table), 0);
	gtk_table_set_col_spacings(GTK_TABLE(table), 4);

	/* First row */
	top = 0;
	label = gtk_label_new(_("Group Name"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, top, (top + 1), GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	entry_name = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), entry_name, 1, 2, top, (top + 1), GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);

	/* List area */
	hboxl = gtk_hbox_new( FALSE, 8 );
	gtk_box_pack_start(GTK_BOX(vbox1), hboxl, TRUE, TRUE, 0);

	/* Available list */
	vboxl = gtk_vbox_new( FALSE, 4 );
	gtk_box_pack_start(GTK_BOX(hboxl), vboxl, TRUE, TRUE, 0);

	hboxh = gtk_hbox_new( FALSE, 0 );
	gtk_box_pack_start(GTK_BOX(vboxl), hboxh, FALSE, FALSE, 0);
	label = gtk_label_new(_("Available Addresses"));
	gtk_box_pack_end(GTK_BOX(hboxh), label, TRUE, TRUE, 0);

	treeview_swin = gtk_scrolled_window_new( NULL, NULL );
	gtk_box_pack_start(GTK_BOX(vboxl), treeview_swin, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(treeview_swin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_ALWAYS);

	store = gtk_tree_store_new(GROUP_N_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store),
					GROUP_COL_NAME,
					edit_group_list_name_compare_func,
					NULL, NULL);
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store),
					GROUP_COL_EMAIL,
					edit_group_list_email_compare_func,
					NULL, NULL);
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store),
					GROUP_COL_REMARKS,
					edit_group_list_remarks_compare_func,
					NULL, NULL);

	treeview_avail = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(G_OBJECT(store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview_avail), TRUE);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview_avail), FALSE);
	gtk_container_add(GTK_CONTAINER(treeview_swin), treeview_avail);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview_avail));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, "ypad", 0, NULL);
	column = gtk_tree_view_column_new_with_attributes(titles[GROUP_COL_NAME], renderer, "text", GROUP_COL_NAME, NULL);
	gtk_tree_view_column_set_spacing(column, 1);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, GROUP_COL_WIDTH_NAME);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column, GROUP_COL_NAME);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_avail), column);
	gtk_tree_view_set_expander_column(GTK_TREE_VIEW(treeview_avail), column);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, "ypad", 0, NULL);
	column = gtk_tree_view_column_new_with_attributes(titles[GROUP_COL_EMAIL], renderer, "text", GROUP_COL_EMAIL, NULL);
	gtk_tree_view_column_set_spacing(column, 1);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, GROUP_COL_WIDTH_EMAIL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column, GROUP_COL_EMAIL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_avail), column);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, "ypad", 0, NULL);
	column = gtk_tree_view_column_new_with_attributes(titles[GROUP_COL_REMARKS], renderer, "text", GROUP_COL_REMARKS, NULL);
	gtk_tree_view_column_set_spacing(column, 1);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, GROUP_COL_WIDTH_REMARKS);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column, GROUP_COL_REMARKS);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_avail), column);

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store),
					     GROUP_COL_NAME, GTK_SORT_ASCENDING);

	/* Add/Remove button */
	vboxb = gtk_vbox_new( FALSE, 0 );
	gtk_box_pack_start(GTK_BOX(hboxl), vboxb, FALSE, FALSE, 0);

	vboxb1 = gtk_vbox_new( FALSE, 8 );
	gtk_box_pack_start(GTK_BOX(vboxb), vboxb1, TRUE, FALSE, 0);

	button_add = gtk_button_new_with_label( _( "  ->  " ) );
	gtk_box_pack_start(GTK_BOX(vboxb1), button_add, FALSE, FALSE, 0);

	button_remove = gtk_button_new_with_label( _( "  <-  " ) );
	gtk_box_pack_start(GTK_BOX(vboxb1), button_remove, FALSE, FALSE, 0);

	/* Group list */
	vboxl = gtk_vbox_new( FALSE, 4 );
	gtk_box_pack_start(GTK_BOX(hboxl), vboxl, TRUE, TRUE, 0);

	hboxh = gtk_hbox_new( FALSE, 0 );
	gtk_box_pack_start(GTK_BOX(vboxl), hboxh, FALSE, FALSE, 0);
	label = gtk_label_new(_("Addresses in Group"));
	gtk_box_pack_start(GTK_BOX(hboxh), label, TRUE, TRUE, 0);

	treeview_swin = gtk_scrolled_window_new( NULL, NULL );
	gtk_box_pack_start(GTK_BOX(vboxl), treeview_swin, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(treeview_swin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_ALWAYS);

	store = gtk_tree_store_new(GROUP_N_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store),
					GROUP_COL_NAME,
					edit_group_list_name_compare_func,
					NULL, NULL);
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store),
					GROUP_COL_EMAIL,
					edit_group_list_email_compare_func,
					NULL, NULL);
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store),
					GROUP_COL_REMARKS,
					edit_group_list_remarks_compare_func,
					NULL, NULL);

	treeview_group = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(G_OBJECT(store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview_group), TRUE);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview_group), FALSE);
	gtk_container_add(GTK_CONTAINER(treeview_swin), treeview_group);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview_group));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, "ypad", 0, NULL);
	column = gtk_tree_view_column_new_with_attributes(titles[GROUP_COL_NAME], renderer, "text", GROUP_COL_NAME, NULL);
	gtk_tree_view_column_set_spacing(column, 1);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, GROUP_COL_WIDTH_NAME);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column, GROUP_COL_NAME);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_group), column);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, "ypad", 0, NULL);
	column = gtk_tree_view_column_new_with_attributes(titles[GROUP_COL_EMAIL], renderer, "text", GROUP_COL_EMAIL, NULL);
	gtk_tree_view_column_set_spacing(column, 1);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, GROUP_COL_WIDTH_EMAIL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column, GROUP_COL_EMAIL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_group), column);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, "ypad", 0, NULL);
	column = gtk_tree_view_column_new_with_attributes(titles[GROUP_COL_REMARKS], renderer, "text", GROUP_COL_REMARKS, NULL);
	gtk_tree_view_column_set_spacing(column, 1);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, GROUP_COL_WIDTH_REMARKS);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column, GROUP_COL_REMARKS);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_group), column);

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store),
					     GROUP_COL_NAME, GTK_SORT_ASCENDING);
	/* Button panel */
	hboxb = gtk_hbox_new(FALSE, 8);
	gtk_box_pack_start(GTK_BOX(vbox1), hboxb, FALSE, FALSE, 0);

	gtkut_stock_button_set_create(&hbbox, &ok_btn, GTK_STOCK_OK,
				      &cancel_btn, GTK_STOCK_CANCEL,
				      NULL, NULL);
	gtk_box_pack_end(GTK_BOX(hboxb), hbbox, FALSE, FALSE, 0);
	gtk_widget_grab_default(ok_btn);

	g_signal_connect(G_OBJECT(ok_btn), "clicked",
			 G_CALLBACK(edit_group_ok), cancelled);
	g_signal_connect(G_OBJECT(cancel_btn), "clicked",
			 G_CALLBACK(edit_group_cancel), cancelled);

	/* Status line */
	hsbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), hsbox, FALSE, FALSE, 0);
	statusbar = gtk_statusbar_new();
	gtk_box_pack_start(GTK_BOX(hsbox), statusbar, TRUE, TRUE, 0);

	gtk_widget_show_all(vbox);

	/* Event handlers */
	g_signal_connect(G_OBJECT(button_add), "clicked",
			 G_CALLBACK( edit_group_to_group ), NULL);
	g_signal_connect(G_OBJECT(button_remove), "clicked",
			 G_CALLBACK( edit_group_to_avail ), NULL);
	g_signal_connect(G_OBJECT(treeview_avail), "button_press_event",
			 G_CALLBACK(edit_group_list_avail_button), NULL);
	g_signal_connect(G_OBJECT(treeview_group), "button_press_event",
			 G_CALLBACK(edit_group_list_group_button), NULL);

	groupeditdlg.window     = window;
	groupeditdlg.hbbox      = hbbox;
	groupeditdlg.ok_btn     = ok_btn;
	groupeditdlg.cancel_btn = cancel_btn;
	groupeditdlg.statusbar  = statusbar;
	groupeditdlg.status_cid = gtk_statusbar_get_context_id( GTK_STATUSBAR(statusbar), "Edit Group Dialog" );

	groupeditdlg.entry_name  = entry_name;
	groupeditdlg.treeview_group = GTK_TREE_VIEW( treeview_group );
	groupeditdlg.treeview_avail = GTK_TREE_VIEW( treeview_avail );

	if( ! _edit_group_dfl_message_ ) {
		_edit_group_dfl_message_ = _( "Move E-Mail Addresses to or from Group with arrow buttons" );
	}
}

/*
* Return list of email items.
*/
static GList *edit_group_build_email_list() {
	GtkTreeView *treeview = groupeditdlg.treeview_group;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean valid;
	GList *listEMail = NULL;
	ItemEMail *email;

	model = gtk_tree_view_get_model(treeview);
	valid = gtk_tree_model_get_iter_first(model, &iter);
	while (valid) {
		gtk_tree_model_get(model, &iter, GROUP_COL_DATA, &email, -1);
		listEMail = g_list_append( listEMail, email );
		valid = gtk_tree_model_iter_next(model, &iter);
	}

	return listEMail;
}

/*
* Edit group.
* Enter: abf    Address book.
*        folder Parent folder for group (or NULL if adding to root folder). Argument is
*               only required for new objects).
*        group  Group to edit, or NULL for a new group object.
* Return: Edited object, or NULL if cancelled.
*/
ItemGroup *addressbook_edit_group( AddressBookFile *abf, ItemFolder *parent, ItemGroup *group ) {
	static gboolean cancelled;
	GList *listEMail = NULL;
	gchar *name;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;

	if (!groupeditdlg.window)
		addressbook_edit_group_create(&cancelled);
	gtkut_box_set_reverse_order(GTK_BOX(groupeditdlg.hbbox),
				    !prefs_common.comply_gnome_hig);
	gtk_widget_grab_focus(groupeditdlg.ok_btn);
	gtk_widget_grab_focus(groupeditdlg.entry_name);
	manage_window_set_transient(GTK_WINDOW(groupeditdlg.window));
	gtk_widget_show(groupeditdlg.window);

	/* Clear all fields */
	edit_group_status_show( "" );
	model = gtk_tree_view_get_model(groupeditdlg.treeview_group);
	gtk_tree_store_clear(GTK_TREE_STORE(model));
	model = gtk_tree_view_get_model(groupeditdlg.treeview_avail);
	gtk_tree_store_clear(GTK_TREE_STORE(model));

	if( group ) {
		if( ADDRITEM_NAME(group) )
			gtk_entry_set_text(GTK_ENTRY(groupeditdlg.entry_name), ADDRITEM_NAME(group) );
		edit_group_load_list( groupeditdlg.treeview_group, group->listEMail );
		gtk_window_set_title( GTK_WINDOW(groupeditdlg.window), _("Edit Group Details"));
	}
	else {
		gtk_window_set_title( GTK_WINDOW(groupeditdlg.window), _("Add New Group"));
		gtk_entry_set_text(GTK_ENTRY(groupeditdlg.entry_name), ADDRESSBOOK_GUESS_GROUP_NAME );
	}

	listEMail = addrbook_get_available_email_list( abf, group );
	edit_group_load_list( groupeditdlg.treeview_avail, listEMail );
	mgu_clear_list( listEMail );
	g_list_free( listEMail );
	listEMail = NULL;

	model = gtk_tree_view_get_model(groupeditdlg.treeview_group);
	selection = gtk_tree_view_get_selection(groupeditdlg.treeview_group);
	if (gtk_tree_model_get_iter_first(model, &iter)) {
		gtk_tree_selection_select_iter(selection, &iter);
	}
	model = gtk_tree_view_get_model(groupeditdlg.treeview_avail);
	selection = gtk_tree_view_get_selection(groupeditdlg.treeview_avail);
	if (gtk_tree_model_get_iter_first(model, &iter)) {
		gtk_tree_selection_select_iter(selection, &iter);
	}

	edit_group_status_show( _edit_group_dfl_message_ );

	gtk_main();
	gtk_widget_hide( groupeditdlg.window );

	if( cancelled ) {
		return NULL;
	}

	listEMail = edit_group_build_email_list();
	if( group ) {
		/* Update email list */
		addrbook_update_group_list( abf, group, listEMail );
	}
	else {
		/* Create new person and email list */
		group = addrbook_add_group_list( abf, parent, listEMail );
	}
	name = gtk_editable_get_chars( GTK_EDITABLE(groupeditdlg.entry_name), 0, -1 );
	addritem_group_set_name( group, name );
	g_free( name );

	listEMail = NULL;
	return group;
}

/*
* Edit folder.
* Enter: abf    Address book.
*        parent Parent folder for folder (or NULL if adding to root folder). Argument is
*               only required for new objects).
*        folder	Folder to edit, or NULL for a new folder object.
* Return: Edited object, or NULL if cancelled.
*/
ItemFolder *addressbook_edit_folder( AddressBookFile *abf, ItemFolder *parent, ItemFolder *folder ) {
	gchar *name = NULL;

	if( folder ) {
		name = g_strdup( ADDRITEM_NAME(folder) );
		name = input_dialog( _("Edit folder"), _("Input the new name of folder:"), name );
	}
	else {
		name = input_dialog( _("New folder"),
				_("Input the name of new folder:"),
				_(ADDRESSBOOK_GUESS_FOLDER_NAME) );
	}
	if( ! name ) return NULL;
	g_strstrip( name );
	if( *name == '\0' ) {
		g_free( name );
		return NULL;
	}
	if( folder ) {
		if( g_ascii_strcasecmp( name, ADDRITEM_NAME(folder) ) == 0 ) {
			g_free( name );
			return NULL;
		}
	}

	if( ! folder ) {
		folder = addrbook_add_new_folder( abf, parent );
	}
	addritem_folder_set_name( folder, name );
	g_free( name );
	return folder;
}

/*
* End of Source.
*/

