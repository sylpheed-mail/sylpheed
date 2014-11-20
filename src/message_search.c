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
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtktable.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkstock.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "message_search.h"
#include "messageview.h"
#include "utils.h"
#include "gtkutils.h"
#include "manage_window.h"
#include "alertpanel.h"

static struct MessageSearchWindow {
	GtkWidget *window;
	GtkWidget *body_entry;
	GtkWidget *case_checkbtn;
	GtkWidget *next_btn;
	GtkWidget *prev_btn;
	GtkWidget *close_btn;

	MessageView *messageview;
} search_window;

static void message_search_create	(void);
static void message_search_execute	(gboolean	 backward);
static void message_search_close	(void);

static void message_search_prev_clicked	(GtkButton	*button,
					 gpointer	 data);
static void message_search_next_clicked	(GtkButton	*button,
					 gpointer	 data);
static void close_clicked		(GtkButton	*button,
					 gpointer	 data);
static void body_activated		(void);
static gboolean window_deleted		(GtkWidget	*widget,
					 GdkEventAny	*event,
					 gpointer	 data);
static gboolean key_pressed		(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gpointer	 data);

static void view_destroyed		(GtkWidget	*widget,
					 gpointer	 data);

void message_search(MessageView *messageview)
{
	if (!search_window.window)
		message_search_create();

	search_window.messageview = messageview;
	g_signal_handlers_disconnect_by_func(GTK_WIDGET_PTR(messageview),
					     view_destroyed, messageview);
	g_signal_connect(G_OBJECT(GTK_WIDGET_PTR(messageview)), "destroy",
			 G_CALLBACK(view_destroyed), messageview);

	gtk_widget_grab_focus(search_window.next_btn);
	gtk_widget_grab_focus(search_window.body_entry);
	gtk_widget_show(search_window.window);
	gtk_window_present(GTK_WINDOW(search_window.window));
}

static void message_search_create(void)
{
	GtkWidget *window;

	GtkWidget *vbox1;
	GtkWidget *hbox1;
	GtkWidget *body_label;
	GtkWidget *body_entry;

	GtkWidget *checkbtn_hbox;
	GtkWidget *case_checkbtn;

	GtkWidget *confirm_area;
	GtkWidget *next_btn;
	GtkWidget *prev_btn;
	GtkWidget *close_btn;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (window),
			      _("Find in current message"));
	gtk_widget_set_size_request (window, 450 * gtkut_get_dpi_multiplier(), -1);
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, TRUE);
	gtk_container_set_border_width (GTK_CONTAINER (window), 8);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(window_deleted), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);

	vbox1 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (window), vbox1);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, TRUE, TRUE, 0);

	body_label = gtk_label_new (_("Find text:"));
	gtk_widget_show (body_label);
	gtk_box_pack_start (GTK_BOX (hbox1), body_label, FALSE, FALSE, 0);

	body_entry = gtk_entry_new ();
	gtk_widget_show (body_entry);
	gtk_box_pack_start (GTK_BOX (hbox1), body_entry, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(body_entry), "activate",
			 G_CALLBACK(body_activated), NULL);

	checkbtn_hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (checkbtn_hbox);
	gtk_box_pack_start (GTK_BOX (vbox1), checkbtn_hbox, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (checkbtn_hbox), 8);

	case_checkbtn = gtk_check_button_new_with_label (_("Case sensitive"));
	gtk_widget_show (case_checkbtn);
	gtk_box_pack_start (GTK_BOX (checkbtn_hbox), case_checkbtn,
			    FALSE, FALSE, 0);

	gtkut_stock_button_set_create(&confirm_area,
				      &next_btn, GTK_STOCK_GO_FORWARD,
				      &prev_btn, GTK_STOCK_GO_BACK,
				      &close_btn, GTK_STOCK_CLOSE);
	gtkut_box_set_reverse_order(GTK_BOX(confirm_area), FALSE);
	gtk_widget_show (confirm_area);
	gtk_box_pack_start (GTK_BOX (vbox1), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default(next_btn);

	g_signal_connect(G_OBJECT(prev_btn), "clicked",
			 G_CALLBACK(message_search_prev_clicked), NULL);
	g_signal_connect(G_OBJECT(next_btn), "clicked",
			 G_CALLBACK(message_search_next_clicked), NULL);
	g_signal_connect(G_OBJECT(close_btn), "clicked",
			 G_CALLBACK(close_clicked), NULL);

	search_window.window = window;
	search_window.body_entry = body_entry;
	search_window.case_checkbtn = case_checkbtn;
	search_window.next_btn = next_btn;
	search_window.prev_btn = prev_btn;
	search_window.close_btn = close_btn;
}

static void message_search_execute(gboolean backward)
{
	MessageView *messageview = search_window.messageview;
	gboolean case_sens;
	gboolean all_searched = FALSE;
	const gchar *body_str;

	body_str = gtk_entry_get_text(GTK_ENTRY(search_window.body_entry));
	if (*body_str == '\0') return;

	case_sens = gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON(search_window.case_checkbtn));

	for (;;) {
		gchar *str;
		AlertValue val;

		if (backward) {
			if (messageview_search_string_backward
				(messageview, body_str, case_sens) == TRUE)
				break;
		} else {
			if (messageview_search_string
				(messageview, body_str, case_sens) == TRUE)
				break;
		}

		if (all_searched) {
			alertpanel_message
				(_("Search failed"),
				 _("Search string not found."),
				 ALERT_WARNING);
			break;
		}

		all_searched = TRUE;

		if (backward)
			str = _("Beginning of message reached; "
				"continue from end?");
		else
			str = _("End of message reached; "
				"continue from beginning?");

		val = alertpanel(_("Search finished"), str,
				 GTK_STOCK_YES, GTK_STOCK_NO, NULL);
		if (G_ALERTDEFAULT == val) {
			manage_window_focus_in(search_window.window,
					       NULL, NULL);
			messageview_set_position(messageview,
						 backward ? -1 : 0);
		} else
			break;
	}
}

static void message_search_close(void)
{
	search_window.messageview = NULL;
	gtk_widget_hide(search_window.window);
}

static void message_search_prev_clicked(GtkButton *button, gpointer data)
{
	message_search_execute(TRUE);
}

static void message_search_next_clicked(GtkButton *button, gpointer data)
{
	message_search_execute(FALSE);
}

static void close_clicked(GtkButton *button, gpointer data)
{
	message_search_close();
}

static void body_activated(void)
{
	gtk_button_clicked(GTK_BUTTON(search_window.next_btn));
}

static gboolean window_deleted(GtkWidget *widget, GdkEventAny *event,
			       gpointer data)
{
	message_search_close();
	return TRUE;
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event,
			    gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		message_search_close();
	return FALSE;
}

static void view_destroyed(GtkWidget *widget, gpointer data)
{
	if ((MessageView *)data == search_window.messageview)
		message_search_close();
}
