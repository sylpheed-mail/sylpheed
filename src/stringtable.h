/*
 * sylpheed -- a gtk+ based, lightweight, and fast e-mail client
 * copyright (c) 1999-2001 hiroyuki yamamoto
 *
 * this program is free software; you can redistribute it and/or modify
 * it under the terms of the gnu general public license as published by
 * the free software foundation; either version 2 of the license, or
 * (at your option) any later version.
 *
 * this program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * merchantability or fitness for a particular purpose.  see the
 * gnu general public license for more details.
 *
 * you should have received a copy of the gnu general public license
 * along with this program; if not, write to the free software
 * foundation, inc., 59 temple place - suite 330, boston, ma 02111-1307, usa.
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
