/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2012 Hiroyuki Yamamoto
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
#include <gtk/gtk.h>

#include "plugin.h"
#include "plugin_manager.h"
#include "manage_window.h"
#include "alertpanel.h"
#include "gtkutils.h"
#include "update_check.h"
#include "utils.h"

static struct PluginManagerWindow {
	GtkWidget *window;
	GtkWidget *close_btn;

	GtkWidget *treeview;
	GtkListStore *store;
	GtkTreeSelection *selection;
} pm_window;

enum {
	COL_INFO,
	N_COLS
};

static void plugin_manager_create	(void);
static void plugin_manager_set_list_row	(GtkTreeIter	*iter,
					 SylPluginInfo	*info,
					 const gchar	*filename);
static gint plugin_manager_deleted	(GtkWidget	*widget,
					 GdkEventAny	*event,
					 gpointer	 data);
static gboolean key_pressed		(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gpointer	 data);

void plugin_manager_open(void)
{
	GSList *list, *cur;
	SylPluginInfo *info;
	GModule *module;
	const gchar *filename;

	if (!pm_window.window)
		plugin_manager_create();
	else
		gtk_window_present(GTK_WINDOW(pm_window.window));

	list = syl_plugin_get_module_list();

	gtk_list_store_clear(pm_window.store);

	for (cur = list; cur != NULL; cur = cur->next) {
		module = (GModule *)cur->data;

		filename = g_module_name(module);
		info = syl_plugin_get_info(module);
		if (info) {
			debug_print("------------------------------\n");
			debug_print("filename      : %s\n", filename);
			debug_print("plugin name   : %s\n", info->name);
			debug_print("plugin version: %s\n", info->version);
			debug_print("plugin author : %s\n", info->author);
			debug_print("description   : %s\n", info->description ? info->description : "");
			debug_print("------------------------------\n");
			plugin_manager_set_list_row(NULL, info, filename);
		} else {
			debug_print("info not found: %s\n", filename);
		}
	}

	gtk_widget_show(pm_window.window);
	manage_window_focus_in(pm_window.window, NULL, NULL);

	syl_plugin_signal_emit("plugin-manager-open", pm_window.window);
}

#ifdef USE_UPDATE_CHECK_PLUGIN
static gint plugin_manager_update_check(void)
{
	update_check_plugin(TRUE);
	return TRUE;
}
#endif /* USE_UPDATE_CHECK_PLUGIN */

static void plugin_manager_create(void)
{
	GtkWidget *window;
	GtkWidget *vbox;
#ifdef USE_UPDATE_CHECK_PLUGIN
	GtkWidget *update_check_btn;
#endif
	GtkWidget *close_btn;
	GtkWidget *confirm_area;

	GtkWidget *scrolledwin;
	GtkWidget *treeview;
	GtkListStore *store;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), _("Plug-in manager"));
	gtk_widget_set_size_request(window, 600, 400);
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, TRUE);
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	gtkut_stock_button_set_create(&confirm_area,
#ifdef USE_UPDATE_CHECK_PLUGIN
				      &update_check_btn, _("Check for _update"),
				      &close_btn, GTK_STOCK_CLOSE,
				      NULL, NULL);
	gtkut_box_set_reverse_order(GTK_BOX(confirm_area), TRUE);
#else
				      &close_btn, GTK_STOCK_CLOSE,
				      NULL, NULL,
				      NULL, NULL);
#endif
	gtk_widget_show(confirm_area);
	gtk_box_pack_end(GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default(close_btn);

	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(plugin_manager_deleted), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(key_pressed), NULL);
#ifdef USE_UPDATE_CHECK_PLUGIN
	g_signal_connect(G_OBJECT(update_check_btn), "clicked",
			 G_CALLBACK(plugin_manager_update_check), NULL);
#endif
	g_signal_connect(G_OBJECT(close_btn), "clicked",
			 G_CALLBACK(plugin_manager_deleted), NULL);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scrolledwin);
	gtk_widget_set_size_request(scrolledwin, -1, -1);
	gtk_box_pack_start(GTK_BOX(vbox), scrolledwin, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_SHADOW_IN);

	store = gtk_list_store_new(N_COLS, G_TYPE_STRING);

	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(G_OBJECT(store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), TRUE);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(treeview), COL_INFO);
#if GTK_CHECK_VERSION(2, 10, 0)
	gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(treeview),
				     GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);
#endif

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes
		(_("Plug-in information"), renderer, "text", COL_INFO, NULL);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	gtk_widget_show(treeview);
	gtk_container_add(GTK_CONTAINER(scrolledwin), treeview);

	gtk_widget_show_all(window);

	pm_window.window = window;
	pm_window.close_btn = close_btn;

	pm_window.treeview = treeview;
	pm_window.store = store;
	pm_window.selection = selection;
}

static void plugin_manager_set_list_row(GtkTreeIter *iter, SylPluginInfo *info,
					const gchar *filename)
{
	GtkListStore *store = pm_window.store;
	GtkTreeIter iter_;
	gchar *plugin_info;

	g_return_if_fail(info != NULL);
	g_return_if_fail(filename != NULL);

	plugin_info = g_strconcat(info->name ? info->name : _("(Unknown)"),
				  "  ", info->version ? info->version : "", "\n",
				  _("Author: "), info->author ? info->author : _("(Unknown)"), "\n",
				  _("File: "), filename ? filename : _("(Unknown)"),
				  info->description ? "\n" : "",
				  info->description ? _("Description: ") : "",
				  info->description ? info->description : "",
				  NULL);

	if (iter)
		iter_ = *iter;
	else
		gtk_list_store_append(store, &iter_);

	gtk_list_store_set(store, &iter_, COL_INFO, plugin_info, -1);

	g_free(plugin_info);
}

static gint plugin_manager_deleted(GtkWidget *widget, GdkEventAny *event,
				   gpointer data)
{
	gtk_widget_hide(pm_window.window);
	return TRUE;
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event,
			    gpointer data)
{
	if (event && event->keyval == GDK_Escape) {
		gtk_widget_hide(pm_window.window);
		return TRUE;
	}

	return FALSE;
}
