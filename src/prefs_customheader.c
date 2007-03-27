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

#include "main.h"
#include "prefs.h"
#include "prefs_ui.h"
#include "prefs_customheader.h"
#include "prefs_common.h"
#include "prefs_account.h"
#include "mainwindow.h"
#include "foldersel.h"
#include "manage_window.h"
#include "customheader.h"
#include "folder.h"
#include "utils.h"
#include "gtkutils.h"
#include "alertpanel.h"

static struct CustomHdr {
	GtkWidget *window;

	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;

	GtkWidget *hdr_combo;
	GtkWidget *hdr_entry;
	GtkWidget *val_entry;
	GtkWidget *customhdr_clist;
} customhdr;

/* widget creating functions */
static void prefs_custom_header_create	(void);

static void prefs_custom_header_set_dialog	(PrefsAccount *ac);
static void prefs_custom_header_set_list	(PrefsAccount *ac);
static gint prefs_custom_header_clist_set_row	(PrefsAccount *ac,
						 gint	       row);

/* callback functions */
static void prefs_custom_header_add_cb		(void);
static void prefs_custom_header_delete_cb	(void);
static void prefs_custom_header_up		(void);
static void prefs_custom_header_down		(void);
static void prefs_custom_header_select		(GtkCList	*clist,
						 gint		 row,
						 gint		 column,
						 GdkEvent	*event);

static void prefs_custom_header_row_moved	(GtkCList	*clist,
						 gint		 source_row,
						 gint		 dest_row,
						 gpointer	 data);

static gboolean prefs_custom_header_key_pressed	(GtkWidget	*widget,
						 GdkEventKey	*event,
						 gpointer	 data);
static void prefs_custom_header_ok		(void);
static void prefs_custom_header_cancel		(void);
static gint prefs_custom_header_deleted		(GtkWidget	*widget,
						 GdkEventAny	*event,
						 gpointer	 data);

static PrefsAccount *cur_ac = NULL;

void prefs_custom_header_open(PrefsAccount *ac)
{
	if (!customhdr.window) {
		prefs_custom_header_create();
	}

	manage_window_set_transient(GTK_WINDOW(customhdr.window));
	gtk_widget_grab_focus(customhdr.ok_btn);

	prefs_custom_header_set_dialog(ac);

	cur_ac = ac;

	gtk_widget_show(customhdr.window);
}

static void prefs_custom_header_create(void)
{
	GtkWidget *window;
	GtkWidget *vbox;

	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;

	GtkWidget *confirm_area;

	GtkWidget *vbox1;

	GtkWidget *table1;
	GtkWidget *hdr_label;
	GtkWidget *hdr_combo;
	GtkWidget *val_label;
	GtkWidget *val_entry;

	GtkWidget *reg_hbox;
	GtkWidget *btn_hbox;
	GtkWidget *arrow;
	GtkWidget *add_btn;
	GtkWidget *del_btn;

	GtkWidget *ch_hbox;
	GtkWidget *ch_scrolledwin;
	GtkWidget *customhdr_clist;

	GtkWidget *btn_vbox;
	GtkWidget *up_btn;
	GtkWidget *down_btn;

	gchar *title[1];

	debug_print("Creating custom header setting window...\n");

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width (GTK_CONTAINER (window), 8);
	gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal (GTK_WINDOW (window), TRUE);
	gtk_window_set_policy (GTK_WINDOW (window), FALSE, TRUE, FALSE);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	gtkut_stock_button_set_create(&confirm_area, &ok_btn, GTK_STOCK_OK,
				      &cancel_btn, GTK_STOCK_CANCEL,
				      NULL, NULL);
	gtk_widget_show (confirm_area);
	gtk_box_pack_end (GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default (ok_btn);

	gtk_window_set_title (GTK_WINDOW(window), _("Custom header setting"));
	MANAGE_WINDOW_SIGNALS_CONNECT (window);
	g_signal_connect (G_OBJECT(window), "delete_event",
			  G_CALLBACK(prefs_custom_header_deleted),
			  NULL);
	g_signal_connect (G_OBJECT(window), "key_press_event",
			  G_CALLBACK(prefs_custom_header_key_pressed),
			  NULL);
	g_signal_connect (G_OBJECT(ok_btn), "clicked",
			  G_CALLBACK(prefs_custom_header_ok), NULL);
	g_signal_connect (G_OBJECT(cancel_btn), "clicked",
			  G_CALLBACK(prefs_custom_header_cancel), NULL);

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_box_pack_start (GTK_BOX (vbox), vbox1, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), 2);

	table1 = gtk_table_new (2, 2, FALSE);
	gtk_widget_show (table1);
	gtk_box_pack_start (GTK_BOX (vbox1), table1,
			    FALSE, FALSE, 0);
	gtk_table_set_row_spacings (GTK_TABLE (table1), 8);
	gtk_table_set_col_spacings (GTK_TABLE (table1), 8);

	hdr_label = gtk_label_new (_("Header"));
	gtk_widget_show (hdr_label);
	gtk_table_attach (GTK_TABLE (table1), hdr_label, 0, 1, 0, 1,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (hdr_label), 0, 0.5);
	
	hdr_combo = gtk_combo_new ();
	gtk_widget_show (hdr_combo);
	gtk_table_attach (GTK_TABLE (table1), hdr_combo, 0, 1, 1, 2,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  0, 0, 0);
	gtk_widget_set_size_request (hdr_combo, 150, -1);
	gtkut_combo_set_items (GTK_COMBO (hdr_combo),
			       "User-Agent", "X-Face", "X-Operating-System",
			       NULL);

	val_label = gtk_label_new (_("Value"));
	gtk_widget_show (val_label);
	gtk_table_attach (GTK_TABLE (table1), val_label, 1, 2, 0, 1,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (val_label), 0, 0.5);
	
	val_entry = gtk_entry_new ();
	gtk_widget_show (val_entry);
	gtk_table_attach (GTK_TABLE (table1), val_entry, 1, 2, 1, 2,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  0, 0, 0);
	gtk_widget_set_size_request (val_entry, 200, -1);

	/* add / delete */

	reg_hbox = gtk_hbox_new (FALSE, 4);
	gtk_widget_show (reg_hbox);
	gtk_box_pack_start (GTK_BOX (vbox1), reg_hbox, FALSE, FALSE, 0);

	arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_widget_show (arrow);
	gtk_box_pack_start (GTK_BOX (reg_hbox), arrow, FALSE, FALSE, 0);
	gtk_widget_set_size_request (arrow, -1, 16);

	btn_hbox = gtk_hbox_new (TRUE, 4);
	gtk_widget_show (btn_hbox);
	gtk_box_pack_start (GTK_BOX (reg_hbox), btn_hbox, FALSE, FALSE, 0);

	add_btn = gtk_button_new_with_label (_("Add"));
	gtk_widget_show (add_btn);
	gtk_box_pack_start (GTK_BOX (btn_hbox), add_btn, FALSE, TRUE, 0);
	g_signal_connect (G_OBJECT (add_btn), "clicked",
			  G_CALLBACK (prefs_custom_header_add_cb), NULL);

	del_btn = gtk_button_new_with_label (_(" Delete "));
	gtk_widget_show (del_btn);
	gtk_box_pack_start (GTK_BOX (btn_hbox), del_btn, FALSE, TRUE, 0);
	g_signal_connect (G_OBJECT (del_btn), "clicked",
			  G_CALLBACK (prefs_custom_header_delete_cb), NULL);


	ch_hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (ch_hbox);
	gtk_box_pack_start (GTK_BOX (vbox1), ch_hbox, TRUE, TRUE, 0);

	ch_scrolledwin = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_set_size_request (ch_scrolledwin, -1, 200);
	gtk_widget_show (ch_scrolledwin);
	gtk_box_pack_start (GTK_BOX (ch_hbox), ch_scrolledwin, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (ch_scrolledwin),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	title[0] = _("Custom headers");
	customhdr_clist = gtk_clist_new_with_titles(1, title);
	gtk_widget_show (customhdr_clist);
	gtk_container_add (GTK_CONTAINER (ch_scrolledwin), customhdr_clist);
	gtk_clist_set_column_width (GTK_CLIST (customhdr_clist), 0, 80);
	gtk_clist_set_selection_mode (GTK_CLIST (customhdr_clist),
				      GTK_SELECTION_BROWSE);
	gtk_clist_set_reorderable (GTK_CLIST (customhdr_clist), TRUE);
	gtk_clist_set_use_drag_icons (GTK_CLIST (customhdr_clist), FALSE);
	gtkut_clist_set_redraw (GTK_CLIST (customhdr_clist));
	GTK_WIDGET_UNSET_FLAGS (GTK_CLIST (customhdr_clist)->column[0].button,
				GTK_CAN_FOCUS);
	g_signal_connect (G_OBJECT (customhdr_clist), "select_row",
			  G_CALLBACK (prefs_custom_header_select), NULL);
	g_signal_connect_after
		(G_OBJECT (customhdr_clist), "row_move",
		 G_CALLBACK (prefs_custom_header_row_moved), NULL);

	btn_vbox = gtk_vbox_new (FALSE, 8);
	gtk_widget_show (btn_vbox);
	gtk_box_pack_start (GTK_BOX (ch_hbox), btn_vbox, FALSE, FALSE, 0);

	up_btn = gtk_button_new_with_label (_("Up"));
	gtk_widget_show (up_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), up_btn, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (up_btn), "clicked",
			  G_CALLBACK (prefs_custom_header_up), NULL);

	down_btn = gtk_button_new_with_label (_("Down"));
	gtk_widget_show (down_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), down_btn, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (down_btn), "clicked",
			  G_CALLBACK (prefs_custom_header_down), NULL);

	gtk_widget_show_all(window);

	customhdr.window     = window;
	customhdr.ok_btn     = ok_btn;
	customhdr.cancel_btn = cancel_btn;

	customhdr.hdr_combo  = hdr_combo;
	customhdr.hdr_entry  = GTK_COMBO (hdr_combo)->entry;
	customhdr.val_entry  = val_entry;

	customhdr.customhdr_clist   = customhdr_clist;
}

static void prefs_custom_header_set_dialog(PrefsAccount *ac)
{
	GtkCList *clist = GTK_CLIST(customhdr.customhdr_clist);
	GSList *cur;
	gchar *ch_str[1];
	gint row;

	gtk_clist_freeze(clist);
	gtk_clist_clear(clist);

	for (cur = ac->customhdr_list; cur != NULL; cur = cur->next) {
 		CustomHeader *ch = (CustomHeader *)cur->data;

		ch_str[0] = g_strdup_printf("%s: %s", ch->name,
					    ch->value ? ch->value : "");
		row = gtk_clist_append(clist, ch_str);
		gtk_clist_set_row_data(clist, row, ch);

		g_free(ch_str[0]);
	}

	gtk_clist_thaw(clist);
}

static void prefs_custom_header_set_list(PrefsAccount *ac)
{
	gint row = 0;
	CustomHeader *ch;

	g_slist_free(ac->customhdr_list);
	ac->customhdr_list = NULL;

	while ((ch = gtk_clist_get_row_data
		(GTK_CLIST(customhdr.customhdr_clist), row)) != NULL) {
		ac->customhdr_list = g_slist_append(ac->customhdr_list, ch);
		row++;
	}
}

static gint prefs_custom_header_clist_set_row(PrefsAccount *ac, gint row)
{
	GtkCList *clist = GTK_CLIST(customhdr.customhdr_clist);
	CustomHeader *ch;
	const gchar *entry_text;
	gchar *ch_str[1];

	entry_text = gtk_entry_get_text(GTK_ENTRY(customhdr.hdr_entry));
	if (entry_text[0] == '\0') {
		alertpanel_error(_("Header name is not set."));
		return -1;
	}

	ch = g_new0(CustomHeader, 1);

	ch->account_id = ac->account_id;

	ch->name = g_strdup(entry_text);
	unfold_line(ch->name);
	g_strstrip(ch->name);
	gtk_entry_set_text(GTK_ENTRY(customhdr.hdr_entry), ch->name);

	entry_text = gtk_entry_get_text(GTK_ENTRY(customhdr.val_entry));
	if (entry_text[0] != '\0') {
		ch->value = g_strdup(entry_text);
		unfold_line(ch->value);
		g_strstrip(ch->value);
		gtk_entry_set_text(GTK_ENTRY(customhdr.val_entry), ch->value);
	}

	ch_str[0] = g_strdup_printf("%s: %s", ch->name,
				    ch->value ? ch->value : "");

	if (row < 0)
		row = gtk_clist_append(clist, ch_str);
	else {
		CustomHeader *tmp_ch;

		gtk_clist_set_text(clist, row, 0, ch_str[0]);
		tmp_ch = gtk_clist_get_row_data(clist, row);
		if (tmp_ch)
			custom_header_free(tmp_ch);
	}

	gtk_clist_set_row_data(clist, row, ch);

	g_free(ch_str[0]);

	prefs_custom_header_set_list(cur_ac);

	return row;
}

static void prefs_custom_header_add_cb(void)
{
	prefs_custom_header_clist_set_row(cur_ac, -1);
}

static void prefs_custom_header_delete_cb(void)
{
	GtkCList *clist = GTK_CLIST(customhdr.customhdr_clist);
	CustomHeader *ch;
	gint row;

	if (!clist->selection) return;
	row = GPOINTER_TO_INT(clist->selection->data);

	if (alertpanel(_("Delete header"),
		       _("Do you really want to delete this header?"),
		       GTK_STOCK_YES, GTK_STOCK_NO, NULL) != G_ALERTDEFAULT)
		return;

	ch = gtk_clist_get_row_data(clist, row);
	custom_header_free(ch);
	gtk_clist_remove(clist, row);
	cur_ac->customhdr_list = g_slist_remove(cur_ac->customhdr_list, ch);
}

static void prefs_custom_header_up(void)
{
	GtkCList *clist = GTK_CLIST(customhdr.customhdr_clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row > 0)
		gtk_clist_row_move(clist, row, row - 1);
}

static void prefs_custom_header_down(void)
{
	GtkCList *clist = GTK_CLIST(customhdr.customhdr_clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row >= 0 && row < clist->rows - 1)
		gtk_clist_row_move(clist, row, row + 1);
}

#define ENTRY_SET_TEXT(entry, str) \
	gtk_entry_set_text(GTK_ENTRY(entry), str ? str : "")

static void prefs_custom_header_select(GtkCList *clist, gint row, gint column,
				       GdkEvent *event)
{
	CustomHeader *ch;
	CustomHeader default_ch = { 0, "", NULL };

	ch = gtk_clist_get_row_data(clist, row);
	if (!ch) ch = &default_ch;

	ENTRY_SET_TEXT(customhdr.hdr_entry, ch->name);
	ENTRY_SET_TEXT(customhdr.val_entry, ch->value);
}

#undef ENTRY_SET_TEXT

static void prefs_custom_header_row_moved(GtkCList *clist, gint source_row,
					  gint dest_row, gpointer data)
{
	prefs_custom_header_set_list(cur_ac);
}

static gboolean prefs_custom_header_key_pressed(GtkWidget *widget,
						GdkEventKey *event,
						gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		prefs_custom_header_cancel();
	return FALSE;
}

static void prefs_custom_header_ok(void)
{
	custom_header_write_config(cur_ac);
	gtk_widget_hide(customhdr.window);
}

static void prefs_custom_header_cancel(void)
{
	custom_header_read_config(cur_ac); 
	gtk_widget_hide(customhdr.window);
}

static gint prefs_custom_header_deleted(GtkWidget *widget, GdkEventAny *event,
					gpointer data)
{
	prefs_custom_header_cancel();
	return TRUE;
}
