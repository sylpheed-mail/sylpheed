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
#include <gtk/gtkdialog.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkstock.h>

#include "progressdialog.h"
#include "gtkutils.h"
#include "utils.h"

#define PROGRESS_DIALOG_WIDTH		460
#define PROGRESS_TREE_VIEW_HEIGHT	120

ProgressDialog *progress_dialog_create(void)
{
	ProgressDialog *progress;
	GtkWidget *scrolledwin;
	GtkWidget *treeview;
	GtkListStore *store;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	progress = progress_dialog_simple_create();

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scrolledwin);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(progress->window)->vbox),
			   scrolledwin, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_SHADOW_IN);

	store = gtk_list_store_new(PROG_N_COLS, GDK_TYPE_PIXBUF, G_TYPE_STRING,
				   G_TYPE_STRING, G_TYPE_STRING,
				   G_TYPE_POINTER);

	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(G_OBJECT(store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), TRUE);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);
	gtk_widget_show(treeview);
	gtk_container_add(GTK_CONTAINER(scrolledwin), treeview);
	gtk_widget_set_size_request(treeview, -1, PROGRESS_TREE_VIEW_HEIGHT * gtkut_get_dpi_multiplier());

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);

	renderer = gtk_cell_renderer_pixbuf_new();
	g_object_set(renderer, "xalign", 0.5, NULL);
	column = gtk_tree_view_column_new_with_attributes
		(NULL, renderer, "pixbuf", PROG_COL_PIXBUF, NULL);
	gtk_tree_view_column_set_alignment(column, 0.5);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, 20);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes
		(_("Account"), renderer, "text", PROG_COL_NAME, NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, 120 * gtkut_get_dpi_multiplier());
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes
		(_("Status"), renderer, "text", PROG_COL_STATUS, NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, 100 * gtkut_get_dpi_multiplier());
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes
		(_("Progress"), renderer, "text", PROG_COL_PROGRESS, NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	progress->treeview = treeview;
	progress->store    = store;

	return progress;
}

ProgressDialog *progress_dialog_simple_create(void)
{
	ProgressDialog *progress;
	GtkWidget *dialog;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *cancel_btn;
	GtkWidget *progressbar;

	debug_print("Creating progress dialog\n");
	progress = g_new0(ProgressDialog, 1);

	dialog = gtk_dialog_new();
	gtk_widget_set_size_request(dialog, PROGRESS_DIALOG_WIDTH * gtkut_get_dpi_multiplier(), -1);
	gtk_container_set_border_width(GTK_CONTAINER(dialog), 8);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, TRUE, TRUE);
	gtk_widget_realize(dialog);

	gtk_container_set_border_width
		(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area), 0);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog)->vbox), 8);
	gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
			   FALSE, FALSE, 8);
	gtk_widget_show(hbox);

	label = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 8);
	gtk_widget_show(label);

	cancel_btn = gtk_dialog_add_button(GTK_DIALOG(dialog),
					   GTK_STOCK_CANCEL,
					   GTK_RESPONSE_NONE);
	gtk_widget_grab_default(cancel_btn);
	gtk_widget_grab_focus(cancel_btn);

	progressbar = gtk_progress_bar_new();
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), progressbar,
			   FALSE, FALSE, 0);
	gtk_widget_show(progressbar);

	progress->window      = dialog;
	progress->label       = label;
	progress->cancel_btn  = cancel_btn;
	progress->progressbar = progressbar;
	progress->treeview    = NULL;
	progress->store       = NULL;

	return progress;
}

void progress_dialog_destroy(ProgressDialog *progress)
{
	if (progress) {
		gtk_widget_destroy(progress->window);
		g_free(progress);
	}
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

void progress_dialog_append(ProgressDialog *progress, GdkPixbuf *pixbuf,
			    const gchar *name, const gchar *status,
			    const gchar *progress_str, gpointer data)
{
	GtkListStore *store = progress->store;
	GtkTreeIter iter;

	gtk_list_store_append(store, &iter);

	gtk_list_store_set(store, &iter,
			   PROG_COL_PIXBUF, pixbuf,
			   PROG_COL_NAME, name,
			   PROG_COL_STATUS, status,
			   PROG_COL_PROGRESS, progress_str,
			   PROG_COL_POINTER, data,
			   -1);
}

void progress_dialog_set_row(ProgressDialog *progress, gint row,
			     GdkPixbuf *pixbuf, const gchar *name,
			     const gchar *status, const gchar *progress_str,
			     gpointer data)
{
	GtkListStore *store = progress->store;
	GtkTreeIter iter;

	if (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store),
					  &iter, NULL, row)) {
		gtk_list_store_set(store, &iter,
				   PROG_COL_PIXBUF, pixbuf,
				   PROG_COL_NAME, name,
				   PROG_COL_STATUS, status,
				   PROG_COL_PROGRESS, progress_str,
				   PROG_COL_POINTER, data,
				   -1);
	}
}

void progress_dialog_set_row_pixbuf(ProgressDialog *progress, gint row,
				    GdkPixbuf *pixbuf)
{
	GtkListStore *store = progress->store;
	GtkTreeIter iter;

	if (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store),
					  &iter, NULL, row)) {
		gtk_list_store_set(store, &iter, PROG_COL_PIXBUF, pixbuf, -1);
	}
}

void progress_dialog_set_row_name(ProgressDialog *progress, gint row,
				  const gchar *name)
{
	GtkListStore *store = progress->store;
	GtkTreeIter iter;

	if (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store),
					  &iter, NULL, row)) {
		gtk_list_store_set(store, &iter, PROG_COL_NAME, name, -1);
	}
}

void progress_dialog_set_row_status(ProgressDialog *progress, gint row,
				    const gchar *status)
{
	GtkListStore *store = progress->store;
	GtkTreeIter iter;

	if (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store),
					  &iter, NULL, row)) {
		gtk_list_store_set(store, &iter, PROG_COL_STATUS, status, -1);
	}
}

void progress_dialog_set_row_progress(ProgressDialog *progress, gint row,
				      const gchar *progress_str)
{
	GtkListStore *store = progress->store;
	GtkTreeIter iter;

	if (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store),
					  &iter, NULL, row)) {
		gtk_list_store_set(store, &iter, PROG_COL_PROGRESS,
				   progress_str, -1);
	}
}

void progress_dialog_scroll_to_row(ProgressDialog *progress, gint row)
{
	GtkTreeModel *model = GTK_TREE_MODEL(progress->store);
	GtkTreeIter iter;
	GtkTreePath *path;

	if (!gtk_tree_model_iter_nth_child(model, &iter, NULL, row))
		return;

	path = gtk_tree_model_get_path(model, &iter);
	gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(progress->treeview),
				     path, NULL, FALSE, 0.0, 0.0);
	gtk_tree_path_free(path);
}
