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

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkstock.h>

#include "progressdialog.h"
#include "gtkutils.h"
#include "utils.h"

ProgressDialog *progress_dialog_create(void)
{
	ProgressDialog *progress;
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *cancel_btn;
	GtkWidget *cancel_area;
	GtkWidget *progressbar;
	GtkWidget *scrolledwin;
	GtkWidget *clist;
	gchar *text[] = {NULL, NULL, NULL};

	text[1] = _("Account");
	text[2] = _("Status");

	debug_print(_("Creating progress dialog...\n"));
	progress = g_new0(ProgressDialog, 1);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_size_request(window, 460, -1);
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, TRUE);
	gtk_widget_realize(window);

	vbox = gtk_vbox_new(FALSE, 8);
	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_widget_show(vbox);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 8);
	gtk_widget_show(hbox);

	label = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 8);
	gtk_widget_show(label);

	gtkut_stock_button_set_create
		(&cancel_area, &cancel_btn, GTK_STOCK_CANCEL,
		 NULL, NULL, NULL, NULL);
	gtk_box_pack_end(GTK_BOX(vbox), cancel_area, FALSE, FALSE, 0);
	gtk_widget_grab_default(cancel_btn);
	gtk_widget_show_all(cancel_area);

	progressbar = gtk_progress_bar_new();
	gtk_box_pack_start(GTK_BOX(vbox), progressbar, FALSE, FALSE, 0);
	gtk_widget_show(progressbar);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scrolledwin);
	gtk_box_pack_start(GTK_BOX(vbox), scrolledwin, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);

	clist = gtk_clist_new_with_titles(3, text);
	gtk_widget_show(clist);
	gtk_container_add(GTK_CONTAINER(scrolledwin), clist);
	gtk_widget_set_size_request(clist, -1, 120);
	gtk_clist_set_column_justification(GTK_CLIST(clist), 0,
					   GTK_JUSTIFY_CENTER);
	gtk_clist_set_column_width(GTK_CLIST(clist), 0, 16);
	gtk_clist_set_column_width(GTK_CLIST(clist), 1, 160);

	progress->window      = window;
	progress->label       = label;
	progress->cancel_btn  = cancel_btn;
	progress->progressbar = progressbar;
	progress->clist       = clist;

	return progress;
}

void progress_dialog_set_label(ProgressDialog *progress, gchar *str)
{
	gtk_label_set_text(GTK_LABEL(progress->label), str);
}

void progress_dialog_set_value(ProgressDialog *progress, gfloat value)
{
	gtk_progress_set_value(GTK_PROGRESS(progress->progressbar), value);
}

void progress_dialog_set_percentage(ProgressDialog *progress,
				    gfloat percentage)
{
	gtk_progress_set_percentage(GTK_PROGRESS(progress->progressbar),
				    percentage);
}

void progress_dialog_destroy(ProgressDialog *progress)
{
	if (progress) {
		gtk_widget_destroy(progress->window);
		g_free(progress);
	}
}
