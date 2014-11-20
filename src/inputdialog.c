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

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkcombo.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkstock.h>

#include "inputdialog.h"
#include "manage_window.h"
#include "inc.h"
#include "filesel.h"
#include "prefs_common.h"
#include "gtkutils.h"
#include "utils.h"

#define DIALOG_WIDTH	420

typedef enum
{
	INPUT_DIALOG_NORMAL,
	INPUT_DIALOG_INVISIBLE,
	INPUT_DIALOG_COMBO,
	INPUT_DIALOG_FILESEL
} InputDialogType;

static gboolean ack;
static gboolean fin;

static InputDialogType type;
static GtkFileChooserAction chooser_action;

static GtkWidget *dialog;
static GtkWidget *msg_label;
static GtkWidget *entry;
static GtkWidget *combo;
static GtkWidget *confirm_area;
static GtkWidget *ok_button;

static void input_dialog_create	(InputDialogType	dialog_type);

static gchar *input_dialog_open	(const gchar	*title,
				 const gchar	*message,
				 const gchar	*default_string);
static void input_dialog_set	(const gchar	*title,
				 const gchar	*message,
				 const gchar	*default_string);

static void ok_clicked		(GtkWidget	*widget,
				 gpointer	 data);
static void cancel_clicked	(GtkWidget	*widget,
				 gpointer	 data);
static gint delete_event	(GtkWidget	*widget,
				 GdkEventAny	*event,
				 gpointer	 data);
static gboolean key_pressed	(GtkWidget	*widget,
				 GdkEventKey	*event,
				 gpointer	 data);
static void entry_activated	(GtkEditable	*editable);
static void combo_activated	(GtkEditable	*editable);
static void sel_btn_clicked	(GtkButton	*button,
				 gpointer	 data);
static gint focus_out		(GtkWidget	*widget,
				 GdkEventFocus	*event,
				 gpointer	 data);


gchar *input_dialog(const gchar *title, const gchar *message,
		    const gchar *default_string)
{
	if (dialog)
		return NULL;

	input_dialog_create(INPUT_DIALOG_NORMAL);

	return input_dialog_open(title, message, default_string);
}

gchar *input_dialog_with_invisible(const gchar *title, const gchar *message,
				   const gchar *default_string)
{
	if (dialog)
		return NULL;

	input_dialog_create(INPUT_DIALOG_INVISIBLE);

	return input_dialog_open(title, message, default_string);
}

gchar *input_dialog_combo(const gchar *title, const gchar *message,
			  const gchar *default_string, GList *list,
			  gboolean case_sensitive)
{
	if (dialog)
		return NULL;

	input_dialog_create(INPUT_DIALOG_COMBO);

	if (!list) {
		GList empty_list;

		empty_list.data = (gpointer)"";
		empty_list.next = NULL;
		empty_list.prev = NULL;
		gtk_combo_set_popdown_strings(GTK_COMBO(combo), &empty_list);
	} else
		gtk_combo_set_popdown_strings(GTK_COMBO(combo), list);

	gtk_combo_set_case_sensitive(GTK_COMBO(combo), case_sensitive);

	return input_dialog_open(title, message, default_string);
}

gchar *input_dialog_query_password(const gchar *server, const gchar *user)
{
	gchar *message;
	gchar *pass;

	message = g_strdup_printf(_("Input password for %s on %s:"),
				  user, server);
	pass = input_dialog_with_invisible(_("Input password"), message, NULL);
	g_free(message);

	return pass;
}

gchar *input_dialog_with_filesel(const gchar *title, const gchar *message,
				 const gchar *default_string,
				 GtkFileChooserAction action)
{
	if (dialog)
		return NULL;

	input_dialog_create(INPUT_DIALOG_FILESEL);
	chooser_action = action;

	return input_dialog_open(title, message, default_string);
}

static void input_dialog_create(InputDialogType dialog_type)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *sel_btn;
	GtkWidget *cancel_button;

	dialog = gtk_dialog_new();
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_widget_set_size_request(dialog, DIALOG_WIDTH * gtkut_get_dpi_multiplier(), -1);
	gtk_container_set_border_width
		(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area), 5 * gtkut_get_dpi_multiplier());
	gtk_window_set_position(GTK_WINDOW(dialog),
				GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	manage_window_set_transient(GTK_WINDOW(dialog));
	g_signal_connect(G_OBJECT(dialog), "delete_event",
			 G_CALLBACK(delete_event), NULL);
	g_signal_connect(G_OBJECT(dialog), "key_press_event",
			 G_CALLBACK(key_pressed), NULL);
	g_signal_connect(G_OBJECT(dialog), "focus_out_event",
			 G_CALLBACK(focus_out), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT(dialog);

	vbox = gtk_vbox_new(FALSE, 8 * gtkut_get_dpi_multiplier());
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), vbox);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	msg_label = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(hbox), msg_label, FALSE, FALSE, 0);
	gtk_label_set_justify(GTK_LABEL(msg_label), GTK_JUSTIFY_LEFT);

	hbox = gtk_hbox_new(FALSE, 4 * gtkut_get_dpi_multiplier());
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	type = dialog_type;

	if (dialog_type == INPUT_DIALOG_COMBO) {
		combo = gtk_combo_new();
		gtk_box_pack_start(GTK_BOX(hbox), combo, TRUE, TRUE, 0);
		g_signal_connect(G_OBJECT(GTK_COMBO(combo)->entry), "activate",
				 G_CALLBACK(combo_activated), NULL);
	} else {
		entry = gtk_entry_new();
		gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
		g_signal_connect(G_OBJECT(entry), "activate",
				 G_CALLBACK(entry_activated), NULL);
		if (dialog_type == INPUT_DIALOG_FILESEL) {
			sel_btn = gtk_button_new_with_label("...");
			gtk_box_pack_start(GTK_BOX(hbox), sel_btn,
					   FALSE, FALSE, 0);
			g_signal_connect(G_OBJECT(sel_btn), "clicked",
					 G_CALLBACK(sel_btn_clicked), NULL);
		}
		if (dialog_type == INPUT_DIALOG_INVISIBLE)
			gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
	}

	gtkut_stock_button_set_create(&confirm_area,
				      &ok_button, GTK_STOCK_OK,
				      &cancel_button, GTK_STOCK_CANCEL,
				      NULL, NULL);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area),
			  confirm_area);
	gtk_widget_grab_default(ok_button);

	g_signal_connect(G_OBJECT(ok_button), "clicked",
			 G_CALLBACK(ok_clicked), NULL);
	g_signal_connect(G_OBJECT(cancel_button), "clicked",
			 G_CALLBACK(cancel_clicked), NULL);

	gtk_widget_show_all(GTK_DIALOG(dialog)->vbox);
}

static gchar *input_dialog_open(const gchar *title, const gchar *message,
				const gchar *default_string)
{
	gchar *str;

	gtkut_box_set_reverse_order(GTK_BOX(confirm_area),
				    !prefs_common.comply_gnome_hig);
	input_dialog_set(title, message, default_string);
	gtk_widget_show(dialog);

	ack = fin = FALSE;

	inc_lock();

	while (fin == FALSE)
		gtk_main_iteration();

	manage_window_focus_out(dialog, NULL, NULL);

	if (ack) {
		GtkEditable *editable;

		if (type == INPUT_DIALOG_COMBO)
			editable = GTK_EDITABLE(GTK_COMBO(combo)->entry);
		else
			editable = GTK_EDITABLE(entry);

		str = gtk_editable_get_chars(editable, 0, -1);
		if (str && *str == '\0') {
			g_free(str);
			str = NULL;
		}
	} else
		str = NULL;

	gtk_widget_destroy(dialog);
	dialog = msg_label = entry = combo = confirm_area = ok_button = NULL;

	GTK_EVENTS_FLUSH();

	inc_unlock();

	if (type != INPUT_DIALOG_INVISIBLE)
		debug_print("return string = %s\n", str ? str : "(none)");

	return str;
}

static void input_dialog_set(const gchar *title, const gchar *message,
			     const gchar *default_string)
{
	GtkWidget *entry_;

	if (type == INPUT_DIALOG_COMBO)
		entry_ = GTK_COMBO(combo)->entry;
	else
		entry_ = entry;

	gtk_window_set_title(GTK_WINDOW(dialog), title);
	gtk_label_set_text(GTK_LABEL(msg_label), message);
	if (default_string && *default_string) {
		gtk_entry_set_text(GTK_ENTRY(entry_), default_string);
		gtk_entry_set_position(GTK_ENTRY(entry_), 0);
		gtk_entry_select_region(GTK_ENTRY(entry_), 0, -1);
	} else
		gtk_entry_set_text(GTK_ENTRY(entry_), "");

	gtk_widget_grab_focus(ok_button);
	gtk_widget_grab_focus(entry_);
}

static void ok_clicked(GtkWidget *widget, gpointer data)
{
	ack = TRUE;
	fin = TRUE;
}

static void cancel_clicked(GtkWidget *widget, gpointer data)
{
	ack = FALSE;
	fin = TRUE;
}

static gint delete_event(GtkWidget *widget, GdkEventAny *event, gpointer data)
{
	ack = FALSE;
	fin = TRUE;

	return TRUE;
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event && event->keyval == GDK_Escape) {
		ack = FALSE;
		fin = TRUE;
	}

	return FALSE;
}

static void entry_activated(GtkEditable *editable)
{
	ack = TRUE;
	fin = TRUE;
}

static void combo_activated(GtkEditable *editable)
{
	ack = TRUE;
	fin = TRUE;
}

static void sel_btn_clicked(GtkButton *button, gpointer data)
{
	gchar *file;
	gchar *utf8_file;

	g_signal_handlers_block_by_func(dialog, focus_out, NULL);

	if (chooser_action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
		file = filesel_select_dir(NULL);
	else
		file = filesel_select_file(_("Select file"), NULL,
					   chooser_action);
	if (file) {
		utf8_file = conv_filename_to_utf8(file);
		gtk_entry_set_text(GTK_ENTRY(entry), utf8_file);
		g_free(utf8_file);
	}

	g_signal_handlers_unblock_by_func(dialog, focus_out, NULL);
}

static gint focus_out(GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
#ifdef G_OS_WIN32
	gtk_window_present(GTK_WINDOW(widget));
#endif
	return FALSE;
}
