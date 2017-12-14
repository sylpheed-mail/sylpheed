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
#include "exportcsv.h"
#include "utils.h"

#define PAGE_FILE_INFO             0
#define PAGE_FINISH                1

#define EXPORTCSV_WIDTH           480
#define EXPORTCSV_HEIGHT          320

static struct _ExpCSVDlg {
	GtkWidget *window;
	GtkWidget *notebook;
	GtkWidget *file_entry;
	GtkWidget *comma_radiobtn;
	GtkWidget *tab_radiobtn;
	GtkWidget *labelFile;
	GtkWidget *labelRecords;
	GtkWidget *btnNext;
	GtkWidget *btnCancel;
	GtkWidget *statusbar;
	gint      status_cid;
	gint      rowCount;
	gchar     *fileName;
	gchar     delimiter;
	gboolean  cancelled;
} expcsv_dlg;

static AddressObject *_exp_object_;
static gint exportCount = 0;
static gint result;

static void exp_csv_status_show( gchar *msg ) {
	if( expcsv_dlg.statusbar != NULL ) {
		gtk_statusbar_pop( GTK_STATUSBAR(expcsv_dlg.statusbar), expcsv_dlg.status_cid );
		if( msg ) {
			gtk_statusbar_push( GTK_STATUSBAR(expcsv_dlg.statusbar), expcsv_dlg.status_cid, msg );
		}
	}
}

static void exp_csv_message( void ) {
	gchar *sMsg = NULL;
	gint pageNum;

	pageNum = gtk_notebook_get_current_page( GTK_NOTEBOOK(expcsv_dlg.notebook) );
	if( pageNum == PAGE_FILE_INFO ) {
		sMsg = _( "Please specify file to export." );
	}
	else if( pageNum == PAGE_FINISH ) {
		sMsg = _( "File exported." );
	}
	exp_csv_status_show( sMsg );
}

static gint exp_csv_write_email_record( ItemPerson *person, ItemEMail *email, FILE *fp ) {
	gchar *sName, *sAddress, *sAlias, *sNickName;
	gchar *sFirstName, *sLastName;
	gchar *sRemarks;
	gchar *csv;

	g_return_val_if_fail(person != NULL, MGU_BAD_ARGS);

	sName = ADDRITEM_NAME(person) ? ADDRITEM_NAME(person) : "";
	sFirstName = person->firstName ? person->firstName : "";
	sLastName = person->lastName ? person->lastName : "";
	sNickName = person->nickName ? person->nickName : "";

	if (email) {
		sAddress = email->address ? email->address : "";
		sRemarks = email->remarks ? email->remarks : "";
		sAlias = ADDRITEM_NAME(email) ? ADDRITEM_NAME(email) : "";
	} else {
		sAddress = "";
		sRemarks = "";
		sAlias = "";
	}

	csv = strconcat_csv(expcsv_dlg.delimiter, sFirstName, sLastName, sName, sNickName, sAddress, sRemarks, sAlias, NULL);
	debug_print("%s\n", csv);
	fputs(csv, fp);
	fputc('\n', fp);
	g_free(csv);

	return MGU_SUCCESS;
}

static gint exp_csv_export_folder_recursive( ItemFolder *itemFolder, FILE *fp ) {
	GList *items;
	GList *cur;
	gint count = 0;

	g_return_val_if_fail(itemFolder != NULL, MGU_BAD_ARGS);

	debug_print("exp_csv_export_folder_recursive: export folder: %s\n", ADDRITEM_NAME(itemFolder) ? ADDRITEM_NAME(itemFolder) : "(root)");

	items = addritem_folder_get_person_list(itemFolder);
	for (cur = items; cur != NULL; cur = cur->next) {
		ItemPerson *person;
		ItemEMail *email;
		GList *node;

		person = (ItemPerson *)cur->data;
		if (!person) {
			continue;
		}

		node = person->listEMail;
		if (!node) {
			if (exp_csv_write_email_record(person, NULL, fp) == MGU_SUCCESS) {
				count++;
			}
		} else {
			while (node) {
				email = node->data;
				node = g_list_next(node);
				if (exp_csv_write_email_record(person, email, fp) == MGU_SUCCESS) {
					count++;
				}
			}
		}
	}
	g_list_free(items);

	exportCount += count;

	/* Export subfolders */
	items = itemFolder->listFolder;
	for (cur = items; cur != NULL; cur = cur->next) {
		ItemFolder *item = cur->data;
		exp_csv_export_folder_recursive(item, fp);
	}

	return MGU_SUCCESS;
}

static gint exp_csv_export_data( gchar *csvFile ) {
	FILE *fp;
	ItemFolder *itemFolder = NULL;
	ItemGroup *itemGroup = NULL;
	GList *items;
	GList *cur;
	gchar *csv;
	gint count = 0;

	g_return_val_if_fail(csvFile != NULL, MGU_BAD_ARGS);

	if (_exp_object_->type == ADDR_INTERFACE) {
		return MGU_BAD_ARGS;
	}

	if ((fp = g_fopen(csvFile, "w")) == NULL) {
		return MGU_OPEN_FILE;
	}

	if (_exp_object_->type == ADDR_DATASOURCE) {
		AddressDataSource *ds = ADAPTER_DSOURCE(_exp_object_)->dataSource;

		if (ds) {
			itemFolder = addrindex_ds_get_root_folder(ds);
		}
	} else if (_exp_object_->type == ADDR_ITEM_FOLDER) {
		itemFolder = ADAPTER_FOLDER(_exp_object_)->itemFolder;
	} else if (_exp_object_->type == ADDR_ITEM_GROUP) {
		itemGroup = ADAPTER_GROUP(_exp_object_)->itemGroup;
	}

	csv = strconcat_csv(expcsv_dlg.delimiter, _("First Name"), _("Last Name"), _("Display Name"), _("Nick Name"), _("E-Mail Address"), _("Remarks"), _("Alias"), NULL);
	debug_print("%s\n", csv);
	fputs(csv, fp);
	fputc('\n', fp);
	g_free(csv);

	if (itemFolder) {
		exp_csv_export_folder_recursive(itemFolder, fp);
	} else if (itemGroup) {
		items = itemGroup->listEMail;
		for (cur = items; cur != NULL; cur = cur->next) {
			ItemEMail *email = (ItemEMail *)cur->data;
			ItemPerson *person;

			if (!email) {
				continue;
			}
			person = (ItemPerson *)ADDRITEM_PARENT(email);
			if (!person) {
				continue;
			}

			if (exp_csv_write_email_record(person, email, fp) == MGU_SUCCESS) {
				count++;
			}
		}
		exportCount = count;
	}

	fclose(fp);

	return MGU_SUCCESS;
}

/*
* Move off file page.
* return: TRUE if OK to move off page.
*/
static gboolean exp_csv_file_move() {
	gboolean retVal = FALSE;
	gchar *sFile;
	gchar *sMsg = NULL;
	gboolean errFlag = FALSE;

	sFile = gtk_editable_get_chars( GTK_EDITABLE(expcsv_dlg.file_entry), 0, -1 );
	g_strchug( sFile ); g_strchomp( sFile );

	g_free( expcsv_dlg.fileName );
	expcsv_dlg.fileName = sFile;

	gtk_entry_set_text( GTK_ENTRY(expcsv_dlg.file_entry), sFile );

	if (gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON(expcsv_dlg.comma_radiobtn))) {
		expcsv_dlg.delimiter = ',';
	} else {
		expcsv_dlg.delimiter = '\t';
	}

	if( *sFile == '\0'|| strlen( sFile ) < 1 ) {
		sMsg = _( "Please select a file." );
		gtk_widget_grab_focus(expcsv_dlg.file_entry);
		errFlag = TRUE;
	}

	if( ! errFlag ) {
		if( exp_csv_export_data( sFile ) == MGU_SUCCESS ) {
			retVal = TRUE;
		} else {
			sMsg = _( "Cannot write to file." );
		}
	}
	exp_csv_status_show( sMsg );

	return retVal;
}

/*
 * Display finish page.
 */
static void exp_csv_finish_show() {
	gchar *sMsg;

	gtk_label_set_text( GTK_LABEL(expcsv_dlg.labelFile), expcsv_dlg.fileName );
	gtk_label_set_text( GTK_LABEL(expcsv_dlg.labelRecords), itos( exportCount ) );
	gtk_widget_set_sensitive( expcsv_dlg.btnNext, FALSE );
	if( result == MGU_SUCCESS ) {
		sMsg = _( "CSV file exported successfully." );
	}
	else {
		sMsg = mgu_error2string( result );
	}
	exp_csv_status_show( sMsg );
	gtk_button_set_label(GTK_BUTTON(expcsv_dlg.btnCancel), GTK_STOCK_CLOSE);
	gtk_widget_grab_focus(expcsv_dlg.btnCancel);
}

static void exp_csv_next( GtkWidget *widget ) {
	gint pageNum;

	pageNum = gtk_notebook_get_current_page( GTK_NOTEBOOK(expcsv_dlg.notebook) );
	if( pageNum == PAGE_FILE_INFO ) {
		if( exp_csv_file_move() ) {
			gtk_notebook_set_current_page(
				GTK_NOTEBOOK(expcsv_dlg.notebook), PAGE_FINISH );
			exp_csv_finish_show();
		}
	}
}

static void exp_csv_cancel( GtkWidget *widget, gpointer data ) {
	gint pageNum;

	pageNum = gtk_notebook_get_current_page( GTK_NOTEBOOK(expcsv_dlg.notebook) );
	if( pageNum != PAGE_FINISH ) {
		expcsv_dlg.cancelled = TRUE;
	}
	gtk_main_quit();
}

static void exp_csv_file_select( void ) {
	gchar *sSelFile;

	sSelFile = filesel_select_file( _("Enter CSV File Name"), NULL,
				        GTK_FILE_CHOOSER_ACTION_SAVE );
	if (sSelFile) {
		gchar *sUTF8File;
		sUTF8File = conv_filename_to_utf8( sSelFile );
		gtk_entry_set_text( GTK_ENTRY(expcsv_dlg.file_entry), sUTF8File );
		g_free( sUTF8File );
		g_free( sSelFile );
	}
}

static gint exp_csv_delete_event( GtkWidget *widget, GdkEventAny *event, gpointer data ) {
	exp_csv_cancel( widget, data );
	return TRUE;
}

static gboolean exp_csv_key_pressed( GtkWidget *widget, GdkEventKey *event, gpointer data ) {
	if (event && event->keyval == GDK_Escape) {
		exp_csv_cancel( widget, data );
	}
	return FALSE;
}

static void exp_csv_page_file( gint pageNum, gchar *pageLbl ) {
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *file_entry;
	GtkWidget *file_btn;
	GtkWidget *hbox;
	GtkWidget *comma_radiobtn;
	GtkWidget *tab_radiobtn;
	gint top;

	vbox = gtk_vbox_new(FALSE, 4);
	gtk_container_add( GTK_CONTAINER( expcsv_dlg.notebook ), vbox );
	gtk_container_set_border_width( GTK_CONTAINER (vbox), 4 );

	label = gtk_label_new( pageLbl );
	gtk_widget_show( label );
	gtk_notebook_set_tab_label(
		GTK_NOTEBOOK( expcsv_dlg.notebook ),
		gtk_notebook_get_nth_page(
			GTK_NOTEBOOK( expcsv_dlg.notebook ), pageNum ),
		label );

	table = gtk_table_new(2, 3, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	gtk_container_set_border_width( GTK_CONTAINER(table), 6 );
	gtk_table_set_row_spacings(GTK_TABLE(table), 8);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	/* First row */
	top = 0;
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
			 G_CALLBACK(exp_csv_file_select), NULL);

	expcsv_dlg.file_entry = file_entry;
	expcsv_dlg.comma_radiobtn = comma_radiobtn;
	expcsv_dlg.tab_radiobtn = tab_radiobtn;
}

static void exp_csv_page_finish( gint pageNum, gchar *pageLbl ) {
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *labelFile;
	GtkWidget *labelRecs;
	gint top;

	vbox = gtk_vbox_new(FALSE, 8);
	gtk_container_add( GTK_CONTAINER( expcsv_dlg.notebook ), vbox );
	gtk_container_set_border_width( GTK_CONTAINER (vbox), BORDER_WIDTH );

	label = gtk_label_new( pageLbl );
	gtk_widget_show( label );
	gtk_notebook_set_tab_label(
		GTK_NOTEBOOK( expcsv_dlg.notebook ),
		gtk_notebook_get_nth_page( GTK_NOTEBOOK( expcsv_dlg.notebook ), pageNum ), label );

	table = gtk_table_new(3, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	gtk_container_set_border_width( GTK_CONTAINER(table), 8 );
	gtk_table_set_row_spacings(GTK_TABLE(table), 8);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8 );

	/* First row */
	top = 0;
	label = gtk_label_new(_("File Name :"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, top, (top + 1), GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);

	labelFile = gtk_label_new("");
	gtk_table_attach(GTK_TABLE(table), labelFile, 1, 2, top, (top + 1), GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(labelFile), 0, 0.5);
	gtk_label_set_line_wrap(GTK_LABEL(labelFile), TRUE);

	/* Secondrow */
	top++;
	label = gtk_label_new(_("Records :"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, top, (top + 1), GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);

	labelRecs = gtk_label_new("");
	gtk_table_attach(GTK_TABLE(table), labelRecs, 1, 2, top, (top + 1), GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(labelRecs), 0, 0.5);

	expcsv_dlg.labelFile    = labelFile;
	expcsv_dlg.labelRecords = labelRecs;
}

static void exp_csv_dialog_create() {
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *vnbox;
	GtkWidget *notebook;
	GtkWidget *hbbox;
	GtkWidget *btnNext;
	GtkWidget *btnCancel;
	GtkWidget *hsbox;
	GtkWidget *statusbar;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_size_request(window, EXPORTCSV_WIDTH, EXPORTCSV_HEIGHT );
	gtk_window_set_title( GTK_WINDOW(window), _("Export Address Book into CSV file") );
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);	
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(exp_csv_delete_event), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(exp_csv_key_pressed), NULL);
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
	gtkut_stock_button_set_create(&hbbox, &btnNext, _("Next"),
				      &btnCancel, GTK_STOCK_CANCEL,
				      NULL, NULL);
	gtk_box_pack_end(GTK_BOX(vnbox), hbbox, FALSE, FALSE, 0);
	gtk_widget_grab_default(btnNext);

	/* Button handlers */
	g_signal_connect(G_OBJECT(btnNext), "clicked",
			 G_CALLBACK(exp_csv_next), NULL);
	g_signal_connect(G_OBJECT(btnCancel), "clicked",
			 G_CALLBACK(exp_csv_cancel), NULL);

	gtk_widget_show_all(vbox);

	expcsv_dlg.window     = window;
	expcsv_dlg.notebook   = notebook;
	expcsv_dlg.btnNext    = btnNext;
	expcsv_dlg.btnCancel  = btnCancel;
	expcsv_dlg.statusbar  = statusbar;
	expcsv_dlg.status_cid = gtk_statusbar_get_context_id(
			GTK_STATUSBAR(statusbar), "Export CSV Dialog" );

}

static void exp_csv_create() {
	exp_csv_dialog_create();
	exp_csv_page_file( PAGE_FILE_INFO, _( "File Info" ) );
	exp_csv_page_finish( PAGE_FINISH, _( "Finish" ) );
	gtk_widget_show_all( expcsv_dlg.window );
}

gint addressbook_exp_csv( AddressObject *obj ) {
	g_return_val_if_fail(obj != NULL, -1);

	if (obj->type == ADDR_INTERFACE)
		return -1;

	_exp_object_ = obj;

	if( ! expcsv_dlg.window )
		exp_csv_create();
	expcsv_dlg.cancelled = FALSE;
	manage_window_set_transient(GTK_WINDOW(expcsv_dlg.window));
	gtk_widget_grab_default(expcsv_dlg.btnNext);

	gtk_entry_set_text( GTK_ENTRY(expcsv_dlg.file_entry), "" );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(expcsv_dlg.comma_radiobtn), TRUE );
	gtk_notebook_set_current_page( GTK_NOTEBOOK(expcsv_dlg.notebook), PAGE_FILE_INFO );
	gtk_widget_set_sensitive( expcsv_dlg.btnNext, TRUE );
	gtk_button_set_label( GTK_BUTTON(expcsv_dlg.btnCancel), GTK_STOCK_CANCEL );
	exp_csv_message();
	gtk_widget_grab_focus(expcsv_dlg.file_entry);

	expcsv_dlg.rowCount = 0;
	g_free( expcsv_dlg.fileName );
	expcsv_dlg.fileName = NULL;
	expcsv_dlg.delimiter = ',';
	exportCount = 0;

	gtk_widget_show(expcsv_dlg.window);

	gtk_main();
	gtk_widget_hide(expcsv_dlg.window);

	if (expcsv_dlg.cancelled == FALSE && expcsv_dlg.fileName) {
		debug_print("addressbook_exp_csv: exported to %s\n", expcsv_dlg.fileName);
	}

	g_free( expcsv_dlg.fileName );
	expcsv_dlg.fileName = NULL;
	_exp_object_ = NULL;

	if( expcsv_dlg.cancelled == TRUE ) return -1;
	return 0;
}

/*
* End of Source.
*/

