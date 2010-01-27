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
 * Edit new address book entry.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtktable.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkstatusbar.h>
#include <gtk/gtkhseparator.h>
#include <gtk/gtkstock.h>

#include "utils.h"
#include "prefs_common.h"
#include "mgutils.h"
#include "addressbook.h"
#include "addressitem.h"
#include "addrindex.h"
#include "addrbook.h"
#include "manage_window.h"
#include "gtkutils.h"

#define ADDRESSBOOK_GUESS_BOOK  "MyAddressBook"

static struct _AddrBookEdit_Dlg {
	GtkWidget *window;
	GtkWidget *name_entry;
	GtkWidget *file_label;
	GtkWidget *hbbox;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	GtkWidget *check_btn;
	/* GtkWidget *file_btn; */
	GtkWidget *statusbar;
	gint status_cid;
	AddressBookFile *bookFile;
} addrbookedit_dlg;

/* static struct _AddressFileSelection vcard_file_selector; */

/*
* Edit functions.
*/
void edit_book_status_show( gchar *msg ) {
	if( addrbookedit_dlg.statusbar != NULL ) {
		gtk_statusbar_pop( GTK_STATUSBAR(addrbookedit_dlg.statusbar), addrbookedit_dlg.status_cid );
		if( msg ) {
			gtk_statusbar_push(
				GTK_STATUSBAR(addrbookedit_dlg.statusbar), addrbookedit_dlg.status_cid, msg );
		}
		else {
			gtk_statusbar_push(
				GTK_STATUSBAR(addrbookedit_dlg.statusbar), addrbookedit_dlg.status_cid, "" );
		}
	}
}

static void edit_book_ok( GtkWidget *widget, gboolean *cancelled ) {
	*cancelled = FALSE;
	gtk_main_quit();
}

static void edit_book_cancel( GtkWidget *widget, gboolean *cancelled ) {
	*cancelled = TRUE;
	gtk_main_quit();
}

static gint edit_book_delete_event( GtkWidget *widget, GdkEventAny *event, gboolean *cancelled ) {
	*cancelled = TRUE;
	gtk_main_quit();
	return TRUE;
}

static gboolean edit_book_key_pressed( GtkWidget *widget, GdkEventKey *event, gboolean *cancelled ) {
	if (event && event->keyval == GDK_Escape) {
		*cancelled = TRUE;
		gtk_main_quit();
	}
	return FALSE;
}

static void edit_book_file_check( void ) {
	gint t;
	gchar *sMsg;
	AddressBookFile *abf = addrbookedit_dlg.bookFile;

	t = addrbook_test_read_file( abf, abf->fileName );
	if( t == MGU_SUCCESS ) {
		sMsg = _("File appears to be Ok.");
	}
	else if( t == MGU_BAD_FORMAT ) {
		sMsg = _("File does not appear to be a valid address book format.");
	}
	else {
		sMsg = _("Could not read file.");
	}
	edit_book_status_show( sMsg );
}

static void edit_book_enable_buttons( gboolean enable ) {
	gtk_widget_set_sensitive( addrbookedit_dlg.check_btn, enable );
	/* gtk_widget_set_sensitive( addrbookedit_dlg.file_btn, enable ); */
}

#if 0
static void edit_book_name_focus( GtkWidget *widget, GdkEventFocus *event, gpointer data) {
	edit_book_status_show( "" );
}
#endif

static gchar *edit_book_guess_file( AddressBookFile *abf ) {
	gchar *newFile = NULL;
	GList *fileList = NULL;
	gint fileNum = 1;
	fileList = addrbook_get_bookfile_list( abf );
	if( fileList ) {
		fileNum = 1 + abf->maxValue;
	}
	newFile = addrbook_gen_new_file_name( fileNum );
	g_list_free( fileList );
	fileList = NULL;
	return newFile;
}

static void addressbook_edit_book_create( gboolean *cancelled ) {
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *name_entry;
	GtkWidget *file_label;
	GtkWidget *hbbox;
	GtkWidget *hsep;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	GtkWidget *check_btn;
	/* GtkWidget *file_btn; */
	GtkWidget *statusbar;
	GtkWidget *hsbox;
	gint top;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_size_request(window, 450, -1);
	gtk_container_set_border_width( GTK_CONTAINER(window), 0 );
	gtk_window_set_title(GTK_WINDOW(window), _("Edit Address Book"));
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);	
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(edit_book_delete_event),
			 cancelled);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(edit_book_key_pressed),
			 cancelled);

	vbox = gtk_vbox_new(FALSE, 8);
	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_container_set_border_width( GTK_CONTAINER(vbox), 0 );

	table = gtk_table_new(2, 3, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	gtk_container_set_border_width( GTK_CONTAINER(table), 8 );
	gtk_table_set_row_spacings(GTK_TABLE(table), 8);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8 );

	/* First row */
	top = 0;
	label = gtk_label_new(_("Name"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, top, (top + 1), GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	name_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), name_entry, 1, 2, top, (top + 1), GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);

	check_btn = gtk_button_new_with_label( _(" Check File "));
	gtk_table_attach(GTK_TABLE(table), check_btn, 2, 3, top, (top + 1), GTK_FILL, 0, 3, 0);

	/* Second row */
	top = 1;
	label = gtk_label_new(_("File"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, top, (top + 1), GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	file_label = gtk_label_new( "" );
	gtk_misc_set_alignment(GTK_MISC(file_label), 0, 0.5);
	gtk_table_attach(GTK_TABLE(table), file_label, 1, 2, top, (top + 1), GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);

	/* file_btn = gtk_button_new_with_label( _(" ... ")); */
	/* gtk_table_attach(GTK_TABLE(table), file_btn, 2, 3, top, (top + 1), GTK_FILL, 0, 3, 0); */

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
	gtk_container_set_border_width( GTK_CONTAINER(hbbox), 0 );
	gtk_widget_grab_default(ok_btn);

	hsep = gtk_hseparator_new();
	gtk_box_pack_end(GTK_BOX(vbox), hsep, FALSE, FALSE, 0);

	/* g_signal_connect(G_OBJECT(name_entry), "focus_in_event",
			 G_CALLBACK(edit_book_name_focus), NULL ); */
	g_signal_connect(G_OBJECT(ok_btn), "clicked",
			 G_CALLBACK(edit_book_ok), cancelled);
	g_signal_connect(G_OBJECT(cancel_btn), "clicked",
			 G_CALLBACK(edit_book_cancel), cancelled);
/*	g_signal_connect(G_OBJECT(file_btn), "clicked", */
/*			 G_CALLBACK(edit_book_file_select), NULL); */
	g_signal_connect(G_OBJECT(check_btn), "clicked",
			 G_CALLBACK(edit_book_file_check), NULL);

	gtk_widget_show_all(vbox);

	addrbookedit_dlg.window     = window;
	addrbookedit_dlg.name_entry = name_entry;
	addrbookedit_dlg.file_label = file_label;
	addrbookedit_dlg.hbbox      = hbbox;
	addrbookedit_dlg.ok_btn     = ok_btn;
	addrbookedit_dlg.cancel_btn = cancel_btn;
	addrbookedit_dlg.check_btn  = check_btn;
	/* addrbookedit_dlg.file_btn   = file_btn; */
	addrbookedit_dlg.statusbar  = statusbar;
	addrbookedit_dlg.status_cid = gtk_statusbar_get_context_id( GTK_STATUSBAR(statusbar), "Edit Address Book Dialog" );
}

AdapterDSource *addressbook_edit_book( AddressIndex *addrIndex, AdapterDSource *ads ) {
	static gboolean cancelled;
	gchar *sName;
	AddressDataSource *ds = NULL;
	AddressBookFile *abf;
	gboolean fin;
	gboolean newBook = FALSE;
	gchar *newFile = NULL;

	if (!addrbookedit_dlg.window)
		addressbook_edit_book_create(&cancelled);
	gtkut_box_set_reverse_order(GTK_BOX(addrbookedit_dlg.hbbox),
				    !prefs_common.comply_gnome_hig);
	gtk_widget_grab_focus(addrbookedit_dlg.ok_btn);
	gtk_widget_grab_focus(addrbookedit_dlg.name_entry);
	gtk_widget_show(addrbookedit_dlg.window);
	manage_window_set_transient(GTK_WINDOW(addrbookedit_dlg.window));

	edit_book_status_show( "" );
	gtk_label_set_text( GTK_LABEL(addrbookedit_dlg.file_label), "" );
	if( ads ) {
		ds = ads->dataSource;
		abf = ds->rawDataSource;
		if (abf->name)
			gtk_entry_set_text(GTK_ENTRY(addrbookedit_dlg.name_entry), abf->name);
		if( abf->fileName )
			gtk_label_set_text(GTK_LABEL(addrbookedit_dlg.file_label), abf->fileName);
		gtk_window_set_title( GTK_WINDOW(addrbookedit_dlg.window), _("Edit Address Book"));
		edit_book_enable_buttons( TRUE );
	}
	else {
		gchar *tmp = NULL;
		newBook = TRUE;
		abf = addrbook_create_book();
		addrbook_set_path( abf, addrIndex->filePath );

		/* Take initial guess at file name */
		newFile = edit_book_guess_file( abf );
		if( newFile ) {
			tmp = g_strdup_printf( "<%s>", newFile );
			gtk_label_set_text(GTK_LABEL(addrbookedit_dlg.file_label), tmp );
			g_free( tmp );
		}
		g_free( newFile );

		gtk_entry_set_text( GTK_ENTRY(addrbookedit_dlg.name_entry), ADDRESSBOOK_GUESS_BOOK );
		gtk_window_set_title( GTK_WINDOW(addrbookedit_dlg.window), _("Add New Address Book") );
		edit_book_enable_buttons( FALSE );
	}

	addrbookedit_dlg.bookFile = abf;

	gtk_main();
	gtk_widget_hide(addrbookedit_dlg.window);

	if( cancelled == TRUE ) {
		if( newBook ) {
			addrbook_free_book( abf );
			abf = NULL;
		}
		return NULL;
	}

	fin = FALSE;
	sName = gtk_editable_get_chars( GTK_EDITABLE(addrbookedit_dlg.name_entry), 0, -1 );
	g_strstrip(sName);
	if( *sName == '\0' || *sName == '@' ) fin = TRUE;

	if( fin ) {
		if( newBook ) {
			addrbook_free_book( abf );
			abf = NULL;
		}
	}
	else {
		if( newBook ) {
			/* Get final file name in case it changed */
			newFile = edit_book_guess_file( abf );
			addrbook_set_file( abf, newFile );
			g_free( newFile );
			ds = addrindex_index_add_datasource( addrIndex, ADDR_IF_BOOK, abf );
			ads = addressbook_create_ds_adapter( ds, ADDR_BOOK, NULL );
		}
		addressbook_ads_set_name( ads, sName );
		addrbook_set_name( abf, sName );
		abf->dirtyFlag = TRUE;
	}
	g_free( sName );

	/* Save data */
	if( abf ) addrbook_save_data( abf );

	return ads;
}

/*
* End of Source.
*/
