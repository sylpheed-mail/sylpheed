/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2001-2010 Hiroyuki Yamamoto & The Sylpheed Claws Team
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

#if !defined(COLORLABEL_H__)
#define COLORLABEL_H__

#include <glib.h>

gint colorlabel_get_color_count			(void);
GdkColor colorlabel_get_color			(gint		 color_index);
const gchar *colorlabel_get_color_text		(gint		 color_index);
const gchar *colorlabel_get_custom_color_text	(gint		 color_index);
void colorlabel_set_color_text			(gint		 color_index,
						 const gchar	*label);
gboolean colorlabel_changed			(void);
GtkWidget *colorlabel_create_color_widget	(GdkColor	 color);
GtkWidget *colorlabel_create_check_color_menu_item
						(gint		 color_index);
GtkWidget *colorlabel_create_color_menu		(void);
guint colorlabel_get_color_menu_active_item	(GtkWidget	*menu);
void colorlabel_update_menu			(void);

gint colorlabel_read_config			(void);
gint colorlabel_write_config			(void);

#endif /* COLORLABEL_H__ */
