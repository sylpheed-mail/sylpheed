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

#ifndef __ADDRESSBOOK_H__
#define __ADDRESSBOOK_H__

#include <glib.h>
#include <gtk/gtkwidget.h>

#include "compose.h"

typedef gint (*AddressBookCompletionFunc) (const gchar *name, const gchar *firstname, const gchar *lastname, const gchar *nickname, const gchar *address);

void addressbook_open			(Compose	*target);
void addressbook_set_target_compose	(Compose	*target);
Compose *addressbook_get_target_compose	(void);
void addressbook_read_file		(void);
void addressbook_export_to_file		(void);
gint addressbook_obj_name_compare	(gconstpointer	 a,
					 gconstpointer	 b);
/* static gint addressbook_obj_name_compare(gconstpointer	 a,
 					 gconstpointer	 b); */

/* provisional API for accessing the address book */

void addressbook_access (void);
void addressbook_unaccess (void);

gboolean addressbook_add_contact	(const gchar	*name,
					 const gchar	*address,
					 const gchar	*remarks);
gboolean addressbook_add_contact_autoreg(const gchar	*name,
					 const gchar	*address,
					 const gchar	*remarks);

gboolean addressbook_load_completion_full
					(AddressBookCompletionFunc func);
gboolean addressbook_load_completion	( gint (*callBackFunc) ( const gchar *, const gchar *, const gchar * ) );

gboolean addressbook_has_address	(const gchar	*address);

gboolean addressbook_import_ldif_file	(const gchar	*file,
					 const gchar	*book_name);

#endif /* __ADDRESSBOOK_H__ */
