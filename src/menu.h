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

#ifndef __MENU_H__
#define __MENU_H__

#include <glib.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkitemfactory.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenushell.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkimagemenuitem.h>
#include <gtk/gtkradiomenuitem.h>

#define MENU_VAL_ID "Sylpheed::Menu::ValueID"

#define MENUITEM_ADD(menu, menuitem, label, data)			\
{									\
	if (label)							\
		menuitem = gtk_menu_item_new_with_label(label);		\
	else {								\
		menuitem = gtk_menu_item_new();				\
		gtk_widget_set_sensitive(menuitem, FALSE);		\
	}								\
	gtk_widget_show(menuitem);					\
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);		\
	if (data)							\
		g_object_set_data(G_OBJECT(menuitem),			\
				  MENU_VAL_ID,				\
				  GINT_TO_POINTER(data));		\
}

#define MENUITEM_ADD_WITH_MNEMONIC(menu, menuitem, label, data)		\
{									\
	if (label)							\
		menuitem = gtk_menu_item_new_with_mnemonic(label);	\
	else {								\
		menuitem = gtk_menu_item_new();				\
		gtk_widget_set_sensitive(menuitem, FALSE);		\
	}								\
	gtk_widget_show(menuitem);					\
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);		\
	if (data)							\
		g_object_set_data(G_OBJECT(menuitem),			\
				  MENU_VAL_ID,				\
				  GINT_TO_POINTER(data));		\
}

#define MENUITEM_ADD_FROM_STOCK(menu, menuitem, label, data)		\
{									\
	if (label)							\
		menuitem = gtk_image_menu_item_new_from_stock(label, NULL); \
	else {								\
		menuitem = gtk_menu_item_new();				\
		gtk_widget_set_sensitive(menuitem, FALSE);		\
	}								\
	gtk_widget_show(menuitem);					\
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);		\
	if (data)							\
		g_object_set_data(G_OBJECT(menuitem),			\
				  MENU_VAL_ID,				\
				  GINT_TO_POINTER(data));		\
}

#define MENUITEM_ADD_RADIO(menu, menuitem, widget, label, data)		\
{									\
	if (label) {							\
		if (widget)						\
			menuitem = gtk_radio_menu_item_new_with_mnemonic_from_widget \
				(GTK_RADIO_MENU_ITEM(widget), label);	\
		else							\
			menuitem = gtk_radio_menu_item_new_with_mnemonic \
				(NULL, label);				\
	} else {							\
		menuitem = gtk_menu_item_new();				\
		gtk_widget_set_sensitive(menuitem, FALSE);		\
	}								\
	gtk_widget_show(menuitem);					\
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);		\
	if (data)							\
		g_object_set_data(G_OBJECT(menuitem),			\
				  MENU_VAL_ID,				\
				  GINT_TO_POINTER(data));		\
}

#define menu_set_insensitive_all(menu_shell) \
	menu_set_sensitive_all(menu_shell, FALSE);

GtkWidget *menubar_create	(GtkWidget		*window,
				 GtkItemFactoryEntry	*entries,
				 guint			 n_entries,
				 const gchar		*path,
				 gpointer		 data);
GtkWidget *menu_create_items	(GtkItemFactoryEntry	*entries,
				 guint			 n_entries,
				 const gchar		*path,
				 GtkItemFactory	       **factory,
				 gpointer		 data);

GString *menu_factory_get_rc	(const gchar		*path);
void menu_factory_clear_rc	(const gchar		*rc_str);
void menu_factory_copy_rc	(const gchar		*src_path,
				 const gchar		*dest_path);

void menu_set_sensitive		(GtkItemFactory		*ifactory,
				 const gchar		*path,
				 gboolean		 sensitive);
void menu_set_sensitive_all	(GtkMenuShell		*menu_shell,
				 gboolean		 sensitive);

void menu_set_active		(GtkItemFactory		*ifactory,
				 const gchar		*path,
				 gboolean		 is_active);

void menu_button_position	(GtkMenu		*menu,
				 gint			*x,
				 gint			*y,
				 gboolean		*push_in,
				 gpointer		 user_data);
void menu_widget_position	(GtkMenu		*menu,
				 gint			*x,
				 gint			*y,
				 gboolean		*push_in,
				 gpointer		 user_data);

gint menu_find_option_menu_index(GtkOptionMenu		*optmenu,
				 gpointer		 data,
				 GCompareFunc		 func);

gint menu_get_option_menu_active_index
				(GtkOptionMenu		*optmenu);

#endif /* __MENU_H__ */
