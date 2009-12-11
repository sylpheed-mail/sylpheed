/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2009 Hiroyuki Yamamoto
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
#include <gtk/gtk.h>

#include "inputdialog.h"
#include "alertpanel.h"
#include "mainwindow.h"
#include "folderview.h"
#include "manage_window.h"
#include "gtkutils.h"
#include "filesel.h"
#include "prefs_common.h"

static void scan_tree_func(Folder *folder, FolderItem *item, gpointer data);


static void button_toggled(GtkToggleButton *button, GtkWidget *widget)
{
	gboolean is_active;

	is_active = gtk_toggle_button_get_active(button);
	gtk_widget_set_sensitive(widget, is_active);
}

static void sel_btn_clicked(GtkButton *button, GtkWidget *entry)
{
	gchar *folder;
	gchar *utf8_folder;
	gchar *base;

	folder = filesel_select_dir(NULL);
	if (folder) {
		utf8_folder = conv_filename_to_utf8(folder);
		base = g_path_get_basename(utf8_folder);
		if (!g_ascii_strcasecmp(base, "Mail")) {
			gtk_entry_set_text(GTK_ENTRY(entry), utf8_folder);
		} else {
			gchar *text;

			text = g_strconcat(utf8_folder, G_DIR_SEPARATOR_S, "Mail", NULL);
			gtk_entry_set_text(GTK_ENTRY(entry), text);
			g_free(text);
		}
		g_free(base);
		g_free(utf8_folder);
		g_free(folder);
	}
}

#ifdef G_OS_WIN32
#define MODIFY_LABEL_STYLE() \
	{ \
		GtkStyle *style; \
		style = gtk_widget_get_style(dialog); \
		gtk_widget_modify_base(label, GTK_STATE_ACTIVE, \
				       &style->base[GTK_STATE_SELECTED]); \
		gtk_widget_modify_text(label, GTK_STATE_ACTIVE, \
				       &style->text[GTK_STATE_SELECTED]); \
	}
#else
#define MODIFY_LABEL_STYLE()
#endif

void setup(MainWindow *mainwin)
{
	static PangoFontDescription *font_desc;
	GtkWidget *dialog;
	GtkWidget *hbox;
	GtkWidget *image;
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *radio;
	GtkWidget *entry;
	GtkWidget *sel_btn;
	GtkWidget *ok_btn;
	gchar *path = NULL;
	gchar *fullpath;
	Folder *folder;
	gint result;

	manage_window_focus_in(mainwin->window, NULL, NULL);

	dialog = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dialog), _("Mailbox setting"));
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, FALSE);
	gtk_widget_set_size_request(dialog, 540, -1);
	gtk_window_set_position(GTK_WINDOW(dialog),
				GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	manage_window_set_transient(GTK_WINDOW(dialog));
	gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
	MANAGE_WINDOW_SIGNALS_CONNECT(dialog);
	gtk_widget_realize(dialog);

	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 12);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
			   hbox, FALSE, FALSE, 0);

	image = gtk_image_new_from_stock
		(GTK_STOCK_PREFERENCES, GTK_ICON_SIZE_DIALOG);

	gtk_misc_set_alignment(GTK_MISC(image), 0.5, 0.0);
	gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);

	vbox = gtk_vbox_new(FALSE, 12);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Mailbox setting"));
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);

	if (!font_desc) {
		gint size;

		size = pango_font_description_get_size
			(label->style->font_desc);
		font_desc = pango_font_description_new();
		pango_font_description_set_weight
			(font_desc, PANGO_WEIGHT_BOLD);
		pango_font_description_set_size
			(font_desc, size * PANGO_SCALE_LARGE);
	}
	if (font_desc)
		gtk_widget_modify_font(label, font_desc);

	label = gtk_label_new(_("This dialog will make initial setup of mailbox."));
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	GTK_WIDGET_UNSET_FLAGS(label, GTK_CAN_FOCUS);
	MODIFY_LABEL_STYLE();

	vbox = gtk_vbox_new(FALSE, 8);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), vbox,
			   TRUE, TRUE, 0);

	radio = gtk_radio_button_new_with_label
		(NULL, _("Create mailbox at the following default location:"));
	gtk_box_pack_start(GTK_BOX(vbox), radio, FALSE, FALSE, 0);

	fullpath = g_strdup_printf("%s%cMail", get_mail_base_dir(),
				   G_DIR_SEPARATOR);

	label = gtk_label_new(fullpath);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
	gtk_label_set_line_wrap(GTK_LABEL(label), FALSE);
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_MIDDLE);
#endif
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	GTK_WIDGET_UNSET_FLAGS(label, GTK_CAN_FOCUS);
	MODIFY_LABEL_STYLE();

	g_free(fullpath);

	radio = gtk_radio_button_new_with_label_from_widget
		(GTK_RADIO_BUTTON(radio), _("Create mailbox at the following location:\n(enter folder name or full folder path)"));
	gtk_box_pack_start(GTK_BOX(vbox), radio, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 4);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);

	sel_btn = gtk_button_new_with_label("...");
	gtk_box_pack_start(GTK_BOX(hbox), sel_btn, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(sel_btn), "clicked",
			 G_CALLBACK(sel_btn_clicked), entry);

	gtk_widget_set_sensitive(hbox, FALSE);
	g_signal_connect(G_OBJECT(radio), "toggled", G_CALLBACK(button_toggled),
			 hbox);

	label = gtk_label_new(_("If you want to add a mailbox at another location afterward, please select 'File - Mailbox - Add mailbox...' in the menu."));
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	GTK_WIDGET_UNSET_FLAGS(label, GTK_CAN_FOCUS);
	MODIFY_LABEL_STYLE();

	if (prefs_common.comply_gnome_hig) {
		gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
		ok_btn = gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_OK, GTK_RESPONSE_OK);
	} else {
		ok_btn = gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_OK, GTK_RESPONSE_OK);
		gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	}
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
	gtk_widget_grab_focus(ok_btn);

	gtk_widget_show_all(dialog);

	do {
		result = gtk_dialog_run(GTK_DIALOG(dialog));
		if (result != GTK_RESPONSE_OK) {
			if (alertpanel(_("Cancel"), _("Continue without creating mailbox?"), GTK_STOCK_YES, GTK_STOCK_NO, NULL) == G_ALERTDEFAULT)
				break;
			else
				continue;
		}

		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio))) {
			path = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
			g_strstrip(path);
			if (*path == '\0') {
				alertpanel_error(_("Please input folder name or full folder path."));
				g_free(path);
				path = NULL;
			}
		} else
			path = g_strdup("Mail");

		if (path && folder_find_from_path(path)) {
			alertpanel_error(_("The mailbox '%s' already exists."), path);
			g_warning("The mailbox '%s' already exists.", path);
			g_free(path);
			path = NULL;
		}
	} while (path == NULL);

	gtk_widget_destroy(dialog);
	if (path == NULL)
		return;

	folder = folder_new(F_MH, _("Mailbox"), path);
	g_free(path);

	if (folder->klass->create_tree(folder) < 0) {
		alertpanel_error(_("Creation of the mailbox failed.\n"
				   "Maybe some files already exist, or you don't have the permission to write there."));
		folder_destroy(folder);
		return;
	}

	folder_add(folder);
	folder_set_ui_func(folder, scan_tree_func, mainwin);
	folder->klass->scan_tree(folder);
	folder_set_ui_func(folder, NULL, NULL);

	folderview_set(mainwin->folderview);
}

static void scan_tree_func(Folder *folder, FolderItem *item, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	gchar *str;

	if (item->path)
		str = g_strdup_printf(_("Scanning folder %s%c%s ..."),
				      LOCAL_FOLDER(folder)->rootpath,
				      G_DIR_SEPARATOR,
				      item->path);
	else
		str = g_strdup_printf(_("Scanning folder %s ..."),
				      LOCAL_FOLDER(folder)->rootpath);

	gtk_statusbar_push(GTK_STATUSBAR(mainwin->statusbar),
			   mainwin->mainwin_cid, str);
	gtkut_widget_draw_now(mainwin->statusbar);
	gtk_statusbar_pop(GTK_STATUSBAR(mainwin->statusbar),
			  mainwin->mainwin_cid);
	g_free(str);
}
