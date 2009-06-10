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

#include <glib.h>
#include <gtk/gtk.h>

#include "plugin.h"
#include "test.h"
#include "folder.h"

static SylPluginInfo info = {
	"Test Plugin",
	"1.0.0",
	"Hiroyuki Yamamoto",
	"Test plug-in for Sylpheed plug-in system"
};

static void create_window(void);

void plugin_load(void)
{
	GList *list, *cur;
	const gchar *ver;
	gpointer mainwin;

	g_print("test plug-in loaded!\n");
	list = folder_get_list();
	g_print("list = %p\n", list);
	for (cur = list; cur != NULL; cur = cur->next) {
		Folder *folder = FOLDER(cur->data);
		gchar *id = folder_get_identifier(folder);
		g_print("id = %s\n", id);
	}

	ver = syl_plugin_get_prog_version();
	g_print("ver: %s\n", ver);

	mainwin = syl_plugin_main_window_get();
	g_print("mainwin: %p\n", mainwin);
	syl_plugin_main_window_popup(mainwin);

	syl_plugin_add_menuitem("/Tools", NULL, NULL, NULL);
	syl_plugin_add_menuitem("/Tools", "Plugin test", create_window, NULL);

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

static void button_clicked(GtkWidget *widget, gpointer data)
{
	g_print("button_clicked\n");
	syl_plugin_app_will_exit(TRUE);
}

static void create_window(void)
{
	GtkWidget *window;
	GtkWidget *button;

	g_print("creating window\n");

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	button = gtk_button_new_with_label("Click this to quit");
	gtk_window_set_default_size(GTK_WINDOW(window), 400, 200);
	gtk_container_add(GTK_CONTAINER(window), button);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(button_clicked), NULL);
	gtk_widget_show_all(window);
}
