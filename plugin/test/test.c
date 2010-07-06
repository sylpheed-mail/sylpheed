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

#include <glib.h>
#include <gtk/gtk.h>

#include "sylmain.h"
#include "plugin.h"
#include "test.h"
#include "folder.h"

static SylPluginInfo info = {
	"Test Plugin",
	"3.0.99",
	"Hiroyuki Yamamoto",
	"Test plug-in for Sylpheed plug-in system"
};

static void init_done_cb(GObject *obj, gpointer data);
static void app_exit_cb(GObject *obj, gpointer data);

static void folderview_menu_popup_cb(GObject *obj, GtkItemFactory *ifactory,
				     gpointer data);
static void summaryview_menu_popup_cb(GObject *obj, GtkItemFactory *ifactory,
				      gpointer data);

static void menu_selected_cb(void);

static void compose_created_cb(GObject *obj, gpointer compose);
static void compose_destroy_cb(GObject *obj, gpointer compose);

static void create_window(void);
static void create_folderview_sub_widget(void);

void plugin_load(void)
{
	GList *list, *cur;
	const gchar *ver;
	gpointer mainwin;

	g_print("test plug-in loaded!\n");

	list = folder_get_list();
	g_print("folder list = %p\n", list);
	for (cur = list; cur != NULL; cur = cur->next) {
		Folder *folder = FOLDER(cur->data);
		gchar *id = folder_get_identifier(folder);
		g_print("folder id = %s\n", id);
	}

	ver = syl_plugin_get_prog_version();
	g_print("program ver: %s\n", ver);

	mainwin = syl_plugin_main_window_get();
	g_print("mainwin: %p\n", mainwin);
	syl_plugin_main_window_popup(mainwin);

	create_folderview_sub_widget();

	syl_plugin_add_menuitem("/Tools", NULL, NULL, NULL);
	syl_plugin_add_menuitem("/Tools", "Plugin test", create_window, NULL);

	g_signal_connect(syl_app_get(), "init-done", G_CALLBACK(init_done_cb),
			 NULL);
	g_signal_connect(syl_app_get(), "app-exit", G_CALLBACK(app_exit_cb),
			 NULL);
	syl_plugin_signal_connect("folderview-menu-popup",
				  G_CALLBACK(folderview_menu_popup_cb), NULL);
	syl_plugin_signal_connect("summaryview-menu-popup",
				  G_CALLBACK(summaryview_menu_popup_cb), NULL);
	syl_plugin_signal_connect("compose-created",
				  G_CALLBACK(compose_created_cb), NULL);
	syl_plugin_signal_connect("compose-destroy",
				  G_CALLBACK(compose_destroy_cb), NULL);

	syl_plugin_add_factory_item("<SummaryView>", "/---", NULL, NULL);
	syl_plugin_add_factory_item("<SummaryView>", "/Test Plug-in menu",
				    menu_selected_cb, NULL);

	g_print("test plug-in loading done\n");
}

void plugin_unload(void)
{
	g_print("test plug-in unloaded!\n");
}

SylPluginInfo *plugin_info(void)
{
	return &info;
}

gint plugin_interface_version(void)
{
	return SYL_PLUGIN_INTERFACE_VERSION;
}

static void init_done_cb(GObject *obj, gpointer data)
{
	g_print("test: %p: app init done\n", obj);
}

static void app_exit_cb(GObject *obj, gpointer data)
{
	g_print("test: %p: app will exit\n", obj);
}

static void folderview_menu_popup_cb(GObject *obj, GtkItemFactory *ifactory,
				     gpointer data)
{
	g_print("test: %p: folderview menu popup\n", obj);
}

static void summaryview_menu_popup_cb(GObject *obj, GtkItemFactory *ifactory,
				      gpointer data)
{
	GtkWidget *widget;

	g_print("test: %p: summaryview menu popup\n", obj);
	widget = gtk_item_factory_get_item(ifactory, "/Test Plug-in menu");
	if (widget)
		gtk_widget_set_sensitive(widget, TRUE);
}

static void menu_selected_cb(void)
{
	gint sel;
	GSList *mlist;

	g_print("test: summary menu selected\n");
	sel = syl_plugin_summary_get_selection_type();
	mlist = syl_plugin_summary_get_selected_msg_list();
	g_print("test: selection type: %d\n", sel);
	g_print("test: number of selected summary message: %d\n",
		g_slist_length(mlist));
	g_slist_free(mlist);
}

static void compose_created_cb(GObject *obj, gpointer compose)
{
	gchar *text;

	g_print("test: %p: compose created (%p)\n", obj, compose);

	text = syl_plugin_compose_entry_get_text(compose, 0);
	g_print("test: compose To: %s\n", text);
	g_free(text);
#if 0
	syl_plugin_compose_entry_set(compose, "test-plugin@test", 1);
	syl_plugin_compose_entry_append(compose, "second@test", 1);
#endif
}

static void compose_destroy_cb(GObject *obj, gpointer compose)
{
	g_print("test: %p: compose will be destroyed (%p)\n", obj, compose);
}

static void button_clicked(GtkWidget *widget, gpointer data)
{
	g_print("button_clicked\n");
	/* syl_plugin_app_will_exit(TRUE); */
}

static void create_window(void)
{
	GtkWidget *window;
	GtkWidget *button;

	g_print("creating window\n");

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	button = gtk_button_new_with_label("Click this button");
	gtk_window_set_default_size(GTK_WINDOW(window), 400, 200);
	gtk_container_add(GTK_CONTAINER(window), button);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(button_clicked), NULL);
	gtk_widget_show_all(window);
}

static void create_folderview_sub_widget(void)
{
	GtkWidget *vbox;
	GtkWidget *button;

	g_print("creating sub widget\n");

	vbox = gtk_vbox_new(FALSE, 2);
	button = gtk_button_new_with_label("Test");
	gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
	gtk_widget_show_all(vbox);
	syl_plugin_folderview_add_sub_widget(vbox);
}
