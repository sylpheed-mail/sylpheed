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
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkprogressbar.h>

#include "main.h"
#include "inc.h"
#include "mbox.h"
#include "folder.h"
#include "procmsg.h"
#include "menu.h"
#include "filesel.h"
#include "foldersel.h"
#include "gtkutils.h"
#include "manage_window.h"
#include "folder.h"
#include "utils.h"
#include "progressdialog.h"
#include "alertpanel.h"
#include "mainwindow.h"
#include "summaryview.h"
#include "prefs_ui.h"

enum
{
	EXPORT_MBOX,
	EXPORT_EML,
	EXPORT_MH
};

static GtkWidget *window;
static GtkWidget *format_optmenu;
static GtkWidget *desc_label;
static GtkWidget *file_label;
static GtkWidget *src_entry;
static GtkWidget *file_entry;
static GtkWidget *src_button;
static GtkWidget *file_button;
static GtkWidget *selected_only_chkbtn;
static GtkWidget *ok_button;
static GtkWidget *cancel_button;
static gboolean export_finished;
static gboolean export_ack;
static ProgressDialog *progress;

static gboolean progress_cancel = FALSE;

static void export_create	(void);
static gint export_do		(void);
static gint export_eml		(FolderItem	*src,
				 GSList		*sel_mlist,
				 const gchar	*path,
				 gint		 type);

static void export_format_menu_cb	(GtkWidget	*widget,
					 gpointer	 data);

static void export_ok_cb	(GtkWidget	*widget,
				 gpointer	 data);
static void export_cancel_cb	(GtkWidget	*widget,
				 gpointer	 data);
static void export_srcsel_cb	(GtkWidget	*widget,
				 gpointer	 data);
static void export_filesel_cb	(GtkWidget	*widget,
				 gpointer	 data);
static gint delete_event	(GtkWidget	*widget,
				 GdkEventAny	*event,
				 gpointer	 data);
static gboolean key_pressed	(GtkWidget	*widget,
				 GdkEventKey	*event,
				 gpointer	 data);

static void export_progress_cancel_cb	(GtkWidget	*widget,
					 gpointer	 data);


static gboolean export_mbox_func(Folder *folder, FolderItem *item, guint count, guint total, gpointer data)
{
	gchar str[64];
	static GTimeVal tv_prev = {0, 0};
	GTimeVal tv_cur;

	g_get_current_time(&tv_cur);
	g_snprintf(str, sizeof(str), "%u / %d", count, total);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress->progressbar), str);

	if (tv_prev.tv_sec == 0 ||
	    (tv_cur.tv_sec - tv_prev.tv_sec) * G_USEC_PER_SEC +
	    tv_cur.tv_usec - tv_prev.tv_usec > 100 * 1000) {
		if (item->total > 0)
			gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress->progressbar), (gdouble)count / item->total);
		else
			gtk_progress_bar_pulse(GTK_PROGRESS_BAR(progress->progressbar));
		ui_update();
		tv_prev = tv_cur;
	}

	if (progress_cancel)
		return FALSE;
	else
		return TRUE;
}

gint export_mail(FolderItem *default_src)
{
	gint ok = 0;
	gchar *src_id = NULL;

	export_create();

	change_dir(get_startup_dir());

	if (default_src && default_src->path)
		src_id = folder_item_get_identifier(default_src);

	if (src_id) {
		gtk_entry_set_text(GTK_ENTRY(src_entry), src_id);
		g_free(src_id);
	}
	gtk_widget_grab_focus(file_entry);

	manage_window_set_transient(GTK_WINDOW(window));

	export_finished = FALSE;
	export_ack = FALSE;

	inc_lock();

	while (!export_finished)
		gtk_main_iteration();

	if (export_ack)
		ok = export_do();

	gtk_widget_destroy(window);
	window = NULL;
	src_entry = file_entry = NULL;
	src_button = file_button = ok_button = cancel_button = NULL;

	inc_unlock();

	return ok;
}

static gint export_do(void)
{
	gint ok = 0;
	const gchar *srcdir, *utf8mbox;
	FolderItem *src;
	gchar *mbox;
	gchar *msg;
	gint type;
	gboolean selected_only;
	MainWindow *mainwin;
	GSList *mlist = NULL;

	type = menu_get_option_menu_active_index
		(GTK_OPTION_MENU(format_optmenu));

	srcdir = gtk_entry_get_text(GTK_ENTRY(src_entry));
	utf8mbox = gtk_entry_get_text(GTK_ENTRY(file_entry));

	if (!utf8mbox || !*utf8mbox)
		return -1;

	mbox = g_filename_from_utf8(utf8mbox, -1, NULL, NULL, NULL);
	if (!mbox) {
		g_warning("failed to convert character set.");
		mbox = g_strdup(utf8mbox);
	}

	selected_only = gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON(selected_only_chkbtn));

	if (selected_only) {
		mainwin = main_window_get();
		src = mainwin->summaryview->folder_item;
		mlist = summary_get_selected_msg_list(mainwin->summaryview);
	} else
		src = folder_find_item_from_identifier(srcdir);

	if (!src) {
		g_warning("Can't find the folder.");
		g_free(mbox);
		return -1;
	}

	msg = g_strdup_printf(_("Exporting %s ..."), src->name);
	progress = progress_dialog_simple_create();
	gtk_window_set_title(GTK_WINDOW(progress->window), _("Exporting"));
	progress_dialog_set_label(progress, msg);
	g_free(msg);
	gtk_window_set_modal(GTK_WINDOW(progress->window), TRUE);
	manage_window_set_transient(GTK_WINDOW(progress->window));
	g_signal_connect(G_OBJECT(progress->cancel_btn), "clicked",
			 G_CALLBACK(export_progress_cancel_cb), NULL);
	g_signal_connect(G_OBJECT(progress->window), "delete_event",
			 G_CALLBACK(gtk_true), NULL);
	gtk_widget_show(progress->window);
	ui_update();

	progress_cancel = FALSE;

	if (type == EXPORT_MBOX) {
		folder_set_ui_func2(src->folder, export_mbox_func, NULL);
		if (mlist)
			ok = export_msgs_to_mbox(src, mlist, mbox);
		else
			ok = export_to_mbox(src, mbox);
		folder_set_ui_func2(src->folder, NULL, NULL);
	} else if (type == EXPORT_EML || type == EXPORT_MH) {
		ok = export_eml(src, mlist, mbox, type);
	}

	progress_dialog_destroy(progress);
	progress = NULL;

	if (mlist)
		g_slist_free(mlist);
	g_free(mbox);

	if (ok == -1)
		alertpanel_error(_("Error occurred on export."));

	return ok;
}

static gint export_eml(FolderItem *src, GSList *sel_mlist, const gchar *path,
		       gint type)
{
	const gchar *ext = "";
	GSList *mlist, *cur;
	MsgInfo *msginfo;
	gchar *file, *dest;
	guint count = 0;
	guint total;
	gint ok = 0;

	g_return_val_if_fail(src != NULL, -1);
	g_return_val_if_fail(path != NULL, -1);

	if (type == EXPORT_EML)
		ext = ".eml";

	if (!g_file_test(path, G_FILE_TEST_IS_DIR)) {
		if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
			make_dir_hier(path);
			if (!g_file_test(path, G_FILE_TEST_IS_DIR))
				return -1;
		} else {
			g_warning("export_eml(): directory %s already exists.",
				  path);
			return -1;
		}
	}

	if (sel_mlist)
		mlist = sel_mlist;
	else {
		mlist = folder_item_get_msg_list(src, TRUE);
		if (!mlist)
			return 0;
	}
	total = g_slist_length(mlist);

	for (cur = mlist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;

		count++;
		if (export_mbox_func(src->folder, src, count, total, NULL) == FALSE) {
			ok = -2;
			break;
		}

		file = folder_item_fetch_msg(src, msginfo->msgnum);
		if (!file) {
			ok = -1;
			break;
		}
		dest = g_strdup_printf("%s%c%u%s", path, G_DIR_SEPARATOR,
				       count, ext);
		if (g_file_test(dest, G_FILE_TEST_EXISTS)) {
			g_warning("export_eml(): %s already exists.", dest);
			g_free(dest);
			g_free(file);
			ok = -1;
			break;
		}
		if (copy_file(file, dest, FALSE) < 0) {
			g_free(dest);
			g_free(file);
			ok = -1;
			break;
		}
		g_free(dest);
		g_free(file);
	}

	if (!sel_mlist)
		procmsg_msg_list_free(mlist);

	return ok;
}

static void export_create(void)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *table;
	GtkWidget *menu;
	GtkWidget *menuitem;
	GtkWidget *format_label;
	GtkWidget *src_label;
	GtkWidget *confirm_area;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), _("Export"));
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
		(_("Specify source folder and destination file."));
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

	src_label = gtk_label_new(_("Source folder:"));
	gtk_table_attach(GTK_TABLE(table), src_label, 0, 1, 1, 2,
			 GTK_FILL, GTK_EXPAND|GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(src_label), 1, 0.5);

	file_label = gtk_label_new(_("Destination:"));
	gtk_table_attach(GTK_TABLE(table), file_label, 0, 1, 2, 3,
			 GTK_FILL, GTK_EXPAND|GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(file_label), 1, 0.5);

	format_optmenu = gtk_option_menu_new();
	gtk_table_attach(GTK_TABLE(table), format_optmenu, 1, 2, 0, 1,
			 GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);

	menu = gtk_menu_new();
	MENUITEM_ADD(menu, menuitem, _("UNIX mbox"), EXPORT_MBOX);
	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(export_format_menu_cb), NULL);
	MENUITEM_ADD(menu, menuitem, _("eml (number + .eml)"), EXPORT_EML);
	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(export_format_menu_cb), NULL);
	MENUITEM_ADD(menu, menuitem, _("MH (number only)"), EXPORT_MH);
	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(export_format_menu_cb), NULL);

	gtk_option_menu_set_menu(GTK_OPTION_MENU(format_optmenu), menu);

	src_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), src_entry, 1, 2, 1, 2,
			 GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);

	file_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), file_entry, 1, 2, 2, 3,
			 GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);

	src_button = gtk_button_new_with_label(_(" Select... "));
	gtk_table_attach(GTK_TABLE(table), src_button, 2, 3, 1, 2,
			 0, 0, 0, 0);
	g_signal_connect(G_OBJECT(src_button), "clicked",
			 G_CALLBACK(export_srcsel_cb), NULL);

	file_button = gtk_button_new_with_label(_(" Select... "));
	gtk_table_attach(GTK_TABLE(table), file_button, 2, 3, 2, 3,
			 0, 0, 0, 0);
	g_signal_connect(G_OBJECT(file_button), "clicked",
			 G_CALLBACK(export_filesel_cb), NULL);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);

	selected_only_chkbtn = gtk_check_button_new_with_label
		(_("Export only selected messages"));
	gtk_box_pack_start(GTK_BOX(hbox), selected_only_chkbtn,
			   FALSE, FALSE, 0);

	SET_TOGGLE_SENSITIVITY_REV(selected_only_chkbtn, src_entry);
	SET_TOGGLE_SENSITIVITY_REV(selected_only_chkbtn, src_button);

	gtkut_stock_button_set_create(&confirm_area,
				      &ok_button, GTK_STOCK_OK,
				      &cancel_button, GTK_STOCK_CANCEL,
				      NULL, NULL);
	gtk_box_pack_end(GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default(ok_button);

	g_signal_connect(G_OBJECT(ok_button), "clicked",
			 G_CALLBACK(export_ok_cb), NULL);
	g_signal_connect(G_OBJECT(cancel_button), "clicked",
			 G_CALLBACK(export_cancel_cb), NULL);

	gtk_widget_show_all(window);
}

static void export_format_menu_cb(GtkWidget *widget, gpointer data)
{
	gint type;

	type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget),
			       MENU_VAL_ID));
	if (type == EXPORT_MBOX) {
		gtk_label_set_text(GTK_LABEL(desc_label),
				   _("Specify source folder and destination file."));
	} else {
		gtk_label_set_text(GTK_LABEL(desc_label),
				   _("Specify source folder and destination folder."));
	}
}

static void export_ok_cb(GtkWidget *widget, gpointer data)
{
	export_finished = TRUE;
	export_ack = TRUE;
}

static void export_cancel_cb(GtkWidget *widget, gpointer data)
{
	export_finished = TRUE;
	export_ack = FALSE;
}

static void export_filesel_cb(GtkWidget *widget, gpointer data)
{
	gchar *filename;
	gchar *utf8_filename;
	gint type;

	type = menu_get_option_menu_active_index
		(GTK_OPTION_MENU(format_optmenu));

	if (type == EXPORT_MBOX)
		filename = filesel_select_file(_("Select destination file"),
					       NULL,
					       GTK_FILE_CHOOSER_ACTION_SAVE);
	else
		filename = filesel_select_file(_("Select destination folder"),
					       NULL,
					       GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
	if (!filename) return;

	utf8_filename = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);
	if (!utf8_filename) {
		g_warning("export_filesel_cb(): failed to convert character set.");
		utf8_filename = g_strdup(filename);
	}
	gtk_entry_set_text(GTK_ENTRY(file_entry), utf8_filename);
	g_free(utf8_filename);

	g_free(filename);
}

static void export_srcsel_cb(GtkWidget *widget, gpointer data)
{
	FolderItem *src;
	gchar *src_id;

	src = foldersel_folder_sel(NULL, FOLDER_SEL_ALL, NULL);
	if (src && src->path) {
		src_id = folder_item_get_identifier(src);
		if (src_id) {
			gtk_entry_set_text(GTK_ENTRY(src_entry), src_id);
			g_free(src_id);
		}
	}
}

static gint delete_event(GtkWidget *widget, GdkEventAny *event, gpointer data)
{
	export_cancel_cb(NULL, NULL);
	return TRUE;
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		export_cancel_cb(NULL, NULL);
	return FALSE;
}

static void export_progress_cancel_cb(GtkWidget *widget, gpointer data)
{
	progress_cancel = TRUE;
}
