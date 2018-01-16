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

/*
 * DBX file conversion engine is based on OutlookExpress-To by Tietew.
 * OutlookExpress-To - OE5/6 Multi Converter
 * Copyright (C) 2002 by Tietew
 * http://www.tietew.net/
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtktable.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtkmenu.h>

#include <stdio.h>

#include "main.h"
#include "inc.h"
#include "mbox.h"
#include "folderview.h"
#include "menu.h"
#include "filesel.h"
#include "foldersel.h"
#include "gtkutils.h"
#include "manage_window.h"
#include "folder.h"
#include "procmsg.h"
#include "procheader.h"
#include "progressdialog.h"
#include "alertpanel.h"

enum
{
	IMPORT_MBOX,
	IMPORT_EML_FOLDER,
	IMPORT_DBX
};

static GtkWidget *window;
static GtkWidget *format_optmenu;
static GtkWidget *desc_label;
static GtkWidget *file_label;
static GtkWidget *file_entry;
static GtkWidget *dest_entry;
static GtkWidget *file_button;
static GtkWidget *dest_button;
static GtkWidget *ok_button;
static GtkWidget *cancel_button;
static gboolean import_finished;
static gboolean import_ack;
static ProgressDialog *progress;
static gboolean import_progress_cancelled;

static void import_create	(void);
static gint import_do		(void);
static gint import_eml_folder	(FolderItem	*dest,
				 const gchar	*path);
static gint import_dbx		(FolderItem	*dest,
				 const gchar	*file);

static void import_format_menu_cb	(GtkWidget	*widget,
					 gpointer	 data);

static void import_ok_cb	(GtkWidget	*widget,
				 gpointer	 data);
static void import_cancel_cb	(GtkWidget	*widget,
				 gpointer	 data);
static void import_filesel_cb	(GtkWidget	*widget,
				 gpointer	 data);
static void import_destsel_cb	(GtkWidget	*widget,
				 gpointer	 data);
static gint delete_event	(GtkWidget	*widget,
				 GdkEventAny	*event,
				 gpointer	 data);
static gboolean key_pressed	(GtkWidget	*widget,
				 GdkEventKey	*event,
				 gpointer	 data);

static void import_progress_cancel_cb	(GtkWidget	*widget,
					 gpointer	 data);
static gint import_progress_delete_event(GtkWidget	*widget,
					 GdkEventAny	*event,
					 gpointer	 data);


static gboolean import_mbox_func(Folder *folder, FolderItem *item, guint count, guint total, gpointer data)
{
	gchar str[64];
	static GTimeVal tv_prev = {0, 0};
	GTimeVal tv_cur;

	g_get_current_time(&tv_cur);
	g_snprintf(str, sizeof(str), "%u", count);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress->progressbar), str);

	if (tv_prev.tv_sec == 0 ||
	    (tv_cur.tv_sec - tv_prev.tv_sec) * G_USEC_PER_SEC +
	    tv_cur.tv_usec - tv_prev.tv_usec > 100 * 1000) {
		gtk_progress_bar_pulse(GTK_PROGRESS_BAR(progress->progressbar));
		ui_update();
		tv_prev = tv_cur;
	}

	if (import_progress_cancelled)
		return FALSE;
	else
		return TRUE;
}

gint import_mail(FolderItem *default_dest)
{
	gint ok = 0;
	gchar *dest_id = NULL;

	import_create();

	if (default_dest && default_dest->path)
		dest_id = folder_item_get_identifier(default_dest);

	if (dest_id) {
		gtk_entry_set_text(GTK_ENTRY(dest_entry), dest_id);
		g_free(dest_id);
	}
	gtk_widget_grab_focus(file_entry);

	manage_window_set_transient(GTK_WINDOW(window));

	import_finished = FALSE;
	import_ack = FALSE;
	import_progress_cancelled = FALSE;

	inc_lock();

	while (!import_finished)
		gtk_main_iteration();

	if (import_ack)
		ok = import_do();

	gtk_widget_destroy(window);
	window = NULL;
	format_optmenu = NULL;
	desc_label = file_label = NULL;
	file_entry = dest_entry = NULL;
	file_button = dest_button = ok_button = cancel_button = NULL;

	inc_unlock();

	return ok;
}

static gint import_do(void)
{
	gint ok = 0;
	const gchar *utf8filename, *destdir;
	FolderItem *dest;
	gchar *filename;
	gchar *msg;
	gint type;

	type = menu_get_option_menu_active_index
		(GTK_OPTION_MENU(format_optmenu));

	utf8filename = gtk_entry_get_text(GTK_ENTRY(file_entry));
	destdir = gtk_entry_get_text(GTK_ENTRY(dest_entry));
	if (!utf8filename || !*utf8filename)
		return -1;

	filename = g_filename_from_utf8(utf8filename, -1, NULL, NULL, NULL);
	if (!filename) {
		g_warning("failed to convert character set.");
		filename = g_strdup(utf8filename);
	}
	if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
		alertpanel_error(_("The source file does not exist."));
		g_free(filename);
		return -1;
	}

	if (!destdir || !*destdir) {
		dest = folder_find_item_from_path(INBOX_DIR);
	} else
		dest = folder_find_item_from_identifier(destdir);

	if (!dest) {
		alertpanel_error(_("Can't find the destination folder."));
		g_free(filename);
		return -1;
	}

	msg = g_strdup_printf(_("Importing %s ..."), g_basename(utf8filename));
	progress = progress_dialog_simple_create();
	gtk_window_set_title(GTK_WINDOW(progress->window), _("Importing"));
	progress_dialog_set_label(progress, msg);
	g_free(msg);
	gtk_window_set_modal(GTK_WINDOW(progress->window), TRUE);
	manage_window_set_transient(GTK_WINDOW(progress->window));
	g_signal_connect(G_OBJECT(progress->cancel_btn), "clicked",
			 G_CALLBACK(import_progress_cancel_cb), NULL);
	g_signal_connect(G_OBJECT(progress->window), "delete_event",
			 G_CALLBACK(import_progress_delete_event), NULL);
	gtk_widget_show(progress->window);
	ui_update();

	if (type == IMPORT_MBOX) {
		folder_set_ui_func2(dest->folder, import_mbox_func, NULL);
		ok = proc_mbox(dest, filename, NULL);
		folder_set_ui_func2(dest->folder, NULL, NULL);
	} else if (type == IMPORT_EML_FOLDER) {
		ok = import_eml_folder(dest, filename);
	} else if (type == IMPORT_DBX) {
		ok = import_dbx(dest, filename);
	}

	progress_dialog_set_label(progress, _("Scanning folder..."));
	ui_update();
	folder_item_scan(dest);
	folderview_update_item(dest, TRUE);

	progress_dialog_destroy(progress);
	progress = NULL;

	g_free(filename);

	if (ok == -1)
		alertpanel_error(_("Error occurred on import."));

	return ok;
}

static gint import_eml_folder(FolderItem *dest, const gchar *path)
{
	GDir *dir;
	const gchar *dir_name, *p;
	gchar *file;
	MsgInfo *msginfo;
	MsgFlags flags = {MSG_NEW|MSG_UNREAD, MSG_RECEIVED};
	gint count = 0;
	gint ok = 0;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(path != NULL, -1);

	if ((dir = g_dir_open(path, 0, NULL)) == NULL) {
		g_warning("failed to open directory: %s", path);
		return -1;
	}

	while ((dir_name = g_dir_read_name(dir)) != NULL) {
		if (((p = strrchr(dir_name, '.')) &&
		     !g_ascii_strcasecmp(p + 1, "eml")) ||
		    to_number(dir_name) > 0) {
			file = g_strconcat(path, G_DIR_SEPARATOR_S, dir_name,
					   NULL);
			if (!g_file_test(file, G_FILE_TEST_IS_REGULAR)) {
				g_free(file);
				continue;
			}

			msginfo = procheader_parse_file(file, flags, FALSE);
			if (!msginfo) {
				g_warning("import_eml_folder(): procheader_parse_file() failed.");
				g_free(file);
				continue;
			}
			msginfo->file_path = file;
			file = NULL;
			count++;
			if (import_mbox_func(dest->folder, dest, count, 0, NULL) == FALSE) {
				ok = -2;
				break;
			}
			ok = folder_item_add_msg_msginfo(dest, msginfo, FALSE);
			procmsg_msginfo_free(msginfo);
			if (ok < 0) {
				g_warning("import_eml_folder(): folder_item_add_msg_msginfo() failed.");
				break;
			}
		}
	}

	g_dir_close(dir);

	return ok;
}

static gint32 read_dword(FILE *fp, off_t offset)
{
	gint32 dw;

	if (fseek(fp, offset, SEEK_SET) < 0) {
		perror("read_dword: fseek");
		return 0;
	}

	if (fread(&dw, sizeof(dw), 1, fp) != 1)
		return 0;

	dw = GINT32_FROM_LE(dw);

	return dw;
}

static void get_dbx_index(FILE *fp, gint32 table_pos, GArray *array)
{
	gint32 another_pos, data_pos;
	gint32 num_index, num_elems;
	gint i;

	debug_print("get_dbx_index(%08x)\n", table_pos);

	another_pos = read_dword(fp, table_pos + 0x08);
	num_elems = read_dword(fp, table_pos + 0x11) & 0x00ffffff;
	num_index = read_dword(fp, table_pos + 0x14);
	debug_print("table_pos: %08x another_pos: %08x num_elems: %08x num_index: %08x\n", table_pos, another_pos, num_elems, num_index);
	if (another_pos > 0 && num_index > 0)
		get_dbx_index(fp, another_pos, array);

	table_pos += 0x18;
	for (i = 0; i < num_elems; i++) {
		data_pos = read_dword(fp, table_pos);
		if (data_pos == 0) {
			g_warning("get_dbx_index: null data_pos at %08x",
				  table_pos);
			break;
		}
		g_array_append_val(array, data_pos);
		another_pos = read_dword(fp, table_pos + 0x04);
		num_index = read_dword(fp, table_pos + 0x08);
		debug_print("data_pos: %08x another_pos: %08x num_index: %08x\n", data_pos, another_pos, num_index);
		table_pos += 0x0c;
		if (another_pos > 0 && num_index > 0)
			get_dbx_index(fp, another_pos, array);
	}

	debug_print("get_dbx_index end\n");
}

static gint get_dbx_data(FILE *fp, gint32 data_pos, FolderItem *dest)
{
	gchar *tmp;
	FILE *outfp;
	gint32 mail_flag, news_flag;
	gint32 data_ptr, data_len, next_ptr;
	MsgFlags flags = {MSG_NEW|MSG_UNREAD, MSG_RECEIVED};
	gint ok = 0;

	debug_print("get_dbx_data(%08x)\n", data_pos);

	mail_flag = read_dword(fp, data_pos + 0x18);
	news_flag = read_dword(fp, data_pos + 0x1c);
	if ((news_flag & 0x0f) == 4)
		data_ptr = news_flag;
	else if ((mail_flag & 0x0f) == 4)
		data_ptr = mail_flag;
	else
		return 0;

	if ((data_ptr & 0xff) >= 0x80)
		data_ptr >>= 8;
	else {
		guchar b1 = (guchar)(data_ptr >>= 8);
		guchar b2 = (guchar)read_dword(fp, data_pos + 0x0a);
		data_ptr = read_dword(fp, data_pos + 0x0c + b2 * 4 + b1);
	}
	if (data_ptr == 0) {
		g_warning("get_dbx_data(%08x): could not get data_ptr.", data_pos);
		return 0;
	}

	tmp = get_tmp_file();
	if ((outfp = g_fopen(tmp, "wb")) == NULL) {
		FILE_OP_ERROR(tmp, "fopen");
		ok = -1;
		goto finish;
	}

	while (data_ptr) {
		data_len = read_dword(fp, data_ptr + 0x08);
		next_ptr = read_dword(fp, data_ptr + 0x0c);
		if (append_file_part(fp, data_ptr + 0x10, data_len, outfp) < 0) {
			fclose(outfp);
			g_unlink(tmp);
			ok = -1;
			goto finish;
		}
		data_ptr = next_ptr;
	}

	if (fclose(outfp) == EOF) {
		FILE_OP_ERROR(tmp, "fclose");
		g_unlink(tmp);
		ok = -1;
		goto finish;
	}

	if (folder_item_add_msg(dest, tmp, &flags, TRUE) < 0) {
		g_warning("get_dbx_data: folder_item_add_msg() failed.");
		g_unlink(tmp);
		ok = -1;
	}

finish:
	g_free(tmp);
	return ok;
}

static gint import_dbx(FolderItem *dest, const gchar *file)
{
	FILE *fp;
	gint32 dw;
	gint32 table_pos;
	guint count = 0;
	GArray *array;
	gint i;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(file != NULL, -1);

	if ((fp = g_fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return -1;
	}

	if ((dw = read_dword(fp, 0xc4)) == 0) {
		fclose(fp);
		return -1;
	}

	array = g_array_sized_new(FALSE, FALSE, sizeof(gint32), 1024);

	table_pos = read_dword(fp, 0xe4);
	if (table_pos > 0)
		get_dbx_index(fp, table_pos, array);

	for (i = 0; i < array->len; i++) {
		if (import_mbox_func(dest->folder, dest, count + 1, 0, NULL) == FALSE)
			break;
		if (get_dbx_data(fp, g_array_index(array, gint32, i), dest) < 0)
			break;
		count++;
	}

	debug_print("import_dbx: %u imported\n", count);

	g_array_free(array, TRUE);
	fclose(fp);

	return 0;
}

gint import_dbx_folders(FolderItem *dest, const gchar *path)
{
	GDir *dir;
	const gchar *dir_name, *p;
	gchar *orig_name, *folder_name, *file;
	gchar *msg;
	FolderItem *sub_folder;
	gint count;
	gint ok = 0;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(dest->folder != NULL, -1);
	g_return_val_if_fail(path != NULL, -1);

	if ((dir = g_dir_open(path, 0, NULL)) == NULL) {
		g_warning("failed to open directory: %s", path);
		return -1;
	}

	import_progress_cancelled = FALSE;

	progress = progress_dialog_simple_create();
	gtk_window_set_title(GTK_WINDOW(progress->window), _("Importing"));
	progress_dialog_set_label(progress, _("Importing Outlook Express folders"));
	gtk_window_set_modal(GTK_WINDOW(progress->window), TRUE);
	manage_window_set_transient(GTK_WINDOW(progress->window));
	g_signal_connect(G_OBJECT(progress->cancel_btn), "clicked",
			 G_CALLBACK(import_progress_cancel_cb), NULL);
	g_signal_connect(G_OBJECT(progress->window), "delete_event",
			 G_CALLBACK(import_progress_delete_event), NULL);
	gtk_widget_show(progress->window);
	ui_update();

	while ((dir_name = g_dir_read_name(dir)) != NULL) {
		if ((p = strrchr(dir_name, '.')) &&
		    !g_ascii_strcasecmp(p + 1, "dbx")) {
			file = g_strconcat(path, G_DIR_SEPARATOR_S, dir_name,
					   NULL);
			orig_name = g_strndup(dir_name, p - dir_name);
			if (!g_file_test(file, G_FILE_TEST_IS_REGULAR) ||
			    !g_ascii_strcasecmp(orig_name, "Folders") ||
			    !g_ascii_strcasecmp(orig_name, "Offline") ||
			    !g_ascii_strcasecmp(orig_name, "Pop3uidl")) {
				g_free(orig_name);
				g_free(file);
				continue;
			}

			folder_name = g_strdup(orig_name);
			count = 1;
			while (folder_find_child_item_by_name(dest, folder_name)) {
				g_free(folder_name);
				folder_name = g_strdup_printf("%s (%d)", orig_name, count++);
			}
			debug_print("orig_name: %s , folder_name: %s\n", orig_name, folder_name);

			sub_folder = dest->folder->klass->create_folder(dest->folder, dest, folder_name);
			if (!sub_folder) {
				alertpanel_error(_("Cannot create the folder '%s'."), folder_name);
				ok = -1;
				break;
			}
			folderview_append_item(folderview_get(), NULL, sub_folder, TRUE);
			folder_write_list();
			msg = g_strdup_printf(_("Importing %s ..."), orig_name);
			progress_dialog_set_label(progress, msg);
			g_free(msg);
			import_dbx(sub_folder, file);

			progress_dialog_set_label(progress, _("Scanning folder..."));
			ui_update();
			folder_item_scan(sub_folder);
			folderview_update_item(sub_folder, TRUE);

			g_free(folder_name);
			g_free(orig_name);
			g_free(file);
			if (import_progress_cancelled) {
				ok = -2;
				break;
			}
		}
	}

	g_dir_close(dir);

	progress_dialog_destroy(progress);
	progress = NULL;

	folder_write_list();

	return ok;
}

static void import_create(void)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *table;
	GtkWidget *menu;
	GtkWidget *menuitem;
	GtkWidget *format_label;
	GtkWidget *dest_label;
	GtkWidget *confirm_area;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), _("Import"));
	gtk_container_set_border_width(GTK_CONTAINER(window), 5);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, FALSE);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(delete_event), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);

	vbox = gtk_vbox_new(FALSE, 4);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);

	desc_label = gtk_label_new
		(_("Specify source file and destination folder."));
	gtk_box_pack_start(GTK_BOX(hbox), desc_label, FALSE, FALSE, 0);

	table = gtk_table_new(2, 3, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(table), 8);
	gtk_table_set_row_spacings(GTK_TABLE(table), 8);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);
	gtk_widget_set_size_request(table, 420 * gtkut_get_dpi_multiplier(), -1);

	format_label = gtk_label_new(_("File format:"));
	gtk_table_attach(GTK_TABLE(table), format_label, 0, 1, 0, 1,
			 GTK_FILL, GTK_EXPAND|GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(format_label), 1, 0.5);

	file_label = gtk_label_new(_("Source:"));
	gtk_table_attach(GTK_TABLE(table), file_label, 0, 1, 1, 2,
			 GTK_FILL, GTK_EXPAND|GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(file_label), 1, 0.5);

	dest_label = gtk_label_new(_("Destination folder:"));
	gtk_table_attach(GTK_TABLE(table), dest_label, 0, 1, 2, 3,
			 GTK_FILL, GTK_EXPAND|GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(dest_label), 1, 0.5);

	format_optmenu = gtk_option_menu_new();
	gtk_table_attach(GTK_TABLE(table), format_optmenu, 1, 2, 0, 1,
			 GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);

	menu = gtk_menu_new();
	MENUITEM_ADD(menu, menuitem, _("UNIX mbox"), IMPORT_MBOX);
	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(import_format_menu_cb), NULL);
	MENUITEM_ADD(menu, menuitem, _("eml (folder)"), IMPORT_EML_FOLDER);
	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(import_format_menu_cb), NULL);
	MENUITEM_ADD(menu, menuitem, _("Outlook Express (dbx)"), IMPORT_DBX);
	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(import_format_menu_cb), NULL);

	gtk_option_menu_set_menu(GTK_OPTION_MENU(format_optmenu), menu);

	file_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), file_entry, 1, 2, 1, 2,
			 GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);

	dest_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), dest_entry, 1, 2, 2, 3,
			 GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);

	file_button = gtk_button_new_with_label(_(" Select... "));
	gtk_table_attach(GTK_TABLE(table), file_button, 2, 3, 1, 2,
			 0, 0, 0, 0);
	g_signal_connect(G_OBJECT(file_button), "clicked",
			 G_CALLBACK(import_filesel_cb), NULL);

	dest_button = gtk_button_new_with_label(_(" Select... "));
	gtk_table_attach(GTK_TABLE(table), dest_button, 2, 3, 2, 3,
			 0, 0, 0, 0);
	g_signal_connect(G_OBJECT(dest_button), "clicked",
			 G_CALLBACK(import_destsel_cb), NULL);

	gtkut_stock_button_set_create(&confirm_area,
				      &ok_button, GTK_STOCK_OK,
				      &cancel_button, GTK_STOCK_CANCEL,
				      NULL, NULL);
	gtk_box_pack_end(GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default(ok_button);

	g_signal_connect(G_OBJECT(ok_button), "clicked",
			 G_CALLBACK(import_ok_cb), NULL);
	g_signal_connect(G_OBJECT(cancel_button), "clicked",
			 G_CALLBACK(import_cancel_cb), NULL);

	gtk_widget_show_all(window);
}

static void import_format_menu_cb(GtkWidget *widget, gpointer data)
{
	gint type;

	type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget),
			       MENU_VAL_ID));
	if (type == IMPORT_EML_FOLDER) {
		gtk_label_set_text(GTK_LABEL(desc_label),
				   _("Specify source folder including eml files and destination folder."));
	} else {
		gtk_label_set_text(GTK_LABEL(desc_label),
				   _("Specify source file and destination folder."));
	}
}

static void import_ok_cb(GtkWidget *widget, gpointer data)
{
	import_finished = TRUE;
	import_ack = TRUE;
}

static void import_cancel_cb(GtkWidget *widget, gpointer data)
{
	import_finished = TRUE;
	import_ack = FALSE;
}

static void import_filesel_cb(GtkWidget *widget, gpointer data)
{
	gchar *filename;
	gchar *utf8_filename;
	gint type;

	type = menu_get_option_menu_active_index
		(GTK_OPTION_MENU(format_optmenu));

	if (type == IMPORT_EML_FOLDER)
		filename = filesel_select_file
			(_("Select importing folder"), NULL,
			 GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
	else
		filename = filesel_select_file(_("Select importing file"), NULL,
					       GTK_FILE_CHOOSER_ACTION_OPEN);
	if (!filename) return;

	utf8_filename = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);
	if (!utf8_filename) {
		g_warning("import_filesel_cb(): failed to convert characer set.");
		utf8_filename = g_strdup(filename);
	}
	gtk_entry_set_text(GTK_ENTRY(file_entry), utf8_filename);
	g_free(utf8_filename);

	g_free(filename);
}

static void import_destsel_cb(GtkWidget *widget, gpointer data)
{
	FolderItem *dest;
	gchar *dest_id;

	dest = foldersel_folder_sel(NULL, FOLDER_SEL_COPY, NULL);
	if (dest && dest->path) {
		dest_id = folder_item_get_identifier(dest);
		if (dest_id) {
			gtk_entry_set_text(GTK_ENTRY(dest_entry), dest_id);
			g_free(dest_id);
		}
	}
}

static gint delete_event(GtkWidget *widget, GdkEventAny *event, gpointer data)
{
	import_cancel_cb(NULL, NULL);
	return TRUE;
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		import_cancel_cb(NULL, NULL);
	return FALSE;
}

static void import_progress_cancel_cb(GtkWidget *widget, gpointer data)
{
	debug_print("import cancelled\n");
	import_progress_cancelled = TRUE;
}

static gint import_progress_delete_event(GtkWidget *widget, GdkEventAny *event,
					 gpointer data)
{
	import_progress_cancel_cb(NULL, NULL);
	return TRUE;
}
