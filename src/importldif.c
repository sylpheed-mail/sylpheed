/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2017 Hiroyuki Yamamoto
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
#include <gtk/gtkhbbox.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkstatusbar.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtkstock.h>

#include "addrbook.h"
#include "addressbook.h"
#include "addressitem.h"
#include "filesel.h"
#include "gtkutils.h"
#include "stock_pixmap.h"
#include "prefs_common.h"
#include "manage_window.h"
#include "mgutils.h"
#include "ldif.h"
#include "codeconv.h"
#include "utils.h"

#define IMPORTLDIF_GUESS_NAME "LDIF Import"

#define PAGE_FILE_INFO             0
#define PAGE_ATTRIBUTES            1
#define PAGE_FINISH                2

#define IMPORTLDIF_WIDTH           480
#define IMPORTLDIF_HEIGHT          300

#define FIELDS_N_COLS              3
#define FIELDS_COL_WIDTH_SELECT    10
#define FIELDS_COL_WIDTH_FIELD     140
#define FIELDS_COL_WIDTH_ATTRIB    140

typedef enum {
	FIELD_COL_SELECT  = 0,
	FIELD_COL_FIELD   = 1,
	FIELD_COL_ATTRIB  = 2
} ImpLdif_FieldColPos;

static struct _ImpLdif_Dlg {
	GtkWidget *window;
	GtkWidget *notebook;
	GtkWidget *file_entry;
	GtkWidget *name_entry;
	GtkWidget *clist_field;
	GtkWidget *name_ldif;
	GtkWidget *name_attrib;
	GtkWidget *check_select;
	GtkWidget *labelBook;
	GtkWidget *labelFile;
	GtkWidget *labelRecords;
	GtkWidget *btnPrev;
	GtkWidget *btnNext;
	GtkWidget *btnCancel;
	GtkWidget *statusbar;
	gint      status_cid;
	gint      rowIndSelect;
	gint      rowCount;
	gchar     *nameBook;
	gchar     *fileName;
	gboolean  cancelled;
} impldif_dlg;

static AddressBookFile *_importedBook_;
static AddressIndex *_imp_addressIndex_;
static LdifFile *_ldifFile_ = NULL;

static GdkPixmap *markxpm;
static GdkBitmap *markxpmmask;

static void imp_ldif_status_show( gchar *msg ) {
	if( impldif_dlg.statusbar != NULL ) {
		gtk_statusbar_pop( GTK_STATUSBAR(impldif_dlg.statusbar), impldif_dlg.status_cid );
		if( msg ) {
			gtk_statusbar_push( GTK_STATUSBAR(impldif_dlg.statusbar), impldif_dlg.status_cid, msg );
		}
	}
}

static void imp_ldif_message( void ) {
	gchar *sMsg = NULL;
	gint pageNum;

	pageNum = gtk_notebook_get_current_page( GTK_NOTEBOOK(impldif_dlg.notebook) );
	if( pageNum == PAGE_FILE_INFO ) {
		sMsg = _( "Please specify address book name and file to import." );
	}
	else if( pageNum == PAGE_ATTRIBUTES ) {
		sMsg = _( "Select and rename LDIF field names to import." );
	}
	else if( pageNum == PAGE_FINISH ) {
		sMsg = _( "File imported." );
	}
	imp_ldif_status_show( sMsg );
}

static gchar *imp_ldif_guess_file( AddressBookFile *abf ) {
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

static void imp_ldif_update_row( GtkCList *clist ) {
	Ldif_FieldRec *rec;
	gchar *text[ FIELDS_N_COLS ];
	gint row;

	if( impldif_dlg.rowIndSelect < 0 ) return;
	row = impldif_dlg.rowIndSelect;

	rec = gtk_clist_get_row_data( clist, row );
	text[ FIELD_COL_SELECT ] = "";
	text[ FIELD_COL_FIELD  ] = rec->tagName;
	text[ FIELD_COL_ATTRIB ] = rec->userName;

	gtk_clist_freeze( clist );
	gtk_clist_remove( clist, row );
	if( row == impldif_dlg.rowCount - 1 ) {
		gtk_clist_append( clist, text );
	}
	else {
		gtk_clist_insert( clist, row, text );
	}
	if( rec->selected )
		gtk_clist_set_pixmap( clist, row, FIELD_COL_SELECT, markxpm, markxpmmask );

	gtk_clist_set_row_data( clist, row, rec );
	gtk_clist_thaw( clist );
}

static void imp_ldif_load_fields( LdifFile *ldf ) {
	GtkCList *clist = GTK_CLIST(impldif_dlg.clist_field);
	GList *node, *list;
	gchar *text[ FIELDS_N_COLS ];

	impldif_dlg.rowIndSelect = -1;
	impldif_dlg.rowCount = 0;
	if( ! ldf->accessFlag ) return;
	gtk_clist_clear( clist );
	list = ldif_get_fieldlist( ldf );
	node = list;
	while( node ) {
		Ldif_FieldRec *rec = node->data;
		gint row;

		if( ! rec->reserved ) {
			text[ FIELD_COL_SELECT ] = "";
			text[ FIELD_COL_FIELD  ] = rec->tagName;
			text[ FIELD_COL_ATTRIB ] = rec->userName;
			row = gtk_clist_append( clist, text );
			gtk_clist_set_row_data( clist, row, rec );
			if( rec->selected )
				gtk_clist_set_pixmap( clist, row, FIELD_COL_SELECT, markxpm, markxpmmask );
			impldif_dlg.rowCount++;
		}
		node = g_list_next( node );
	}
	g_list_free( list );
	list = NULL;
	ldif_set_accessed( ldf, FALSE );
}

static void imp_ldif_field_list_selected( GtkCList *clist, gint row, gint column, GdkEvent *event, gpointer data ) {
	Ldif_FieldRec *rec = gtk_clist_get_row_data( clist, row );

	impldif_dlg.rowIndSelect = row;
	gtk_entry_set_text( GTK_ENTRY(impldif_dlg.name_attrib), "" );
	if( rec ) {
		gtk_label_set_text( GTK_LABEL(impldif_dlg.name_ldif), rec->tagName );
		if( rec->userName )
			gtk_entry_set_text( GTK_ENTRY(impldif_dlg.name_attrib), rec->userName );
		gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON( impldif_dlg.check_select),
			rec->selected );
	}
	gtk_widget_grab_focus(impldif_dlg.name_attrib);
}

static gboolean imp_ldif_field_list_toggle( GtkCList *clist, GdkEventButton *event, gpointer data ) {
	if( ! event ) return FALSE;
	if( impldif_dlg.rowIndSelect < 0 ) return FALSE;
	if( event->button == 1 ) {
		if( event->type == GDK_2BUTTON_PRESS ) {
			Ldif_FieldRec *rec = gtk_clist_get_row_data( clist, impldif_dlg.rowIndSelect );
			if( rec ) {
				rec->selected = ! rec->selected;
				imp_ldif_update_row( clist );
			}
		}
	}
	return FALSE;
}

static void imp_ldif_modify_pressed( GtkWidget *widget, gpointer data ) {
	GtkCList *clist = GTK_CLIST(impldif_dlg.clist_field);
	Ldif_FieldRec *rec;
	gint row;

	if( impldif_dlg.rowIndSelect < 0 ) return;
	row = impldif_dlg.rowIndSelect;
	rec = gtk_clist_get_row_data( clist, impldif_dlg.rowIndSelect );

	g_free( rec->userName );
	rec->userName = gtk_editable_get_chars( GTK_EDITABLE(impldif_dlg.name_attrib), 0, -1 );
	rec->selected = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( impldif_dlg.check_select) );
	imp_ldif_update_row( clist );
	gtk_clist_select_row( clist, row, 0 );
	gtk_label_set_text( GTK_LABEL(impldif_dlg.name_ldif), "" );
	gtk_entry_set_text( GTK_ENTRY(impldif_dlg.name_attrib), "" );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( impldif_dlg.check_select), FALSE );
}

/*
* Move off fields page.
* return: TRUE if OK to move off page.
*/
static gboolean imp_ldif_field_move() {
	gboolean retVal = FALSE;
	gchar *newFile;
	AddressBookFile *abf = NULL;

	if( _importedBook_ ) {
		addrbook_free_book( _importedBook_ );
	}

	abf = addrbook_create_book();
	addrbook_set_path( abf, _imp_addressIndex_->filePath );
	addrbook_set_name( abf, impldif_dlg.nameBook );
	newFile = imp_ldif_guess_file( abf );
	addrbook_set_file( abf, newFile );
	g_free( newFile );

	/* Import data into file */
	if( ldif_import_data( _ldifFile_, abf->addressCache ) == MGU_SUCCESS ) {
		addrbook_save_data( abf );
		abf->dirtyFlag = TRUE;
		_importedBook_ = abf;
		retVal = TRUE;
	}
	else {
		addrbook_free_book( abf );
	}

	return retVal;
}

/*
* Move off fields page.
* return: TRUE if OK to move off page.
*/
static gboolean imp_ldif_file_move() {
	gboolean retVal = FALSE;
	gchar *sName;
	gchar *sFile;
	gchar *sMsg = NULL;
	gboolean errFlag = FALSE;

	sFile = gtk_editable_get_chars( GTK_EDITABLE(impldif_dlg.file_entry), 0, -1 );
	g_strchug( sFile ); g_strchomp( sFile );

	sName = gtk_editable_get_chars( GTK_EDITABLE(impldif_dlg.name_entry), 0, -1 );
	g_strchug( sName ); g_strchomp( sName );

	g_free( impldif_dlg.nameBook );
	g_free( impldif_dlg.fileName );
	impldif_dlg.nameBook = sName;
	impldif_dlg.fileName = sFile;

	gtk_entry_set_text( GTK_ENTRY(impldif_dlg.file_entry), sFile );
	gtk_entry_set_text( GTK_ENTRY(impldif_dlg.name_entry), sName );

	if( *sFile == '\0'|| strlen( sFile ) < 1 ) {
		sMsg = _( "Please select a file." );
		gtk_widget_grab_focus(impldif_dlg.file_entry);
		errFlag = TRUE;
	}

	if( *sName == '\0'|| strlen( sName ) < 1 ) {
		if( ! errFlag ) sMsg = _( "Address book name must be supplied." );
		gtk_widget_grab_focus(impldif_dlg.name_entry);
		errFlag = TRUE;
	}

	if( ! errFlag ) {
		gchar *sFSFile;
		/* Read attribute list */
		sFSFile = conv_filename_from_utf8( sFile );
		ldif_set_file( _ldifFile_, sFSFile );
		g_free( sFSFile );
		if( ldif_read_tags( _ldifFile_ ) == MGU_SUCCESS ) {
			/* Load fields */
			/* ldif_print_file( _ldifFile_, stdout ); */
			imp_ldif_load_fields( _ldifFile_ );
			retVal = TRUE;
		}
		else {
			sMsg = _( "Error reading LDIF fields." );
		}
	}
	imp_ldif_status_show( sMsg );

	return retVal;
}

/*
 * Display finish page.
 */
static void imp_ldif_finish_show() {
	gchar *sMsg;
	gchar *name;

	name = gtk_editable_get_chars( GTK_EDITABLE(impldif_dlg.name_entry), 0, -1 );
	gtk_label_set_text( GTK_LABEL(impldif_dlg.labelBook), name );
	g_free( name );
	gtk_label_set_text( GTK_LABEL(impldif_dlg.labelFile), _ldifFile_->path );
	gtk_label_set_text( GTK_LABEL(impldif_dlg.labelRecords), itos( _ldifFile_->importCount ) );
	gtk_widget_set_sensitive( impldif_dlg.btnPrev, FALSE );
	gtk_widget_set_sensitive( impldif_dlg.btnNext, FALSE );
	if( _ldifFile_->retVal == MGU_SUCCESS ) {
		sMsg = _( "LDIF file imported successfully." );
	}
	else {
		sMsg = mgu_error2string( _ldifFile_->retVal );
	}
	imp_ldif_status_show( sMsg );
	gtk_button_set_label(GTK_BUTTON(impldif_dlg.btnCancel), GTK_STOCK_CLOSE);
	gtk_widget_grab_focus(impldif_dlg.btnCancel);
}

static void imp_ldif_prev( GtkWidget *widget ) {
	gint pageNum;

	pageNum = gtk_notebook_get_current_page( GTK_NOTEBOOK(impldif_dlg.notebook) );
	if( pageNum == PAGE_ATTRIBUTES ) {
		/* Goto file page stuff */
		gtk_notebook_set_current_page(
			GTK_NOTEBOOK(impldif_dlg.notebook), PAGE_FILE_INFO );
		gtk_widget_set_sensitive( impldif_dlg.btnPrev, FALSE );
	}
	imp_ldif_message();
}

static void imp_ldif_next( GtkWidget *widget ) {
	gint pageNum;

	pageNum = gtk_notebook_get_current_page( GTK_NOTEBOOK(impldif_dlg.notebook) );
	if( pageNum == PAGE_FILE_INFO ) {
		/* Goto attributes stuff */
		if( imp_ldif_file_move() ) {
			gtk_notebook_set_current_page(
				GTK_NOTEBOOK(impldif_dlg.notebook), PAGE_ATTRIBUTES );
			imp_ldif_message();
			gtk_widget_set_sensitive( impldif_dlg.btnPrev, TRUE );
		}
		else {
			gtk_widget_set_sensitive( impldif_dlg.btnPrev, FALSE );
		}
	}
	else if( pageNum == PAGE_ATTRIBUTES ) {
		/* Goto finish stuff */
		if( imp_ldif_field_move() ) {
			gtk_notebook_set_current_page(
				GTK_NOTEBOOK(impldif_dlg.notebook), PAGE_FINISH );
			imp_ldif_finish_show();
		}
	}
}

static void imp_ldif_cancel( GtkWidget *widget, gpointer data ) {
	gint pageNum;

	pageNum = gtk_notebook_get_current_page( GTK_NOTEBOOK(impldif_dlg.notebook) );
	if( pageNum != PAGE_FINISH ) {
		impldif_dlg.cancelled = TRUE;
	}
	gtk_main_quit();
}

static void imp_ldif_file_select( void ) {
	gchar *sSelFile;

	sSelFile = filesel_select_file( _("Select LDIF File"), NULL,
					GTK_FILE_CHOOSER_ACTION_OPEN );
	if ( sSelFile ) {
		gchar *sUTF8File;
		sUTF8File = conv_filename_to_utf8( sSelFile );
		gtk_entry_set_text( GTK_ENTRY(impldif_dlg.file_entry), sUTF8File );
		g_free( sUTF8File );
		g_free( sSelFile );
	}
}

static gint imp_ldif_delete_event( GtkWidget *widget, GdkEventAny *event, gpointer data ) {
	imp_ldif_cancel( widget, data );
	return TRUE;
}

static gboolean imp_ldif_key_pressed( GtkWidget *widget, GdkEventKey *event, gpointer data ) {
	if (event && event->keyval == GDK_Escape) {
		imp_ldif_cancel( widget, data );
	}
	return FALSE;
}

static void imp_ldif_page_file( gint pageNum, gchar *pageLbl ) {
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *file_entry;
	GtkWidget *name_entry;
	GtkWidget *file_btn;
	gint top;

	vbox = gtk_vbox_new(FALSE, 8);
	gtk_container_add( GTK_CONTAINER( impldif_dlg.notebook ), vbox );
	gtk_container_set_border_width( GTK_CONTAINER (vbox), BORDER_WIDTH );

	label = gtk_label_new( pageLbl );
	gtk_widget_show( label );
	gtk_notebook_set_tab_label(
		GTK_NOTEBOOK( impldif_dlg.notebook ),
		gtk_notebook_get_nth_page(
			GTK_NOTEBOOK( impldif_dlg.notebook ), pageNum ),
		label );

	table = gtk_table_new(2, 3, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	gtk_container_set_border_width( GTK_CONTAINER(table), 8 );
	gtk_table_set_row_spacings(GTK_TABLE(table), 8);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8 );

	/* First row */
	top = 0;
	label = gtk_label_new(_("Address Book"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, top, (top + 1),
		GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	name_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), name_entry, 1, 2, top, (top + 1),
		GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);

	/* Second row */
	top = 1;
	label = gtk_label_new(_("File Name"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, top, (top + 1),
		GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	file_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), file_entry, 1, 2, top, (top + 1),
		GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);

	file_btn = gtk_button_new_with_label( _(" ... "));
	gtk_table_attach(GTK_TABLE(table), file_btn, 2, 3, top, (top + 1),
		GTK_FILL, 0, 3, 0);

	gtk_widget_show_all(vbox);

	/* Button handler */
	g_signal_connect(G_OBJECT(file_btn), "clicked",
			 G_CALLBACK(imp_ldif_file_select), NULL);

	impldif_dlg.file_entry = file_entry;
	impldif_dlg.name_entry = name_entry;
}

static void imp_ldif_page_fields( gint pageNum, gchar *pageLbl ) {
	GtkWidget *vbox;
	GtkWidget *vboxt;
	GtkWidget *vboxb;
	GtkWidget *buttonMod;

	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *clist_swin;
	GtkWidget *clist_field;
	GtkWidget *name_ldif;
	GtkWidget *name_attrib;
	GtkWidget *check_select;
	gint top;

	gchar *titles[ FIELDS_N_COLS ];
	gint i;

	titles[ FIELD_COL_SELECT ] = _("S");
	titles[ FIELD_COL_FIELD  ] = _("LDIF Field");
	titles[ FIELD_COL_ATTRIB ] = _("Attribute Name");

	vbox = gtk_vbox_new(FALSE, 8);
	gtk_container_add( GTK_CONTAINER( impldif_dlg.notebook ), vbox );
	gtk_container_set_border_width( GTK_CONTAINER (vbox), 4 );

	label = gtk_label_new( pageLbl );
	gtk_widget_show( label );
	gtk_notebook_set_tab_label(
		GTK_NOTEBOOK( impldif_dlg.notebook ),
		gtk_notebook_get_nth_page(GTK_NOTEBOOK( impldif_dlg.notebook ), pageNum ),
		label );

	/* Upper area - Field list */
	vboxt = gtk_vbox_new( FALSE, 4 );
	gtk_container_add( GTK_CONTAINER( vbox ), vboxt );

	clist_swin = gtk_scrolled_window_new( NULL, NULL );
	gtk_container_add( GTK_CONTAINER(vboxt), clist_swin );
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(clist_swin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_ALWAYS);

	clist_field = gtk_clist_new_with_titles( FIELDS_N_COLS, titles );
	gtk_container_add( GTK_CONTAINER(clist_swin), clist_field );
	gtk_clist_set_selection_mode( GTK_CLIST(clist_field), GTK_SELECTION_BROWSE );
	gtk_clist_set_column_width(
			GTK_CLIST(clist_field), FIELD_COL_SELECT, FIELDS_COL_WIDTH_SELECT );
	gtk_clist_set_column_width(
			GTK_CLIST(clist_field), FIELD_COL_FIELD, FIELDS_COL_WIDTH_FIELD );
	gtk_clist_set_column_width(
			GTK_CLIST(clist_field), FIELD_COL_ATTRIB, FIELDS_COL_WIDTH_ATTRIB );
	gtkut_clist_set_redraw( GTK_CLIST(clist_field) );

	for( i = 0; i < FIELDS_N_COLS; i++ )
		GTK_WIDGET_UNSET_FLAGS(GTK_CLIST(clist_field)->column[i].button, GTK_CAN_FOCUS);

	/* Lower area - Edit area */
	vboxb = gtk_vbox_new( FALSE, 4 );
	gtk_box_pack_end(GTK_BOX(vbox), vboxb, FALSE, FALSE, 2);

	/* Data entry area */
	table = gtk_table_new( 3, 3, FALSE);
	gtk_box_pack_start(GTK_BOX(vboxb), table, FALSE, FALSE, 0);
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 4);

	/* First row */
	top = 0;
	label = gtk_label_new(_("LDIF Field"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, top, (top + 1), GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	name_ldif = gtk_label_new( "" );
	gtk_misc_set_alignment(GTK_MISC(name_ldif), 0.01, 0.5);
	gtk_table_attach(GTK_TABLE(table), name_ldif, 1, 3, top, (top + 1), GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);

	/* Second row */
	++top;
	label = gtk_label_new(_("Attribute"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, top, (top + 1), GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	name_attrib = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), name_attrib, 1, 3, top, (top + 1), GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);

	/* Next row */
	++top;
	label = gtk_label_new(_("Select"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, top, (top + 1), GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	check_select = gtk_check_button_new();
	gtk_table_attach(GTK_TABLE(table), check_select, 1, 2, top, (top + 1), GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);

	buttonMod = gtk_button_new_with_label( _("Modify"));
	gtk_table_attach(GTK_TABLE(table), buttonMod, 2, 3, top, (top + 1), GTK_FILL, 0, 3, 0);

	gtk_widget_show_all(vbox);

	/* Event handlers */
	g_signal_connect( G_OBJECT(clist_field), "select_row",
			  G_CALLBACK(imp_ldif_field_list_selected), NULL );
	g_signal_connect( G_OBJECT(clist_field), "button_press_event",
			  G_CALLBACK(imp_ldif_field_list_toggle), NULL );
	g_signal_connect( G_OBJECT(buttonMod), "clicked",
			  G_CALLBACK(imp_ldif_modify_pressed), NULL );

	impldif_dlg.clist_field  = clist_field;
	impldif_dlg.name_ldif    = name_ldif;
	impldif_dlg.name_attrib  = name_attrib;
	impldif_dlg.check_select = check_select;
}

static void imp_ldif_page_finish( gint pageNum, gchar *pageLbl ) {
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *labelBook;
	GtkWidget *labelFile;
	GtkWidget *labelRecs;
	gint top;

	vbox = gtk_vbox_new(FALSE, 8);
	gtk_container_add( GTK_CONTAINER( impldif_dlg.notebook ), vbox );
	gtk_container_set_border_width( GTK_CONTAINER (vbox), BORDER_WIDTH );

	label = gtk_label_new( pageLbl );
	gtk_widget_show( label );
	gtk_notebook_set_tab_label(
		GTK_NOTEBOOK( impldif_dlg.notebook ),
		gtk_notebook_get_nth_page( GTK_NOTEBOOK( impldif_dlg.notebook ), pageNum ), label );

	table = gtk_table_new(3, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	gtk_container_set_border_width( GTK_CONTAINER(table), 8 );
	gtk_table_set_row_spacings(GTK_TABLE(table), 8);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8 );

	/* First row */
	top = 0;
	label = gtk_label_new(_("Address Book :"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, top, (top + 1), GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);

	labelBook = gtk_label_new("");
	gtk_table_attach(GTK_TABLE(table), labelBook, 1, 2, top, (top + 1), GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(labelBook), 0, 0.5);

	/* Second row */
	top++;
	label = gtk_label_new(_("File Name :"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, top, (top + 1), GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);

	labelFile = gtk_label_new("");
	gtk_table_attach(GTK_TABLE(table), labelFile, 1, 2, top, (top + 1), GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(labelFile), 0, 0.5);

	/* Third row */
	top++;
	label = gtk_label_new(_("Records :"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, top, (top + 1), GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);

	labelRecs = gtk_label_new("");
	gtk_table_attach(GTK_TABLE(table), labelRecs, 1, 2, top, (top + 1), GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(labelRecs), 0, 0.5);

	impldif_dlg.labelBook    = labelBook;
	impldif_dlg.labelFile    = labelFile;
	impldif_dlg.labelRecords = labelRecs;
}

static void imp_ldif_dialog_create() {
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *vnbox;
	GtkWidget *notebook;
	GtkWidget *hbbox;
	GtkWidget *btnPrev;
	GtkWidget *btnNext;
	GtkWidget *btnCancel;
	GtkWidget *hsbox;
	GtkWidget *statusbar;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_size_request(window, IMPORTLDIF_WIDTH, IMPORTLDIF_HEIGHT );
	gtk_window_set_title( GTK_WINDOW(window), _("Import LDIF file into Address Book") );
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);	
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(imp_ldif_delete_event), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(imp_ldif_key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);

	vbox = gtk_vbox_new(FALSE, 4);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	vnbox = gtk_vbox_new(FALSE, 4);
	gtk_container_set_border_width(GTK_CONTAINER(vnbox), 4);
	gtk_widget_show(vnbox);
	gtk_box_pack_start(GTK_BOX(vbox), vnbox, TRUE, TRUE, 0);

	/* Notebook */
	notebook = gtk_notebook_new();
	gtk_notebook_set_show_tabs( GTK_NOTEBOOK(notebook), FALSE );
	gtk_widget_show(notebook);
	gtk_box_pack_start(GTK_BOX(vnbox), notebook, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(notebook), 6);

	/* Status line */
	hsbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), hsbox, FALSE, FALSE, 0);
	statusbar = gtk_statusbar_new();
	gtk_box_pack_start(GTK_BOX(hsbox), statusbar, TRUE, TRUE, 0);

	/* Button panel */
	if (prefs_common.comply_gnome_hig) {
		gtkut_stock_button_set_create(&hbbox, &btnNext, _("Next"),
					      &btnPrev, _("Prev"),
					      &btnCancel, GTK_STOCK_CANCEL);
	} else {
		gtkut_stock_button_set_create(&hbbox, &btnPrev, _("Prev"),
					      &btnNext, _("Next"),
					      &btnCancel, GTK_STOCK_CANCEL);
	}
	gtk_box_pack_end(GTK_BOX(vnbox), hbbox, FALSE, FALSE, 0);
	gtk_widget_grab_default(btnNext);

	/* Button handlers */
	g_signal_connect(G_OBJECT(btnPrev), "clicked",
			 G_CALLBACK(imp_ldif_prev), NULL);
	g_signal_connect(G_OBJECT(btnNext), "clicked",
			 G_CALLBACK(imp_ldif_next), NULL);
	g_signal_connect(G_OBJECT(btnCancel), "clicked",
			 G_CALLBACK(imp_ldif_cancel), NULL);

	gtk_widget_show_all(vbox);

	impldif_dlg.window     = window;
	impldif_dlg.notebook   = notebook;
	impldif_dlg.btnPrev    = btnPrev;
	impldif_dlg.btnNext    = btnNext;
	impldif_dlg.btnCancel  = btnCancel;
	impldif_dlg.statusbar  = statusbar;
	impldif_dlg.status_cid = gtk_statusbar_get_context_id(
			GTK_STATUSBAR(statusbar), "Import LDIF Dialog" );

}

static void imp_ldif_create() {
	imp_ldif_dialog_create();
	imp_ldif_page_file( PAGE_FILE_INFO, _( "File Info" ) );
	imp_ldif_page_fields( PAGE_ATTRIBUTES, _( "Attributes" ) );
	imp_ldif_page_finish( PAGE_FINISH, _( "Finish" ) );
	gtk_widget_show_all( impldif_dlg.window );
}

AddressBookFile *addressbook_imp_ldif( AddressIndex *addrIndex ) {
	_importedBook_ = NULL;
	_imp_addressIndex_ = addrIndex;

	if( ! impldif_dlg.window )
		imp_ldif_create();
	impldif_dlg.cancelled = FALSE;
	manage_window_set_transient(GTK_WINDOW(impldif_dlg.window));
	gtk_widget_grab_default(impldif_dlg.btnNext);

	gtk_entry_set_text( GTK_ENTRY(impldif_dlg.name_entry), IMPORTLDIF_GUESS_NAME );
	gtk_entry_set_text( GTK_ENTRY(impldif_dlg.file_entry), "" );
	gtk_label_set_text( GTK_LABEL(impldif_dlg.name_ldif), "" );
	gtk_entry_set_text( GTK_ENTRY(impldif_dlg.name_attrib), "" );
	gtk_clist_clear( GTK_CLIST(impldif_dlg.clist_field) );
	gtk_notebook_set_current_page( GTK_NOTEBOOK(impldif_dlg.notebook), PAGE_FILE_INFO );
	gtk_widget_set_sensitive( impldif_dlg.btnPrev, FALSE );
	gtk_widget_set_sensitive( impldif_dlg.btnNext, TRUE );
	gtk_button_set_label( GTK_BUTTON(impldif_dlg.btnCancel), GTK_STOCK_CANCEL );
	stock_pixmap_gdk( impldif_dlg.window, STOCK_PIXMAP_MARK,
			  &markxpm, &markxpmmask );
	imp_ldif_message();
	gtk_widget_grab_focus(impldif_dlg.file_entry);

	impldif_dlg.rowIndSelect = -1;
	impldif_dlg.rowCount = 0;
	g_free( impldif_dlg.nameBook );
	g_free( impldif_dlg.fileName );
	impldif_dlg.nameBook = NULL;
	impldif_dlg.fileName = NULL;

	_ldifFile_ = ldif_create();

	gtk_widget_show(impldif_dlg.window);

	gtk_main();
	gtk_widget_hide(impldif_dlg.window);
	ldif_free( _ldifFile_ );
	_ldifFile_ = NULL;
	_imp_addressIndex_ = NULL;

	g_free( impldif_dlg.nameBook );
	g_free( impldif_dlg.fileName );
	impldif_dlg.nameBook = NULL;
	impldif_dlg.fileName = NULL;

	if( impldif_dlg.cancelled == TRUE ) return NULL;
	return _importedBook_;
}

AddressBookFile *addressbook_imp_ldif_file( AddressIndex *addrIndex,
					    const gchar *file,
					    const gchar *book_name ) {
	gchar *fsfile;
	GList *node, *list;
	gboolean ret = FALSE;

	g_return_val_if_fail(addrIndex != NULL, NULL);
	g_return_val_if_fail(file != NULL, NULL);
	g_return_val_if_fail(book_name != NULL, NULL);

	debug_print("addressbook_imp_ldif_file: file: %s name: %s\n",
		    file, book_name);

	_importedBook_ = NULL;
	_imp_addressIndex_ = addrIndex;
	_ldifFile_ = ldif_create();

	fsfile = conv_filename_from_utf8( file );
	ldif_set_file(_ldifFile_, fsfile);
	g_free( fsfile );

	if( ldif_read_tags( _ldifFile_ ) != MGU_SUCCESS )
		goto finish;
	list = ldif_get_fieldlist( _ldifFile_ );
	node = list;
	while( node ) {
		Ldif_FieldRec *rec = node->data;
		if( ! rec->reserved ) {
			if( g_ascii_strcasecmp( rec->tagName, "dn" ) != 0 ) {
				rec->selected = TRUE;
			}
		}
		node = g_list_next( node );
	}
	g_list_free( list );

	g_free( impldif_dlg.nameBook );
	impldif_dlg.nameBook = g_strdup(book_name);

	ret = imp_ldif_field_move();

	g_free( impldif_dlg.nameBook );
	impldif_dlg.nameBook = NULL;

finish:
	ldif_free( _ldifFile_ );
	_ldifFile_ = NULL;
	_imp_addressIndex_ = NULL;

	if (ret)
		debug_print("addressbook_imp_ldif_file: import succeeded\n");

	return _importedBook_;
}

/*
* End of Source.
*/

