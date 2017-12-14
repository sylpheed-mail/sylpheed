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
#include <gtk/gtkwindow.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtktable.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkradiobutton.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkstatusbar.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtkstock.h>

#include <stdio.h>

#include "addrbook.h"
#include "addressbook.h"
#include "addressitem.h"
#include "filesel.h"
#include "gtkutils.h"
#include "stock_pixmap.h"
#include "prefs_common.h"
#include "manage_window.h"
#include "mgutils.h"
#include "importcsv.h"
#include "codeconv.h"
#include "utils.h"

#define IMPORTCSV_GUESS_NAME "CSV Import"

#define PAGE_FILE_INFO             0
#define PAGE_ATTRIBUTES            1
#define PAGE_FINISH                2

#define IMPORTCSV_WIDTH           480
#define IMPORTCSV_HEIGHT          320

#define FIELDS_N_COLS              3
#define FIELDS_COL_WIDTH_SELECT    10
#define FIELDS_COL_WIDTH_FIELD     140
#define FIELDS_COL_WIDTH_ATTRIB    140

typedef enum {
	FIELD_COL_SELECT  = 0,
	FIELD_COL_FIELD   = 1,
	FIELD_COL_ATTRIB  = 2
} ImpCSV_FieldColPos;

static struct _ImpCSVDlg {
	GtkWidget *window;
	GtkWidget *notebook;
	GtkWidget *file_entry;
	GtkWidget *name_entry;
	GtkWidget *comma_radiobtn;
	GtkWidget *tab_radiobtn;
	GtkWidget *clist_field;
	GtkWidget *check_select;
	GtkWidget *labelBook;
	GtkWidget *labelFile;
	GtkWidget *labelRecords;
	GtkWidget *btnPrev;
	GtkWidget *btnNext;
	GtkWidget *btnCancel;
	GtkWidget *statusbar;
	gint      status_cid;
	gint      rowCount;
	gchar     *nameBook;
	gchar     *fileName;
	gchar     delimiter;
	gboolean  cancelled;
} impcsv_dlg;

typedef enum {
	ATTR_FIRST_NAME,
	ATTR_LAST_NAME,
	ATTR_DISPLAY_NAME,
	ATTR_NICK_NAME,
	ATTR_EMAIL_ADDRESS,
	ATTR_REMARKS,
	ATTR_ALIAS,

	N_CSV_ATTRIB
} ImpCSVAttribIndex;

static struct _ImpCSVAttrib {
	gchar *name;
	gchar *value;
	gint col;
	gboolean enabled;
} imp_csv_attrib[] = {
	{N_("First Name"),	NULL, 0, TRUE},
	{N_("Last Name"),	NULL, 1, TRUE},
	{N_("Display Name"),	NULL, 2, TRUE},
	{N_("Nick Name"),	NULL, 3, TRUE},
	{N_("E-Mail Address"),	NULL, 4, TRUE},
	{N_("Remarks"),		NULL, 5, TRUE},
	{N_("Alias"),		NULL, 6, TRUE}
};

static AddressBookFile *_importedBook_;
static AddressIndex *_imp_addressIndex_;
static gint importCount = 0;
static gint result;

static GdkPixmap *markxpm;
static GdkBitmap *markxpmmask;

static void imp_csv_status_show( gchar *msg ) {
	if( impcsv_dlg.statusbar != NULL ) {
		gtk_statusbar_pop( GTK_STATUSBAR(impcsv_dlg.statusbar), impcsv_dlg.status_cid );
		if( msg ) {
			gtk_statusbar_push( GTK_STATUSBAR(impcsv_dlg.statusbar), impcsv_dlg.status_cid, msg );
		}
	}
}

static void imp_csv_message( void ) {
	gchar *sMsg = NULL;
	gint pageNum;

	pageNum = gtk_notebook_get_current_page( GTK_NOTEBOOK(impcsv_dlg.notebook) );
	if( pageNum == PAGE_FILE_INFO ) {
		sMsg = _( "Please specify address book name and file to import." );
	}
	else if( pageNum == PAGE_ATTRIBUTES ) {
		sMsg = _( "Select and reorder CSV field names to import." );
	}
	else if( pageNum == PAGE_FINISH ) {
		sMsg = _( "File imported." );
	}
	imp_csv_status_show( sMsg );
}

static gchar *imp_csv_guess_file( AddressBookFile *abf ) {
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

static gboolean imp_csv_load_fields( gchar *sFile ) {
	GtkCList *clist = GTK_CLIST(impcsv_dlg.clist_field);
	FILE *fp;
	gchar buf[BUFFSIZE];
	CharSet enc;

	g_return_val_if_fail(sFile != NULL, FALSE);

	impcsv_dlg.rowCount = 0;
	gtk_clist_clear( clist );

	enc = conv_check_file_encoding(sFile);

	if ((fp = g_fopen(sFile, "rb")) == NULL) {
		return FALSE;
	}

	if (fgets(buf, sizeof(buf), fp) != NULL) {
		gchar *str;
		gchar **strv;
		gchar *text[ FIELDS_N_COLS ];
		gint i;
		guint fields_len;
		guint data_len = 0;
		guint len;
		gint row;

		strretchomp(buf);
		if (enc == C_UTF_8)
			str = g_strdup(buf);
		else
			str = conv_localetodisp(buf, NULL);
		strv = strsplit_csv(str, impcsv_dlg.delimiter, 0);
		fields_len = sizeof(imp_csv_attrib) / sizeof(imp_csv_attrib[0]);
		while (strv[data_len])
			++data_len;
		len = MAX(fields_len, data_len);

		gtk_clist_freeze(clist);

		for (i = 0; i < len; i++) {
			text[ FIELD_COL_SELECT ] = "";
			if (i < data_len)
				text[ FIELD_COL_FIELD  ] = strv[i];
			else
				text[ FIELD_COL_FIELD  ] = "";
			if (i < fields_len)
				text[ FIELD_COL_ATTRIB ] =
					gettext(imp_csv_attrib[i].name);
			else
				text[ FIELD_COL_ATTRIB ] = "";
			row = gtk_clist_append(clist, text);
			if (i < fields_len) {
				imp_csv_attrib[i].value = NULL;
				imp_csv_attrib[i].col = row;
				gtk_clist_set_row_data
					(clist, row, &imp_csv_attrib[i]);
				if (imp_csv_attrib[i].enabled)
					gtk_clist_set_pixmap(clist, row, FIELD_COL_SELECT, markxpm, markxpmmask);
			}
		}

		gtk_clist_thaw(clist);
		gtk_widget_queue_resize(GTK_WIDGET(clist));

		g_strfreev(strv);
		g_free(str);
	}

	fclose(fp);

	return TRUE;
}

static void imp_csv_field_list_selected( GtkCList *clist, gint row, gint column, GdkEvent *event ) {
	if (event && event->type == GDK_2BUTTON_PRESS)
		return;

	if (column == FIELD_COL_SELECT) {
		struct _ImpCSVAttrib *attr;
		attr = gtk_clist_get_row_data( clist, row );
		if (attr) {
			attr->enabled ^= TRUE;
			if (attr->enabled)
				gtk_clist_set_pixmap(clist, row, FIELD_COL_SELECT, markxpm, markxpmmask);
			else
				gtk_clist_set_text(clist, row, FIELD_COL_SELECT, "");
		}
	}
}

static void imp_csv_field_list_up( GtkWidget *button, gpointer data ) {
	GtkCList *clist = GTK_CLIST(impcsv_dlg.clist_field);
	gchar *text;
	gint row;
	struct _ImpCSVAttrib *src_attr;
	struct _ImpCSVAttrib *dest_attr;
	gchar *src_text;
	gchar *dest_text;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT( clist->selection->data );
	if ( row > 0 && row < clist->rows ) {
		gtk_clist_freeze( clist );

		src_attr = gtk_clist_get_row_data( clist, row );
		dest_attr = gtk_clist_get_row_data( clist, row - 1 );

		gtk_clist_row_move( clist, row, row - 1 );

		gtk_clist_get_text( clist, row, FIELD_COL_FIELD, &text );
		src_text = g_strdup( text );
		gtk_clist_get_text( clist, row - 1, FIELD_COL_FIELD, &text );
		dest_text = g_strdup( text );
		gtk_clist_set_text( clist, row - 1, FIELD_COL_FIELD, src_text );
		gtk_clist_set_text( clist, row, FIELD_COL_FIELD, dest_text );
		g_free( dest_text );
		g_free( src_text );

		if ( src_attr )
			src_attr->col = row - 1;
		if ( dest_attr )
			dest_attr->col = row;

		gtk_clist_thaw( clist );

		if (gtk_clist_row_is_visible(clist, row - 1)
			!= GTK_VISIBILITY_FULL)
			gtk_clist_moveto(clist, row - 1, 0, 0.5, 0);
	}
}

static void imp_csv_field_list_down( GtkWidget *button, gpointer data ) {
	GtkCList *clist = GTK_CLIST(impcsv_dlg.clist_field);
	gchar *text;
	gint row;
	struct _ImpCSVAttrib *src_attr;
	struct _ImpCSVAttrib *dest_attr;
	gchar *src_text;
	gchar *dest_text;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT( clist->selection->data );
	if ( row >= 0 && row < clist->rows - 1 ) {
		gtk_clist_freeze( clist );

		src_attr = gtk_clist_get_row_data( clist, row );
		dest_attr = gtk_clist_get_row_data( clist, row + 1 );

		gtk_clist_row_move( clist, row, row + 1 );

		gtk_clist_get_text( clist, row, FIELD_COL_FIELD, &text );
		src_text = g_strdup( text );
		gtk_clist_get_text( clist, row + 1, FIELD_COL_FIELD, &text );
		dest_text = g_strdup( text );
		gtk_clist_set_text( clist, row + 1, FIELD_COL_FIELD, src_text );
		gtk_clist_set_text( clist, row, FIELD_COL_FIELD, dest_text );
		g_free( dest_text );
		g_free( src_text );

		if ( src_attr )
			src_attr->col = row + 1;
		if ( dest_attr )
			dest_attr->col = row;

		gtk_clist_thaw( clist );

		if (gtk_clist_row_is_visible(clist, row + 1)
			!= GTK_VISIBILITY_FULL)
			gtk_clist_moveto(clist, row + 1, 0, 0.5, 0);
	}
}

static gint imp_csv_import_data( gchar *csvFile, AddressCache *cache ) {
	FILE *fp;
	gchar buf[BUFFSIZE];
	gint i;
	gchar **strv;
	CharSet enc;
	guint fields_len;
	gchar *firstName = NULL;
	gchar *lastName = NULL;
	gchar *fullName = NULL;
	gchar *nickName = NULL;
	gchar *address = NULL;
	gchar *remarks = NULL;
	gchar *alias = NULL;
	ItemPerson *person;
	ItemEMail *email;
	gint count = 0;

	g_return_val_if_fail( csvFile != NULL, MGU_BAD_ARGS );

	addrcache_clear( cache );
	cache->dataRead = FALSE;

	enc = conv_check_file_encoding(csvFile);

	if ((fp = g_fopen(csvFile, "rb")) == NULL) {
		return MGU_OPEN_FILE;
	}

	fields_len = sizeof(imp_csv_attrib) / sizeof(imp_csv_attrib[0]);

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		gchar *str;
		guint col, cols = 0;

		strretchomp(buf);
		if (enc == C_UTF_8)
			str = g_strdup(buf);
		else
			str = conv_localetodisp(buf, NULL);
		strv = strsplit_csv(str, impcsv_dlg.delimiter, 0);
		while (strv[cols])
			++cols;

		for (i = 0; i < fields_len; i++) {
			if (!imp_csv_attrib[i].enabled)
				continue;
			col = imp_csv_attrib[i].col;
			if (col >= cols || !*strv[col])
				continue;
			imp_csv_attrib[i].value = strv[col];
		}

		firstName = imp_csv_attrib[ATTR_FIRST_NAME].value;
		lastName  = imp_csv_attrib[ATTR_LAST_NAME].value;
		fullName  = imp_csv_attrib[ATTR_DISPLAY_NAME].value;
		nickName  = imp_csv_attrib[ATTR_NICK_NAME].value;
		address   = imp_csv_attrib[ATTR_EMAIL_ADDRESS].value;
		remarks   = imp_csv_attrib[ATTR_REMARKS].value;
		alias     = imp_csv_attrib[ATTR_ALIAS].value;

		if (!fullName && !firstName && !lastName && address)
			fullName = address;

		person = addritem_create_item_person();
		addritem_person_set_common_name( person, fullName );
		addritem_person_set_first_name( person, firstName );
		addritem_person_set_last_name( person, lastName );
		addritem_person_set_nick_name( person, nickName );
		addrcache_id_person( cache, person );
		addrcache_add_person( cache, person );

		if (address) {
			email = addritem_create_item_email();
			addritem_email_set_address( email, address );
			addritem_email_set_remarks( email, remarks );
			addritem_email_set_alias( email, alias );
			addrcache_id_email( cache, email );
			addrcache_person_add_email( cache, person, email );
		}

		for (i = 0; i < fields_len; i++)
			imp_csv_attrib[i].value = NULL;
		g_strfreev(strv);
		g_free(str);

		count++;
	}

	fclose(fp);

	cache->modified = FALSE;
	cache->dataRead = TRUE;
	importCount = count;

	return MGU_SUCCESS;
}

/*
* Move off fields page.
* return: TRUE if OK to move off page.
*/
static gboolean imp_csv_field_move() {
	gboolean retVal = FALSE;
	gchar *newFile;
	AddressBookFile *abf = NULL;

	if( _importedBook_ ) {
		addrbook_free_book( _importedBook_ );
	}

	abf = addrbook_create_book();
	addrbook_set_path( abf, _imp_addressIndex_->filePath );
	addrbook_set_name( abf, impcsv_dlg.nameBook );
	newFile = imp_csv_guess_file( abf );
	addrbook_set_file( abf, newFile );
	g_free( newFile );

	/* Import data into file */
	if( imp_csv_import_data( impcsv_dlg.fileName, abf->addressCache ) == MGU_SUCCESS ) {
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
static gboolean imp_csv_file_move() {
	gboolean retVal = FALSE;
	gchar *sName;
	gchar *sFile;
	gchar *sMsg = NULL;
	gboolean errFlag = FALSE;

	sFile = gtk_editable_get_chars( GTK_EDITABLE(impcsv_dlg.file_entry), 0, -1 );
	g_strchug( sFile ); g_strchomp( sFile );

	sName = gtk_editable_get_chars( GTK_EDITABLE(impcsv_dlg.name_entry), 0, -1 );
	g_strchug( sName ); g_strchomp( sName );

	g_free( impcsv_dlg.nameBook );
	g_free( impcsv_dlg.fileName );
	impcsv_dlg.nameBook = sName;
	impcsv_dlg.fileName = sFile;

	gtk_entry_set_text( GTK_ENTRY(impcsv_dlg.file_entry), sFile );
	gtk_entry_set_text( GTK_ENTRY(impcsv_dlg.name_entry), sName );

	if (gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON(impcsv_dlg.comma_radiobtn))) {
		impcsv_dlg.delimiter = ',';
	} else {
		impcsv_dlg.delimiter = '\t';
	}

	if( *sFile == '\0'|| strlen( sFile ) < 1 ) {
		sMsg = _( "Please select a file." );
		gtk_widget_grab_focus(impcsv_dlg.file_entry);
		errFlag = TRUE;
	}

	if( *sName == '\0'|| strlen( sName ) < 1 ) {
		if( ! errFlag ) sMsg = _( "Address book name must be supplied." );
		gtk_widget_grab_focus(impcsv_dlg.name_entry);
		errFlag = TRUE;
	}

	if( ! errFlag ) {
		gchar *sFSFile;
		sFSFile = conv_filename_from_utf8( sFile );
		if ( ! imp_csv_load_fields( sFSFile ) ) {
			sMsg = _( "Error reading CSV fields." );
		} else {
			retVal = TRUE;
		}
		g_free( sFSFile );
	}
	imp_csv_status_show( sMsg );

	return retVal;
}

/*
 * Display finish page.
 */
static void imp_csv_finish_show() {
	gchar *sMsg;
	gchar *name;

	name = gtk_editable_get_chars( GTK_EDITABLE(impcsv_dlg.name_entry), 0, -1 );
	gtk_label_set_text( GTK_LABEL(impcsv_dlg.labelBook), name );
	g_free( name );
	gtk_label_set_text( GTK_LABEL(impcsv_dlg.labelFile), impcsv_dlg.fileName );
	gtk_label_set_text( GTK_LABEL(impcsv_dlg.labelRecords), itos( importCount ) );
	gtk_widget_set_sensitive( impcsv_dlg.btnPrev, FALSE );
	gtk_widget_set_sensitive( impcsv_dlg.btnNext, FALSE );
	if( result == MGU_SUCCESS ) {
		sMsg = _( "CSV file imported successfully." );
	}
	else {
		sMsg = mgu_error2string( result );
	}
	imp_csv_status_show( sMsg );
	gtk_button_set_label(GTK_BUTTON(impcsv_dlg.btnCancel), GTK_STOCK_CLOSE);
	gtk_widget_grab_focus(impcsv_dlg.btnCancel);
}

static void imp_csv_prev( GtkWidget *widget ) {
	gint pageNum;

	pageNum = gtk_notebook_get_current_page( GTK_NOTEBOOK(impcsv_dlg.notebook) );
	if( pageNum == PAGE_ATTRIBUTES ) {
		/* Goto file page stuff */
		gtk_notebook_set_current_page(
			GTK_NOTEBOOK(impcsv_dlg.notebook), PAGE_FILE_INFO );
		gtk_widget_set_sensitive( impcsv_dlg.btnPrev, FALSE );
	}
	imp_csv_message();
}

static void imp_csv_next( GtkWidget *widget ) {
	gint pageNum;

	pageNum = gtk_notebook_get_current_page( GTK_NOTEBOOK(impcsv_dlg.notebook) );
	if( pageNum == PAGE_FILE_INFO ) {
		/* Goto attributes stuff */
		if( imp_csv_file_move() ) {
			gtk_notebook_set_current_page(
				GTK_NOTEBOOK(impcsv_dlg.notebook), PAGE_ATTRIBUTES );
			imp_csv_message();
			gtk_widget_set_sensitive( impcsv_dlg.btnPrev, TRUE );
		}
		else {
			gtk_widget_set_sensitive( impcsv_dlg.btnPrev, FALSE );
		}
	}
	else if( pageNum == PAGE_ATTRIBUTES ) {
		/* Goto finish stuff */
		if( imp_csv_field_move() ) {
			gtk_notebook_set_current_page(
				GTK_NOTEBOOK(impcsv_dlg.notebook), PAGE_FINISH );
			imp_csv_finish_show();
		}
	}
}

static void imp_csv_cancel( GtkWidget *widget, gpointer data ) {
	gint pageNum;

	pageNum = gtk_notebook_get_current_page( GTK_NOTEBOOK(impcsv_dlg.notebook) );
	if( pageNum != PAGE_FINISH ) {
		impcsv_dlg.cancelled = TRUE;
	}
	gtk_main_quit();
}

static void imp_csv_file_select( void ) {
	gchar *sSelFile;

	sSelFile = filesel_select_file( _("Select CSV File"), NULL,
				        GTK_FILE_CHOOSER_ACTION_OPEN );
	if (sSelFile) {
		gchar *sUTF8File;
		sUTF8File = conv_filename_to_utf8( sSelFile );
		gtk_entry_set_text( GTK_ENTRY(impcsv_dlg.file_entry), sUTF8File );
		g_free( sUTF8File );
		g_free( sSelFile );
	}
}

static gint imp_csv_delete_event( GtkWidget *widget, GdkEventAny *event, gpointer data ) {
	imp_csv_cancel( widget, data );
	return TRUE;
}

static gboolean imp_csv_key_pressed( GtkWidget *widget, GdkEventKey *event, gpointer data ) {
	if (event && event->keyval == GDK_Escape) {
		imp_csv_cancel( widget, data );
	}
	return FALSE;
}

static void imp_csv_page_file( gint pageNum, gchar *pageLbl ) {
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *file_entry;
	GtkWidget *name_entry;
	GtkWidget *file_btn;
	GtkWidget *hbox;
	GtkWidget *comma_radiobtn;
	GtkWidget *tab_radiobtn;
	gint top;

	vbox = gtk_vbox_new(FALSE, 4);
	gtk_container_add( GTK_CONTAINER( impcsv_dlg.notebook ), vbox );
	gtk_container_set_border_width( GTK_CONTAINER (vbox), 4 );

	label = gtk_label_new( pageLbl );
	gtk_widget_show( label );
	gtk_notebook_set_tab_label(
		GTK_NOTEBOOK( impcsv_dlg.notebook ),
		gtk_notebook_get_nth_page(
			GTK_NOTEBOOK( impcsv_dlg.notebook ), pageNum ),
		label );

	table = gtk_table_new(2, 3, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	gtk_container_set_border_width( GTK_CONTAINER(table), 6 );
	gtk_table_set_row_spacings(GTK_TABLE(table), 8);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

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

	file_btn = gtk_button_new_with_label(_(" ... "));
	gtk_table_attach(GTK_TABLE(table), file_btn, 2, 3, top, (top + 1),
		GTK_FILL, 0, 3, 0);

	hbox = gtk_hbox_new(FALSE, 8);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_container_set_border_width( GTK_CONTAINER (hbox), 4 );

	comma_radiobtn = gtk_radio_button_new_with_label
		(NULL, _("Comma-separated"));
	gtk_box_pack_start(GTK_BOX(hbox), comma_radiobtn, FALSE, FALSE, 0);

	tab_radiobtn = gtk_radio_button_new_with_label_from_widget
		(GTK_RADIO_BUTTON(comma_radiobtn), _("Tab-separated"));
	gtk_box_pack_start(GTK_BOX(hbox), tab_radiobtn, FALSE, FALSE, 0);

	gtk_widget_show_all(vbox);

	/* Button handler */
	g_signal_connect(G_OBJECT(file_btn), "clicked",
			 G_CALLBACK(imp_csv_file_select), NULL);

	impcsv_dlg.file_entry = file_entry;
	impcsv_dlg.name_entry = name_entry;
	impcsv_dlg.comma_radiobtn = comma_radiobtn;
	impcsv_dlg.tab_radiobtn = tab_radiobtn;
}

static void imp_csv_page_fields( gint pageNum, gchar *pageLbl ) {
	GtkWidget *vbox;
	GtkWidget *hbox;

	GtkWidget *label;
	GtkWidget *clist_swin;
	GtkWidget *clist_field;

	GtkWidget *btn_vbox;
	GtkWidget *btn_vbox1;
	GtkWidget *up_btn;
	GtkWidget *down_btn;

	gchar *titles[ FIELDS_N_COLS ];
	gint i;

	titles[ FIELD_COL_SELECT ] = _("S");
	titles[ FIELD_COL_FIELD  ] = _("CSV Field");
	titles[ FIELD_COL_ATTRIB ] = _("Address Book Field");

	vbox = gtk_vbox_new(FALSE, 4);
	gtk_container_add( GTK_CONTAINER( impcsv_dlg.notebook ), vbox );
	gtk_container_set_border_width( GTK_CONTAINER (vbox), 4 );

	label = gtk_label_new( pageLbl );
	gtk_widget_show( label );
	gtk_notebook_set_tab_label(
		GTK_NOTEBOOK( impcsv_dlg.notebook ),
		gtk_notebook_get_nth_page(GTK_NOTEBOOK( impcsv_dlg.notebook ), pageNum ),
		label );

	hbox = gtk_hbox_new( FALSE, 0 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 4 );

	label = gtk_label_new
		( _("Reorder address book fields with the Up and Down button.") );
	gtk_box_pack_start( GTK_BOX( hbox ), label, FALSE, FALSE, 0 );
	gtk_label_set_justify( GTK_LABEL( label ), GTK_JUSTIFY_LEFT );
	gtk_label_set_line_wrap( GTK_LABEL( label ), TRUE );

	hbox = gtk_hbox_new( FALSE, 4 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, TRUE, TRUE, 0 );

	clist_swin = gtk_scrolled_window_new( NULL, NULL );
	gtk_box_pack_start( GTK_BOX(hbox), clist_swin, TRUE, TRUE, 0 );
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

	btn_vbox = gtk_vbox_new( FALSE, 0 );
	gtk_box_pack_start( GTK_BOX( hbox ), btn_vbox, FALSE, FALSE, 0 );
	gtk_container_set_border_width( GTK_CONTAINER (btn_vbox), 4 );

	btn_vbox1 = gtk_vbox_new( FALSE, 8 );
	gtk_box_pack_start( GTK_BOX( btn_vbox ), btn_vbox1, TRUE, FALSE, 0 );

	up_btn = gtk_button_new_with_label( _("Up") );
	gtk_box_pack_start( GTK_BOX( btn_vbox1 ), up_btn, FALSE, FALSE, 0 );
	down_btn = gtk_button_new_with_label( _("Down") );
	gtk_box_pack_start( GTK_BOX( btn_vbox1 ), down_btn, FALSE, FALSE, 0 );

	gtk_widget_show_all(vbox);

	g_signal_connect( G_OBJECT(clist_field), "select_row",
			  G_CALLBACK(imp_csv_field_list_selected), NULL );
	g_signal_connect( G_OBJECT(up_btn), "clicked",
			  G_CALLBACK(imp_csv_field_list_up), NULL );
	g_signal_connect( G_OBJECT(down_btn), "clicked",
			  G_CALLBACK(imp_csv_field_list_down), NULL );

	impcsv_dlg.clist_field  = clist_field;
}

static void imp_csv_page_finish( gint pageNum, gchar *pageLbl ) {
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *labelBook;
	GtkWidget *labelFile;
	GtkWidget *labelRecs;
	gint top;

	vbox = gtk_vbox_new(FALSE, 8);
	gtk_container_add( GTK_CONTAINER( impcsv_dlg.notebook ), vbox );
	gtk_container_set_border_width( GTK_CONTAINER (vbox), BORDER_WIDTH );

	label = gtk_label_new( pageLbl );
	gtk_widget_show( label );
	gtk_notebook_set_tab_label(
		GTK_NOTEBOOK( impcsv_dlg.notebook ),
		gtk_notebook_get_nth_page( GTK_NOTEBOOK( impcsv_dlg.notebook ), pageNum ), label );

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
	gtk_label_set_line_wrap(GTK_LABEL(labelFile), TRUE);

	/* Third row */
	top++;
	label = gtk_label_new(_("Records :"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, top, (top + 1), GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);

	labelRecs = gtk_label_new("");
	gtk_table_attach(GTK_TABLE(table), labelRecs, 1, 2, top, (top + 1), GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(labelRecs), 0, 0.5);

	impcsv_dlg.labelBook    = labelBook;
	impcsv_dlg.labelFile    = labelFile;
	impcsv_dlg.labelRecords = labelRecs;
}

static void imp_csv_dialog_create() {
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
	gtk_widget_set_size_request(window, IMPORTCSV_WIDTH, IMPORTCSV_HEIGHT );
	gtk_window_set_title( GTK_WINDOW(window), _("Import CSV file into Address Book") );
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);	
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(imp_csv_delete_event), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(imp_csv_key_pressed), NULL);
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
			 G_CALLBACK(imp_csv_prev), NULL);
	g_signal_connect(G_OBJECT(btnNext), "clicked",
			 G_CALLBACK(imp_csv_next), NULL);
	g_signal_connect(G_OBJECT(btnCancel), "clicked",
			 G_CALLBACK(imp_csv_cancel), NULL);

	gtk_widget_show_all(vbox);

	impcsv_dlg.window     = window;
	impcsv_dlg.notebook   = notebook;
	impcsv_dlg.btnPrev    = btnPrev;
	impcsv_dlg.btnNext    = btnNext;
	impcsv_dlg.btnCancel  = btnCancel;
	impcsv_dlg.statusbar  = statusbar;
	impcsv_dlg.status_cid = gtk_statusbar_get_context_id(
			GTK_STATUSBAR(statusbar), "Import CSV Dialog" );

}

static void imp_csv_create() {
	imp_csv_dialog_create();
	imp_csv_page_file( PAGE_FILE_INFO, _( "File Info" ) );
	imp_csv_page_fields( PAGE_ATTRIBUTES, _( "Fields" ) );
	imp_csv_page_finish( PAGE_FINISH, _( "Finish" ) );
	gtk_widget_show_all( impcsv_dlg.window );
}

AddressBookFile *addressbook_imp_csv( AddressIndex *addrIndex ) {
	_importedBook_ = NULL;
	_imp_addressIndex_ = addrIndex;

	if( ! impcsv_dlg.window )
		imp_csv_create();
	impcsv_dlg.cancelled = FALSE;
	manage_window_set_transient(GTK_WINDOW(impcsv_dlg.window));
	gtk_widget_grab_default(impcsv_dlg.btnNext);

	gtk_entry_set_text( GTK_ENTRY(impcsv_dlg.name_entry), IMPORTCSV_GUESS_NAME );
	gtk_entry_set_text( GTK_ENTRY(impcsv_dlg.file_entry), "" );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(impcsv_dlg.comma_radiobtn), TRUE );
	gtk_clist_clear( GTK_CLIST(impcsv_dlg.clist_field) );
	gtk_notebook_set_current_page( GTK_NOTEBOOK(impcsv_dlg.notebook), PAGE_FILE_INFO );
	gtk_widget_set_sensitive( impcsv_dlg.btnPrev, FALSE );
	gtk_widget_set_sensitive( impcsv_dlg.btnNext, TRUE );
	gtk_button_set_label( GTK_BUTTON(impcsv_dlg.btnCancel), GTK_STOCK_CANCEL );
	stock_pixmap_gdk( impcsv_dlg.window, STOCK_PIXMAP_MARK,
			  &markxpm, &markxpmmask );
	imp_csv_message();
	gtk_widget_grab_focus(impcsv_dlg.file_entry);

	impcsv_dlg.rowCount = 0;
	g_free( impcsv_dlg.nameBook );
	g_free( impcsv_dlg.fileName );
	impcsv_dlg.nameBook = NULL;
	impcsv_dlg.fileName = NULL;
	impcsv_dlg.delimiter = ',';
	importCount = 0;

	gtk_widget_show(impcsv_dlg.window);

	gtk_main();
	gtk_widget_hide(impcsv_dlg.window);
	_imp_addressIndex_ = NULL;

	g_free( impcsv_dlg.nameBook );
	g_free( impcsv_dlg.fileName );
	impcsv_dlg.nameBook = NULL;
	impcsv_dlg.fileName = NULL;

	if( impcsv_dlg.cancelled == TRUE ) return NULL;
	return _importedBook_;
}

/*
* End of Source.
*/

