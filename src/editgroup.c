/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2007 Hiroyuki Yamamoto
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
	GROUP_COL_REMARKS = 2
} GroupEditEMailColumnPos;

#define GROUP_N_COLS          3
#define GROUP_COL_WIDTH_NAME  140
#define GROUP_COL_WIDTH_EMAIL 120

static struct _GroupEdit_dlg {
	GtkWidget *window;
	GtkWidget *hbbox;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	GtkWidget *statusbar;
	gint status_cid;

	/* Basic data tab */
	GtkWidget *entry_name;
	GtkCList *clist_avail;
	GtkCList *clist_group;

	GHashTable *hashEMail;
	gint rowIndGroup;
	gint rowIndAvail;

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

static gchar *edit_group_format_item_clist( ItemPerson *person, ItemEMail *email ) {
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

static gint edit_group_clist_add_email( GtkCList *clist, ItemEMail *email ) {
	ItemPerson *person = ( ItemPerson * ) ADDRITEM_PARENT(email);
	gchar *str = edit_group_format_item_clist( person, email );
	gchar *text[ GROUP_N_COLS ];
	gint row;
	if( str ) {
		text[ GROUP_COL_NAME ] = str;
	}
	else {
		text[ GROUP_COL_NAME ] = ADDRITEM_NAME(person);
	}
	text[ GROUP_COL_EMAIL   ] = email->address;
	text[ GROUP_COL_REMARKS ] = email->remarks;
	row = gtk_clist_append( clist, text );
	gtk_clist_set_row_data( clist, row, email );
	return row;
}

static void edit_group_load_clist( GtkCList *clist, GList *listEMail ) {
	GList *node = listEMail;
	gtk_clist_freeze( clist );
	while( node ) {
		ItemEMail *email = node->data;
		edit_group_clist_add_email( clist, email );
		node = g_list_next( node );
	}
	gtk_clist_thaw( clist );
}

static void edit_group_group_selected( GtkCList *clist, gint row, gint column, GdkEvent *event, gpointer data ) {
	groupeditdlg.rowIndGroup = row;
}

static void edit_group_avail_selected( GtkCList *clist, gint row, gint column, GdkEvent *event, gpointer data ) {
	groupeditdlg.rowIndAvail = row;
}

static gint edit_group_move_email( GtkCList *clist_from, GtkCList *clist_to, gint row ) {
	ItemEMail *email = gtk_clist_get_row_data( clist_from, row );
	gint rrow = -1;
	if( email ) {
		gtk_clist_remove( clist_from, row );
		rrow = edit_group_clist_add_email( clist_to, email );
		gtk_clist_select_row( clist_to, rrow, 0 );
		gtkut_clist_set_focus_row( clist_to, rrow );
	}
	return rrow;
}

static void edit_group_to_group( GtkWidget *widget, gpointer data ) {
	groupeditdlg.rowIndGroup = edit_group_move_email( groupeditdlg.clist_avail,
					groupeditdlg.clist_group, groupeditdlg.rowIndAvail );
}

static void edit_group_to_avail( GtkWidget *widget, gpointer data ) {
	groupeditdlg.rowIndAvail = edit_group_move_email( groupeditdlg.clist_group,
					groupeditdlg.clist_avail, groupeditdlg.rowIndGroup );
}

static gboolean edit_group_list_group_button( GtkCList *clist, GdkEventButton *event, gpointer data ) {
	if( ! event ) return FALSE;
	if( event->button == 1 ) {
		if( event->type == GDK_2BUTTON_PRESS ) {
			edit_group_to_avail( NULL, NULL );
		}
	}
	return FALSE;
}

static gboolean edit_group_list_avail_button( GtkCList *clist, GdkEventButton *event, gpointer data ) {
	if( ! event ) return FALSE;
	if( event->button == 1 ) {
		if( event->type == GDK_2BUTTON_PRESS ) {
			edit_group_to_group( NULL, NULL );
		}
	}
	return FALSE;
}

static gint edit_group_list_compare_func( GtkCList *clist, gconstpointer ptr1, gconstpointer ptr2 ) {
	GtkCell *cell1 = ((GtkCListRow *)ptr1)->cell;
	GtkCell *cell2 = ((GtkCListRow *)ptr2)->cell;
	gchar *name1 = NULL, *name2 = NULL;
	if( cell1 ) name1 = cell1->u.text;
	if( cell2 ) name2 = cell2->u.text;
	if( ! name1 ) return ( name2 != NULL );
	if( ! name2 ) return -1;
	return g_ascii_strcasecmp( name1, name2 );
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

	GtkWidget *clist_swin;
	GtkWidget *clist_avail;
	GtkWidget *clist_group;

	GtkWidget *button_add;
	GtkWidget *button_remove;
	gint top;

	gchar *titles[ GROUP_N_COLS ];
	gint i;

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

	clist_swin = gtk_scrolled_window_new( NULL, NULL );
	gtk_box_pack_start(GTK_BOX(vboxl), clist_swin, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(clist_swin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_ALWAYS);

	clist_avail = gtk_clist_new_with_titles( GROUP_N_COLS, titles );
	gtk_container_add( GTK_CONTAINER(clist_swin), clist_avail );
	gtk_clist_set_selection_mode( GTK_CLIST(clist_avail), GTK_SELECTION_BROWSE );
	gtk_clist_set_column_width( GTK_CLIST(clist_avail), GROUP_COL_NAME, GROUP_COL_WIDTH_NAME );
	gtk_clist_set_column_width( GTK_CLIST(clist_avail), GROUP_COL_EMAIL, GROUP_COL_WIDTH_EMAIL );
	gtk_clist_set_compare_func( GTK_CLIST(clist_avail), edit_group_list_compare_func );
	gtk_clist_set_auto_sort( GTK_CLIST(clist_avail), TRUE );
	gtkut_clist_set_redraw( GTK_CLIST(clist_avail) );

	for( i = 0; i < GROUP_N_COLS; i++ )
		GTK_WIDGET_UNSET_FLAGS(GTK_CLIST(clist_avail)->column[i].button, GTK_CAN_FOCUS);

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

	clist_swin = gtk_scrolled_window_new( NULL, NULL );
	gtk_box_pack_start(GTK_BOX(vboxl), clist_swin, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(clist_swin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_ALWAYS);

	clist_group = gtk_clist_new_with_titles( GROUP_N_COLS, titles );
	gtk_container_add( GTK_CONTAINER(clist_swin), clist_group );
	gtk_clist_set_selection_mode( GTK_CLIST(clist_group), GTK_SELECTION_BROWSE );
	gtk_clist_set_column_width( GTK_CLIST(clist_group), GROUP_COL_NAME, GROUP_COL_WIDTH_NAME );
	gtk_clist_set_column_width( GTK_CLIST(clist_group), GROUP_COL_EMAIL, GROUP_COL_WIDTH_EMAIL );
	gtk_clist_set_compare_func( GTK_CLIST(clist_group), edit_group_list_compare_func );
	gtk_clist_set_auto_sort( GTK_CLIST(clist_group), TRUE );
	gtkut_clist_set_redraw( GTK_CLIST(clist_group) );

	for( i = 0; i < GROUP_N_COLS; i++ )
		GTK_WIDGET_UNSET_FLAGS(GTK_CLIST(clist_group)->column[i].button, GTK_CAN_FOCUS);

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
	g_signal_connect(G_OBJECT(clist_group), "select_row",
			 G_CALLBACK( edit_group_group_selected), NULL);
	g_signal_connect(G_OBJECT(clist_avail), "select_row",
			 G_CALLBACK( edit_group_avail_selected), NULL);
	g_signal_connect(G_OBJECT(button_add), "clicked",
			 G_CALLBACK( edit_group_to_group ), NULL);
	g_signal_connect(G_OBJECT(button_remove), "clicked",
			 G_CALLBACK( edit_group_to_avail ), NULL);
	g_signal_connect(G_OBJECT(clist_avail), "button_press_event",
			 G_CALLBACK(edit_group_list_avail_button), NULL);
	g_signal_connect(G_OBJECT(clist_group), "button_press_event",
			 G_CALLBACK(edit_group_list_group_button), NULL);

	groupeditdlg.window     = window;
	groupeditdlg.hbbox      = hbbox;
	groupeditdlg.ok_btn     = ok_btn;
	groupeditdlg.cancel_btn = cancel_btn;
	groupeditdlg.statusbar  = statusbar;
	groupeditdlg.status_cid = gtk_statusbar_get_context_id( GTK_STATUSBAR(statusbar), "Edit Group Dialog" );

	groupeditdlg.entry_name  = entry_name;
	groupeditdlg.clist_group = GTK_CLIST( clist_group );
	groupeditdlg.clist_avail = GTK_CLIST( clist_avail );

	if( ! _edit_group_dfl_message_ ) {
		_edit_group_dfl_message_ = _( "Move E-Mail Addresses to or from Group with arrow buttons" );
	}
}

/*
* Return list of email items.
*/
static GList *edit_group_build_email_list() {
	GtkCList *clist = GTK_CLIST(groupeditdlg.clist_group);
	GList *listEMail = NULL;
	ItemEMail *email;
	gint row = 0;
	while( (email = gtk_clist_get_row_data( clist, row )) ) {
		listEMail = g_list_append( listEMail, email );
		row++;
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

	if (!groupeditdlg.window)
		addressbook_edit_group_create(&cancelled);
	gtkut_box_set_reverse_order(GTK_BOX(groupeditdlg.hbbox),
				    !prefs_common.comply_gnome_hig);
	gtk_widget_grab_focus(groupeditdlg.ok_btn);
	gtk_widget_grab_focus(groupeditdlg.entry_name);
	manage_window_set_transient(GTK_WINDOW(groupeditdlg.window));
	gtk_widget_show(groupeditdlg.window);

	/* Clear all fields */
	groupeditdlg.rowIndGroup = -1;
	groupeditdlg.rowIndAvail = -1;
	edit_group_status_show( "" );
	gtk_clist_clear( GTK_CLIST(groupeditdlg.clist_group) );
	gtk_clist_clear( GTK_CLIST(groupeditdlg.clist_avail) );

	if( group ) {
		if( ADDRITEM_NAME(group) )
			gtk_entry_set_text(GTK_ENTRY(groupeditdlg.entry_name), ADDRITEM_NAME(group) );
		edit_group_load_clist( groupeditdlg.clist_group, group->listEMail );
		gtk_window_set_title( GTK_WINDOW(groupeditdlg.window), _("Edit Group Details"));
	}
	else {
		gtk_window_set_title( GTK_WINDOW(groupeditdlg.window), _("Add New Group"));
		gtk_entry_set_text(GTK_ENTRY(groupeditdlg.entry_name), ADDRESSBOOK_GUESS_GROUP_NAME );
	}

	listEMail = addrbook_get_available_email_list( abf, group );
	edit_group_load_clist( groupeditdlg.clist_avail, listEMail );
	mgu_clear_list( listEMail );
	g_list_free( listEMail );
	listEMail = NULL;
	gtk_clist_select_row( groupeditdlg.clist_group, 0, 0 );
	gtkut_clist_set_focus_row( groupeditdlg.clist_group, 0 );
	gtk_clist_select_row( groupeditdlg.clist_avail, 0, 0 );
	gtkut_clist_set_focus_row( groupeditdlg.clist_avail, 0 );

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

