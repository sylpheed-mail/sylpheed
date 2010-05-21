/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2001 Match Grun
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

/*
 * Edit LDAP address book data.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_LDAP

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtksignal.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtktable.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkstatusbar.h>
#include <gtk/gtkspinbutton.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtkoptionmenu.h>

#include "addressbook.h"
#include "prefs_common.h"
#include "addressitem.h"
#include "mgutils.h"
#include "syldap.h"
#include "editldap_basedn.h"
#include "manage_window.h"
#include "gtkutils.h"

#define ADDRESSBOOK_GUESS_LDAP_NAME	"MyServer"
#define ADDRESSBOOK_GUESS_LDAP_SERVER	"localhost"

#define LDAPEDIT_TABLE_ROWS	6
#define LDAPEDIT_TABLE_COLS	3

static struct _LDAPEdit {
	GtkWidget *window;
	GtkWidget *notebook;
	GtkWidget *hbbox;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	GtkWidget *statusbar;
	gint status_cid;
	GtkWidget *entry_name;
	GtkWidget *entry_server;
	GtkWidget *spinbtn_port;
	GtkWidget *entry_baseDN;
	GtkWidget *spinbtn_timeout;
	GtkWidget *entry_bindDN;
	GtkWidget *entry_bindPW;
	GtkWidget *entry_criteria;
	GtkWidget *spinbtn_maxentry;
} ldapedit;

/*
* Edit functions.
*/
static void edit_ldap_status_show( gchar *msg ) {
	if( ldapedit.statusbar != NULL ) {
		gtk_statusbar_pop( GTK_STATUSBAR(ldapedit.statusbar), ldapedit.status_cid );
		if( msg ) {
			gtk_statusbar_push( GTK_STATUSBAR(ldapedit.statusbar), ldapedit.status_cid, msg );
		}
	}
}

static void edit_ldap_ok( GtkWidget *widget, gboolean *cancelled ) {
	*cancelled = FALSE;
	gtk_main_quit();
}

static void edit_ldap_cancel( GtkWidget *widget, gboolean *cancelled ) {
	*cancelled = TRUE;
	gtk_main_quit();
}

static gint edit_ldap_delete_event( GtkWidget *widget, GdkEventAny *event, gboolean *cancelled ) {
	*cancelled = TRUE;
	gtk_main_quit();
	return TRUE;
}

static gboolean edit_ldap_key_pressed( GtkWidget *widget, GdkEventKey *event, gboolean *cancelled ) {
	if (event && event->keyval == GDK_Escape) {
		*cancelled = TRUE;
		gtk_main_quit();
	}
	return FALSE;
}

static void edit_ldap_switch_page( GtkWidget *widget ) {
	edit_ldap_status_show( "" );
}

static void edit_ldap_server_check( void ) {
	gchar *sHost, *sBind, *sPass;
	gint iPort, iTime;
	gchar *sMsg;
	gchar *sBaseDN = NULL;
	gint iBaseDN = 0;
	gboolean flg;

	edit_ldap_status_show( "" );
	flg = FALSE;
	sHost = gtk_editable_get_chars( GTK_EDITABLE(ldapedit.entry_server), 0, -1 );
	sBind = gtk_editable_get_chars( GTK_EDITABLE(ldapedit.entry_bindDN), 0, -1 );
	sPass = gtk_editable_get_chars( GTK_EDITABLE(ldapedit.entry_bindPW), 0, -1 );
	iPort = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( ldapedit.spinbtn_port ) );
	iTime = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( ldapedit.spinbtn_timeout ) );
	g_strchomp( sHost ); g_strchug( sHost );
	g_strchomp( sBind ); g_strchug( sBind );
	g_strchomp( sPass ); g_strchug( sPass );
	if( *sHost != '\0' ) {
		/* Test connection to server */
		if( syldap_test_connect_s( sHost, iPort ) ) {
			/* Attempt to read base DN */
			GList *baseDN = syldap_read_basedn_s( sHost, iPort, sBind, sPass, iTime );
			if( baseDN ) {
				GList *node = baseDN;
				while( node ) {
					++iBaseDN;
					if( ! sBaseDN ) {
						sBaseDN = g_strdup( node->data );
					}
					node = g_list_next( node );
				}
				mgu_free_dlist( baseDN );
				baseDN = node = NULL;
			}
			flg = TRUE;
		}
	}
	g_free( sHost );
	g_free( sBind );
	g_free( sPass );

	if( sBaseDN ) {
		/* Load search DN */
		gtk_entry_set_text(GTK_ENTRY(ldapedit.entry_baseDN), sBaseDN);
		g_free( sBaseDN );
	}

	/* Display appropriate message */
	if( flg ) {
		sMsg = _( "Connected successfully to server" );
	}
	else {
		sMsg = _( "Could not connect to server" );
	}
	edit_ldap_status_show( sMsg );
}

static void edit_ldap_basedn_select( void ) {
	gchar *sHost, *sBind, *sPass, *sBase;
	gint iPort, iTime;
	gchar *selectDN;

	sHost = gtk_editable_get_chars( GTK_EDITABLE(ldapedit.entry_server), 0, -1 );
	sBase = gtk_editable_get_chars( GTK_EDITABLE(ldapedit.entry_baseDN), 0, -1 );
	sBind = gtk_editable_get_chars( GTK_EDITABLE(ldapedit.entry_bindDN), 0, -1 );
	sPass = gtk_editable_get_chars( GTK_EDITABLE(ldapedit.entry_bindPW), 0, -1 );
	iPort = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( ldapedit.spinbtn_port ) );
	iTime = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( ldapedit.spinbtn_timeout ) );
	g_strchomp( sHost ); g_strchug( sHost );
	g_strchomp( sBind ); g_strchug( sBind );
	g_strchomp( sPass ); g_strchug( sPass );
	selectDN = edit_ldap_basedn_selection( sHost, iPort, sBase, iTime, sBind, sPass );
	if( selectDN ) {
		gtk_entry_set_text(GTK_ENTRY(ldapedit.entry_baseDN), selectDN);
		g_free( selectDN );
		selectDN = NULL;
	}
	g_free( sHost );
	g_free( sBase );
	g_free( sBind );
	g_free( sPass );
}

static void edit_ldap_search_reset( void ) {
	gtk_entry_set_text(GTK_ENTRY(ldapedit.entry_criteria), SYLDAP_DFL_CRITERIA );
}

static void addressbook_edit_ldap_dialog_create( gboolean *cancelled ) {
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *notebook;
	GtkWidget *hbbox;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	GtkWidget *hsbox;
	GtkWidget *statusbar;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_size_request(window, 450, -1);
	gtk_container_set_border_width(GTK_CONTAINER(window), 0);
	gtk_window_set_title(GTK_WINDOW(window), _("Edit LDAP Server"));
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);	
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(edit_ldap_delete_event),
			 cancelled);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(edit_ldap_key_pressed),
			 cancelled);

	vbox = gtk_vbox_new( FALSE, 6 );
	/* gtk_container_set_border_width(GTK_CONTAINER(vbox), BORDER_WIDTH); */
	gtk_widget_show( vbox );
	gtk_container_add( GTK_CONTAINER( window ), vbox );

	/* Notebook */
	notebook = gtk_notebook_new();
	gtk_widget_show( notebook );
	gtk_box_pack_start( GTK_BOX( vbox ), notebook, TRUE, TRUE, 0 );
	gtk_container_set_border_width( GTK_CONTAINER( notebook ), 6 );

	/* Status line */
	hsbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), hsbox, FALSE, FALSE, 0);
	statusbar = gtk_statusbar_new();
	gtk_box_pack_start(GTK_BOX(hsbox), statusbar, TRUE, TRUE, 0);

	/* Button panel */
	gtkut_stock_button_set_create(&hbbox, &ok_btn, GTK_STOCK_OK,
				      &cancel_btn, GTK_STOCK_CANCEL,
				      NULL, NULL);
	gtk_box_pack_end(GTK_BOX(vbox), hbbox, FALSE, FALSE, 0);
	gtk_widget_grab_default(ok_btn);

	g_signal_connect(G_OBJECT(ok_btn), "clicked",
			 G_CALLBACK(edit_ldap_ok), cancelled);
	g_signal_connect(G_OBJECT(cancel_btn), "clicked",
			 G_CALLBACK(edit_ldap_cancel), cancelled);
	g_signal_connect(G_OBJECT(notebook), "switch_page",
			 G_CALLBACK(edit_ldap_switch_page), NULL );

	gtk_widget_show_all(vbox);

	ldapedit.window     = window;
	ldapedit.notebook   = notebook;
	ldapedit.hbbox      = hbbox;
	ldapedit.ok_btn     = ok_btn;
	ldapedit.cancel_btn = cancel_btn;
	ldapedit.statusbar  = statusbar;
	ldapedit.status_cid = gtk_statusbar_get_context_id( GTK_STATUSBAR(statusbar), "Edit LDAP Server Dialog" );
}

void addressbook_edit_ldap_page_basic( gint pageNum, gchar *pageLbl ) {
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *entry_name;
	GtkWidget *entry_server;
	GtkWidget *hbox_spin;
	GtkObject *spinbtn_port_adj;
	GtkWidget *spinbtn_port;
	GtkWidget *entry_baseDN;
	GtkWidget *check_btn;
	GtkWidget *lookdn_btn;
	gint top;

	vbox = gtk_vbox_new( FALSE, 8 );
	gtk_widget_show( vbox );
	gtk_container_add( GTK_CONTAINER( ldapedit.notebook ), vbox );
	/* gtk_container_set_border_width( GTK_CONTAINER (vbox), BORDER_WIDTH ); */

	label = gtk_label_new( pageLbl );
	gtk_widget_show( label );
	gtk_notebook_set_tab_label(
		GTK_NOTEBOOK( ldapedit.notebook ),
		gtk_notebook_get_nth_page( GTK_NOTEBOOK( ldapedit.notebook ), pageNum ), label );

	table = gtk_table_new( LDAPEDIT_TABLE_ROWS, LDAPEDIT_TABLE_COLS, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	gtk_container_set_border_width( GTK_CONTAINER(table), 8 );
	gtk_table_set_row_spacings(GTK_TABLE(table), 8);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	/* First row */
	top = 0;
	label = gtk_label_new(_("Name"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, top, (top + 1), GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	entry_name = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), entry_name, 1, 2, top, (top + 1), GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);

	/* Next row */
	++top;
	label = gtk_label_new(_("Hostname"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, top, (top + 1), GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	entry_server = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), entry_server, 1, 2, top, (top + 1), GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);

	/* Next row */
	++top;
	label = gtk_label_new(_("Port"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, top, (top + 1), GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	hbox_spin = gtk_hbox_new (FALSE, 8);
	spinbtn_port_adj = gtk_adjustment_new (389, 1, 65535, 100, 1000, 0);
	spinbtn_port = gtk_spin_button_new(GTK_ADJUSTMENT (spinbtn_port_adj), 1, 0);
	gtk_box_pack_start (GTK_BOX (hbox_spin), spinbtn_port, FALSE, FALSE, 0);
	gtk_widget_set_size_request (spinbtn_port, 64, -1);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_port), TRUE);
	gtk_table_attach(GTK_TABLE(table), hbox_spin, 1, 2, top, (top + 1), GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);

	check_btn = gtk_button_new_with_label( _(" Check Server "));
	gtk_table_attach(GTK_TABLE(table), check_btn, 2, 3, top, (top + 1), GTK_FILL, 0, 3, 0);

	/* Next row */
	++top;
	label = gtk_label_new(_("Search Base"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, top, (top + 1), GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	entry_baseDN = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), entry_baseDN, 1, 2, top, (top + 1), GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);

	lookdn_btn = gtk_button_new_with_label( _(" ... "));
	gtk_table_attach(GTK_TABLE(table), lookdn_btn, 2, 3, top, (top + 1), GTK_FILL, 0, 3, 0);

	g_signal_connect(G_OBJECT(check_btn), "clicked",
			 G_CALLBACK(edit_ldap_server_check), NULL);
	g_signal_connect(G_OBJECT(lookdn_btn), "clicked",
			 G_CALLBACK(edit_ldap_basedn_select), NULL);

	gtk_widget_show_all(vbox);

	ldapedit.entry_name   = entry_name;
	ldapedit.entry_server = entry_server;
	ldapedit.spinbtn_port = spinbtn_port;
	ldapedit.entry_baseDN = entry_baseDN;
}

void addressbook_edit_ldap_page_extended( gint pageNum, gchar *pageLbl ) {
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *entry_bindDN;
	GtkWidget *entry_bindPW;
	GtkWidget *entry_criteria;
	GtkWidget *hbox_spin;
	GtkObject *spinbtn_timeout_adj;
	GtkWidget *spinbtn_timeout;
	GtkObject *spinbtn_maxentry_adj;
	GtkWidget *spinbtn_maxentry;
	GtkWidget *reset_btn;
	gint top;

	vbox = gtk_vbox_new( FALSE, 8 );
	gtk_widget_show( vbox );
	gtk_container_add( GTK_CONTAINER( ldapedit.notebook ), vbox );
	/* gtk_container_set_border_width( GTK_CONTAINER (vbox), BORDER_WIDTH ); */

	label = gtk_label_new( pageLbl );
	gtk_widget_show( label );
	gtk_notebook_set_tab_label(
		GTK_NOTEBOOK( ldapedit.notebook ),
		gtk_notebook_get_nth_page( GTK_NOTEBOOK( ldapedit.notebook ), pageNum ), label );

	table = gtk_table_new( LDAPEDIT_TABLE_ROWS, LDAPEDIT_TABLE_COLS, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	gtk_container_set_border_width( GTK_CONTAINER(table), 8 );
	gtk_table_set_row_spacings(GTK_TABLE(table), 8);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	/* First row */
	top = 0;
	label = gtk_label_new(_("Search Criteria"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, top, (top + 1), GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	entry_criteria = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), entry_criteria, 1, 2, top, (top + 1), GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);

	reset_btn = gtk_button_new_with_label( _(" Reset "));
	gtk_table_attach(GTK_TABLE(table), reset_btn, 2, 3, top, (top + 1), GTK_FILL, 0, 3, 0);

	/* Next row */
	++top;
	label = gtk_label_new(_("Bind DN"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, top, (top + 1), GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	entry_bindDN = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), entry_bindDN, 1, 2, top, (top + 1), GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);

	/* Next row */
	++top;
	label = gtk_label_new(_("Bind Password"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, top, (top + 1), GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	entry_bindPW = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), entry_bindPW, 1, 2, top, (top + 1), GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);
	gtk_entry_set_visibility(GTK_ENTRY(entry_bindPW), FALSE);

	/* Next row */
	++top;
	label = gtk_label_new(_("Timeout (secs)"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, top, (top + 1), GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	hbox_spin = gtk_hbox_new (FALSE, 8);
	spinbtn_timeout_adj = gtk_adjustment_new (0, 0, 300, 1, 10, 0);
	spinbtn_timeout = gtk_spin_button_new(GTK_ADJUSTMENT (spinbtn_timeout_adj), 1, 0);
	gtk_box_pack_start (GTK_BOX (hbox_spin), spinbtn_timeout, FALSE, FALSE, 0);
	gtk_widget_set_size_request (spinbtn_timeout, 64, -1);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_timeout), TRUE);
	gtk_table_attach(GTK_TABLE(table), hbox_spin, 1, 2, top, (top + 1), GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);

	/* Next row */
	++top;
	label = gtk_label_new(_("Maximum Entries"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, top, (top + 1), GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	hbox_spin = gtk_hbox_new (FALSE, 8);
	spinbtn_maxentry_adj = gtk_adjustment_new (0, 0, 500, 1, 10, 0);
	spinbtn_maxentry = gtk_spin_button_new(GTK_ADJUSTMENT (spinbtn_maxentry_adj), 1, 0);
	gtk_box_pack_start (GTK_BOX (hbox_spin), spinbtn_maxentry, FALSE, FALSE, 0);
	gtk_widget_set_size_request (spinbtn_maxentry, 64, -1);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_maxentry), TRUE);
	gtk_table_attach(GTK_TABLE(table), hbox_spin, 1, 2, top, (top + 1), GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);

	g_signal_connect(G_OBJECT(reset_btn), "clicked",
			 G_CALLBACK(edit_ldap_search_reset), NULL);

	gtk_widget_show_all(vbox);

	ldapedit.entry_criteria   = entry_criteria;
	ldapedit.entry_bindDN     = entry_bindDN;
	ldapedit.entry_bindPW     = entry_bindPW;
	ldapedit.spinbtn_timeout  = spinbtn_timeout;
	ldapedit.spinbtn_maxentry = spinbtn_maxentry;
}

static void addressbook_edit_ldap_create( gboolean *cancelled ) {
	gint page = 0;
	addressbook_edit_ldap_dialog_create( cancelled );
	addressbook_edit_ldap_page_basic( page++, _( "Basic" ) );
	addressbook_edit_ldap_page_extended( page++, _( "Extended" ) );
	gtk_widget_show_all( ldapedit.window );
}

void edit_ldap_set_optmenu( GtkOptionMenu *optmenu, const gint value ) {
	GList *cur;
	GtkWidget *menu;
	GtkWidget *menuitem;
	gint menuVal;
	gint n = 0;

	g_return_if_fail(optmenu != NULL);

	menu = gtk_option_menu_get_menu(optmenu);
	for( cur = GTK_MENU_SHELL(menu)->children; cur != NULL; cur = cur->next ) {
		menuitem = GTK_WIDGET(cur->data);
		menuVal = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menuitem), "user_data"));
		if( menuVal == value ) {
			gtk_option_menu_set_history(optmenu, n);
			return;
		}
		n++;
	}
	gtk_option_menu_set_history(optmenu, 0);
}

gint edit_ldap_get_optmenu( GtkOptionMenu *optmenu ) {
	GtkWidget *menu;
	GtkWidget *menuitem;

	g_return_val_if_fail(optmenu != NULL, -1);

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(optmenu));
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	return GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menuitem), "user_data"));
}

AdapterDSource *addressbook_edit_ldap( AddressIndex *addrIndex, AdapterDSource *ads ) {
	static gboolean cancelled;
	gchar *sName, *sHost, *sBase, *sBind, *sPass, *sCrit;
	gint iPort, iMaxE, iTime;
	AddressDataSource *ds = NULL;
	SyldapServer *server = NULL;
	gboolean fin;

	if (!ldapedit.window)
		addressbook_edit_ldap_create(&cancelled);
	gtkut_box_set_reverse_order(GTK_BOX(ldapedit.hbbox),
				    !prefs_common.comply_gnome_hig);
	gtk_notebook_set_current_page( GTK_NOTEBOOK(ldapedit.notebook), 0 );
	gtk_widget_grab_focus(ldapedit.ok_btn);
	gtk_widget_grab_focus(ldapedit.entry_name);
	gtk_widget_show(ldapedit.window);
	manage_window_set_transient(GTK_WINDOW(ldapedit.window));

	edit_ldap_status_show( "" );
	if( ads ) {
		ds = ads->dataSource;
		server = ds->rawDataSource;
		if (server->name)
			gtk_entry_set_text(GTK_ENTRY(ldapedit.entry_name), server->name);
		if (server->hostName)
			gtk_entry_set_text(GTK_ENTRY(ldapedit.entry_server), server->hostName);
		gtk_spin_button_set_value( GTK_SPIN_BUTTON( ldapedit.spinbtn_port ), server->port );
		gtk_spin_button_set_value( GTK_SPIN_BUTTON( ldapedit.spinbtn_timeout ), server->timeOut );
		if (server->baseDN)
			gtk_entry_set_text(GTK_ENTRY(ldapedit.entry_baseDN), server->baseDN);
		if (server->searchCriteria)
			gtk_entry_set_text(GTK_ENTRY(ldapedit.entry_criteria), server->searchCriteria);
		if (server->bindDN)
			gtk_entry_set_text(GTK_ENTRY(ldapedit.entry_bindDN), server->bindDN);
		if (server->bindPass)
			gtk_entry_set_text(GTK_ENTRY(ldapedit.entry_bindPW), server->bindPass);
		gtk_spin_button_set_value( GTK_SPIN_BUTTON( ldapedit.spinbtn_maxentry ), server->maxEntries );
		gtk_window_set_title( GTK_WINDOW(ldapedit.window), _("Edit LDAP Server"));
	}
	else {
		gtk_entry_set_text(GTK_ENTRY(ldapedit.entry_name), ADDRESSBOOK_GUESS_LDAP_NAME );
		gtk_entry_set_text(GTK_ENTRY(ldapedit.entry_server), ADDRESSBOOK_GUESS_LDAP_SERVER );
		gtk_spin_button_set_value( GTK_SPIN_BUTTON( ldapedit.spinbtn_port ), SYLDAP_DFL_PORT );
		gtk_spin_button_set_value( GTK_SPIN_BUTTON( ldapedit.spinbtn_timeout ), SYLDAP_DFL_TIMEOUT );
		gtk_entry_set_text(GTK_ENTRY(ldapedit.entry_baseDN), "");
		gtk_entry_set_text(GTK_ENTRY(ldapedit.entry_criteria), SYLDAP_DFL_CRITERIA );
		gtk_entry_set_text(GTK_ENTRY(ldapedit.entry_bindDN), "");
		gtk_entry_set_text(GTK_ENTRY(ldapedit.entry_bindPW), "");
		gtk_spin_button_set_value( GTK_SPIN_BUTTON( ldapedit.spinbtn_maxentry ), SYLDAP_MAX_ENTRIES );
		gtk_window_set_title( GTK_WINDOW(ldapedit.window), _("Add New LDAP Server"));
	}

	gtk_main();
	gtk_widget_hide(ldapedit.window);
	if (cancelled == TRUE) return NULL;

	fin = FALSE;
	sName = gtk_editable_get_chars( GTK_EDITABLE(ldapedit.entry_name), 0, -1 );
	sHost = gtk_editable_get_chars( GTK_EDITABLE(ldapedit.entry_server), 0, -1 );
	iPort = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( ldapedit.spinbtn_port ) );
	iTime = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( ldapedit.spinbtn_timeout ) );
	sBase = gtk_editable_get_chars( GTK_EDITABLE(ldapedit.entry_baseDN), 0, -1 );
	sCrit = gtk_editable_get_chars( GTK_EDITABLE(ldapedit.entry_criteria), 0, -1 );
	sBind = gtk_editable_get_chars( GTK_EDITABLE(ldapedit.entry_bindDN), 0, -1 );
	sPass = gtk_editable_get_chars( GTK_EDITABLE(ldapedit.entry_bindPW), 0, -1 );
	iMaxE = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( ldapedit.spinbtn_maxentry ) );

	if( *sName == '\0' ) fin = TRUE;
	if( *sHost == '\0' ) fin = TRUE;
	if( *sBase == '\0' ) fin = TRUE;

	if( ! fin ) {
		if( ! ads ) {
			server = syldap_create();
			ds = addrindex_index_add_datasource( addrIndex, ADDR_IF_LDAP, server );
			ads = addressbook_create_ds_adapter( ds, ADDR_LDAP, NULL );
		}
		addressbook_ads_set_name( ads, sName );
		syldap_set_name( server, sName );
		syldap_set_host( server, sHost );
		syldap_set_port( server, iPort );
		syldap_set_base_dn( server, sBase );
		syldap_set_bind_dn( server, sBind );
		syldap_set_bind_password( server, sPass );
		syldap_set_search_criteria( server, sCrit );
		syldap_set_max_entries( server, iMaxE );
		syldap_set_timeout( server, iTime );
	}
	g_free( sName );
	g_free( sHost );
	g_free( sBase );
	g_free( sBind );
	g_free( sPass );
	g_free( sCrit );

	return ads;
}

#endif /* USE_LDAP */

/*
* End of Source.
*/
