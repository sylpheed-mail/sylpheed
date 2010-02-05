/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2010 Hiroyuki Yamamoto
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
#include "account_dialog.h"
#include "gtkutils.h"
#include "filesel.h"
#include "prefs_common.h"
#include "stock_pixmap.h"
#include "account.h"

static PangoFontDescription *font_desc;

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

void setup_mailbox(void)
{
	MainWindow *mainwin;
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

	mainwin = main_window_get();
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

	image = stock_pixbuf_widget(dialog, STOCK_PIXMAP_SYLPHEED);

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

		if (path) {
			if (folder_find_from_path(path)) {
				alertpanel_error(_("The mailbox '%s' already exists."), path);
				g_warning("The mailbox '%s' already exists.", path);
				g_free(path);
				path = NULL;
			} else if (is_path_parent(path, get_rc_dir()) ||
				   is_path_parent(path, get_mail_base_dir())) {
				alertpanel_error(_("The location '%s' includes settings folder. Please specify another location."), path);
				g_warning("The location '%s' includes settings folder.", path);
				g_free(path);
				path = NULL;
			}
		}
	} while (path == NULL);

	gtk_widget_destroy(dialog);
	if (path == NULL)
		return;

	if (!strcmp(g_basename(path), "Mail"))
		folder = folder_new(F_MH, _("Mailbox"), path);
	else
		folder = folder_new(F_MH, g_basename(path), path);
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

static struct
{
	GtkWidget *dialog;
	GtkWidget *notebook;
	GtkWidget *prev_btn;
	GtkWidget *next_btn;
	GtkWidget *cancel_btn;
	GtkWidget *pop3_radio;
	GtkWidget *imap_radio;
	GtkWidget *pop3g_radio;
	GtkWidget *imapg_radio;
	GtkWidget *name_entry;
	GtkWidget *addr_entry;
	GtkWidget *id_entry;
	GtkWidget *serv_entry;
	GtkWidget *smtp_entry;
	GtkWidget *servssl_chkbtn;
	GtkWidget *smtpauth_chkbtn;
	GtkWidget *smtpssl_chkbtn;
	GtkWidget *serv_label_name1;
	GtkWidget *serv_label_name2;
	GtkWidget *name_label;
	GtkWidget *addr_label;
	GtkWidget *id_label;
	GtkWidget *serv_label;
	GtkWidget *smtp_label;
	gboolean finished;
	gboolean cancelled;

	gint type;
	gchar *name;
	gchar *addr;
	gchar *userid;
	gchar *serv;
	gchar *smtpserv;
	gushort serv_port;
	gushort smtp_port;
	gboolean serv_ssl;
	gboolean smtp_ssl;
	gboolean smtp_auth;
} setupac;

enum
{
	SETUP_PAGE_START,
	SETUP_PAGE_ADDRESS,
	SETUP_PAGE_ACCOUNT,
	SETUP_PAGE_FINISH
};

enum
{
	SETUP_TYPE_POP3,
	SETUP_TYPE_IMAP,
	SETUP_TYPE_POP3G,
	SETUP_TYPE_IMAPG
};

#define GMAIL_POP3_SERVER	"pop.gmail.com"
#define GMAIL_IMAP_SERVER	"imap.gmail.com"
#define GMAIL_SMTP_SERVER	"smtp.gmail.com"
#define POP3_PORT		110
#define IMAP_PORT		143
#define SMTP_PORT		25
#define POP3S_PORT		995
#define IMAPS_PORT		993
#define SMTPS_PORT		465

static void entry_changed(GtkEditable *editable, gpointer data)
{
	const gchar *name, *addr, *userid, *serv, *smtp;
	gint page;
	gboolean next_enable = FALSE;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(setupac.notebook));
	if (page != SETUP_PAGE_ADDRESS && page != SETUP_PAGE_ACCOUNT)
		return;

	name = gtk_entry_get_text(GTK_ENTRY(setupac.name_entry));
	addr = gtk_entry_get_text(GTK_ENTRY(setupac.addr_entry));
	userid = gtk_entry_get_text(GTK_ENTRY(setupac.id_entry));
	serv = gtk_entry_get_text(GTK_ENTRY(setupac.serv_entry));
	smtp = gtk_entry_get_text(GTK_ENTRY(setupac.smtp_entry));

	if (setupac.type == SETUP_TYPE_POP3G ||
	    setupac.type == SETUP_TYPE_IMAPG) {
		if (GTK_WIDGET(editable) == setupac.addr_entry)
			gtk_entry_set_text(GTK_ENTRY(setupac.id_entry), addr);
	}

	if (page == SETUP_PAGE_ADDRESS && name && *name && addr && *addr)
		next_enable = TRUE;
	else if (page == SETUP_PAGE_ACCOUNT &&
		 userid && *userid && serv && *serv && smtp && *smtp)
		next_enable = TRUE;

	gtk_dialog_set_response_sensitive(GTK_DIALOG(setupac.dialog),
					  GTK_RESPONSE_ACCEPT, next_enable);
}

#define GET_STR(s, m) \
{ \
	setupac.s = gtk_editable_get_chars(GTK_EDITABLE(setupac.m), 0, -1); \
	g_strstrip(setupac.s); \
}

static void setup_account_response_cb(GtkDialog *dialog, gint response_id,
				      gpointer data)
{
	gint page, prev_page;
	gboolean next_enable = TRUE;
	gboolean prev_enable = TRUE;
	gchar buf[1024];

	prev_page = page =
		gtk_notebook_get_current_page(GTK_NOTEBOOK(setupac.notebook));

	g_print("response_id = %d\n", response_id);
	g_print("page = %d\n", page);

	if (response_id == GTK_RESPONSE_CANCEL ||
	    response_id == GTK_RESPONSE_DELETE_EVENT) {
		setupac.finished = TRUE;
		if (page != SETUP_PAGE_FINISH)
			setupac.cancelled = TRUE;
	} else if (response_id == GTK_RESPONSE_ACCEPT) {
		if (prev_page == SETUP_PAGE_ADDRESS &&
		    (setupac.type == SETUP_TYPE_POP3G ||
		     setupac.type == SETUP_TYPE_IMAPG)) {
			gtk_notebook_set_current_page
				(GTK_NOTEBOOK(setupac.notebook),
				 SETUP_PAGE_FINISH);
		} else {
			gtk_notebook_set_current_page
				(GTK_NOTEBOOK(setupac.notebook), page + 1);
		}
		if (prev_page == SETUP_PAGE_START) {
			setupac.type = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(setupac.pop3_radio)) ? SETUP_TYPE_POP3
				: gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(setupac.imap_radio)) ? SETUP_TYPE_IMAP
				: gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(setupac.pop3g_radio)) ? SETUP_TYPE_POP3G
				: gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(setupac.imapg_radio)) ? SETUP_TYPE_IMAPG : SETUP_TYPE_POP3;
		}
	} else if (response_id == GTK_RESPONSE_REJECT) {
		gtk_notebook_set_current_page(GTK_NOTEBOOK(setupac.notebook),
					      page - 1);
	} else {
		g_warning("setup_account_response_cb: invalid response_id: %d\n", response_id);
	}

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(setupac.notebook));
	g_print("new page = %d\n", page);

	if (page == SETUP_PAGE_START)
		prev_enable = FALSE;
	else if (page == SETUP_PAGE_ADDRESS || page == SETUP_PAGE_ACCOUNT) {
		switch (setupac.type) {
		case SETUP_TYPE_POP3:
			gtk_widget_set_sensitive(setupac.id_entry, TRUE);
			gtk_label_set_text(GTK_LABEL(setupac.serv_label_name1), _("POP3 server:"));
			gtk_widget_set_sensitive(setupac.serv_entry, TRUE);
			gtk_widget_set_sensitive(setupac.smtp_entry, TRUE);
			break;
		case SETUP_TYPE_POP3G:
			if (prev_page == SETUP_PAGE_START)
				gtk_entry_set_text(GTK_ENTRY(setupac.addr_entry), "(username)@gmail.com");
			gtk_widget_set_sensitive(setupac.id_entry, FALSE);
			gtk_label_set_text(GTK_LABEL(setupac.serv_label_name1), _("POP3 server:"));
			gtk_entry_set_text(GTK_ENTRY(setupac.serv_entry), GMAIL_POP3_SERVER);
			gtk_widget_set_sensitive(setupac.serv_entry, FALSE);
			gtk_entry_set_text(GTK_ENTRY(setupac.smtp_entry), GMAIL_SMTP_SERVER);
			gtk_widget_set_sensitive(setupac.smtp_entry, FALSE);
			break;
		case SETUP_TYPE_IMAP:
			gtk_widget_set_sensitive(setupac.id_entry, TRUE);
			gtk_label_set_text(GTK_LABEL(setupac.serv_label_name1), _("IMAP4 server:"));
			gtk_widget_set_sensitive(setupac.serv_entry, TRUE);
			gtk_widget_set_sensitive(setupac.smtp_entry, TRUE);
			break;
		case SETUP_TYPE_IMAPG:
			if (prev_page == SETUP_PAGE_START)
				gtk_entry_set_text(GTK_ENTRY(setupac.addr_entry), "(username)@gmail.com");
			gtk_widget_set_sensitive(setupac.id_entry, FALSE);
			gtk_label_set_text(GTK_LABEL(setupac.serv_label_name1), _("IMAP4 server:"));
			gtk_entry_set_text(GTK_ENTRY(setupac.serv_entry), GMAIL_IMAP_SERVER);
			gtk_widget_set_sensitive(setupac.serv_entry, FALSE);
			gtk_entry_set_text(GTK_ENTRY(setupac.smtp_entry), GMAIL_SMTP_SERVER);
			gtk_widget_set_sensitive(setupac.smtp_entry, FALSE);
			break;
		}
	} else if (page == SETUP_PAGE_FINISH) {
		prev_enable = FALSE;
		next_enable = FALSE;
		gtk_button_set_label(GTK_BUTTON(setupac.cancel_btn),
				     GTK_STOCK_CLOSE);

		switch (setupac.type) {
		case SETUP_TYPE_POP3:
			setupac.serv_ssl = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(setupac.servssl_chkbtn));
			setupac.smtp_ssl = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(setupac.smtpssl_chkbtn));
			setupac.smtp_auth = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(setupac.smtpauth_chkbtn));
			setupac.serv_port = setupac.serv_ssl ? POP3S_PORT : POP3_PORT;
			setupac.smtp_port = setupac.smtp_ssl ? SMTPS_PORT : SMTP_PORT;
			gtk_label_set_text(GTK_LABEL(setupac.serv_label_name2), _("POP3 server:"));
			break;
		case SETUP_TYPE_POP3G:
			setupac.serv_ssl = TRUE;
			setupac.smtp_ssl = TRUE;
			setupac.smtp_auth = TRUE;
			setupac.serv_port = POP3S_PORT;
			setupac.smtp_port = SMTPS_PORT;
			gtk_label_set_text(GTK_LABEL(setupac.serv_label_name2), _("POP3 server:"));
			break;
		case SETUP_TYPE_IMAP:
			setupac.serv_ssl = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(setupac.servssl_chkbtn));
			setupac.smtp_ssl = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(setupac.smtpssl_chkbtn));
			setupac.smtp_auth = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(setupac.smtpauth_chkbtn));
			setupac.serv_port = setupac.serv_ssl ? IMAPS_PORT : IMAP_PORT;
			setupac.smtp_port = setupac.smtp_ssl ? SMTPS_PORT : SMTP_PORT;
			gtk_label_set_text(GTK_LABEL(setupac.serv_label_name2), _("IMAP4 server:"));
			break;
		case SETUP_TYPE_IMAPG:
			setupac.serv_ssl = TRUE;
			setupac.smtp_ssl = TRUE;
			setupac.smtp_auth = TRUE;
			setupac.serv_port = IMAPS_PORT;
			setupac.smtp_port = SMTPS_PORT;
			gtk_label_set_text(GTK_LABEL(setupac.serv_label_name2), _("IMAP4 server:"));
			break;
		}

		GET_STR(name, name_entry);
		GET_STR(addr, addr_entry);
		GET_STR(userid, id_entry);
		GET_STR(serv, serv_entry);
		GET_STR(smtpserv, smtp_entry);
		gtk_label_set_text(GTK_LABEL(setupac.name_label), setupac.name);
		gtk_label_set_text(GTK_LABEL(setupac.addr_label), setupac.addr);
		gtk_label_set_text(GTK_LABEL(setupac.id_label), setupac.userid);
		if (setupac.serv_ssl)
			g_snprintf(buf, sizeof(buf), "%s:%u (SSL)",
				   setupac.serv, setupac.serv_port);
		else
			g_snprintf(buf, sizeof(buf), "%s:%u",
				   setupac.serv, setupac.serv_port);
		gtk_label_set_text(GTK_LABEL(setupac.serv_label), buf);
		if (setupac.smtp_ssl)
			g_snprintf(buf, sizeof(buf), "%s:%u (SSL)",
				   setupac.smtpserv, setupac.smtp_port);
		else
			g_snprintf(buf, sizeof(buf), "%s:%u",
				   setupac.smtpserv, setupac.smtp_port);
		gtk_label_set_text(GTK_LABEL(setupac.smtp_label), buf);
	}

	gtk_dialog_set_response_sensitive(GTK_DIALOG(setupac.dialog),
					  GTK_RESPONSE_REJECT, prev_enable);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(setupac.dialog),
					  GTK_RESPONSE_ACCEPT, next_enable);

	if (page == SETUP_PAGE_ADDRESS || page == SETUP_PAGE_ACCOUNT)
		entry_changed(GTK_EDITABLE(setupac.addr_entry), NULL);
}

PrefsAccount *setup_account(void)
{
	MainWindow *mainwin;
	GtkWidget *dialog;
	GtkWidget *hbox;
	GtkWidget *image;
	GtkWidget *vbox;
	GtkWidget *vbox2;
	GtkWidget *label;
	GtkWidget *table;
	GtkWidget *chkbtn;
	gint result;
	PrefsAccount *ac;

	mainwin = main_window_get();
	manage_window_focus_in(mainwin->window, NULL, NULL);

	dialog = gtk_dialog_new_with_buttons(_("Account setting"), NULL, GTK_DIALOG_MODAL, NULL);
	setupac.dialog = dialog;

	setupac.prev_btn = gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_GO_BACK, GTK_RESPONSE_REJECT);
	setupac.next_btn = gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_GO_FORWARD, GTK_RESPONSE_ACCEPT);
	setupac.cancel_btn = gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, FALSE);
	gtk_widget_set_size_request(dialog, 540, -1);
	gtk_window_set_position(GTK_WINDOW(dialog),
				GTK_WIN_POS_CENTER_ON_PARENT);
	//gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	manage_window_set_transient(GTK_WINDOW(dialog));
	gtk_dialog_set_has_separator(GTK_DIALOG(dialog), TRUE);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog),
					  GTK_RESPONSE_REJECT, FALSE);
	MANAGE_WINDOW_SIGNALS_CONNECT(dialog);
	gtk_widget_realize(dialog);

	g_signal_connect(dialog, "response",
			 G_CALLBACK(setup_account_response_cb), NULL);

	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 12);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
			   hbox, FALSE, FALSE, 0);

	image = stock_pixbuf_widget(dialog, STOCK_PIXMAP_SYLPHEED);

	gtk_misc_set_alignment(GTK_MISC(image), 0.5, 0.0);
	gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);

	vbox = gtk_vbox_new(FALSE, 12);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Account setting"));
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

	label = gtk_label_new(_("This dialog will make initial setup of account."));
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	GTK_WIDGET_UNSET_FLAGS(label, GTK_CAN_FOCUS);
	MODIFY_LABEL_STYLE();

	vbox = gtk_vbox_new(FALSE, 8);
	//gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), vbox,
			   TRUE, TRUE, 0);

	setupac.notebook = gtk_notebook_new();
	gtk_notebook_set_show_border(GTK_NOTEBOOK(setupac.notebook), FALSE);
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(setupac.notebook), FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), setupac.notebook, TRUE, TRUE, 0);

	/* Page 1 */
	vbox = gtk_vbox_new(FALSE, 12);
	gtk_notebook_append_page(GTK_NOTEBOOK(setupac.notebook), vbox, NULL);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);

	label = gtk_label_new(_("Select account type:"));
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 8);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);

	setupac.pop3_radio = gtk_radio_button_new_with_label(NULL, "POP3");
	gtk_box_pack_start(GTK_BOX(vbox), setupac.pop3_radio, FALSE, FALSE, 0);
	setupac.imap_radio = gtk_radio_button_new_with_label_from_widget
		(GTK_RADIO_BUTTON(setupac.pop3_radio), "IMAP4");
	gtk_box_pack_start(GTK_BOX(vbox), setupac.imap_radio, FALSE, FALSE, 0);
	setupac.pop3g_radio = gtk_radio_button_new_with_label_from_widget
		(GTK_RADIO_BUTTON(setupac.pop3_radio), "POP3 (Gmail)");
	gtk_box_pack_start(GTK_BOX(vbox), setupac.pop3g_radio, FALSE, FALSE, 0);
	setupac.imapg_radio = gtk_radio_button_new_with_label_from_widget
		(GTK_RADIO_BUTTON(setupac.pop3_radio), "IMAP4 (Gmail)");
	gtk_box_pack_start(GTK_BOX(vbox), setupac.imapg_radio, FALSE, FALSE, 0);

	/* Page 2 */
	vbox = gtk_vbox_new(FALSE, 12);
	gtk_notebook_append_page(GTK_NOTEBOOK(setupac.notebook), vbox, NULL);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);

	label = gtk_label_new(_("Input your name and mail address:"));
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 8);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);

	table = gtk_table_new(4, 4, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	gtk_table_set_row_spacings(GTK_TABLE(table), 8);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	label = gtk_label_new(_("Display name:"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
			 GTK_FILL, GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	label = gtk_label_new(_("E-mail address:"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4,
			 GTK_FILL, GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);

	setupac.name_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), setupac.name_entry, 1, 2, 0, 1,
			 GTK_EXPAND|GTK_FILL, GTK_FILL, 0, 0);
	g_signal_connect(setupac.name_entry, "changed",
			 G_CALLBACK(entry_changed), NULL);
	setupac.addr_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), setupac.addr_entry, 1, 2, 3, 4,
			 GTK_EXPAND|GTK_FILL, GTK_FILL, 0, 0);
	g_signal_connect(setupac.addr_entry, "changed",
			 G_CALLBACK(entry_changed), NULL);

	label = gtk_label_new(_("This name will be seen at the side of recipients."));
	gtk_table_attach(GTK_TABLE(table), label, 1, 2, 1, 2,
			 GTK_FILL, GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	/* Page 3 */
	vbox = gtk_vbox_new(FALSE, 12);
	gtk_notebook_append_page(GTK_NOTEBOOK(setupac.notebook), vbox, NULL);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);
	gtk_widget_show_all(dialog);

	label = gtk_label_new(_("Input user ID and mail server:"));
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 8);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);

	table = gtk_table_new(6, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	gtk_table_set_row_spacings(GTK_TABLE(table), 8);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	label = gtk_label_new(_("User ID:"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
			 GTK_FILL, GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	label = gtk_label_new(_("POP3 Server:"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
			 GTK_FILL, GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	setupac.serv_label_name1 = label;
	label = gtk_label_new(_("SMTP server:"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 4, 5,
			 GTK_FILL, GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);

	setupac.id_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), setupac.id_entry, 1, 2, 0, 1,
			 GTK_EXPAND|GTK_FILL, GTK_FILL, 0, 0);
	g_signal_connect(setupac.id_entry, "changed",
			 G_CALLBACK(entry_changed), NULL);
	setupac.serv_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), setupac.serv_entry, 1, 2, 1, 2,
			 GTK_EXPAND|GTK_FILL, GTK_FILL, 0, 0);
	g_signal_connect(setupac.serv_entry, "changed",
			 G_CALLBACK(entry_changed), NULL);
	setupac.smtp_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), setupac.smtp_entry, 1, 2, 4, 5,
			 GTK_EXPAND|GTK_FILL, GTK_FILL, 0, 0);
	g_signal_connect(setupac.smtp_entry, "changed",
			 G_CALLBACK(entry_changed), NULL);

	hbox = gtk_hbox_new(FALSE, 12);
	gtk_table_attach(GTK_TABLE(table), hbox, 1, 2, 2, 3,
			 GTK_FILL, GTK_FILL, 0, 0);
	chkbtn = gtk_check_button_new_with_mnemonic(_("Use SSL"));
	gtk_box_pack_start(GTK_BOX(hbox), chkbtn, FALSE, FALSE, 0);
	setupac.servssl_chkbtn = chkbtn;

	hbox = gtk_hbox_new(FALSE, 12);
	gtk_table_attach(GTK_TABLE(table), hbox, 1, 2, 5, 6,
			 GTK_FILL, GTK_FILL, 0, 0);
	chkbtn = gtk_check_button_new_with_mnemonic(_("Use SMTP authentication"));
	gtk_box_pack_start(GTK_BOX(hbox), chkbtn, FALSE, FALSE, 0);
	setupac.smtpauth_chkbtn = chkbtn;
	chkbtn = gtk_check_button_new_with_mnemonic(_("Use SSL"));
	gtk_box_pack_start(GTK_BOX(hbox), chkbtn, FALSE, FALSE, 0);
	setupac.smtpssl_chkbtn = chkbtn;

	/* Page 4 */
	vbox = gtk_vbox_new(FALSE, 12);
	gtk_notebook_append_page(GTK_NOTEBOOK(setupac.notebook), vbox, NULL);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);

	vbox2 = gtk_vbox_new(FALSE, 8);
	gtk_box_pack_start(GTK_BOX(vbox), vbox2, FALSE, FALSE, 8);

	label = gtk_label_new(_("Your mail accout has been set up with the following setting."));
	gtk_box_pack_start(GTK_BOX(vbox2), label, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
	label = gtk_label_new(_("If you want to modify the settings, select\n"
				"'Configuration - Preferences for current account' or\n"
				"'Configuration - Edit accounts'."));
	gtk_box_pack_start(GTK_BOX(vbox2), label, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
	//gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);

	table = gtk_table_new(5, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	gtk_table_set_row_spacings(GTK_TABLE(table), 8);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	label = gtk_label_new(_("Display name:"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
			 GTK_FILL, GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	label = gtk_label_new(_("E-mail address:"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
			 GTK_FILL, GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	label = gtk_label_new(_("User ID:"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3,
			 GTK_FILL, GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	setupac.serv_label_name2 = gtk_label_new(_("POP3 server:"));
	gtk_table_attach(GTK_TABLE(table), setupac.serv_label_name2, 0, 1, 3, 4,
			 GTK_FILL, GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(setupac.serv_label_name2), 1, 0.5);
	label = gtk_label_new(_("SMTP server:"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 4, 5,
			 GTK_FILL, GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);

	setupac.name_label = gtk_label_new("");
	gtk_table_attach(GTK_TABLE(table), setupac.name_label, 1, 2, 0, 1,
			 GTK_FILL, GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(setupac.name_label), 0, 0.5);
	setupac.addr_label = gtk_label_new("");
	gtk_table_attach(GTK_TABLE(table), setupac.addr_label, 1, 2, 1, 2,
			 GTK_FILL, GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(setupac.addr_label), 0, 0.5);
	setupac.id_label = gtk_label_new("");
	gtk_table_attach(GTK_TABLE(table), setupac.id_label, 1, 2, 2, 3,
			 GTK_FILL, GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(setupac.id_label), 0, 0.5);
	setupac.serv_label = gtk_label_new("");
	gtk_table_attach(GTK_TABLE(table), setupac.serv_label, 1, 2, 3, 4,
			 GTK_FILL, GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(setupac.serv_label), 0, 0.5);
	setupac.smtp_label = gtk_label_new("");
	gtk_table_attach(GTK_TABLE(table), setupac.smtp_label, 1, 2, 4, 5,
			 GTK_FILL, GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(setupac.smtp_label), 0, 0.5);


	gtk_widget_show_all(dialog);

	while (!setupac.finished)
		result = gtk_dialog_run(GTK_DIALOG(dialog));

	gtk_widget_destroy(dialog);

	if (setupac.cancelled) {
		memset(&setupac, 0, sizeof(setupac));
		return NULL;
	}

	/* Create account */
	ac = prefs_account_new();

	if (!cur_account) {
		account_set_as_default(ac);
		cur_account = ac;
	}
	g_free(ac->account_name);
	ac->account_name = g_strdup(setupac.addr);
	g_free(ac->name);
	ac->name = g_strdup(setupac.name);
	g_free(ac->address);
	ac->address = g_strdup(setupac.addr);
	g_free(ac->recv_server);
	ac->recv_server = g_strdup(setupac.serv);
	g_free(ac->smtp_server);
	ac->smtp_server = g_strdup(setupac.smtpserv);
	g_free(ac->userid);
	ac->userid = g_strdup(setupac.userid);
	if (setupac.smtp_ssl)
		ac->ssl_smtp = SSL_TUNNEL;
	ac->smtpport = setupac.smtp_port;
	ac->use_smtp_auth = setupac.smtp_auth;

	switch (setupac.type) {
	case SETUP_TYPE_POP3:
		ac->protocol = A_POP3;
		if (setupac.serv_ssl)
			ac->ssl_pop = SSL_TUNNEL;
		ac->popport = setupac.serv_port;
		break;
	case SETUP_TYPE_POP3G:
		ac->protocol = A_POP3;
		ac->ssl_pop = SSL_TUNNEL;
		ac->popport = setupac.serv_port;
		break;
	case SETUP_TYPE_IMAP:
		ac->protocol = A_IMAP4;
		if (setupac.serv_ssl)
			ac->ssl_imap = SSL_TUNNEL;
		ac->imapport = setupac.serv_port;
		break;
	case SETUP_TYPE_IMAPG:
		ac->protocol = A_IMAP4;
		ac->ssl_imap = SSL_TUNNEL;
		ac->imapport = setupac.serv_port;
		break;
	}

	g_free(ac->sig_text);
	ac->sig_text = g_strdup_printf("%s <%s>\\n", ac->name, ac->address);

	account_append(ac);

	g_free(setupac.name);
	g_free(setupac.addr);
	g_free(setupac.serv);
	g_free(setupac.smtpserv);
	g_free(setupac.userid);
	memset(&setupac, 0, sizeof(setupac));

	return ac;
}
