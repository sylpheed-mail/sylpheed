/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * 
 * Copyright (C) 2000-2004 by Alfons Hoogervorst & The Sylpheed Claws Team.
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
 
#ifndef __ADDR_COMPL_H__
#define __ADDR_COMPL_H__

gint start_address_completion		(void);
gint invalidate_address_completion	(void);

guint complete_address			(const gchar	*str);

gchar *get_address_from_edit		(GtkEntry	*entry,
					 gint		*start_pos);
void replace_address_in_edit		(GtkEntry	*entry,
					 const gchar	*newtext,
					 gint		 start_pos);

gchar *get_complete_address		(gint		 index);

gchar *get_next_complete_address	(void);
gchar *get_prev_complete_address	(void);
guint get_completion_count		(void);

gboolean is_completion_pending		(void);

void clear_completion_cache		(void);

gint end_address_completion		(void);

/* ui functions */

void address_completion_start	(GtkWidget *mainwindow);
void address_completion_register_entry	(GtkEntry *entry);
void address_completion_unregister_entry (GtkEntry *entry);
void address_completion_end	(GtkWidget *mainwindow);

#endif /* __ADDR_COMPL_H__ */
