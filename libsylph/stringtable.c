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

#include <glib.h>
#include <string.h>

#include "stringtable.h"
#include "utils.h"

/* alfons - hashed string table (I wasn't content with GStringChunk; 
 * can't recall why :-) */

#if 0
#define XXX_DEBUG \
	debug_print
#else
#define XXX_DEBUG \
	if (0) debug_print
#endif

typedef struct StringEntry_ {
	gint	ref_count;
	gchar  *string;
} StringEntry;

static StringEntry *string_entry_new(const gchar *str)
{
	StringEntry *entry;

	entry = g_new0(StringEntry, 1);
	entry->ref_count = 1;
	entry->string = g_strdup(str);
	return entry;
}

static void string_entry_free(StringEntry *entry)
{
	g_return_if_fail(entry != NULL);

	g_free(entry->string);
	g_free(entry);
}

StringTable *string_table_new(void)
{
	StringTable *strtable;

	strtable = g_new0(StringTable, 1);
	g_return_val_if_fail(strtable != NULL, NULL);
	strtable->hash_table = g_hash_table_new(g_str_hash, g_str_equal);
	g_return_val_if_fail(strtable->hash_table, NULL);
	return strtable;
}

gchar *string_table_lookup_string(StringTable *table, const gchar *str)
{
	StringEntry *entry;

	entry = g_hash_table_lookup(table->hash_table, str);

	if (entry) {
		return entry->string;
	} else {
		return NULL;
	}
}

gchar *string_table_insert_string(StringTable *table, const gchar *str)
{
	StringEntry *entry;

	entry = g_hash_table_lookup(table->hash_table, str);

	if (entry) {
		entry->ref_count++;
		XXX_DEBUG ("ref++ for %s (%d)\n", entry->string,
			   entry->ref_count);
	} else {
		entry = string_entry_new(str);
		XXX_DEBUG ("inserting %s\n", str);
		/* insert entry->string instead of str, since it can be
		 * invalid pointer after this. */
		g_hash_table_insert(table->hash_table, entry->string, entry);
	}

	return entry->string;
}

void string_table_free_string(StringTable *table, const gchar *str)
{
	StringEntry *entry;

	entry = g_hash_table_lookup(table->hash_table, str);

	if (entry) {
		entry->ref_count--;
		if (entry->ref_count <= 0) {
			XXX_DEBUG ("refcount of string %s dropped to zero\n",
				   entry->string);
			g_hash_table_remove(table->hash_table, str);
			string_entry_free(entry);
		} else {
			XXX_DEBUG ("ref-- for %s (%d)\n", entry->string,
				   entry->ref_count); 
		}
	}
}

static gboolean string_table_remove_for_each_fn(gchar *key, StringEntry *entry,
						gpointer user_data)
{
	g_return_val_if_fail(key != NULL, TRUE);
	g_return_val_if_fail(entry != NULL, TRUE);

	string_entry_free(entry);

	return TRUE;
}

void string_table_free(StringTable *table)
{
	g_return_if_fail(table != NULL);
	g_return_if_fail(table->hash_table != NULL);

	g_hash_table_foreach_remove(table->hash_table,
				    (GHRFunc)string_table_remove_for_each_fn,
				    NULL);
	g_hash_table_destroy(table->hash_table);
	g_free(table);
}

static void string_table_stats_for_each_fn(gchar *key, StringEntry *entry,
					   guint *totals)
{
	if (entry->ref_count > 1) {
		*totals += strlen(key) * (entry->ref_count - 1);
	}
}

void string_table_get_stats(StringTable *table)
{
	guint totals = 0;

	g_hash_table_foreach(table->hash_table,
			     (GHFunc)string_table_stats_for_each_fn, &totals);
	XXX_DEBUG ("TOTAL UNSPILLED %d (%dK)\n", totals, totals / 1024);
}
