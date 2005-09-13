/*
 * LibSylph -- E-Mail client library
 * Copyright (C) 1999-2005 Hiroyuki Yamamoto
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef STRINGTABLE_H__
#define STRINGTABLE_H__

#include <glib.h>

typedef struct {
	GHashTable *hash_table;
} StringTable;

StringTable *string_table_new     (void);
void         string_table_free    (StringTable *table);

gchar *string_table_lookup_string (StringTable *table, const gchar *str);
gchar *string_table_insert_string (StringTable *table, const gchar *str);
void   string_table_free_string   (StringTable *table, const gchar *str);

void   string_table_get_stats     (StringTable *table);

#endif /* STRINGTABLE_H__ */
