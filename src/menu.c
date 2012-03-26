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

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenubar.h>
#include <gtk/gtkcheckmenuitem.h>
#include <gtk/gtkitemfactory.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkwindow.h>

#include "menu.h"
#include "utils.h"

static gchar *menu_translate(const gchar *path, gpointer data);

GtkWidget *menubar_create(GtkWidget *window, GtkItemFactoryEntry *entries,
			  guint n_entries, const gchar *path, gpointer data)
{
	GtkItemFactory *factory;

	factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, path, NULL);
	gtk_item_factory_set_translate_func(factory, menu_translate,
					    NULL, NULL);
	gtk_item_factory_create_items(factory, n_entries, entries, data);
	gtk_window_add_accel_group(GTK_WINDOW(window), factory->accel_group);

	return gtk_item_factory_get_widget(factory, path);
}

GtkWidget *menu_create_items(GtkItemFactoryEntry *entries,
			     guint n_entries, const gchar *path,
			     GtkItemFactory **factory, gpointer data)
{
	*factory = gtk_item_factory_new(GTK_TYPE_MENU, path, NULL);
	gtk_item_factory_set_translate_func(*factory, menu_translate,
					    NULL, NULL);
	gtk_item_factory_create_items(*factory, n_entries, entries, data);

	return gtk_item_factory_get_widget(*factory, path);
}

static gchar *menu_translate(const gchar *path, gpointer data)
{
	gchar *retval;

	retval = gettext(path);

	return retval;
}

#if 0
static void factory_print_func(gpointer data, gchar *string)
{
	GString *out_str = data;

	g_string_append(out_str, string);
	g_string_append_c(out_str, '\n');
}

GString *menu_factory_get_rc(const gchar *path)
{
	GString *string;
	GtkPatternSpec *pspec;
	gchar pattern[256];

	pspec = g_new(GtkPatternSpec, 1);
	g_snprintf(pattern, sizeof(pattern), "%s*", path);
	gtk_pattern_spec_init(pspec, pattern);
	string = g_string_new("");
	gtk_item_factory_dump_items(pspec, FALSE, factory_print_func,
				    string);
	gtk_pattern_spec_free_segs(pspec);

	return string;
}

void menu_factory_clear_rc(const gchar *rc_str)
{
	GString *string;
	gchar *p;
	gchar *start, *end;
	guint pos = 0;

	string = g_string_new(rc_str);
	while ((p = strstr(string->str + pos, "(menu-path \"")) != NULL) {
		pos = p + 12 - string->str;
		p = strchr(p + 12, '"');
		if (!p) continue;
		start = strchr(p + 1, '"');
		if (!start) continue;
		end = strchr(start + 1, '"');
		if (!end) continue;
		pos = start + 1 - string->str;
		if (end > start + 1)
			g_string_erase(string, pos, end - (start + 1));
	}

	gtk_item_factory_parse_rc_string(string->str);
	g_string_free(string, TRUE);
}

void menu_factory_copy_rc(const gchar *src_path, const gchar *dest_path)
{
	GString *string;
	gint src_path_len;
	gint dest_path_len;
	gchar *p;
	guint pos = 0;

	string = menu_factory_get_rc(src_path);
	src_path_len = strlen(src_path);
	dest_path_len = strlen(dest_path);

	while ((p = strstr(string->str + pos, src_path)) != NULL) {
		pos = p - string->str;
		g_string_erase(string, pos, src_path_len);
		g_string_insert(string, pos, dest_path);
		pos += dest_path_len;
	}

	pos = 0;
	while ((p = strchr(string->str + pos, ';')) != NULL) {
		pos = p - string->str;
		if (pos == 0 || *(p - 1) == '\n')
			g_string_erase(string, pos, 1);
	}

	menu_factory_clear_rc(string->str);
	gtk_item_factory_parse_rc_string(string->str);
	g_string_free(string, TRUE);
}
#endif

void menu_set_sensitive(GtkItemFactory *ifactory, const gchar *path,
			gboolean sensitive)
{
	GtkWidget *widget;

	g_return_if_fail(ifactory != NULL);

	widget = gtk_item_factory_get_item(ifactory, path);
	gtk_widget_set_sensitive(widget, sensitive);
}

void menu_set_sensitive_all(GtkMenuShell *menu_shell, gboolean sensitive)
{
	GList *cur;

	for (cur = menu_shell->children; cur != NULL; cur = cur->next)
		gtk_widget_set_sensitive(GTK_WIDGET(cur->data), sensitive);
}

void menu_set_active(GtkItemFactory *ifactory, const gchar *path,
		     gboolean is_active)
{
	GtkWidget *widget;

	g_return_if_fail(ifactory != NULL);

	widget = gtk_item_factory_get_item(ifactory, path);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widget), is_active);
}

void menu_button_position(GtkMenu *menu, gint *x, gint *y, gboolean *push_in,
			  gpointer user_data)
{
	GtkWidget *button;
	GtkRequisition requisition;
	gint button_xpos, button_ypos;
	gint xpos, ypos;
	gint width, height;
	gint scr_width, scr_height;

	g_return_if_fail(x != NULL && y != NULL && push_in != NULL);

	button = GTK_WIDGET(user_data);

	gtk_widget_get_child_requisition(GTK_WIDGET(menu), &requisition);
	width = requisition.width;
	height = requisition.height;
	gdk_window_get_origin(button->window, &button_xpos, &button_ypos);

	xpos = button_xpos + button->allocation.x;
	ypos = button_ypos + button->allocation.y + button->allocation.height;

	scr_width = gdk_screen_width();
	scr_height = gdk_screen_height();

	if (xpos + width > scr_width)
		xpos -= (xpos + width) - scr_width;
	if (ypos + height > scr_height)
		ypos -= button->allocation.height + height;
	if (xpos < 0)
		xpos = 0;
	if (ypos < 0)
		ypos = 0;

	*x = xpos;
	*y = ypos;
	*push_in = FALSE;
}

void menu_widget_position(GtkMenu *menu, gint *x, gint *y, gboolean *push_in,
			  gpointer user_data)
{
	GtkWidget *widget;
	GtkRequisition requisition;
	gint w_xpos, w_ypos;
	gint xpos, ypos;
	gint width, height;
	gint scr_width, scr_height;

	g_return_if_fail(x != NULL && y != NULL && push_in != NULL);

	widget = GTK_WIDGET(user_data);

	gtk_widget_get_child_requisition(GTK_WIDGET(menu), &requisition);
	width = requisition.width;
	height = requisition.height;
	gdk_window_get_origin(widget->window, &w_xpos, &w_ypos);

	xpos = w_xpos;
	ypos = w_ypos;

	scr_width = gdk_screen_width();
	scr_height = gdk_screen_height();

	if (xpos + width > scr_width)
		xpos -= (xpos + width) - scr_width;
	if (ypos + height > scr_height)
		ypos -= height;
	if (xpos < 0)
		xpos = 0;
	if (ypos < 0)
		ypos = 0;

	*x = xpos;
	*y = ypos;
	*push_in = FALSE;
}

gint menu_find_option_menu_index(GtkOptionMenu *optmenu, gpointer data,
				 GCompareFunc func)
{
	GtkWidget *menu;
	GtkWidget *menuitem;
	gpointer menu_data;
	GList *cur;
	gint n;

	menu = gtk_option_menu_get_menu(optmenu);

	for (cur = GTK_MENU_SHELL(menu)->children, n = 0;
	     cur != NULL; cur = cur->next, n++) {
		menuitem = GTK_WIDGET(cur->data);
		menu_data = g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID);
		if (func) {
			if (func(menu_data, data) == 0)
				return n;
		} else if (menu_data == data)
			return n;
	}

	return -1;
}

gint menu_get_option_menu_active_index(GtkOptionMenu *optmenu)
{
	GtkWidget *menu;
	GtkWidget *menuitem;

	menu = gtk_option_menu_get_menu(optmenu);
	menuitem = gtk_menu_get_active(GTK_MENU(menu));

	return GPOINTER_TO_INT
		(g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID));
}
