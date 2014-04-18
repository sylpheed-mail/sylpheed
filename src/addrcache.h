/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2001 Match Grun
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

/*
 * Definitions for address cache.
 */

#ifndef __ADDRCACHE_H__
#define __ADDRCACHE_H__

#include <time.h>
#include <stdio.h>
#include <glib.h>
#include "addritem.h"
#include "utils.h"

/* Address cache */
typedef struct _AddressCache AddressCache;

struct _AddressCache {
	gint       nextID;
	gboolean   dataRead;
	gboolean   modified;
	stime_t    modifyTime;
	GHashTable *itemHash;
	GList      *tempList;
	ItemFolder *rootFolder;
};

/* Function prototypes */
AddressCache *addrcache_create();
ItemFolder *addrcache_get_root_folder	( AddressCache *cache );
GList *addrcache_get_list_folder	( AddressCache *cache );
GList *addrcache_get_list_person	( AddressCache *cache );

void addrcache_refresh			( AddressCache *cache );
/* void addrcache_empty			( AddressCache *cache ); */
void addrcache_clear			( AddressCache *cache );
void addrcache_free			( AddressCache *cache );
gboolean addrcache_check_file		( AddressCache *cache, gchar *path );
gboolean addrcache_mark_file		( AddressCache *cache, gchar *path );

void addrcache_print_item_list		( GList *list, FILE *stream );
void addrcache_print			( AddressCache *cache, FILE *stream );
void addrcache_dump_hash		( AddressCache *cache, FILE *stream );

void addrcache_id_person		( AddressCache *cache, ItemPerson *person );
void addrcache_id_group			( AddressCache *cache, ItemGroup *group );
void addrcache_id_folder		( AddressCache *cache, ItemFolder *folder );
void addrcache_id_email			( AddressCache *cache, ItemEMail *email );
void addrcache_id_attribute		( AddressCache *cache, UserAttribute *attrib );

gboolean addrcache_hash_add_person	( AddressCache *cache, ItemPerson *person );
gboolean addrcache_hash_add_group	( AddressCache *cache, ItemGroup *group );
gboolean addrcache_hash_add_folder	( AddressCache *cache, ItemFolder *folder );

gboolean addrcache_folder_add_person	( AddressCache *cache, ItemFolder *folder, ItemPerson *item );
gboolean addrcache_folder_add_folder	( AddressCache *cache, ItemFolder *folder, ItemFolder *item );
gboolean addrcache_folder_add_group	( AddressCache *cache, ItemFolder *folder, ItemGroup *item );

gboolean addrcache_add_person		( AddressCache *cache, ItemPerson *person );
gboolean addrcache_add_group		( AddressCache *cache, ItemGroup *group );
gboolean addrcache_person_add_email	( AddressCache *cache, ItemPerson *person, ItemEMail *email );
gboolean addrcache_group_add_email	( AddressCache *cache, ItemGroup *group, ItemEMail *email );
gboolean addrcache_add_folder		( AddressCache *cache, ItemFolder *folder );

AddrItemObject *addrcache_get_object	( AddressCache *cache, const gchar *uid );
ItemPerson *addrcache_get_person	( AddressCache *cache, const gchar *uid );
ItemGroup *addrcache_get_group		( AddressCache *cache, const gchar *uid );
ItemEMail *addrcache_get_email		( AddressCache *cache, const gchar *uid, const gchar *eid );

UserAttribute *addrcache_person_remove_attrib_id	( AddressCache *cache, const gchar *uid,
							  const gchar *aid );
UserAttribute *addrcache_person_remove_attribute	( AddressCache *cache, ItemPerson *person,
							  UserAttribute *attrib );

ItemGroup *addrcache_remove_group_id		( AddressCache *cache, const gchar *uid );
ItemGroup *addrcache_remove_group		( AddressCache *cache, ItemGroup *group );

ItemPerson *addrcache_remove_person_id		( AddressCache *cache, const gchar *uid );
ItemPerson *addrcache_remove_person		( AddressCache *cache, ItemPerson *person );
ItemEMail *addrcache_person_remove_email_id	( AddressCache *cache, const gchar *uid, const gchar *eid );
ItemEMail *addrcache_person_remove_email	( AddressCache *cache, ItemPerson *person, ItemEMail *email );

GList *addrcache_folder_get_address_list	( AddressCache *cache, ItemFolder *folder );
GList *addrcache_folder_get_person_list		( AddressCache *cache, ItemFolder *folder );
GList *addrcache_folder_get_group_list		( AddressCache *cache, ItemFolder *folder );
GList *addrcache_folder_get_folder_list		( AddressCache *cache, ItemFolder *folder );

GList *addrcache_get_address_list		( AddressCache *cache );
GList *addrcache_get_person_list		( AddressCache *cache );
GList *addrcache_get_group_list			( AddressCache *cache );
GList *addrcache_get_folder_list		( AddressCache *cache );

GList *addrcache_get_group_for_person		( AddressCache *cache, ItemPerson *person );

ItemFolder *addrcache_find_root_folder		( ItemFolder *folder );
GList *addrcache_get_all_persons		( AddressCache *cache );
GList *addrcache_get_all_groups			( AddressCache *cache );

ItemFolder *addrcache_remove_folder		( AddressCache *cache, ItemFolder *folder );
ItemFolder *addrcache_remove_folder_delete	( AddressCache *cache, ItemFolder *folder );

ItemPerson *addrcache_add_contact		( AddressCache *cache, ItemFolder *folder,
						  const gchar *name, const gchar *address,
						  const gchar *remarks ); 

#endif /* __ADDRCACHE_H__ */
