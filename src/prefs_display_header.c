/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2005 Hiroyuki Yamamoto
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
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "prefs.h"
#include "prefs_ui.h"
#include "prefs_display_header.h"
#include "prefs_common.h"
#include "manage_window.h"
#include "alertpanel.h"
#include "displayheader.h"
#include "utils.h"
#include "gtkutils.h"

static struct DisplayHeader {
	GtkWidget *window;

	GtkWidget *confirm_area;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;

	GtkWidget *hdr_combo;
	GtkWidget *hdr_entry;
	GtkWidget *key_check;
	GtkWidget *headers_clist;
	GtkWidget *hidden_headers_clist;

	GtkWidget *other_headers;
} dispheader;

/* widget creating functions */
static void prefs_display_header_create	(void);

static void prefs_display_header_set_dialog	(void);
static void prefs_display_header_set_list	(void);
static gint prefs_display_header_clist_set_row	(gboolean hidden);

/* callback functions */
static void prefs_display_header_register_cb	(GtkButton	*btn,
						 gpointer	 hidden_data);
static void prefs_display_header_delete_cb	(GtkButton	*btn,
						 gpointer	 clist_data);
static void prefs_display_header_up		(void);
static void prefs_display_header_down		(void);

static void prefs_display_header_row_moved	(GtkCList	*clist,
						 gint		 source_row,
						 gint		 dest_row,
						 gpointer	 data);

static gboolean prefs_display_header_key_pressed(GtkWidget	*widget,
						 GdkEventKey	*event,
						 gpointer	 data);
static void prefs_display_header_ok		(void);
static void prefs_display_header_cancel		(void);
static gint prefs_display_header_deleted	(GtkWidget	*widget,
						 GdkEventAny	*event,
						 gpointer	 data);

static gchar *defaults[] =
{
	"From",
	"To",
	"Cc",
	"Reply-To",
	"Newsgroups",
	"Followup-To",
	"Subject",
	"Date",
	"Sender",
	"Organization",
	"X-Mailer",
	"X-Newsreader",
	"User-Agent",
	"-Received",
	"-Message-Id",
	"-In-Reply-To",
	"-References",
	"-Mime-Version",
	"-Content-Type",
	"-Content-Transfer-Encoding",
	"-X-UIDL",
	"-Precedence",
	"-Status",
	"-Priority",
	"-X-Face"
};

static void prefs_display_header_set_default(void)
{
	gint i;
	DisplayHeaderProp *dp;

	for(i = 0; i < sizeof(defaults) / sizeof(defaults[0]); i++) {
		dp = display_header_prop_read_str(defaults[i]);
		prefs_common.disphdr_list =
			g_slist_append(prefs_common.disphdr_list, dp);
	}
}

void prefs_display_header_open(void)
{
	if (!dispheader.window) {
		prefs_display_header_create();
	}

	gtkut_box_set_reverse_order(GTK_BOX(dispheader.confirm_area),
				    !prefs_common.comply_gnome_hig);
	manage_window_set_transient(GTK_WINDOW(dispheader.window));
	gtk_widget_grab_focus(dispheader.ok_btn);

	prefs_display_header_set_dialog();

	gtk_widget_show(dispheader.window);
}

static void prefs_display_header_create(void)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *btn_hbox;
	GtkWidget *confirm_area;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;

	GtkWidget *vbox1;

	GtkWidget *hbox1;
	GtkWidget *hdr_label;
	GtkWidget *hdr_combo;

	GtkWidget *btn_vbox;
	GtkWidget *reg_btn;
	GtkWidget *del_btn;
	GtkWidget *up_btn;
	GtkWidget *down_btn;

	GtkWidget *clist_hbox;
	GtkWidget *clist_hbox1;
	GtkWidget *clist_hbox2;
	GtkWidget *clist_scrolledwin;
	GtkWidget *headers_clist;
	GtkWidget *hidden_headers_clist;

	GtkWidget *checkbtn_other_headers;

	gchar *title[1];

	debug_print(_("Creating display header setting window...\n"));

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width (GTK_CONTAINER (window), 8);
	gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal (GTK_WINDOW (window), TRUE);
	gtk_window_set_policy (GTK_WINDOW (window), FALSE, TRUE, FALSE);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	btn_hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (btn_hbox);
	gtk_box_pack_end (GTK_BOX (vbox), btn_hbox, FALSE, FALSE, 0);

	gtkut_stock_button_set_create(&confirm_area, &ok_btn, GTK_STOCK_OK,
				      &cancel_btn, GTK_STOCK_CANCEL,
				      NULL, NULL);
	gtk_widget_show (confirm_area);
	gtk_box_pack_end (GTK_BOX(btn_hbox), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default (ok_btn);

	gtk_window_set_title (GTK_WINDOW(window),
			      _("Display header setting"));
	MANAGE_WINDOW_SIGNALS_CONNECT(window);
	g_signal_connect (G_OBJECT(window), "delete_event",
			  G_CALLBACK(prefs_display_header_deleted), NULL);
	g_signal_connect (G_OBJECT(window), "key_press_event",
			  G_CALLBACK(prefs_display_header_key_pressed), NULL);
	g_signal_connect (G_OBJECT(ok_btn), "clicked",
			  G_CALLBACK(prefs_display_header_ok), NULL);
	g_signal_connect (G_OBJECT(cancel_btn), "clicked",
			  G_CALLBACK(prefs_display_header_cancel), NULL);

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_box_pack_start (GTK_BOX (vbox), vbox1, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), 2);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, TRUE, 0);

	hdr_label = gtk_label_new (_("Header name"));
	gtk_widget_show (hdr_label);
	gtk_box_pack_start (GTK_BOX (hbox1), hdr_label, FALSE, FALSE, 0);

	hdr_combo = gtk_combo_new ();
	gtk_widget_show (hdr_combo);
	gtk_box_pack_start (GTK_BOX (hbox1), hdr_combo, TRUE, TRUE, 0);
	gtk_widget_set_size_request (hdr_combo, 150, -1);
	gtkut_combo_set_items (GTK_COMBO (hdr_combo),
			       "From", "To", "Cc", "Subject", "Date",
			       "Reply-To", "Sender", "User-Agent", "X-Mailer",
			       NULL);

	clist_hbox = gtk_hbox_new (FALSE, 10);
	gtk_widget_show (clist_hbox);
	gtk_box_pack_start (GTK_BOX (vbox1), clist_hbox, TRUE, TRUE, 0);

	/* display headers list */

	clist_hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (clist_hbox1);
	gtk_box_pack_start (GTK_BOX (clist_hbox), clist_hbox1, TRUE, TRUE, 0);

	clist_scrolledwin = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_set_size_request (clist_scrolledwin, 200, 210);
	gtk_widget_show (clist_scrolledwin);
	gtk_box_pack_start (GTK_BOX (clist_hbox1), clist_scrolledwin,
			    TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (clist_scrolledwin),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	title[0] = _("Displayed Headers");
	headers_clist = gtk_clist_new_with_titles(1, title);
	gtk_widget_show (headers_clist);
	gtk_container_add (GTK_CONTAINER (clist_scrolledwin), headers_clist);
	gtk_clist_set_selection_mode (GTK_CLIST (headers_clist),
				      GTK_SELECTION_BROWSE);
	gtk_clist_set_reorderable (GTK_CLIST (headers_clist), TRUE);
	gtk_clist_set_use_drag_icons (GTK_CLIST (headers_clist), FALSE);
	gtkut_clist_set_redraw (GTK_CLIST (headers_clist));
	GTK_WIDGET_UNSET_FLAGS (GTK_CLIST (headers_clist)->column[0].button,
				GTK_CAN_FOCUS);
	g_signal_connect_after
		(G_OBJECT (headers_clist), "row_move",
		 G_CALLBACK (prefs_display_header_row_moved), NULL);

	btn_vbox = gtk_vbox_new (FALSE, 8);
	gtk_widget_show (btn_vbox);
	gtk_box_pack_start (GTK_BOX (clist_hbox1), btn_vbox, FALSE, FALSE, 0);

	reg_btn = gtk_button_new_with_label (_("Add"));
	gtk_widget_show (reg_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), reg_btn, FALSE, TRUE, 0);
	g_signal_connect (G_OBJECT (reg_btn), "clicked",
			  G_CALLBACK (prefs_display_header_register_cb),
			  GINT_TO_POINTER(FALSE));
	del_btn = gtk_button_new_with_label (_("Delete"));
	gtk_widget_show (del_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), del_btn, FALSE, TRUE, 0);
	g_signal_connect (G_OBJECT (del_btn), "clicked",
			  G_CALLBACK (prefs_display_header_delete_cb),
			  headers_clist);

	up_btn = gtk_button_new_with_label (_("Up"));
	gtk_widget_show (up_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), up_btn, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (up_btn), "clicked",
			  G_CALLBACK (prefs_display_header_up), NULL);

	down_btn = gtk_button_new_with_label (_("Down"));
	gtk_widget_show (down_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), down_btn, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (down_btn), "clicked",
			  G_CALLBACK (prefs_display_header_down), NULL);

	/* hidden headers list */

	clist_hbox2 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (clist_hbox2);
	gtk_box_pack_start (GTK_BOX (clist_hbox), clist_hbox2, TRUE, TRUE, 0);

	clist_scrolledwin = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_set_size_request (clist_scrolledwin, 200, 210);
	gtk_widget_show (clist_scrolledwin);
	gtk_box_pack_start (GTK_BOX (clist_hbox2), clist_scrolledwin,
			    TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (clist_scrolledwin),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	title[0] = _("Hidden headers");
	hidden_headers_clist = gtk_clist_new_with_titles(1, title);
	gtk_widget_show (hidden_headers_clist);
	gtk_container_add (GTK_CONTAINER (clist_scrolledwin),
			   hidden_headers_clist);
	gtk_clist_set_selection_mode (GTK_CLIST (hidden_headers_clist),
				      GTK_SELECTION_BROWSE);
	gtk_clist_set_auto_sort(GTK_CLIST (hidden_headers_clist), TRUE);
	gtkut_clist_set_redraw (GTK_CLIST (hidden_headers_clist));
	GTK_WIDGET_UNSET_FLAGS (GTK_CLIST (hidden_headers_clist)->
				column[0].button, GTK_CAN_FOCUS);

	btn_vbox = gtk_vbox_new (FALSE, 8);
	gtk_widget_show (btn_vbox);
	gtk_box_pack_start (GTK_BOX (clist_hbox2), btn_vbox, FALSE, FALSE, 0);

	reg_btn = gtk_button_new_with_label (_("Add"));
	gtk_widget_show (reg_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), reg_btn, FALSE, TRUE, 0);
	g_signal_connect (G_OBJECT (reg_btn), "clicked",
			  G_CALLBACK (prefs_display_header_register_cb),
			  GINT_TO_POINTER (TRUE));
	del_btn = gtk_button_new_with_label (_("Delete"));
	gtk_widget_show (del_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), del_btn, FALSE, TRUE, 0);
	g_signal_connect (G_OBJECT (del_btn), "clicked",
			  G_CALLBACK (prefs_display_header_delete_cb),
			  hidden_headers_clist);

	PACK_CHECK_BUTTON (btn_hbox, checkbtn_other_headers,
			   _("Show all unspecified headers"));
	SET_TOGGLE_SENSITIVITY (checkbtn_other_headers, clist_hbox2);

	gtk_widget_show_all(window);

	dispheader.window        = window;

	dispheader.confirm_area  = confirm_area;
	dispheader.ok_btn        = ok_btn;
	dispheader.cancel_btn    = cancel_btn;

	dispheader.hdr_combo     = hdr_combo;
	dispheader.hdr_entry     = GTK_COMBO (hdr_combo)->entry;

	dispheader.headers_clist        = headers_clist;
	dispheader.hidden_headers_clist = hidden_headers_clist;

	dispheader.other_headers = checkbtn_other_headers;
}

void prefs_display_header_read_config(void)
{
	gchar *rcpath;
	FILE *fp;
	gchar buf[PREFSBUFSIZE];
	DisplayHeaderProp *dp;

	debug_print(_("Reading configuration for displaying headers...\n"));

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
			     DISPLAY_HEADER_RC, NULL);
	if ((fp = g_fopen(rcpath, "rb")) == NULL) {
		if (ENOENT != errno) FILE_OP_ERROR(rcpath, "fopen");
		g_free(rcpath);
		prefs_common.disphdr_list = NULL;
		prefs_display_header_set_default();
		return;
	}
	g_free(rcpath);

	/* remove all previous headers list */
	while (prefs_common.disphdr_list != NULL) {
		dp = (DisplayHeaderProp *)prefs_common.disphdr_list->data;
		display_header_prop_free(dp);
		prefs_common.disphdr_list =
			g_slist_remove(prefs_common.disphdr_list, dp);
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		g_strchomp(buf);
		dp = display_header_prop_read_str(buf);
		if (dp)
			prefs_common.disphdr_list =
				g_slist_append(prefs_common.disphdr_list, dp);
	}

	fclose(fp);
}

void prefs_display_header_write_config(void)
{
	gchar *rcpath;
	PrefFile *pfile;
	GSList *cur;

	debug_print(_("Writing configuration for displaying headers...\n"));

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
			     DISPLAY_HEADER_RC, NULL);

	if ((pfile = prefs_file_open(rcpath)) == NULL) {
		g_warning(_("failed to write configuration to file\n"));
		g_free(rcpath);
		return;
	}

	for (cur = prefs_common.disphdr_list; cur != NULL;
	     cur = cur->next) {
		DisplayHeaderProp *dp = (DisplayHeaderProp *)cur->data;
		gchar *dpstr;

		dpstr = display_header_prop_get_str(dp);
		if (fputs(dpstr, pfile->fp) == EOF ||
		    fputc('\n', pfile->fp) == EOF) {
			FILE_OP_ERROR(rcpath, "fputs || fputc");
			prefs_file_close_revert(pfile);
			g_free(rcpath);
			g_free(dpstr);
			return;
		}
		g_free(dpstr);
	}

	g_free(rcpath);

	if (prefs_file_close(pfile) < 0) {
		g_warning(_("failed to write configuration to file\n"));
		return;
	}
}

static void prefs_display_header_set_dialog(void)
{
	GtkCList *clist = GTK_CLIST(dispheader.headers_clist);
	GtkCList *hidden_clist = GTK_CLIST(dispheader.hidden_headers_clist);
	GSList *cur;
	gchar *dp_str[1];
	gint row;

	gtk_clist_freeze(clist);
	gtk_clist_freeze(hidden_clist);

	gtk_clist_clear(clist);
	gtk_clist_clear(hidden_clist);

	for (cur = prefs_common.disphdr_list; cur != NULL;
	     cur = cur->next) {
		DisplayHeaderProp *dp = (DisplayHeaderProp *)cur->data;

		dp_str[0] = dp->name;

		if (dp->hidden) {
			row = gtk_clist_append(hidden_clist, dp_str);
			gtk_clist_set_row_data(hidden_clist, row, dp);
		} else {
			row = gtk_clist_append(clist, dp_str);
			gtk_clist_set_row_data(clist, row, dp);
		}
	}

	gtk_clist_thaw(hidden_clist);
	gtk_clist_thaw(clist);

	gtk_toggle_button_set_active
		(GTK_TOGGLE_BUTTON(dispheader.other_headers),
		 prefs_common.show_other_header);
}

static void prefs_display_header_set_list()
{
	gint row = 0;
	DisplayHeaderProp *dp;

	g_slist_free(prefs_common.disphdr_list);
	prefs_common.disphdr_list = NULL;

	while ((dp = gtk_clist_get_row_data
		(GTK_CLIST(dispheader.headers_clist), row)) != NULL) {
		prefs_common.disphdr_list =
			g_slist_append(prefs_common.disphdr_list, dp);
		row++;
	}

	row = 0;
	while ((dp = gtk_clist_get_row_data
		(GTK_CLIST(dispheader.hidden_headers_clist), row)) != NULL) {
		prefs_common.disphdr_list =
			g_slist_append(prefs_common.disphdr_list, dp);
		row++;
	}
}

static gint prefs_display_header_find_header(GtkCList *clist,
					     const gchar *header)
{
	gint row = 0;
	DisplayHeaderProp *dp;

	while ((dp = gtk_clist_get_row_data(clist, row)) != NULL) {
		if (g_ascii_strcasecmp(dp->name, header) == 0)
			return row;
		row++;
	}

	return -1;
}

static gint prefs_display_header_clist_set_row(gboolean hidden)
{
	GtkCList *clist;
	DisplayHeaderProp *dp;
	const gchar *entry_text;
	gchar *dp_str[1];
	gint row;

	entry_text = gtk_entry_get_text(GTK_ENTRY(dispheader.hdr_entry));
	if (entry_text[0] == '\0') {
		alertpanel_error(_("Header name is not set."));
		return -1;
	}

	if (hidden)
		clist = GTK_CLIST(dispheader.hidden_headers_clist);
	else
		clist = GTK_CLIST(dispheader.headers_clist);

	if (prefs_display_header_find_header(clist, entry_text) != -1) {
		alertpanel_error(_("This header is already in the list."));
		return -1;
	}

	dp = g_new0(DisplayHeaderProp, 1);

	dp->name = g_strdup(entry_text);
	dp->hidden = hidden;

	dp_str[0] = dp->name;
	row = gtk_clist_append(clist, dp_str);
	gtk_clist_set_row_data(clist, row, dp);

	prefs_display_header_set_list();

	return row;
}

static void prefs_display_header_register_cb(GtkButton *btn,
					     gpointer hidden_data)
{
	prefs_display_header_clist_set_row(GPOINTER_TO_INT(hidden_data));
}

static void prefs_display_header_delete_cb(GtkButton *btn, gpointer clist_data)
{
	GtkCList *clist = GTK_CLIST(clist_data);
	DisplayHeaderProp *dp;
	gint row;

	if (!clist->selection) return;
	row = GPOINTER_TO_INT(clist->selection->data);

	dp = gtk_clist_get_row_data(clist, row);
	display_header_prop_free(dp);
	gtk_clist_remove(clist, row);
	prefs_common.disphdr_list =
		g_slist_remove(prefs_common.disphdr_list, dp);
}

static void prefs_display_header_up(void)
{
	GtkCList *clist = GTK_CLIST(dispheader.headers_clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row > 0)
		gtk_clist_row_move(clist, row, row - 1);
}

static void prefs_display_header_down(void)
{
	GtkCList *clist = GTK_CLIST(dispheader.headers_clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row >= 0 && row < clist->rows - 1)
		gtk_clist_row_move(clist, row, row + 1);
}

static void prefs_display_header_row_moved(GtkCList *clist, gint source_row,
					   gint dest_row, gpointer data)
{
	prefs_display_header_set_list();
}

static gboolean prefs_display_header_key_pressed(GtkWidget *widget,
					     GdkEventKey *event,
					     gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		prefs_display_header_cancel();
	return FALSE;
}

static void prefs_display_header_ok(void)
{
	prefs_common.show_other_header =
		gtk_toggle_button_get_active
			(GTK_TOGGLE_BUTTON(dispheader.other_headers));
	prefs_display_header_write_config();
	gtk_widget_hide(dispheader.window);
}

static void prefs_display_header_cancel(void)
{
	prefs_display_header_read_config();
	gtk_widget_hide(dispheader.window);
}

static gint prefs_display_header_deleted(GtkWidget *widget, GdkEventAny *event,
					 gpointer data)
{
	prefs_display_header_cancel();
	return TRUE;
}
